// Copyright (c) 2026 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <rpc/util.h>
#include <validation.h>
#include <key_io.h>
#include <evo/domaindb.h>
#include <evo/domainpayloads.h>
#include <evo/domaintx.h>
#include <util/time.h>
#include <univalue.h>

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#include <rpc/specialtx_utilities.h>
#endif // ENABLE_WALLET

static UniValue resolvename(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2) {
        throw std::runtime_error(
            "resolvename \"name\" ( \"type\" )\n"
            "\nResolves an RNS domain name.\n"
            "\nArguments:\n"
            "1. \"name\"             (string, required) The domain name (e.g. \"y3ti.rtm\")\n"
            "2. \"type\"             (string, optional) Specific record type (e.g. \"A\")\n"
            "\nResult:\n"
            "{\n"
            "  \"registered\": true/false,\n"
            "  \"name\": \"...\",\n"
            "  \"owner\": \"...\",\n"
            "  \"resolver\": \"...\",\n"
            "  \"ipfs\": \"...\",\n"
            "  \"records\": { ... },\n"
            "  \"registeredAt\": n,\n"
            "  \"expiresAt\": n,\n"
            "  \"status\": \"...\"\n"
            "}\n"
        );
    }

    std::string name = request.params[0].get_str();
    CDomainMetaData meta;
    if (!pdomaindb || !pdomaindb->ReadDomainData(name, meta)) {
        UniValue result(UniValue::VOBJ);
        result.pushKV("registered", false);
        return result;
    }

    uint64_t now = GetTime();
    if (now > meta.expires_at + 30 * 86400) {
        UniValue result(UniValue::VOBJ);
        result.pushKV("registered", false);
        return result;
    }

    std::string status = (now > meta.expires_at) ? "expired" : "active";

    UniValue result(UniValue::VOBJ);
    result.pushKV("registered", true);
    result.pushKV("name", name);
    result.pushKV("owner", EncodeDestination(meta.owner));
    result.pushKV("resolver", EncodeDestination(meta.resolver));
    result.pushKV("ipfs", meta.ipfs_cid);

    UniValue recordsObj;
    if (recordsObj.read(meta.json_metadata)) {
        result.pushKV("records", recordsObj);
    } else {
        UniValue empty(UniValue::VOBJ);
        result.pushKV("records", empty);
    }

    result.pushKV("registeredAt", (int64_t)meta.registered_at);
    result.pushKV("expiresAt", (int64_t)meta.expires_at);
    result.pushKV("status", status);

    if (request.params.size() == 2) {
        std::string type = request.params[1].get_str();
        if (result["records"].isObject()) {
            UniValue val = result["records"][type];
            if (!val.isNull()) {
                return val;
            }
        }
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Record type not found");
    }

    return result;
}

static UniValue reverseresolve(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "reverseresolve \"address\"\n"
            "\nReverse resolves an RTM address to its primary domain name.\n"
            "\nArguments:\n"
            "1. \"address\"          (string, required) Raptoreum address\n"
        );
    }

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Raptoreum address");
    }

    const CKeyID* keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Address does not have key ID");
    }

    std::string domainName;
    if (!pdomaindb || !pdomaindb->ReadReverseRecord(*keyID, domainName)) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "No reverse record found for this address");
    }

    UniValue result(UniValue::VOBJ);
    result.pushKV("address", request.params[0].get_str());
    result.pushKV("domain", domainName);
    return result;
}

#ifdef ENABLE_WALLET
static UniValue registerdomain(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "registerdomain \"name\" \"owner_address\"\n"
            "\nRegisters a new RNS domain name.\n"
            "\nArguments:\n"
            "1. \"name\"             (string, required) Domain name (e.g. \"y3ti.rtm\")\n"
            "2. \"owner_address\"    (string, required) Destination owner address\n"
        );
    }

    std::string name = request.params[0].get_str();
    std::string label, tld;
    if (!IsDomainNameValid(name, label, tld)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid domain name format");
    }

    CTxDestination dest = DecodeDestination(request.params[1].get_str());
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid owner address");
    }

    const CKeyID* keyID = boost::get<CKeyID>(&dest);
    if (!keyID) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Address does not have key ID");
    }

    // Check collision
    CDomainMetaData meta;
    if (pdomaindb && pdomaindb->ReadDomainData(name, meta)) {
        uint64_t now = GetTime();
        if (now <= meta.expires_at + 30 * 86400) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Domain name is already registered or in grace period");
        }
    }

    CAmount requiredFee = 0;
    size_t len = label.length();
    if (len >= 1 && len <= 3) {
        requiredFee = 2000 * COIN;
    } else if (len == 4) {
        requiredFee = 1000 * COIN;
    } else {
        requiredFee = 100 * COIN;
    }

    CMutableTransaction tx;
    tx.nVersion = 3;
    tx.nType = TRANSACTION_DOMAIN_REGISTER;

    // Create 50/50 split outputs
    CAmount devFee = requiredFee / 2;
    CAmount donationFee = requiredFee - devFee;

    CScript devScript = GetScriptForDestination(DecodeDestination(GetRNSDevAddress()));
    CScript donationScript = GetScriptForDestination(DecodeDestination(GetRNSDonationAddress()));

    if (GetRNSDevAddress() == GetRNSDonationAddress()) {
        tx.vout.emplace_back(requiredFee, devScript);
    } else {
        tx.vout.emplace_back(devFee, devScript);
        tx.vout.emplace_back(donationFee, donationScript);
    }

    CDomainRegisterPayload payload;
    payload.strDomainName = name;
    payload.ownerAddress = *keyID;
    payload.nFeePaid = requiredFee;
    payload.nRegistrationTime = GetTime();

    // make sure sig fits
    payload.vchSig.resize(65);

    FundSpecialTx(pwallet, tx, payload, *keyID);

    CKey key;
    if (!pwallet->GetKey(*keyID, key)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for owner address not found in wallet");
    }

    SignSpecialTxPayloadByString(tx, payload, key);

    // Set payload back
    CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
    ds << payload;
    tx.vExtraPayload.assign(ds.begin(), ds.end());

    return SignAndSendSpecialTx(request, tx);
}

