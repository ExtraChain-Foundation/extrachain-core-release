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

#include "utils/exc_utils.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QMimeDatabase>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTcpSocket>

#include <string>
#include <string_view>

#include <sodium.h>

// #include "boost/asio.hpp" // need qmake fix
#include "boost/version.hpp"

#include "cpp-base64/base64.h"
#include "enc/enc_tools.h"
#include "managers/data_mining_manager.h"
#include "sha3.h"
#include "utils/dfs_utils.h"

#ifndef EXTRACHAIN_CMAKE
    #include "preconfig.h"
#endif

std::string Utils::calcHash(const std::string &data, HashEncode encode) {
    std::string res;
    switch (encode) {
    case HashEncode::Sha3_512: {
        SHA3 sha3(SHA3::Bits::Bits512);
        res = sha3(data);
        break;
    }
    case HashEncode::Base64: {
        res = base64_encode(data);
        break;
    }
    case HashEncode::Hex: {
        qDebug() << "Hex encode not supported!";
        break;
    }
    default: {
        qDebug() << "This encode not supported!";
        break;
    }
    }
    return res;
}

// SERIALIZATION //

std::vector<std::string> Utils::split(const std::string &s, char c) {
    auto end = s.cend();
    auto start = end;

    std::vector<std::string> v;
    for (auto it = s.cbegin(); it != end; ++it) {
        if (*it != c) {
            if (start == end)
                start = it;
            continue;
        }
        if (start != end) {
            v.emplace_back(start, it);
            start = end;
        }
    }
    if (start != end)
        v.emplace_back(start, end);
    return v;
}

QString KeyStore::makeKeyFileName(QString name) {
    return name + KEY_TYPE;
}

int Utils::compare(const QByteArray &one, const QByteArray &two) {
    if (one.size() > two.size()) {
        return one.size() - two.size();
    } else if (one.size() == two.size()) {
        return static_cast<int>(one == two);
    } else
        return two.size() - one.size();
}

QByteArray Utils::intToByteArray(const int &number, const int &size) {
    auto num = QByteArray::number(number);
    Q_ASSERT(num.size() <= size);
    auto res = QByteArray(size - num.size(), '0') + num;
    return res;
}

std::string Utils::intToStdString(const int &number, const int &size) {
    auto num = std::to_string(number);
    Q_ASSERT(num.size() <= size);
    auto res = std::string(size - num.size(), '0') + num;
    return res;
}

int Utils::qByteArrayToInt(const QByteArray &number) {
    QByteArray num = "";
    int i = 0;
    //    bool flag = false;
    while (i < number.size()) {
        if (number[i] == '0')
            i++;
        else
            break;
    }
    while (i < number.size()) {
        num += number[i];
        i++;
    }
    int res = num.toInt();
    return res;
}
void Utils::rootMerkleHash(std::vector<std::string> &listHashes, std::vector<MerkleDataBlocks> &branchesTree,
                           const bool isHahsing, std::string &result) {
    if (listHashes.empty()) {
        qFatal("Root merkle hash: list is empty");
    };
    const auto splittedList = splitListIntoPair(listHashes, isHahsing);
    MerkleDataBlocks merkleBlocks;

    for (int index = 0; index < splittedList.size(); index++) {
        const auto pair = splittedList[index];

        if (pair.size() == 1) {
            merkleBlocks.push_back(pair[0]);
        } else {
            merkleBlocks.push_back(merkleFormula(pair[0], pair[1]));
        }
    }
    branchesTree.push_back(merkleBlocks);

    if (merkleBlocks.size() != 1) {
        rootMerkleHash(merkleBlocks, branchesTree, false, result);
    } else {
        result = branchesTree[branchesTree.size() - 1][0];
    }
}

std::string Utils::rootMerkleHash(std::string &data) {
    std::string result;
    std::vector<MerkleDataBlocks> branches;
    std::vector<std::string> dataList;
    dataList.push_back(data);
    rootMerkleHash(dataList, branches, true, result);
    return result;
}

