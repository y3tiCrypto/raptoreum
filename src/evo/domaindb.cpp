// Copyright (c) 2026 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/domaindb.h>
#include <util/system.h>

CDomainDB::CDomainDB(size_t nCacheSize, bool fMemory, bool fWipe) :
    CDBWrapper(GetDataDir() / "domain", nCacheSize, fMemory, fWipe) {}

bool CDomainDB::WriteDomainData(const std::string& name, const CDomainMetaData& metadata) {
    return Write(std::make_pair('d', name), metadata);
}

bool CDomainDB::ReadDomainData(const std::string& name, CDomainMetaData& metadata) {
    return Read(std::make_pair('d', name), metadata);
}

bool CDomainDB::EraseDomainData(const std::string& name) {
    return Erase(std::make_pair('d', name));
}

bool CDomainDB::WriteReverseRecord(const CKeyID& address, const std::string& name) {
    return Write(std::make_pair('r', address), name);
}

bool CDomainDB::ReadReverseRecord(const CKeyID& address, std::string& name) {
    return Read(std::make_pair('r', address), name);
}

bool CDomainDB::EraseReverseRecord(const CKeyID& address) {
    return Erase(std::make_pair('r', address));
}

bool CDomainDB::WriteBlockUndoData(const uint256& blockHash, const std::vector<CDomainBlockUndo>& undoData) {
    return Write(std::make_pair('u', blockHash), undoData);
}

bool CDomainDB::ReadBlockUndoData(const uint256& blockHash, std::vector<CDomainBlockUndo>& undoData) {
    return Read(std::make_pair('u', blockHash), undoData);
}

bool CDomainDB::EraseBlockUndoData(const uint256& blockHash) {
    return Erase(std::make_pair('u', blockHash));
}

bool CDomainDB::WriteCommitment(const uint256& commitHash, int nHeight) {
    return Write(std::make_pair('c', commitHash), nHeight);
}

bool CDomainDB::ReadCommitment(const uint256& commitHash, int& nHeight) {
    return Read(std::make_pair('c', commitHash), nHeight);
}

bool CDomainDB::EraseCommitment(const uint256& commitHash) {
    return Erase(std::make_pair('c', commitHash));
}
