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
    static const uint8_t CURRENT_VERSION = 1;
    uint8_t nVersion = CURRENT_VERSION;
    std::string strDomainName;     // e.g., "y3ti.rtm" or "y3ti.raptoreum"
    CKeyID ownerAddress;           // Primary owner address
    CAmount nFeePaid = 0;          // Standard/Premium RTM fee sent
    uint64_t nRegistrationTime = 0;// Block timestamp
    uint256 inputsHash;            // replay protection
    std::vector<uint8_t> vchSig;   // Owner signature proof

    SERIALIZE_METHODS(CDomainRegisterPayload, obj)
    {
        READWRITE(obj.nVersion, obj.strDomainName, obj.ownerAddress, obj.nFeePaid, obj.nRegistrationTime, obj.inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.vchSig);
        }
    }

    std::string MakeSignString() const {
        return strDomainName + ownerAddress.ToString() + std::to_string(nFeePaid) + std::to_string(nRegistrationTime) + inputsHash.ToString();
    }

    void ToJson(UniValue& obj) const {
        obj.clear();
        obj.setObject();
        obj.pushKV("version", (int)nVersion);
        obj.pushKV("domainName", strDomainName);
        obj.pushKV("ownerAddress", EncodeDestination(ownerAddress));
        obj.pushKV("feePaid", nFeePaid);
        obj.pushKV("registrationTime", (int64_t)nRegistrationTime);
    }
};

class CDomainUpdatePayload {
public:
    static const uint8_t CURRENT_VERSION = 1;
    uint8_t nVersion = CURRENT_VERSION;
    std::string strDomainName;
    CKeyID primaryAddress;         // RTM address this domain resolves to
    std::string strIpfsCid;        // Optional IPFS CID containing record json
    std::string strJsonMetadata;   // Dynamic DNS, subdomains, and multi-chain records
    uint256 txidPrev;              // Previous update TXID to track history
    uint256 inputsHash;            // replay protection
    std::vector<uint8_t> vchSig;   // Owner signature proof

    SERIALIZE_METHODS(CDomainUpdatePayload, obj)
    {
        READWRITE(obj.nVersion, obj.strDomainName, obj.primaryAddress, obj.strIpfsCid, obj.strJsonMetadata, obj.txidPrev, obj.inputsHash);
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(obj.vchSig);
        }
    }

    std::string MakeSignString() const {
        return strDomainName + primaryAddress.ToString() + strIpfsCid + strJsonMetadata + txidPrev.ToString() + inputsHash.ToString();
    }

    void ToJson(UniValue& obj) const {
        obj.clear();
        obj.setObject();
        obj.pushKV("version", (int)nVersion);
        obj.pushKV("domainName", strDomainName);
        obj.pushKV("primaryAddress", EncodeDestination(primaryAddress));
        obj.pushKV("ipfsCid", strIpfsCid);
        obj.pushKV("jsonMetadata", strJsonMetadata);
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

#endif // RAPTOREUM_EVO_DOMAINPAYLOADS_H
