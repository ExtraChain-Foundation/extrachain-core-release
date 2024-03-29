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

#include "datastorage/genesis_block.h"

GenesisBlock::GenesisBlock()
    : Block() {
    this->m_type = Config::GENESIS_BLOCK_TYPE;
}

GenesisBlock::GenesisBlock(const GenesisBlock &block)
    : Block(block) {
    this->prevGenHash = block.getPrevGenHash();
}

GenesisBlock::GenesisBlock(const QByteArray &serialized) {
    deserialize(serialized);
}

GenesisBlock::GenesisBlock(const QByteArray &_data, const Block &prevBlock, const QByteArray &prevGenHash)
    : Block(_data, prevBlock)
    , prevGenHash(prevGenHash) {
    this->m_type = Config::GENESIS_BLOCK_TYPE;
}

void GenesisBlock::addRow(const GenesisDataRow &row) {
    std::vector<std::string> v;
    v.push_back(row.serialize());
    this->data += Serialization::serialize(v);
}

const std::string &GenesisBlock::getDataForDigSig() const {
    return Block::getDataForDigSig();
}

std::string GenesisBlock::getDataForHash() const {
    return Block::getDataForHash();
}

bool GenesisBlock::deserialize(const QByteArray &serialized) {
    *this = MessagePack::deserialize<GenesisBlock>(serialized);
    return true;
}

QByteArray GenesisBlock::serialize() const {
    return QByteArray::fromStdString(MessagePack::serialize(*this));
}

void GenesisBlock::initFields(QList<QByteArray> &list) {
    m_type = list.takeFirst();
    index = BigNumber(list.takeFirst().toStdString());
    date = list.takeFirst().toLongLong();
    data = list.takeFirst();
    prevHash = list.takeFirst();
    hash = list.takeFirst();
    prevGenHash = list.takeFirst();
    QByteArray signs = list.takeFirst();
    std::vector<std::string> lists = Serialization::deserialize(signs.toStdString());
    for (const auto &tmp : lists) {
        std::vector<std::string> tmps = Serialization::deserialize(tmp);
        if (tmps.size() == 3)
            signatures.push_back({ tmps.at(0), tmps.at(1), bool(std::stoi(tmps.at(2))) });
    }
}

QList<GenesisDataRow> GenesisBlock::extractDataRows() const {
    std::vector<std::string> txsData = Serialization::deserialize(data);
    QList<GenesisDataRow> genesisDataRows;
    for (const std::string &dataRow : txsData) {
        genesisDataRows.append(GenesisDataRow(dataRow));
    }
    return genesisDataRows;
}

bool GenesisBlock::isGenesisBlock(const QByteArray &serialized) {
    return serialized.contains(Config::GENESIS_BLOCK_TYPE);
}

void GenesisBlock::setPrevGenHash(const std::string &value) {
    prevGenHash = value;
}

std::string GenesisBlock::getPrevGenHash() const {
    return prevGenHash;
}