std::vector<Utils::MerkleDataBlocks> Utils::splitListIntoPair(std::vector<std::string> &vector,
                                                              const bool isHahsing) {
    std::vector<MerkleDataBlocks> result;

    if (vector.empty())
        return result;

    if (isHahsing)
        hashingElements(vector);

    int position = 0;
    int step = 2;
    const int sizeVector = vector.size();
    bool isLastPair = sizeVector <= 2;
    const bool isPairVector = (sizeVector % 2 == 0) ? true : false;
    const int next = 1;

    while (position < sizeVector) {
        std::vector<std::string> pair;
        if (isLastPair) {
            pair.push_back(vector[position]);
            if (isLastPair)
                pair.push_back(vector[position + next]);
        } else {
            pair.push_back(vector[position]);
            pair.push_back(vector[position + next]);
        }

        if (!isPairVector) {
            position += ((position + step) > sizeVector) ? 1 : 2;
            isLastPair = ((sizeVector - 1) - position) < 1;
        } else {
            position += step;
            isLastPair = (sizeVector - position) < 2;
        }

        result.push_back(pair);
    }
    return result;
}

void Utils::hashingElements(std::vector<std::string> &vector) {
    for (int i = 0; i < vector.size(); i++) {
        vector[i] = Utils::calcHash(vector[i]);
    }
}

std::string Utils::merkleFormula(const std::string &hash1, const std::string &hash2) {
    return Utils::calcHash(hash1 + hash2);
}
std::string Utils::calcHashForFile(const std::filesystem::path &fileName, HashEncode encode) {
    QFile file(QString::fromStdWString(fileName.wstring()));
    if (file.open(QFile::ReadOnly)) {
        std::string data = file.readAll().toStdString();
        std::string hash = rootMerkleHash(data);
        return bytesEncodeStdString(hash, encode);
    }

    qFatal("Utils::calcHashForFile");
    qDebug() << "[Utils] Calc hash for file: can't open file" << fileName.c_str();
    return "";
}

bool Utils::encryptFile(const QString &originalName, const QString &encryptName, const QByteArray &key,
                        int blockSize) {
    QFile orig(originalName);
    if (!orig.exists())
        return false;
    QFile encrypt(encryptName);
    bool origOpen = orig.open(QFile::ReadOnly);
    bool encryptOpen = encrypt.open(QFile::WriteOnly);
    if (!origOpen || !encryptOpen) {
        qDebug() << "[Utils::encryptFile] Error while loading files" << origOpen << encryptOpen;
        return false;
    }
    std::string rkey = SecretKey::getKeyFromPass(key.toStdString());
    while (!orig.atEnd()) {
        QByteArray part = orig.read(blockSize);
        QByteArray encrypted = QByteArray::fromStdString(SecretKey::encrypt(part.toStdString(), rkey));
        encrypt.write(encrypted);
        // qDebug() << "encrypted" << part.size() << encrypted.size();
    }

    qDebug() << "[DFS] Encrypted file" << originalName << "to" << encryptName << "with sizes" << orig.size()
             << encrypt.size();
    orig.close();
    encrypt.close();
    return QFile::exists(encryptName);
}

bool Utils::decryptFile(const QString &encryptName, const QString &decryptName, const QByteArray &key,
                        int blockSize) //
{
    blockSize = (blockSize / 8 + 1) * 8;
    QFile encrypt(encryptName);
    if (!encrypt.exists())
        return false;
    QFile decrypt(decryptName);

    bool encryptOpen = encrypt.open(QFile::ReadOnly);
    bool decryptOpen = decrypt.open(QFile::WriteOnly);
    if (!encryptOpen || !decryptOpen) {
        qDebug() << "[Utils::encryptFile] Error while loading files" << encryptOpen << decryptOpen;
        return false;
    }
    std::string rkey = SecretKey::getKeyFromPass(key.toStdString());
    while (!encrypt.atEnd()) {
        QByteArray part = encrypt.read(blockSize);
        QByteArray decrypted = QByteArray::fromStdString(SecretKey::decrypt(part.toStdString(), rkey));
        decrypt.write(decrypted);
        qDebug() << "decrypted" << part.size() << decrypted.size();
    }

    // qDebug() << "[DFS] Encrypted file" << originalName << "to" << encryptName << "with sizes" <<
    // orig.size()
    //    << encrypt.size();
    encrypt.close();
    decrypt.close();
    return QFile::exists(decryptName);
}

