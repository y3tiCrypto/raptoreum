// Copyright (c) 2026 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAPTOREUM_EVO_DOMAINDB_H
#define RAPTOREUM_EVO_DOMAINDB_H

#include <dbwrapper.h>
#include <serialize.h>
#include <string>
#include <vector>
#include <pubkey.h>
#include <uint256.h>

class CDomainMetaData {
public:
    std::string name;
    CKeyID owner;
    CKeyID resolver;
    CKeyID manager;                // Delegated manager address (optional)
    uint64_t registered_at = 0;
    uint64_t expires_at = 0;
    std::string ipfs_cid;
    std::string json_metadata;

    SERIALIZE_METHODS(CDomainMetaData, obj) {
        READWRITE(obj.name, obj.owner, obj.resolver, obj.registered_at, obj.expires_at, obj.ipfs_cid, obj.json_metadata, obj.manager);
    }
};

struct CDomainBlockUndo {
    std::string strDomainName;
    bool fWasNew = false;
    CDomainMetaData prevMetadata;
    uint256 revealedCommitHash;    // commitment hash spent by this registration
    int nCommitHeight = 0;         // height of commitment transaction

    SERIALIZE_METHODS(CDomainBlockUndo, obj) {
        READWRITE(obj.strDomainName, obj.fWasNew, obj.prevMetadata, obj.revealedCommitHash, obj.nCommitHeight);
    }
};

class CDomainDB : public CDBWrapper {
public:
    explicit CDomainDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    CDomainDB(const CDomainDB&) = delete;
    CDomainDB& operator=(const CDomainDB&) = delete;

    bool WriteDomainData(const std::string& name, const CDomainMetaData& metadata);
    bool ReadDomainData(const std::string& name, CDomainMetaData& metadata);
    bool EraseDomainData(const std::string& name);

    bool WriteReverseRecord(const CKeyID& address, const std::string& name);
    bool ReadReverseRecord(const CKeyID& address, std::string& name);
    bool EraseReverseRecord(const CKeyID& address);

    bool WriteBlockUndoData(const uint256& blockHash, const std::vector<CDomainBlockUndo>& undoData);
    bool ReadBlockUndoData(const uint256& blockHash, std::vector<CDomainBlockUndo>& undoData);
    bool EraseBlockUndoData(const uint256& blockHash);

    // Commitments storage
    bool WriteCommitment(const uint256& commitHash, int nHeight);
    bool ReadCommitment(const uint256& commitHash, int& nHeight);
    bool EraseCommitment(const uint256& commitHash);
};

#endif // RAPTOREUM_EVO_DOMAINDB_H
