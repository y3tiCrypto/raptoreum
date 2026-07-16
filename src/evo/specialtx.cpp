// Copyright (c) 2018-2021 The Dash Core developers
// Copyright (c) 2020-2023 The Raptoreum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <evo/specialtx.h>

#include <chainparams.h>
#include <consensus/validation.h>
#include <hash.h>
#include <primitives/block.h>
#include <validation.h>
#include <evo/cbtx.h>
#include <evo/deterministicmns.h>
#include <llmq/quorums_commitment.h>
#include <llmq/quorums_blockprocessor.h>
#include <evo/domaindb.h>
#include <evo/domainpayloads.h>
#include <evo/domaintx.h>

bool CheckSpecialTx(const CTransaction &tx, const CBlockIndex *pindexPrev, CValidationState &state,
                    const CCoinsViewCache &view, CAssetsCache *assetsCache, bool check_sigs) {
    if (tx.nVersion != 3 || tx.nType == TRANSACTION_NORMAL)
        return true;

    if (!Params().GetConsensus().DIP0003Enabled) {
        return state.DoS(10, false, REJECT_INVALID, "bad-tx-type");
    }

    try {
        switch (tx.nType) {
            case TRANSACTION_PROVIDER_REGISTER:
                return CheckProRegTx(tx, pindexPrev, state, view, check_sigs);
            case TRANSACTION_PROVIDER_UPDATE_SERVICE:
                return CheckProUpServTx(tx, pindexPrev, state, check_sigs);
            case TRANSACTION_PROVIDER_UPDATE_REGISTRAR:
                return CheckProUpRegTx(tx, pindexPrev, state, view, check_sigs);
            case TRANSACTION_PROVIDER_UPDATE_REVOKE:
                return CheckProUpRevTx(tx, pindexPrev, state, check_sigs);
            case TRANSACTION_COINBASE:
                return CheckCbTx(tx, pindexPrev, state);
            case TRANSACTION_QUORUM_COMMITMENT:
                return llmq::CheckLLMQCommitment(tx, pindexPrev, state);
            case TRANSACTION_FUTURE:
                return CheckFutureTx(tx, pindexPrev, state);
            case TRANSACTION_NEW_ASSET:
                return CheckNewAssetTx(tx, pindexPrev, state, assetsCache);
            case TRANSACTION_UPDATE_ASSET:
                return CheckUpdateAssetTx(tx, pindexPrev, state, view, assetsCache);
            case TRANSACTION_MINT_ASSET:
                return CheckMintAssetTx(tx, pindexPrev, state, view, assetsCache);
            case TRANSACTION_DOMAIN_REGISTER:
                return CheckDomainRegisterTx(tx, pindexPrev, state);
            case TRANSACTION_DOMAIN_UPDATE:
                return CheckDomainUpdateTx(tx, pindexPrev, state);
            case TRANSACTION_DOMAIN_TRANSFER:
                return CheckDomainTransferTx(tx, pindexPrev, state);
            case TRANSACTION_DOMAIN_COMMIT:
                return CheckDomainCommitTx(tx, pindexPrev, state);
        }
    } catch (const std::exception &e) {
        LogPrintf("%s -- failed: %s\n", __func__, e.what());
        return state.DoS(100, false, REJECT_INVALID, "failed-check-special-tx");
    }

    return state.DoS(10, false, REJECT_INVALID, "bad-tx-type-check");
}