QByteArray Utils::decryptFileIntoByteArray(const QString &encryptName, const QByteArray &key, int blockSize) {
    blockSize = (blockSize / 8 + 1) * 8;

    if (!QFileInfo::exists(encryptName)) {
        return QByteArray();
    }

    QFile encrypt(encryptName);
    if (!encrypt.open(QFile::ReadOnly)) {
        qDebug() << "[Utils::encryptFile] Error while loading file:" << encrypt.error()
                 << encrypt.errorString();
        return QByteArray();
    }

    QByteArray result;
    std::string rkey = SecretKey::getKeyFromPass(key.toStdString());

    while (!encrypt.atEnd()) {
        QByteArray part = encrypt.read(blockSize);
        QByteArray decrypted = QByteArray::fromStdString(SecretKey::decrypt(part.toStdString(), rkey));
        result.append(decrypted);
        qDebug() << "decrypted" << part.size() << decrypted.size();
    }

    return result;
}

QString Utils::fileMimeType(const QString &filePath) {
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForFile(filePath);
    return type.name();
}

QString Utils::fileMimeSuffix(const QString &filePath) {
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForFile(filePath);
    return type.preferredSuffix();
}

std::string Serialization::serialize(const std::vector<std::string> &list) {
    std::string res;
    std::vector<std::string> reslist;
    for (int i = 0; i < list.size(); i++) {
        reslist.push_back(Utils::bytesEncodeStdString(list.at(i)));
    }
    res = boost::algorithm::join(reslist, "|");
    return res;
}

std::vector<std::string> Serialization::deserialize(const std::string &serialized) {
    std::vector<std::string> templist;
    std::vector<std::string> reslist;
    boost::algorithm::split(templist, serialized, boost::algorithm::is_any_of("|"));
    if (templist.empty()) {
        qDebug() << "decerialize error - empty list after split";
    }
    for (int i = 0; i < templist.size(); i++) {
        reslist.push_back(Utils::bytesDecodeStdString(templist.at(i)));
    }
    return reslist;
}

void Utils::wipeDataFiles() {
    QString current = QDir::currentPath();

    QDir("blockchain").removeRecursively();
    QDir(QString::fromStdString(DFSB::fsActrRoot)).removeRecursively();
    QDir("keystore").removeRecursively();
    QDir("tmp").removeRecursively();
    QFile(".settings").remove();
    QFile(".auth_hash").remove();

    QDir dir(QDir::currentPath());
    dir.cdUp();
    QDir::setCurrent(dir.canonicalPath());
    QString dataName = Utils::dataDir();
    // qDebug() << "[Wipe] Remove path:" << dataName;
    QDir(dataName).removeRecursively();
    QDir().mkpath(dataName);

    QString shareFolder =
        QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).value(0) + "/Share";
    QDir(shareFolder).removeRecursively();

    QDir::setCurrent(current);
}

qint64 Utils::diskFreeMemory() {
    QStorageInfo x(qApp->applicationDirPath());
    return x.bytesFree();
}

qint64 Utils::diskTotalMemory() {
    QStorageInfo x(qApp->applicationDirPath());
    return x.bytesTotal();
}

QString Utils::dataDir(const QString &newDir) {
    static QString current = "extrachain-data";

    if (!newDir.isEmpty())
        current = Utils::fixFileName(newDir);

    return current;
}

bool Serialization::isEmpty(const QByteArray &bytes) {
    return bytes.isEmpty();
}

bool Serialization::isEmpty(const std::string &str) {
    return str.empty();
}

bool Serialization::isEmpty(std::string_view str_view) {
    return str_view.empty();
}

QDebug operator<<(QDebug d, const Notification &n) {
    d.noquote().nospace() << "Notification(time: " << QString::number(n.time)
                          << ", type: " << QString::number(n.type) << ", data: \"" << n.data << "\")";
    return d;
}

std::string Utils::byteToHexString(std::vector<unsigned char> &data) {
    size_t psize = data.size() * 2 + 1;
    std::vector<char> p(psize);
    sodium_bin2hex(p.data(), psize, data.data(), data.size());
    std::string s(p.begin(), p.end());
    // s.erase(--s.end());
    return s;
}

