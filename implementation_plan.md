# Implementation Plan: RNS Commit-Reveal and Owner-Manager Separation

This document describes the technical changes required to add **Commit-Reveal Registration** and **Owner/Manager Separation** features to the Raptoreum Name Service (RNS) native integration.

## Proposed Changes

We will introduce a new Special Transaction type `TRANSACTION_DOMAIN_COMMIT` (Type 14) and update RNS transaction processing and database indexing to enforce commit-reveal windows and separate administrative roles.

---

### 1. Special Transaction Payloads

#### [MODIFY] [transaction.h](file:///g:/Raptoreum/src/primitives/transaction.h)
- Add new transaction type to the Special Transaction enum:
  - `TRANSACTION_DOMAIN_COMMIT = 14`

#### [MODIFY] [domainpayloads.h](file:///g:/Raptoreum/src/evo/domainpayloads.h)
- Define `CDomainCommitPayload`:
  - `uint256 hash`: The cryptographic commitment hash.
  - `int64_t nTime`: The block time or transaction creation timestamp.
- Update `CDomainRegisterPayload` to version 2:
  - Add `uint256 salt` (a 32-byte secret value).
  - Update `MakeSignString` and serialization macro `SERIALIZE_METHODS` to serialize `salt` when `nVersion >= 2`.
- Update `CDomainUpdatePayload` to version 2:
  - Add `CKeyID managerAddress` (optional).
  - Update `MakeSignString` and `SERIALIZE_METHODS` to serialize `managerAddress` when `nVersion >= 2`.

---

### 2. On-Chain Database Indexing (`domaindb`)

#### [MODIFY] [domaindb.h](file:///g:/Raptoreum/src/evo/domaindb.h) & [domaindb.cpp](file:///g:/Raptoreum/src/evo/domaindb.cpp)
- Update `CDomainMetaData`:
  - Add `CKeyID manager` member.
  - Update `SERIALIZE_METHODS` to write and read `manager`.
- Update `CDomainBlockUndo`:
  - Add `uint256 revealedCommitHash` and `int nCommitHeight` to allow restoring spent commitments when a block is disconnected.
- Add commitment lookup methods in `CDomainDB`:
  - `bool WriteCommitment(const uint256& commitHash, int nHeight)`
  - `bool ReadCommitment(const uint256& commitHash, int& nHeight)`
  - `bool EraseCommitment(const uint256& commitHash)`

---

### 3. Consensus & Role Validation Logic

#### [MODIFY] [domaintx.h](file:///g:/Raptoreum/src/evo/domaintx.h) & [domaintx.cpp](file:///g:/Raptoreum/src/evo/domaintx.cpp)
- Implement `bool CheckDomainCommitTx(const CTransaction& tx, const CBlockIndex* pindexPrev, CValidationState& state)`.
- Define `std::string GetRNSBurnAddress()`:
  - Returns `RBurnAddressxxxxxxxxxxxxxxxxxxxxxxxxx` on Mainnet, `yBurnAddressxxxxxxxxxxxxxxxxxxxxxxxxx` on Testnet, or regtest equivalent.
- Update `CheckDomainRegisterTx`:
  - Enforce that `nVersion >= 2` payload is present.
  - Compute the commitment hash: `SHA256(domain_name + owner_address + salt)` using `CHashWriter`.
  - Query `pdomaindb` for `commitHash`.
  - Verify block window: `nHeightRegister - nHeightCommit >= 5` and `nHeightRegister - nHeightCommit <= 100`. Reject if outside this window.
- Update `CheckDomainUpdateTx`:
  - Allow signature verification to pass if signed by `meta.owner` OR `meta.manager`.
  - If signed by manager (and not owner):
    - Prevent changing `payload.managerAddress` (must equal `meta.manager`).
    - Prevent setting `payload.primaryAddress` (resolver) to the burn address.
    - Prevent setting status to "suspended", "revoked", or "inactive" inside `payload.strJsonMetadata`.
- Update `CheckDomainTransferTx`:
  - Strictly require signature of `meta.owner`. Reject manager-signed transfers.

---

### 4. Special Transaction Block Hook Processing

#### [MODIFY] [specialtx.cpp](file:///g:/Raptoreum/src/evo/specialtx.cpp)
- In `CheckSpecialTx`:
  - Route `TRANSACTION_DOMAIN_COMMIT` to `CheckDomainCommitTx`.
- In `ProcessSpecialTx`:
  - For `TRANSACTION_DOMAIN_COMMIT`: Write the commitment hash and current height `pindex->nHeight` to database.
  - For `TRANSACTION_DOMAIN_REGISTER`:
    - Lookup the commitment hash `SHA256(name + owner + salt)`. Save its hash and height in `undoRecord`.
    - Delete the commitment from database to prevent duplicate registration reveal replays.
    - Initialize `meta.manager` as empty/null.
  - For `TRANSACTION_DOMAIN_UPDATE`:
    - If `payload.nVersion >= 2`, update `meta.manager = payload.managerAddress`.
  - For `TRANSACTION_DOMAIN_TRANSFER`:
    - Update `meta.owner = payload.newOwnerAddress`.
    - Clear the manager: set `meta.manager = CKeyID()`.
- In `UndoSpecialTx`:
  - For `TRANSACTION_DOMAIN_COMMIT`: Delete the commitment from database.
  - For `TRANSACTION_DOMAIN_REGISTER`: Restore the deleted commitment `c_[revealedCommitHash]` -> `nCommitHeight`.

---

### 5. Mempool & Serialization Whitelists

#### [MODIFY] [validation.cpp](file:///g:/Raptoreum/src/validation.cpp)
- Add `TRANSACTION_DOMAIN_COMMIT` to the version 3 whitelist check inside `CheckTransaction`.

#### [MODIFY] [core_write.cpp](file:///g:/Raptoreum/src/core_write.cpp) & [rawtransaction.cpp](file:///g:/Raptoreum/src/rpc/rawtransaction.cpp)
- Format `domainCommitTx` payload to JSON response objects.

---

### 6. RPC Commands

#### [MODIFY] [domain.cpp](file:///g:/Raptoreum/src/rpc/domain.cpp)
- Update `resolvename` output to include the `manager` address string (if set).
- Add new RPC command `commitdomain <hash>` to broadcast domain commitments.
- Update `registerdomain <name> <owner_address> <salt>` to support version 2 registers.
- Update `updatedomain` to accept optional `manager_address` parameter.

---

## Verification Plan

### Automated Tests
- Compile the changes and run the domain database tests.

### Manual Verification
- Test commit-reveal registration sequence:
  1. Generate commit hash and broadcast via `commitdomain`.
  2. Verify that `registerdomain` fails if broadcasted before 5 blocks or after 100 blocks.
  3. Verify that `registerdomain` succeeds when revealed within the valid window (5-100 blocks).
  4. Verify that attempting to reuse the same commitment reveals fail.
- Test Owner-Manager Separation:
  1. Register a domain, assign a manager address.
  2. Update metadata using the manager's private key. Verify success.
  3. Attempt to transfer domain using the manager's key. Verify it is rejected.
  4. Transfer domain using the owner's key. Verify success and check that the manager address has been cleared to NULL.
  5. Attempt to suspend the domain using the manager's key. Verify it is rejected.