bool ProcessSpecialTx(const CTransaction &tx, const CBlockIndex *pindex, CValidationState &state) {
    if (tx.nVersion != 3 || tx.nType == TRANSACTION_NORMAL) {
        return true;
    }

    switch (tx.nType) {
        case TRANSACTION_PROVIDER_REGISTER:
        case TRANSACTION_PROVIDER_UPDATE_SERVICE:
        case TRANSACTION_PROVIDER_UPDATE_REGISTRAR:
        case TRANSACTION_PROVIDER_UPDATE_REVOKE:
            return true; // handled in batches per block
        case TRANSACTION_COINBASE:
            return true; // nothing to do
        case TRANSACTION_QUORUM_COMMITMENT:
            return true; // handled per block
        case TRANSACTION_FUTURE:
            return true;
        case TRANSACTION_NEW_ASSET:
            return true;
        case TRANSACTION_UPDATE_ASSET:
            return true;
        case TRANSACTION_MINT_ASSET:
            return true;
        case TRANSACTION_DOMAIN_REGISTER: {
            CDomainRegisterPayload payload;
            if (!GetTxPayload(tx, payload)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-domain-register-payload");
            }
            CDomainMetaData existingMeta;
            bool existed = pdomaindb && pdomaindb->ReadDomainData(payload.strDomainName, existingMeta);

            CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
            ss << payload.strDomainName << payload.ownerAddress << payload.salt;
            uint256 commitHash = ss.GetHash();

            int nCommitHeight = 0;
            if (pdomaindb) {
                pdomaindb->ReadCommitment(commitHash, nCommitHeight);
            }

            CDomainBlockUndo undoRecord;
            undoRecord.strDomainName = payload.strDomainName;
            undoRecord.fWasNew = true;
            undoRecord.revealedCommitHash = commitHash;
            undoRecord.nCommitHeight = nCommitHeight;
            if (existed) {
                undoRecord.prevMetadata = existingMeta;
            }

            CDomainMetaData newMeta;
            newMeta.name = payload.strDomainName;
            newMeta.owner = payload.ownerAddress;
            newMeta.resolver = payload.ownerAddress;
            newMeta.manager = CKeyID();
            newMeta.registered_at = pindex->GetBlockTime();
            newMeta.expires_at = pindex->GetBlockTime() + 31536000;
            newMeta.ipfs_cid = "";
            newMeta.json_metadata = "";

            if (pdomaindb) {
                pdomaindb->WriteDomainData(payload.strDomainName, newMeta);
                pdomaindb->WriteReverseRecord(payload.ownerAddress, payload.strDomainName);
                pdomaindb->EraseCommitment(commitHash);

                std::vector<CDomainBlockUndo> undoData;
                pdomaindb->ReadBlockUndoData(pindex->GetBlockHash(), undoData);
                undoData.push_back(undoRecord);
                pdomaindb->WriteBlockUndoData(pindex->GetBlockHash(), undoData);
            }
            return true;
        }
        case TRANSACTION_DOMAIN_UPDATE: {
            CDomainUpdatePayload payload;
            if (!GetTxPayload(tx, payload)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-domain-update-payload");
            }
            CDomainMetaData existingMeta;
            if (pdomaindb && pdomaindb->ReadDomainData(payload.strDomainName, existingMeta)) {
                CDomainBlockUndo undoRecord;
                undoRecord.strDomainName = payload.strDomainName;
                undoRecord.fWasNew = false;
                undoRecord.prevMetadata = existingMeta;

                CDomainMetaData updatedMeta = existingMeta;
                updatedMeta.resolver = payload.primaryAddress;
                updatedMeta.ipfs_cid = payload.strIpfsCid;
                updatedMeta.json_metadata = payload.strJsonMetadata;
                if (payload.nVersion >= 2) {
                    updatedMeta.manager = payload.managerAddress;
                }

                pdomaindb->WriteDomainData(payload.strDomainName, updatedMeta);

                if (existingMeta.resolver != payload.primaryAddress) {
                    pdomaindb->EraseReverseRecord(existingMeta.resolver);
                    pdomaindb->WriteReverseRecord(payload.primaryAddress, payload.strDomainName);
                }

                std::vector<CDomainBlockUndo> undoData;
                pdomaindb->ReadBlockUndoData(pindex->GetBlockHash(), undoData);
                undoData.push_back(undoRecord);
                pdomaindb->WriteBlockUndoData(pindex->GetBlockHash(), undoData);
            }
            return true;
        }
        case TRANSACTION_DOMAIN_TRANSFER: {
            CDomainTransferPayload payload;
            if (!GetTxPayload(tx, payload)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-domain-transfer-payload");
            }
            CDomainMetaData existingMeta;
            if (pdomaindb && pdomaindb->ReadDomainData(payload.strDomainName, existingMeta)) {
                CDomainBlockUndo undoRecord;
                undoRecord.strDomainName = payload.strDomainName;
                undoRecord.fWasNew = false;
                undoRecord.prevMetadata = existingMeta;

                CDomainMetaData updatedMeta = existingMeta;
                updatedMeta.owner = payload.newOwnerAddress;
                updatedMeta.manager = CKeyID(); // Clear manager on transfer

                pdomaindb->WriteDomainData(payload.strDomainName, updatedMeta);

                std::vector<CDomainBlockUndo> undoData;
                pdomaindb->ReadBlockUndoData(pindex->GetBlockHash(), undoData);
                undoData.push_back(undoRecord);
                pdomaindb->WriteBlockUndoData(pindex->GetBlockHash(), undoData);
            }
            return true;
        }
        case TRANSACTION_DOMAIN_COMMIT: {
            CDomainCommitPayload payload;
            if (!GetTxPayload(tx, payload)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-domain-commit-payload");
            }
            if (pdomaindb) {
                pdomaindb->WriteCommitment(payload.hash, pindex->nHeight);
            }
            return true;
        }
    }
    return state.DoS(100, false, REJECT_INVALID, "bad-tx-type-proc");
}