std::string Utils::byteToHexString(const std::string &data) {
    std::vector<unsigned char> v(data.begin(), data.end());
    return byteToHexString(v);
}

std::string Utils::hexStringToByte(const std::string &data) {
    std::vector<unsigned char> p;
    p.resize(data.length() / 2 + 1);
    const char *end;
    size_t size;
    int r = sodium_hex2bin(p.data(), p.size(), data.c_str(), data.length(), NULL, &size, &end);
    std::string res;
    if (r == 0) {
        res = std::string(p.begin(), p.end());
        res.resize(res.size() - 1);
    }
    return res;
}

std::string Utils::bytesEncodeStdString(const std::string &data, HashEncode encode) {
    std::string res;
    switch (encode) {
    case HashEncode::Base64:
        res = base64_encode(data);
        return res;
    case HashEncode::Hex:
        break;
        //        return data.toHex();
    }
    return res;
}

std::string Utils::bytesDecodeStdString(const std::string &data, HashEncode encode) {
    std::string res;
    switch (encode) {
    case HashEncode::Base64:
        res = base64_decode(data);
        return res;
    case HashEncode::Hex:
        break;
        //        return data.toHex();
    }
    return res;
}

QByteArray Utils::bytesEncode(const QByteArray &data, HashEncode encode) {
    switch (encode) {
    case HashEncode::Base64:
        return data.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    case HashEncode::Hex:
        return data.toHex();
    }
    return data;
}

QByteArray Utils::bytesDecode(const QByteArray &data, HashEncode encode) {
    switch (encode) {
    case HashEncode::Base64:
        return QByteArray::fromBase64(data, QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    case HashEncode::Hex:
        return QByteArray::fromHex(data);
    }
    return data;
}

QString Utils::detectCompiler() {
#ifdef __clang__
    #if __clang_major__ < 11
        #error "Clang must be version 11 or higher"
    #endif
#elif __GNUC__
    #if __GNUC__ < 11
        #error "GCC must be version 11 or higher"
    #endif
#elif _MSC_VER && !__INTEL_COMPILER
#else
    #error "Compiler not supported"
#endif

#if __GNUC__ > 4
    QString gcc = "GCC";
    #ifdef __MINGW32__
    gcc = "MinGW";
    #endif
    return QString("%4 %1.%2.%3").arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__).arg(gcc);
#endif

#if _MSC_VER && !__INTEL_COMPILER
    QString msvcVersion;
    msvcVersion = "MSVC " + QString::number(_MSC_FULL_VER);
    msvcVersion.insert(7, ".");
    msvcVersion.insert(10, ".");
#endif

#ifdef __clang__
    QString compiler =
        QString("Clang %1.%2.%3").arg(__clang_major__).arg(__clang_minor__).arg(__clang_patchlevel__);
    #if __APPLE__
    compiler.prepend("Apple ");
    #endif
    #if _MSC_VER && !__INTEL_COMPILER
    compiler += " (" + msvcVersion + ")";
    #endif
    return compiler;
#endif

#if _MSC_VER && !__INTEL_COMPILER
    return msvcVersion;
#else
    return "unknown";
#endif
}

