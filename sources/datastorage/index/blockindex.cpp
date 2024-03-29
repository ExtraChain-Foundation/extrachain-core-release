/*
 * ExtraChain Core
 * Copyright (C) 2020 ExtraChain Foundation <extrachain@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "datastorage/index/blockindex.h"
#include <QDir>
#include <QFileInfoList>

BlockIndex::BlockIndex() {
    this->folderName = DataStorage::BLOCK_INDEX_FOLDER_NAME;
    this->sectionSize = Config::DataStorage::SECTION_SIZE;
    firstSavedId = loadFirstId();
    lastSavedId = loadLastId();
    QDir dir(DataStorage::BLOCKCHAIN_INDEX + '/' + folderName);
    QFileInfoList sectionList = dir.entryInfoList(QDir::Filter::Dirs | QDir::NoDotAndDotDot);
    int count = 0;
    for (auto &el : sectionList) {
        QFileInfoList files = el.dir().entryInfoList(QDir::Filter::Dirs | QDir::NoDotAndDotDot);
        count += files.size();
    }
    records = count;
}

BlockIndex::BlockIndex(const BigNumber &recordsLimit)
    : BlockIndex() {
    this->recordsLimit = recordsLimit;
    qDebug() << "BLOCK INDEX: constructor: recordLimits - " << recordsLimit;
}

BlockIndex::BlockIndex(const QString &folderName) {
    qDebug() << "BLOCK INDEX: constructor: folder name - " << folderName;
}

BlockIndex::BlockIndex(const QString &folderName, const BigNumber &recordsLimit)
    : BlockIndex(folderName) {
    this->recordsLimit = recordsLimit;
}

int BlockIndex::addBlock(const Block &block) {
    int result = this->add(block.getIndex(), block.serialize());
    return result;
}

Block BlockIndex::getLastBlock() const {
    BigNumber id = this->lastSavedId;
    qDebug() << "BLOCK INDEX: getLastBlock:"
             << "\n      last saved id - " << this->lastSavedId;
    while (id >= getFirstSavedId()) {
        Block block = this->getBlockById(id);
        //        qDebug() << "BLOCK - : " << block.serialize();
        if (!block.isEmpty()) {
            qDebug() << "\n      " << block.getIndex() << " block is not empty";
            return block;
        }
        --id;
    }

    return Block();
}

Block BlockIndex::getLastRealBlock() const {
    BigNumber id = this->lastSavedId;
    qDebug() << "BLOCK INDEX: getLastBlock:"
             << "\n      last saved id - " << this->lastSavedId;
    while (id >= getFirstSavedId()) {
        Block block = this->getBlockById(id);
        if ((!block.isEmpty()) && (block.getType() != Config::DUMMY_BLOCK_TYPE)) {
            return block;
        }
        --id;
    }

    return Block();
}

GenesisBlock BlockIndex::getLastGenesisBlock() const {
    BigNumber id = this->lastSavedId;
    qDebug() << "BLOCK INDEX: getLastGenesisBlock:"
             << "      last saved id - " << this->lastSavedId;
    while (id >= getFirstSavedId()) {
        GenesisBlock block = this->getGenesisBlockById(id);
        if (!block.isEmpty()) {
            qDebug() << "      " << block.getIndex() << " block is empty";
            return block;
        }
        --id;
    }
    return GenesisBlock();
}

GenesisBlock BlockIndex::getGenesisBlockById(const BigNumber &id) const {
    QByteArray serializedBlock = this->getById(id);
    if (!serializedBlock.isEmpty() && GenesisBlock::isGenesisBlock(serializedBlock)) {
        return GenesisBlock(serializedBlock);
    }
    return GenesisBlock();
}

Block BlockIndex::getBlockById(const BigNumber &id) const {
    QByteArray serializedBlock = this->getById(id);
    if (!serializedBlock.isEmpty()) {
        if (Block::isBlock(serializedBlock))
            return Block(serializedBlock);
        else if (GenesisBlock::isGenesisBlock(serializedBlock))
            return GenesisBlock(serializedBlock);
    } else {
        qDebug() << id << "is not block";
    }
    return Block();
}

QByteArray BlockIndex::getBlockDataById(const BigNumber &id) const {
    QByteArray serializedBlock = this->getById(id);
    //    qDebug() << "BLOCK: " << serializedBlock;
    if (!serializedBlock.isEmpty()) {
        return serializedBlock;
    } else {
        qDebug() << "is not block";
        return "";
    }
}

Block BlockIndex::getBlockByPosition(const BigNumber &position) const {
    BigNumber blockId = getFirstSavedId() + position;
    if (blockId <= this->lastSavedId) {
        Block block = this->getBlockById(blockId);
        return block;
    }
    return Block();
}

Block BlockIndex::getBlockByApprover(const BigNumber &approver) const {
    return getBlockByParam(approver, SearchEnum::BlockParam::Approver);
}

Block BlockIndex::getBlockByHash(const QByteArray &hash) const {
    return getBlockByParam(hash.toStdString(), SearchEnum::BlockParam::Hash);
}

Block BlockIndex::getBlockByData(const QByteArray &data) const {
    return getBlockByParam(data.toStdString(), SearchEnum::BlockParam::Data);
}

Block BlockIndex::getBlockByParam(const BigNumber &id, SearchEnum::BlockParam param) const {
    if (param == SearchEnum::BlockParam::Id) {
        return getBlockById(id);
    }

    BigNumber lastBlockId = getLastSavedId();

    // iteration from the last to the first Block
    while (lastBlockId >= getFirstSavedId()) {
        Block lastBlock = getBlockById(lastBlockId);
        switch (param) {
        case SearchEnum::BlockParam::Approver: {
            if (BigNumber(lastBlock.getApprover().toStdString()) == id)
                return lastBlock;
            break;
        }
        case SearchEnum::BlockParam::Data: {
            if (lastBlock.getData() == id.toStdString())
                return lastBlock;
            break;
        }
        case SearchEnum::BlockParam::Hash: {
            if (lastBlock.getHash() == id.toStdString())
                return lastBlock;
            break;
        }
        default:
            break;
        }
        --lastBlockId;
    }
    return Block();
}

Block BlockIndex::getLastRealBlockById() {
    BigNumber id = this->lastSavedId;
    while (id >= getFirstSavedId()) {
        Block block = this->getBlockById(id);
        if (!block.isEmpty() && block.getType() != Config::DUMMY_BLOCK_TYPE) {
            return block;
        }
        --id;
    }
    return Block();
}

std::pair<Transaction, QByteArray> BlockIndex::getLastTxByHash(const QByteArray &hash,
                                                               const QByteArray &token) const {
    return getLastTxByParam(hash.toStdString(), SearchEnum::TxParam::Hash, token);
}

std::pair<Transaction, QByteArray> BlockIndex::getLastTxByData(const std::string &data) const {
    return getLastTxByParam(data, SearchEnum::TxParam::Data, "token");
}

std::pair<Transaction, QByteArray> BlockIndex::getLastTxBySender(const BigNumber &id,
                                                                 const QByteArray &token) const {
    return getLastTxByParam(id.toStdString(), SearchEnum::TxParam::UserSender, token);
}

std::pair<Transaction, QByteArray> BlockIndex::getLastTxByReceiver(const BigNumber &id,
                                                                   const QByteArray &token) const {
    return getLastTxByParam(id.toStdString(), SearchEnum::TxParam::UserReceiver, token);
}

std::pair<Transaction, QByteArray> BlockIndex::getLastTxBySenderOrReceiver(const BigNumber &id,
                                                                           const QByteArray &token) const {
    return getLastTxByParam(id.toStdString(), SearchEnum::TxParam::UserSenderOrReceiver, token);
}

std::pair<Transaction, QByteArray>
BlockIndex::getLastTxBySenderOrReceiverAndToken(const BigNumber &id, const QByteArray &token) const {
    return getLastTxByParam(id.toStdString(), SearchEnum::TxParam::UserSenderOrReceiverOrToken, token);
}

std::pair<Transaction, QByteArray> BlockIndex::getLastTxByApprover(const BigNumber &id,
                                                                   const QByteArray &token) const {
    return getLastTxByParam(id.toStdString(), SearchEnum::TxParam::UserApprover, token);
}

QList<Transaction> BlockIndex::getTxsBySenderOrReceiverInRow(const BigNumber &id, BigNumber from, int count,
                                                             BigNumber token) const {
    return getTxsByParamInRow(id, SearchEnum::TxParam::UserSenderOrReceiver, from, count, token);
}

// QList<Transaction> BlockIndex::getRecentTxList(const BigNumber &last, const BigNumber &first) const {
//    QList<Transaction> txList;

//}

std::pair<Transaction, QByteArray> BlockIndex::getLastTxByParam(const std::string &id,
                                                                SearchEnum::TxParam param,
                                                                const QByteArray &token) const {
    BigNumber records = getRecords();
    ActorId tokenActor = token.toStdString();

    if (records == 0) {
        qDebug() << "There no tx's in blockIndex";
        return { Transaction(), "-1" };
    }

    BigNumber lastBlockId = getLastSavedId();

    // iterating from last to first block
    while (lastBlockId >= getFirstSavedId()) {
        Block lastBlock = getBlockById(lastBlockId);
        auto txs = lastBlock.extractTransactions();

        for (const Transaction &tx : txs) {
            if (tx.getToken() != tokenActor)
                continue;
            switch (param) {
            case SearchEnum::TxParam::UserSenderOrReceiverOrToken: {
                if (tx.getSender().toStdString() == id || tx.getReceiver().toStdString() == id)
                    return { tx, lastBlockId.toByteArray() };
                break;
            }
            case SearchEnum::TxParam::UserSender: {
                if (tx.getSender().toStdString() == id)
                    return { tx, lastBlockId.toByteArray() };
                break;
            }
            case SearchEnum::TxParam::UserReceiver: {
                if (tx.getReceiver().toStdString() == id)
                    return { tx, lastBlockId.toByteArray() };
                break;
            }
            case SearchEnum::TxParam::UserSenderOrReceiver: {
                if (tx.getSender().toStdString() == id || tx.getReceiver().toStdString() == id)
                    return { tx, lastBlockId.toByteArray() };
                break;
            }
            case SearchEnum::TxParam::UserApprover: {
                if (tx.getApprover().toStdString() == id)
                    return { tx, lastBlockId.toByteArray() };
                break;
            }
            case SearchEnum::TxParam::Hash: {
                if (tx.getHash() == id)
                    return { tx, lastBlockId.toByteArray() };
                break;
            }
            case SearchEnum::TxParam::Data: {
                if (lastBlock.getType() == Config::GENESIS_BLOCK_TYPE)
                    return { Transaction(), "-1" };
                if (tx.getData() == id)
                    return { tx, lastBlockId.toByteArray() };
                break;
            }
            default: {
                break;
            }
            }
        }
        --lastBlockId;
    }
    return { Transaction(), "-1" };
}

QList<Transaction> BlockIndex::getTxsByParamInRow(const BigNumber &id, SearchEnum::TxParam param,
                                                  BigNumber from, int count, BigNumber token) const {
    QList<Transaction> currentTxs;
    BigNumber records = getRecords();

    if (records == 0) {
        qDebug() << "There no tx's in blockIndex";
        return currentTxs;
    }

    BigNumber lastBlockId = from == -1 ? getLastSavedId() : from;
    int currentCount = 0;

    while (lastBlockId >= getFirstSavedId()) {
        // qDebug() << count << currentCount << (count < currentCount);

        if (count < currentCount)
            break;

        Block lastBlock = getBlockById(lastBlockId);
        auto txs = lastBlock.extractTransactions();

        for (const Transaction &tx : txs) {
            if (BigNumber(tx.getToken().toStdString()) != token)
                continue;
            switch (param) {
            case SearchEnum::TxParam::UserSender: {
                if (BigNumber(tx.getSender().toStdString()) == id
                    && BigNumber(tx.getToken().toStdString()) == token) {
                    currentTxs << tx;
                    ++currentCount;
                }
                break;
            }
            case SearchEnum::TxParam::UserReceiver: {
                if (BigNumber(tx.getReceiver().toStdString()) == id
                    && BigNumber(tx.getToken().toStdString()) == token) {
                    currentTxs << tx;
                    ++currentCount;
                }
                break;
            }
            case SearchEnum::TxParam::UserSenderOrReceiver: {
                if ((BigNumber(tx.getSender().toStdString()) == id
                     || BigNumber(tx.getReceiver().toStdString()) == id)
                    && BigNumber(tx.getToken().toStdString()) == token) {
                    currentTxs << tx;
                    ++currentCount;
                }
                break;
            }
            case SearchEnum::TxParam::UserApprover: {
                if (BigNumber(tx.getApprover().toStdString()) == id
                    && BigNumber(tx.getToken().toStdString()) == token) {
                    currentTxs << tx;
                    ++currentCount;
                }
                break;
            }
            case SearchEnum::TxParam::Hash: {
                if (BigNumber(tx.getHash()) == id && BigNumber(tx.getToken().toStdString()) == token) {
                    currentTxs << tx;
                    ++currentCount;
                }
                break;
            }
            default: {
            }
            }
        }

        --lastBlockId;
    }

    // qDebug() << "currentTxs" << currentTxs.length();

    return currentTxs;
}

QString BlockIndex::buildFilePath(const BigNumber &id) const {
    BigNumber section = this->calcSection(id);
    QString pathToFolder = getFolderPath() + "/" + section.toByteArray();

    QDir dir(pathToFolder);
    if (!dir.exists()) {
        qDebug() << "Creating dir:" << pathToFolder;
        dir = QDir();
        dir.mkpath(pathToFolder);
    }

    return pathToFolder + "/" + id.toByteArray();
}

BigNumberFloat BlockIndex::calculateCirculativeBalance() const {
    BigNumberFloat circulativeBalance = 0;
    bool isGenesisBlockFounde = false;
    auto lastId = lastSavedId;
    while (!isGenesisBlockFounde) {
        const auto block = getBlockById(lastId);
        if (block.getType() == Config::GENESIS_BLOCK_TYPE) {
            isGenesisBlockFounde = true;
        } else {
            circulativeBalance += calculateCirculativeBalanceBlock(block);
            lastId--;
        }
    }
    return circulativeBalance;
}

BigNumberFloat BlockIndex::calculateCirculativeBalanceBlock(const Block &block) const {
    BigNumberFloat circulativeBalanceBlock(0);

    const auto allTx = block.extractTransactions();
    if (allTx.empty())
        return BigNumberFloat(0);

    for (int numberTx = 0; numberTx < allTx.size(); numberTx++) {
        if (allTx[numberTx].isRewardTransaction()) {
            circulativeBalanceBlock += allTx[numberTx].getAmount();
        }
    }
    return circulativeBalanceBlock;
}

BigNumberFloat BlockIndex::calculateCirculativeBalanceLastGenesisBlock() const {
    BigNumberFloat circulativeBalanceGenesisBlock(0);
    const auto genesisBlock = getLastGenesisBlock();

    const auto dataRows = genesisBlock.extractDataRows();
    for (int numberRow = 0; numberRow < dataRows.size(); numberRow++) {
        const GenesisDataRow dataRow = dataRows[numberRow];
        if (dataRow.type == DataStorage::typeDataRow::UNIVERSAL && dataRow.token == ActorId()) {
            circulativeBalanceGenesisBlock += dataRow.state;
        }
    }
    return circulativeBalanceGenesisBlock;
}
int BlockIndex::add(const BigNumber &id, const QByteArray &_data) {
    QString path = buildFilePath(id);
    QFile file(path);

    qDebug() << "Saving the file:" << path;

    if (file.exists()) {
        qDebug() << "Can't save the file" << path << "(File already exits)";
        return Errors::FILE_ALREADY_EXISTS;
    }

    if (recordLimitIsReached()) {
        if (this->firstSavedId != 0) {
            this->removeById(this->getFirstSavedId());
            this->firstSavedId++; // todo: check!
        }
    }

    DBConnector DB(path.toStdString());
    if (DB.open()) {
        if (GenesisBlock::isGenesisBlock(_data)) {
            GenesisBlock block(_data);
            DB.createTable(Config::DataStorage::GenesisBlockTableCreate);
            DB.createTable(Config::DataStorage::RowGenesisBlockTableCreate);
            DB.createTable(Config::DataStorage::SignBlockTableCreate);
            DBRow row;

            row.insert({ "type", block.getType() });
            row.insert({ "id", block.getIndex().toStdString() });
            row.insert({ "date", QByteArray::number(block.getDate()).toStdString() });
            row.insert({ "data", "" });
            row.insert({ "prevHash", block.getPrevHash() });
            row.insert({ "hash", block.getHash() });
            row.insert({ "prevGenHash", block.getPrevGenHash() });
            DB.insert(Config::DataStorage::GenesisBlockTable, row);

            QList<GenesisDataRow> rows = block.extractDataRows();
            for (const auto &tmp : rows) {
                DBRow rowRow;
                rowRow.insert({ "actorId", tmp.actorId.toStdString() });
                rowRow.insert({ "state", tmp.state.toStdString() });
                rowRow.insert({ "token", tmp.token.toStdString() });
                rowRow.insert({ "type", QByteArray::number(tmp.type).toStdString() });
                DB.insert(Config::DataStorage::RowGenesisBlockTable, rowRow);
            }
            QByteArrayList listSign = block.getListSignatures();
            for (int i = 0; i < listSign.size(); i += 3) {
                DBRow rowRow;
                rowRow.insert({ "actorId", listSign[i].toStdString() });
                rowRow.insert({ "digSig", listSign[i + 1].toStdString() });
                rowRow.insert({ "type", listSign[i + 2].toStdString() });
                DB.insert(Config::DataStorage::SignTable, rowRow);
            }
        } else {
            Block block(_data);
            DB.createTable(Config::DataStorage::BlockTableCreate);
            DB.createTable(Config::DataStorage::TxBlockTableCreate);
            DB.createTable(Config::DataStorage::SignBlockTableCreate);
            DBRow row;

            row.insert({ "type", block.getType() });
            row.insert({ "id", block.getIndex().toStdString() });
            row.insert({ "date", QByteArray::number(block.getDate()).toStdString() });
            row.insert({ "data", "" });
            row.insert({ "prevHash", block.getPrevHash() });
            row.insert({ "hash", block.getHash() });
            DB.insert(Config::DataStorage::BlockTable, row);

            auto rows = block.extractTransactions();
            for (const auto &tmp : rows) {
                DBRow rowRow;
                rowRow.insert({ "sender", tmp.getSender().toByteArray().toStdString() });
                rowRow.insert({ "receiver", tmp.getReceiver().toByteArray().toStdString() });
                rowRow.insert({ "amount", tmp.getAmount().toStdString() });
                rowRow.insert({ "date", QByteArray::number(tmp.getDate()).toStdString() });
                rowRow.insert({ "token", tmp.getToken().toByteArray().toStdString() });
                rowRow.insert({ "data", tmp.getData() });
                rowRow.insert({ "prevBlock", tmp.getPrevBlock().toStdString() });
                rowRow.insert({ "gas", QByteArray::number(tmp.getGas()).toStdString() });
                rowRow.insert({ "hop", QByteArray::number(tmp.getHop()).toStdString() });
                rowRow.insert({ "hash", tmp.getHash() });
                rowRow.insert({ "approver", tmp.getApprover().toByteArray().toStdString() });
                rowRow.insert({ "digSig", tmp.getDigSig() });
                if (tmp.getProducer().isEmpty())
                    rowRow.insert({ "producer", "0" });
                else
                    rowRow.insert({ "producer", tmp.getProducer().toByteArray().toStdString() });
                DB.insert(Config::DataStorage::TxBlockTable, rowRow);
            }
            QByteArrayList listSign = block.getListSignatures();
            for (int i = 0; i < listSign.size(); i += 3) {
                DBRow rowRow;
                rowRow.insert({ "actorId", listSign[i].toStdString() });
                rowRow.insert({ "digSig", listSign[i + 1].toStdString() });
                rowRow.insert({ "type", listSign[i + 2].toStdString() });
                DB.insert(Config::DataStorage::SignTable, rowRow);
            }
        }
        this->records = records + 1;

        // updating last saved id is a regular operation
        if (id > this->lastSavedId) {
            this->lastSavedId = id;
        }

        // but updating the first saved id is rarely (should be logged)
        if (id < this->firstSavedId || firstSavedId.isEmpty()) {
            qDebug() << "First saved id is updated from" << firstSavedId << "to" << id;
            this->firstSavedId = id;
        }

        return 0;
    }
    qDebug() << "Can't save the file" << path << "(File is not opened)";
    return Errors::FILE_IS_NOT_OPENED;
}

bool BlockIndex::hasRecordLimit() const {
    return !this->recordsLimit.isEmpty();
}

bool BlockIndex::recordLimitIsReached() const {
    return this->hasRecordLimit() && (this->records >= this->recordsLimit);
}

int BlockIndex::removeById(const BigNumber &id) {
    qDebug() << "Removing record with id" << id.toByteArray();
    if (id < firstSavedId) {
        removeAll();
    }
    qDebug() << lastSavedId << "(last saved id)" << id << "(id to remove)";

    BigNumber currentIdToRemove = id;

    while (currentIdToRemove <= lastSavedId) {
        QString pathToFile = buildFilePath(currentIdToRemove);
        qDebug() << "To remove:" << pathToFile;
        QFile file(pathToFile);
        if (file.exists() && !file.isOpen()) {
            bool isRemoved = file.remove();
            if (isRemoved) {
                this->records--;
            }
        }
        currentIdToRemove++;
    }

    this->lastSavedId = BigNumber(id) - 1;
    return 0;
}

void BlockIndex::removeDummyBlocks(const BigNumber &id) {
    bool isNotDummyBlock = false;
    auto lastId = lastSavedId;
    while (!isNotDummyBlock) {
        const auto block = getBlockById(lastId);
        if (block.getType() != Config::DUMMY_BLOCK_TYPE) {
            isNotDummyBlock = true;
        } else {
            removeById(lastId);
            lastId--;
        }
    }
}

void BlockIndex::removeAll() {
    QString folderPath = this->getFolderPath();
    qDebug() << "Clearing file index:" << folderPath;

    QDir folder(folderPath);
    const auto folders =
        folder.entryList(QDir::Filter::AllEntries | QDir::Filter::NoDotAndDotDot, QDir::SortFlag::Name);
    for (const QString &section : qAsConst(folders)) {
        QDir dir(folderPath + QString("/") + section);
        dir.removeRecursively();
    }

    // update state
    this->records = 0;
    this->firstSavedId = 0;
    this->lastSavedId = 0;
}
QString BlockIndex::getFolderPath() const {
    return DataStorage::BLOCKCHAIN_INDEX + "/" + this->getFolderName();
}

QString BlockIndex::getFolderName() const {
    return this->folderName;
}

BigNumber BlockIndex::getFirstSavedId() const {
    return this->firstSavedId;
}

BigNumber BlockIndex::calcSection(BigNumber id) const {
    return id / BigNumber(sectionSize);
}

BigNumber BlockIndex::getLastSavedId() const {
    return this->lastSavedId;
}

BigNumber BlockIndex::getRecords() const {
    return this->records;
}

QByteArray BlockIndex::getById(const BigNumber &id) const {
    QString path = buildFilePath(id);
    QFile file(path);

    if (!file.exists()) {
        qDebug() << "Can't get the file" << path << "(File is not exits)";
        return QByteArray();
    }

    DBConnector DB(path.toStdString());
    DB.open();
    if (DB.tableNames().size() == 0)
        return "";
    if (DB.tableNames()[0] == "GenesisBlock") {
        std::vector<DBRow> res = DB.select("SELECT * FROM " + Config::DataStorage::GenesisBlockTable + " ;");
        if (res.size() == 0) {
            qDebug() << "Can't get the file" << path << "(File is empty)";
            return QByteArray();
        }
        QByteArrayList list;
        list << res[0].at("type").c_str() << QByteArray(res[0].at("id").c_str()) << res[0].at("date").c_str()
             << res[0].at("data").c_str() << res[0].at("prevHash").c_str() << res[0].at("hash").c_str()
             << res[0].at("prevGenHash").c_str();
        GenesisBlock b;
        std::vector<DBRow> rowsSign = DB.select("SELECT * FROM " + Config::DataStorage::SignTable + " ;");
        QByteArray signes = "";
        for (const auto &tmp : rowsSign) {
            std::string key, value, type;
            key = tmp.at("actorId");
            value = tmp.at("digSig");
            type = tmp.at("type");
            std::vector<std::string> v;
            v.push_back(key);
            v.push_back(value);
            v.push_back(type);
            std::string sign = Serialization::serialize(v);
            std::vector<std::string> v1;
            v1.push_back(sign);
            signes += Serialization::serialize(v1);
        }
        list << signes;
        b.initFields(list);

        std::vector<DBRow> rows =
            DB.select("SELECT * FROM " + Config::DataStorage::RowGenesisBlockTable + " ;");
        for (const auto &tmp : rows) {
            GenesisDataRow dRow;
            dRow.type = DataStorage::typeDataRow(QByteArray(tmp.at("type").c_str()).toInt());
            dRow.state = BigNumber(tmp.at("state"));
            dRow.token = tmp.at("token");
            dRow.actorId = tmp.at("actorId");
            b.addRow(dRow);
        }

        return b.serialize();
    } else {
        std::vector<DBRow> res = DB.select("SELECT * FROM " + Config::DataStorage::BlockTable + " ;");
        if (res.size() == 0) {
            qDebug() << "Can't get the file" << path << "(File is empty)";
            return QByteArray();
        }
        QByteArrayList list;
        list << res[0].at("type").c_str() << QByteArray(res[0].at("id").c_str()) << res[0].at("date").c_str()
             << res[0].at("data").c_str() << res[0].at("prevHash").c_str() << res[0].at("hash").c_str();
        Block b;
        std::vector<DBRow> rowsSign = DB.select("SELECT * FROM " + Config::DataStorage::SignTable + " ;");
        QByteArray signes = "";
        for (const auto &tmp : rowsSign) {
            std::string key, value, type;
            key = tmp.at("actorId");
            value = tmp.at("digSig");
            type = tmp.at("type");
            std::vector<std::string> v;
            v.push_back(key);
            v.push_back(value);
            v.push_back(type);
            std::string sign = Serialization::serialize(v);
            std::vector<std::string> v1;
            v1.push_back(sign);
            signes += Serialization::serialize(v1);
        }
        list << signes;
        b.initFields(list);

        std::vector<DBRow> rows = DB.select("SELECT * FROM " + Config::DataStorage::TxBlockTable + " ;");
        for (const auto &tmp : rows) {
            QByteArrayList list;
            Transaction tx;
            tx.setSender(ActorId(tmp.at("sender")));
            tx.setReceiver(ActorId(tmp.at("receiver")));
            tx.setAmount(BigNumber(tmp.at("amount")));
            tx.setDate(std::stoll(tmp.at("date")));
            tx.setData(tmp.at("data"));
            tx.setToken(ActorId(tmp.at("token")));
            tx.setPrevBlock(BigNumber(tmp.at("prevBlock")));
            tx.setGas(std::stoi(tmp.at("gas")));
            tx.setHop(std::stoi(tmp.at("hop")));
            tx.setHash(tmp.at("hash").c_str());
            tx.setApprover(ActorId(tmp.at("approver").c_str()));
            tx.setDigSig(tmp.at("digSig").c_str());
            tx.setProducer(ActorId(tmp.at("producer").c_str()));
            b.addData(tx.serialize().c_str());
        }

        return b.serialize();
    }
}

BigNumber BlockIndex::loadFirstId() {
    BigNumber firstSavedId = loadFileFromSection([](const QStringList &folders) { return folders[0]; },
                                                 [](const QStringList &files) { return files[0]; });

    if (!firstSavedId.isEmpty()) {
        qDebug() << "FIFE INDEX: loadFirsId: Loaded first saved id:" << firstSavedId;
    } else {
        qDebug() << "FIFE INDEX: loadFirsId: First saved id is not loaded";
    }

    return firstSavedId;
}

BigNumber BlockIndex::loadFileFromSection(std::function<QString(const QStringList &folders)> getFolder,
                                          std::function<QString(const QStringList &files)> getFile) {
    auto asBigNumComparator = [](const QString &file1, const QString &file2) {
        return BigNumber(file1.toStdString()) < BigNumber(file2.toStdString());
    };

    QDir folder(getFolderPath());

    // sections
    qDebug() << "FILE INDEX:"
             << "loadFileFromSection():" << folder.path();
    QStringList list = folder.entryList(QDir::Filter::Dirs | QDir::Filter::NoDotAndDotDot);
    if (list.isEmpty()) {
        qDebug() << "FILE INDEX:"
                 << "loadFileFromSection():"
                 << "folder.entryList: empty";
        return BigNumber();
    }
    std::sort(list.begin(), list.end(), asBigNumComparator);
    folder.cd(getFolder(list)); // go to section

    // files in sections
    qDebug() << "FILE INDEX:"
             << "loadFileFromSection():" << folder.path();
    list = folder.entryList(QDir::Filter::Files | QDir::Filter::NoDotAndDotDot);
    if (list.isEmpty()) {
        qDebug() << "FILE INDEX:"
                 << "loadFileFromSection():"
                 << "folder.entryList->folder.entryList: empty";
        return BigNumber();
    }
    std::sort(list.begin(), list.end(), asBigNumComparator);

    qDebug() << "FILE INDEX:"
             << "loadFileFromSection(): lastId -"
             << (list.isEmpty() ? BigNumber() : BigNumber(getFile(list).toStdString()));
    return list.isEmpty() ? BigNumber() : BigNumber(getFile(list).toStdString());
}

BigNumber BlockIndex::loadLastId() {
    BigNumber lastSavedId = loadFileFromSection([](const QStringList &folders) { return folders.last(); },
                                                [](const QStringList &files) { return files.last(); });

    if (!lastSavedId.isEmpty()) {
        qDebug() << "Loaded last saved id:" << lastSavedId;
    } else {
        qDebug() << "Last saved id is not loaded";
    }
    return lastSavedId;
}