static UniValue updatedomain(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 4) {
        throw std::runtime_error(
            "updatedomain \"name\" \"resolver_address\" \"ipfs_cid\" \"records_json\"\n"
            "\nUpdates records for a registered domain.\n"
            "\nArguments:\n"
            "1. \"name\"             (string, required) Domain name\n"
            "2. \"resolver_address\" (string, required) Resolution address\n"
            "3. \"ipfs_cid\"         (string, required) IPFS CID\n"
            "4. \"records_json\"     (string, required) Records metadata in JSON format\n"
        );
    }

    std::string name = request.params[0].get_str();
    CDomainMetaData meta;
    if (!pdomaindb || !pdomaindb->ReadDomainData(name, meta)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Domain not found");
    }

    uint64_t now = GetTime();
    if (now > meta.expires_at) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Domain is expired");
    }

    CTxDestination resolverDest = DecodeDestination(request.params[1].get_str());
    if (!IsValidDestination(resolverDest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid resolver address");
    }

    const CKeyID* resolverKeyID = boost::get<CKeyID>(&resolverDest);
    if (!resolverKeyID) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Resolver address does not have key ID");
    }

    std::string ipfsCid = request.params[2].get_str();
    std::string recordsJson = request.params[3].get_str();

    UniValue testObj;
    if (!testObj.read(recordsJson)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "records_json must be valid JSON");
    }

    CMutableTransaction tx;
    tx.nVersion = 3;
    tx.nType = TRANSACTION_DOMAIN_UPDATE;

    CDomainUpdatePayload payload;
    payload.strDomainName = name;
    payload.primaryAddress = *resolverKeyID;
    payload.strIpfsCid = ipfsCid;
    payload.strJsonMetadata = recordsJson;
    payload.txidPrev = uint256(); // can be extended to track history if needed

    payload.vchSig.resize(65);

    FundSpecialTx(pwallet, tx, payload, meta.owner);

    CKey key;
    if (!pwallet->GetKey(meta.owner, key)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for domain owner not found in wallet");
    }

    SignSpecialTxPayloadByString(tx, payload, key);

    CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
    ds << payload;
    tx.vExtraPayload.assign(ds.begin(), ds.end());

    return SignAndSendSpecialTx(request, tx);
}

static UniValue transferdomain(const JSONRPCRequest& request) {
    CWallet* const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "transferdomain \"name\" \"new_owner_address\"\n"
            "\nTransfers ownership of a domain.\n"
            "\nArguments:\n"
            "1. \"name\"             (string, required) Domain name\n"
            "2. \"new_owner_address\" (string, required) New owner address\n"
        );
    }

    std::string name = request.params[0].get_str();
    CDomainMetaData meta;
    if (!pdomaindb || !pdomaindb->ReadDomainData(name, meta)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Domain not found");
    }

    uint64_t now = GetTime();
    if (now > meta.expires_at) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Domain is expired");
    }

    CTxDestination newOwnerDest = DecodeDestination(request.params[1].get_str());
    if (!IsValidDestination(newOwnerDest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid new owner address");
    }

    const CKeyID* newOwnerKeyID = boost::get<CKeyID>(&newOwnerDest);
    if (!newOwnerKeyID) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "New owner address does not have key ID");
    }

    CMutableTransaction tx;
    tx.nVersion = 3;
    tx.nType = TRANSACTION_DOMAIN_TRANSFER;

    CDomainTransferPayload payload;
    payload.strDomainName = name;
    payload.newOwnerAddress = *newOwnerKeyID;

    payload.vchSig.resize(65);

    FundSpecialTx(pwallet, tx, payload, meta.owner);

    CKey key;
    if (!pwallet->GetKey(meta.owner, key)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for domain owner not found in wallet");
    }

    SignSpecialTxPayloadByString(tx, payload, key);

    CDataStream ds(SER_NETWORK, PROTOCOL_VERSION);
    ds << payload;
    tx.vExtraPayload.assign(ds.begin(), ds.end());

    return SignAndSendSpecialTx(request, tx);
}
#endif // ENABLE_WALLET

static const CRPCCommand commands[] = {
    {"domain", "resolvename", &resolvename, {"name", "type"}},
    {"domain", "reverseresolve", &reverseresolve, {"address"}},
#ifdef ENABLE_WALLET
    {"domain", "registerdomain", &registerdomain, {"name", "owner_address"}},
    {"domain", "updatedomain", &updatedomain, {"name", "resolver_address", "ipfs_cid", "records_json"}},
    {"domain", "transferdomain", &transferdomain, {"name", "new_owner_address"}},
#endif // ENABLE_WALLET
};

void RegisterDomainRPCCommands(CRPCTable &tableRPC) {
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
