// Copyright (c) 2026 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef RAPTOREUM_EVO_DOMAINTX_H
#define RAPTOREUM_EVO_DOMAINTX_H

#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <chainparams.h>
#include <string>

class CBlockIndex;

bool IsDomainNameValid(const std::string& fullName, std::string& label, std::string& tld);

bool CheckDomainRegisterTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);
bool CheckDomainUpdateTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);
bool CheckDomainTransferTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state);

std::string GetRNSDevAddress();
std::string GetRNSDonationAddress();

#endif // RAPTOREUM_EVO_DOMAINTX_H
