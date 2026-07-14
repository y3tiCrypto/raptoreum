// Copyright (c) 2026 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/domaintx.h>
#include <evo/domainpayloads.h>
#include <evo/domaindb.h>
#include <validation.h>
#include <messagesigner.h>
#include <chainparams.h>
#include <key_io.h>
#include <util/time.h>

std::string GetRNSDevAddress() {
    std::string net = Params().NetworkIDString();
    if (net == "main") {
        return "R9coYU9tBjqj7LVhc5Q8nkX2nogH1Bm6hY";
    } else if (net == "test") {
        return "yYhBxduZLMnancMkpzvcLFCiTgZRSk8wun";
    } else { // regtest, devnet
        return "yaackz5YDLnFuuX6gGzEs9EMRQGfqmNYjc";
    }
}

std::string GetRNSDonationAddress() {
    std::string net = Params().NetworkIDString();
    if (net == "main") {
        return "RWGvGpd3yJdnfh9ziyHNDEoHMJBvnZ23zK";
    } else if (net == "test") {
        return "yYhBxduZLMnancMkpzvcLFCiTgZRSk8wun";
    } else { // regtest, devnet
        return "yaackz5YDLnFuuX6gGzEs9EMRQGfqmNYjc";
    }
}

bool IsDomainNameValid(const std::string& fullName, std::string& label, std::string& tld) {
    size_t lastDot = fullName.find_last_of('.');
    if (lastDot == std::string::npos) return false;
    label = fullName.substr(0, lastDot);
    tld = fullName.substr(lastDot + 1);

    if (tld != "rtm" && tld != "raptoreum") return false;
    if (label.empty() || label.length() > 30) return false;

    // Check character set: alphanumeric and hyphens
    for (char c : label) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

bool CheckDomainRegisterTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state) {
    CDomainRegisterPayload payload;
    if (!GetTxPayload(tx, payload)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-payload");
    }

    if (payload.nVersion == 0 || payload.nVersion > CDomainRegisterPayload::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-version");
    }

    std::string label, tld;
    if (!IsDomainNameValid(payload.strDomainName, label, tld)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-name");
    }

    // Check fee paid
    CAmount requiredFee = 0;
    size_t len = label.length();
    if (len >= 1 && len <= 3) {
        requiredFee = 2000 * COIN;
    } else if (len == 4) {
        requiredFee = 1000 * COIN;
    } else {
        requiredFee = 100 * COIN;
    }

    if (payload.nFeePaid < requiredFee) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-fee-insufficient");
    }

    // Check fee distribution (50/50 Split)
    CAmount expectedDevFee = payload.nFeePaid / 2;
    CAmount expectedDonationFee = payload.nFeePaid - expectedDevFee;

    CAmount devPaid = 0;
    CAmount donationPaid = 0;
    CScript devScript = GetScriptForDestination(DecodeDestination(GetRNSDevAddress()));
    CScript donationScript = GetScriptForDestination(DecodeDestination(GetRNSDonationAddress()));

    for (const auto& out : tx.vout) {
        if (out.scriptPubKey == devScript) {
            devPaid += out.nValue;
        } else if (out.scriptPubKey == donationScript) {
            donationPaid += out.nValue;
        }
    }

    if (GetRNSDevAddress() == GetRNSDonationAddress()) {
        if (devPaid < payload.nFeePaid) {
            return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-fee-split");
        }
    } else {
        if (devPaid < expectedDevFee || donationPaid < expectedDonationFee) {
            return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-fee-split");
        }
    }

    // Check signature
    std::string strError;
    if (!CMessageSigner::VerifyMessage(payload.ownerAddress, payload.vchSig, payload.MakeSignString(), strError)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-sig", false, strError);
    }

    // Check collision in db
    CDomainMetaData meta;
    if (pdomaindb && pdomaindb->ReadDomainData(payload.strDomainName, meta)) {
        // Active or grace period check
        uint64_t now = pindexPrev ? pindexPrev->GetBlockTime() : GetTime();
        if (now <= meta.expires_at + 30 * 86400) {
            return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-collision");
        }
    }

    return true;
}

bool CheckDomainUpdateTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state) {
    CDomainUpdatePayload payload;
    if (!GetTxPayload(tx, payload)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-update-payload");
    }

    if (payload.nVersion == 0 || payload.nVersion > CDomainUpdatePayload::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-update-version");
    }

    CDomainMetaData meta;
    if (!pdomaindb || !pdomaindb->ReadDomainData(payload.strDomainName, meta)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-update-not-found");
    }

    // Expiration check (cannot update expired domains)
    uint64_t now = pindexPrev ? pindexPrev->GetBlockTime() : GetTime();
    if (now > meta.expires_at) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-update-expired");
    }

    // Verify signature of the CURRENT owner (meta.owner)
    std::string strError;
    if (!CMessageSigner::VerifyMessage(meta.owner, payload.vchSig, payload.MakeSignString(), strError)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-update-sig", false, strError);
    }

    return true;
}

bool CheckDomainTransferTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state) {
    CDomainTransferPayload payload;
    if (!GetTxPayload(tx, payload)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-transfer-payload");
    }

    if (payload.nVersion == 0 || payload.nVersion > CDomainTransferPayload::CURRENT_VERSION) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-transfer-version");
    }

    CDomainMetaData meta;
    if (!pdomaindb || !pdomaindb->ReadDomainData(payload.strDomainName, meta)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-transfer-not-found");
    }

    // Expiration check
    uint64_t now = pindexPrev ? pindexPrev->GetBlockTime() : GetTime();
    if (now > meta.expires_at) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-transfer-expired");
    }

    // Verify signature of the CURRENT owner (meta.owner)
    std::string strError;
    if (!CMessageSigner::VerifyMessage(meta.owner, payload.vchSig, payload.MakeSignString(), strError)) {
        return state.DoS(100, false, REJECT_INVALID, "bad-domain-transfer-sig", false, strError);
    }

    return true;
}