bool UndoSpecialTx(const CTransaction &tx, const CBlockIndex *pindex) {
    if (tx.nVersion != 3 || tx.nType == TRANSACTION_NORMAL) {
        return true;
    }

    switch (tx.nType) {
        case TRANSACTION_PROVIDER_REGISTER:
        case TRANSACTION_PROVIDER_UPDATE_SERVICE:
        case TRANSACTION_PROVIDER_UPDATE_REGISTRAR:
        case TRANSACTION_PROVIDER_UPDATE_REVOKE:
            return true; // handled in batches per block
        case TRANSACTION_COINBASE:
            return true; // nothing to do
        case TRANSACTION_QUORUM_COMMITMENT:
            return true; // handled per block
        case TRANSACTION_FUTURE:
            return true;
        case TRANSACTION_NEW_ASSET:
            return true;
        case TRANSACTION_UPDATE_ASSET:
            return true;
        case TRANSACTION_MINT_ASSET:
            return true;
        case TRANSACTION_DOMAIN_COMMIT: {
            CDomainCommitPayload payload;
            if (!GetTxPayload(tx, payload)) {
                return false;
            }
            if (pdomaindb) {
                pdomaindb->EraseCommitment(payload.hash);
            }
            return true;
        }
        case TRANSACTION_DOMAIN_REGISTER:
        case TRANSACTION_DOMAIN_UPDATE:
        case TRANSACTION_DOMAIN_TRANSFER: {
            if (pdomaindb) {
                std::vector<CDomainBlockUndo> undoData;
                if (pdomaindb->ReadBlockUndoData(pindex->GetBlockHash(), undoData) && !undoData.empty()) {
                    CDomainBlockUndo undo = undoData.back();
                    undoData.pop_back();

                    if (undoData.empty()) {
                        pdomaindb->EraseBlockUndoData(pindex->GetBlockHash());
                    } else {
                        pdomaindb->WriteBlockUndoData(pindex->GetBlockHash(), undoData);
                    }

                    if (undo.fWasNew) {
                        CDomainMetaData currentMeta;
                        if (pdomaindb->ReadDomainData(undo.strDomainName, currentMeta)) {
                            pdomaindb->EraseReverseRecord(currentMeta.owner);
                        }
                        pdomaindb->EraseDomainData(undo.strDomainName);

                        if (!undo.prevMetadata.name.empty()) {
                            pdomaindb->WriteDomainData(undo.strDomainName, undo.prevMetadata);
                            pdomaindb->WriteReverseRecord(undo.prevMetadata.owner, undo.strDomainName);
                        }

                        if (!undo.revealedCommitHash.IsNull()) {
                            pdomaindb->WriteCommitment(undo.revealedCommitHash, undo.nCommitHeight);
                        }
                    } else {
                        CDomainMetaData currentMeta;
                        if (pdomaindb->ReadDomainData(undo.strDomainName, currentMeta)) {
                            pdomaindb->EraseReverseRecord(currentMeta.resolver);
                        }
                        pdomaindb->WriteDomainData(undo.strDomainName, undo.prevMetadata);
                        pdomaindb->WriteReverseRecord(undo.prevMetadata.resolver, undo.strDomainName);
                    }
                }
            }
            return true;
        }
    }
    return false;
}

