// Copyright (c) 2026 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAPTOREUM_EVO_DOMAINPAYLOADS_H
#define RAPTOREUM_EVO_DOMAINPAYLOADS_H

#include <primitives/transaction.h>
#include <pubkey.h>
#include <key_io.h>
#include <serialize.h>
#include <uint256.h>
#include <vector>
#include <string>
#include <univalue.h>

class CDomainRegisterPayload {
public:
    static const uint8_t CURRENT_VERSION = 2;
    uint8_t nVersion = CURRENT_VERSION;
    std::string strDomainName;     // e.g., "y3ti.rtm" or "y3ti.raptoreum"
    CKeyID ownerAddress;           // Primary owner address
    CAmount nFeePaid = 0;          // Standard/Premium RTM fee sent
    uint64_t nRegistrationTime = 0;// Block timestamp
    uint256 salt;                  // Secret salt (v2+)
    uint256 inputsHash;            // replay protection
    std::vector<uint8_t> vchSig;   // Owner signature proof

    SERIALIZE_METHODS(CDomainRegisterPayload, obj)
    {
        READWRITE(obj.nVersion, obj.strDomainName, obj.ownerAddress, obj.nFeePaid, obj.nRegistrationTime, obj.inputsHash);
        if (obj.nVersion >= 2) {
            READWRITE(obj.salt);
        }
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.vchSig);
        }
    }

    std::string MakeSignString() const {
        return strDomainName + ownerAddress.ToString() + std::to_string(nFeePaid) + std::to_string(nRegistrationTime) + (nVersion >= 2 ? salt.ToString() : "") + inputsHash.ToString();
    }

    void ToJson(UniValue& obj) const {
        obj.clear();
        obj.setObject();
        obj.pushKV("version", (int)nVersion);
        obj.pushKV("domainName", strDomainName);
        obj.pushKV("ownerAddress", EncodeDestination(ownerAddress));
        obj.pushKV("feePaid", nFeePaid);
        obj.pushKV("registrationTime", (int64_t)nRegistrationTime);
        if (nVersion >= 2) {
            obj.pushKV("salt", salt.ToString());
        }
    }
};

class CDomainUpdatePayload {
public:
    static const uint8_t CURRENT_VERSION = 2;
    uint8_t nVersion = CURRENT_VERSION;
    std::string strDomainName;
    CKeyID primaryAddress;         // RTM address this domain resolves to
    std::string strIpfsCid;        // Optional IPFS CID containing record json
    std::string strJsonMetadata;   // Dynamic DNS, subdomains, and multi-chain records
    CKeyID managerAddress;         // Delegated manager address (v2+)
    uint256 txidPrev;              // Previous update TXID to track history
    uint256 inputsHash;            // replay protection
    std::vector<uint8_t> vchSig;   // Owner signature proof

    SERIALIZE_METHODS(CDomainUpdatePayload, obj)
    {
        READWRITE(obj.nVersion, obj.strDomainName, obj.primaryAddress, obj.strIpfsCid, obj.strJsonMetadata, obj.txidPrev, obj.inputsHash);
        if (obj.nVersion >= 2) {
            READWRITE(obj.managerAddress);
        }
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.vchSig);
        }
    }

    std::string MakeSignString() const {
        return strDomainName + primaryAddress.ToString() + strIpfsCid + strJsonMetadata + (nVersion >= 2 ? managerAddress.ToString() : "") + txidPrev.ToString() + inputsHash.ToString();
    }

    void ToJson(UniValue& obj) const {
        obj.clear();
        obj.setObject();
        obj.pushKV("version", (int)nVersion);
        obj.pushKV("domainName", strDomainName);
        obj.pushKV("primaryAddress", EncodeDestination(primaryAddress));
        obj.pushKV("ipfsCid", strIpfsCid);
        obj.pushKV("jsonMetadata", strJsonMetadata);
        if (nVersion >= 2) {
            obj.pushKV("managerAddress", managerAddress.IsNull() ? "" : EncodeDestination(managerAddress));
        }
        obj.pushKV("txidPrev", txidPrev.ToString());
    }
};

class CDomainTransferPayload {
public:
    static const uint8_t CURRENT_VERSION = 1;
    uint8_t nVersion = CURRENT_VERSION;
    std::string strDomainName;
    CKeyID newOwnerAddress;        // Recipient address
    uint256 inputsHash;            // replay protection
    std::vector<uint8_t> vchSig;   // Current owner signature proof

    SERIALIZE_METHODS(CDomainTransferPayload, obj)
    {
        READWRITE(obj.nVersion, obj.strDomainName, obj.newOwnerAddress, obj.inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.vchSig);
        }
    }

    std::string MakeSignString() const {
        return strDomainName + newOwnerAddress.ToString() + inputsHash.ToString();
    }

    void ToJson(UniValue& obj) const {
        obj.clear();
        obj.setObject();
        obj.pushKV("version", (int)nVersion);
        obj.pushKV("domainName", strDomainName);
        obj.pushKV("newOwnerAddress", EncodeDestination(newOwnerAddress));
    }
};

class CDomainCommitPayload {
public:
    static const uint8_t CURRENT_VERSION = 1;
    uint8_t nVersion = CURRENT_VERSION;
    uint256 hash;                  // SHA256(domain_name_bytes + owner_address_bytes + salt_bytes)
    int64_t nTime = 0;             // Timestamp

    SERIALIZE_METHODS(CDomainCommitPayload, obj)
    {
        READWRITE(obj.nVersion, obj.hash, obj.nTime);
    }

    std::string MakeSignString() const {
        return hash.ToString() + std::to_string(nTime);
    }

    void ToJson(UniValue& obj) const {
        obj.clear();
        obj.setObject();
        obj.pushKV("version", (int)nVersion);
        obj.pushKV("hash", hash.ToString());
        obj.pushKV("time", nTime);
    }
};

#endif // RAPTOREUM_EVO_DOMAINPAYLOADS_H