QNetworkAddressEntry Utils::findLocalIp(PrintDebug debug) {
    const auto allInterfaces = QNetworkInterface::allInterfaces();
    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    QList<QHostAddress> localIpNotConnect;

    for (const QNetworkInterface &networkInterface : allInterfaces) {
        const auto entries = networkInterface.addressEntries();

        for (const QNetworkAddressEntry &address : entries) {
            if (address.ip().protocol() == QAbstractSocket::IPv4Protocol && address.ip() != localhost) {
                if (debug == PrintDebug::On) {
                    qDebug() << "[FindLocalIp] Find local ip candidate:" << networkInterface;
                }

                localIpNotConnect.append(address.ip());
            }
        }
    }

    for (const QNetworkInterface &networkInterface : allInterfaces) {
        const auto entries = networkInterface.addressEntries();

        for (const QNetworkAddressEntry &entry : entries) {
            const auto flags = networkInterface.flags();

            bool isLoopBack = flags.testFlag(QNetworkInterface::IsLoopBack);
            bool isPointToPoint = flags.testFlag(QNetworkInterface::IsPointToPoint);
            bool isRunning = flags.testFlag(QNetworkInterface::IsRunning);
            if (!isRunning || !networkInterface.isValid() || isLoopBack || isPointToPoint)
                continue;

            auto socket = std::make_unique<QTcpSocket>();
            socket->bind(entry.ip());
            socket->connectToHost("8.8.8.8", 53);
            if (!socket->waitForConnected(1000)) {
                socket->connectToHost("1.1.1.1", 53);
                if (!socket->waitForConnected(1000))
                    continue;
            }

            if (localIpNotConnect.contains(entry.ip())) {
                QString name = networkInterface.name();

                if (name.left(2) == "vm")
                    continue;
                if (name.left(2) == "wl" || name.left(3) == "eth" || name.left(2) == "en"
                    || name.left(8) == "wireless") {
                    return entry;
                }
            }
        }
    }

    qCritical() << "[Network] Can't find local ip, set 0.0.0.0";
    QNetworkAddressEntry entry;
    entry.setIp(QHostAddress::AnyIPv4);
    return entry;
}

QString Utils::fixFileName(const QString &fileName, const QString &replaceSymbol) {
    QString fixedName = fileName.simplified();
    fixedName = fixedName.replace(QRegularExpression("[+%@!:*?/\"<>|«»]+"), replaceSymbol);
    fixedName = fixedName.replace("\\", replaceSymbol);
    return fixedName;
}

bool Utils::isValidIp(const QString &ip) {
    QHostAddress address(ip);
    return QAbstractSocket::IPv4Protocol == address.protocol();
}

void Utils::benchmark(std::function<void()> func, int count) {
    while (true) {
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i != count; i++) {
            func();
        }
        qDebug() << timer.elapsed() << "ms";
    }
}

QString Utils::extrachainVersion() {
    return EXTRACHAIN_VERSION;
}

std::string Utils::sodiumVersion() {
    return sodium_version_string();
}

QString Utils::boostVersion() {
    int major = BOOST_VERSION / 100000;
    int minor = BOOST_VERSION / 100 % 1000;
    int patch = BOOST_VERSION % 100;
    return QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

QString Utils::boostAsioVersion() {
    return "";
    // int major = BOOST_ASIO_VERSION / 100000;
    // int minor = BOOST_ASIO_VERSION / 100 % 1000;
    // int patch = BOOST_ASIO_VERSION % 100;
    // return QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

// QString FileSystem::createSubDirectory(const QString &parentDirStr, const QString &subDirStr) {
//     QString destPathStr = FileSystem::pathConcat(parentDirStr, subDirStr);
//     QDir parentDir(parentDirStr);
//     if (!parentDir.exists(subDirStr)) {
//         if (!parentDir.mkdir(subDirStr)) {
//             destPathStr = "";
//         }
//     }
//     return destPathStr;
// }

// QList<std::tuple<QString, QString>> FileSystem::listFiles(const QString &dirPath,
//                                                           const QStringList &ignoreList) {
//     QList<std::tuple<QString, QString>> dirList;

//    QDir dir(dirPath, QString::fromLatin1("*"), QDir::SortFlag::Name, QDir::Files | QDir::NoDotAndDotDot);
//    QDirIterator dirItor(dir, QDirIterator::Subdirectories);
//    while (dirItor.hasNext()) {
//        dirItor.next();
//        const QFileInfo &fi = dirItor.fileInfo();
//        if (fi.isFile() && !ignoreList.contains(fi.fileName())) {
//            dirList.emplaceBack(std::make_tuple<QString, QString>(fi.fileName(), fi.filePath()));
//        }
//    }

//    return dirList;
//}

std::string Utils::platformDelimeter() {
#ifdef _WIN32
    char del;
    std::wcstombs(&del, &std::filesystem::path::preferred_separator, 1);
    return std::string(1, del);
#else
    return std::string(1, std::filesystem::path::preferred_separator);
#endif
}