bool ProcessSpecialTxsInBlock(const CBlock &block, const CBlockIndex *pindex, CValidationState &state,
                              const CCoinsViewCache &view, CAssetsCache *assetsCache, bool fJustCheck,
                              bool fCheckCbTxMerleRoots) {
    AssertLockHeld(cs_main);

    try {
        static int64_t nTimeLoop = 0;
        static int64_t nTimeQuorum = 0;
        static int64_t nTimeDMN = 0;
        static int64_t nTimeMerkle = 0;

        int64_t nTime1 = GetTimeMicros();

        for (const auto &ptr_tx: block.vtx) {
            if (!CheckSpecialTx(*ptr_tx, pindex->pprev, state, view, assetsCache, fCheckCbTxMerleRoots)) {
                // pass the state returned by the function above
                return false;
            }
            if (!ProcessSpecialTx(*ptr_tx, pindex, state)) {
                // pass the state returned by the function above
                return false;
            }
        }

        int64_t nTime2 = GetTimeMicros();
        nTimeLoop += nTime2 - nTime1;
        LogPrint(BCLog::BENCHMARK, "        - Loop: %.2fms [%.2fs]\n", 0.001 * (nTime2 - nTime1), nTimeLoop * 0.000001);

        if (!llmq::quorumBlockProcessor->ProcessBlock(block, pindex, state, fJustCheck, fCheckCbTxMerleRoots)) {
            // pass the state returned by the function above
            return false;
        }

        int64_t nTime3 = GetTimeMicros();
        nTimeQuorum += nTime3 - nTime2;
        LogPrint(BCLog::BENCHMARK, "        - quorumBlockProcessor: %.2fms [%.2fs]\n", 0.001 * (nTime3 - nTime2),
                 nTimeQuorum * 0.000001);

        if (!deterministicMNManager->ProcessBlock(block, pindex, state, view, fJustCheck)) {
            // pass the state returned by the function above
            return false;
        }

        int64_t nTime4 = GetTimeMicros();
        nTimeDMN += nTime4 - nTime3;
        LogPrint(BCLog::BENCHMARK, "        - deterministicMNManager: %.2fms [%.2fs]\n", 0.001 * (nTime4 - nTime3),
                 nTimeDMN * 0.000001);

        if (fCheckCbTxMerleRoots && !CheckCbTxMerkleRoots(block, pindex, state, view)) {
            // pass the state returned by the function above
            return false;
        }

        int64_t nTime5 = GetTimeMicros();
        nTimeMerkle += nTime5 - nTime4;
        LogPrint(BCLog::BENCHMARK, "        - CheckCbTxMerkleRoots: %.2fms [%.2fs]\n", 0.001 * (nTime5 - nTime4),
                 nTimeMerkle * 0.000001);
    } catch (const std::exception &e) {
        LogPrintf("%s -- failed: %s\n", __func__, e.what());
        return state.DoS(100, false, REJECT_INVALID, "failed-procspectxsinblock");
    }

    return true;
}

bool UndoSpecialTxsInBlock(const CBlock &block, const CBlockIndex *pindex) {
    AssertLockHeld(cs_main);

    try {
        for (int i = (int) block.vtx.size() - 1; i >= 0; --i) {
            const CTransaction &tx = *block.vtx[i];
            if (!UndoSpecialTx(tx, pindex)) {
                return false;
            }
        }

        if (!deterministicMNManager->UndoBlock(block, pindex)) {
            return false;
        }

        if (!llmq::quorumBlockProcessor->UndoBlock(block, pindex)) {
            return false;
        }
    } catch (const std::exception &e) {
        return error(strprintf("%s -- failed: %s\n", __func__, e.what()).c_str());
    }

    return true;
}

uint256 CalcTxInputsHash(const CTransaction &tx) {
    CHashWriter hw(CLIENT_VERSION, SER_GETHASH);
    for (const auto &in: tx.vin) {
        hw << in.prevout;
    }
    return hw.GetHash();
}
