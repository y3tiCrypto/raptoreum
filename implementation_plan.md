# Implementation Plan: Native Protocol-Level RNS Integration in Raptoreum Core

This document outlines the technical implementation plan for introducing native protocol-level support for the Raptoreum Name Service (RNS) in Raptoreum Core, including CLI, consensus, database, and QT Wallet integration.

## User Review Required

> [!IMPORTANT]
> The integration of RNS introduces three new Special Transaction types (`TRANSACTION_DOMAIN_REGISTER`, `TRANSACTION_DOMAIN_UPDATE`, `TRANSACTION_DOMAIN_TRANSFER`) that are enforced by consensus validator nodes. This represents a protocol-level upgrade.
> 
> LevelDB database entries (`domaindb`) will be used to store domain name ownership, resolution records, subdomains, and IPFS CIDs. A 50/50 fee distribution rule will be verified directly on-chain during domain registration blocks.

> [!TIP]
> The QT Wallet is updated to natively validate and resolve RNS domains. Users can enter domain names (e.g. `y3ti.rtm`) in "Pay To" fields for both RTM and asset sending dialogs. The wallet will resolve these on-the-fly via the local LevelDB consensus index, displaying the target address and domain name on final confirmations.

## Proposed Changes

We will introduce domain registry state tracking directly into Raptoreum's consensus, LevelDB, RPC, and QT UI layers.

---

### Raptoreum Core Consensus & Payloads

#### [MODIFY] [transaction.h](file:///g:/Raptoreum/src/primitives/transaction.h)
- Add new transaction types to the Special Transaction enum:
  - `TRANSACTION_DOMAIN_REGISTER = 11`
  - `TRANSACTION_DOMAIN_UPDATE = 12`
  - `TRANSACTION_DOMAIN_TRANSFER = 13`

#### [MODIFY] [tx_verify.cpp](file:///g:/Raptoreum/src/consensus/tx_verify.cpp)
- Add `TRANSACTION_DOMAIN_REGISTER`, `TRANSACTION_DOMAIN_UPDATE`, and `TRANSACTION_DOMAIN_TRANSFER` to `checkSpecialTxFee` switch to bypass special fee requirements (as RNS registrations use standard outputs to route fees).

#### [NEW] [domainpayloads.h](file:///g:/Raptoreum/src/evo/domainpayloads.h)
- Define Special Transaction payload classes:
  - `CDomainRegisterPayload` (version, domain name, owner address, fee paid, registration time, inputs hash replay protection, signature)
  - `CDomainUpdatePayload` (version, domain name, primary address, IPFS CID, JSON metadata, previous update txid, signature)
  - `CDomainTransferPayload` (version, domain name, new owner address, signature)
- Include serialization (`SERIALIZE_METHODS`) and signing helper methods (`MakeSignString` / `ToJson`).

#### [NEW] [domaintx.h](file:///g:/Raptoreum/src/evo/domaintx.h) & [domaintx.cpp](file:///g:/Raptoreum/src/evo/domaintx.cpp)
- Implement validation routines:
  - `IsDomainNameValid`: Validates character boundaries (1-30 characters, label regex `^[a-zA-Z0-9-]*$`, Punycode support `xn--`, extensions `.rtm` and `.raptoreum`).
  - `CheckDomainRegisterTx`: Checks label length fee requirements (1-3 chars: 2000 RTM; 4 chars: 1000 RTM; 5+ chars: 100 RTM), namespace collisions in the database (checking expiration/grace period rules), signature validation, and 50/50 fee-split destinations.
  - `CheckDomainUpdateTx` / `CheckDomainTransferTx`: Verifies ownership signature against current database state.
  - Payout split address getters resolving to Spork addresses or configured constants depending on network mode (Mainnet, Testnet, Regtest).

---

### Mempool Protection & Network Consistency

#### [MODIFY] [txmempool.h](file:///g:/Raptoreum/src/txmempool.h) & [txmempool.cpp](file:///g:/Raptoreum/src/txmempool.cpp)
- Introduce a tracking map `mapDomainsToHash` in the mempool to monitor active domain registrations and prevent mempool collision/front-running.
- Implement conflict checker `existsDomainTxConflict(const CTransaction& tx)` to prevent duplicate registrations from entering the mempool concurrently.

---

### On-Chain Domain Database (`domaindb`)

#### [NEW] [domaindb.h](file:///g:/Raptoreum/src/evo/domaindb.h) & [domaindb.cpp](file:///g:/Raptoreum/src/evo/domaindb.cpp)
- Create LevelDB-backed database wrapper class `CDomainDB` subclassing `CDBWrapper`.
- Implement CRUD operations for active domains:
  - Read/Write domain metadata (`d_[domainName]` -> `CDomainMetaData`).
  - Read/Write reverse resolution mapping (`r_[address]` -> `domainName`).
  - Read/Write block undo markers (`u_[blockHash]` -> `std::vector<CDomainBlockUndo>`) to rollback state changes during network reorganization.

#### [MODIFY] [specialtx.cpp](file:///g:/Raptoreum/src/evo/specialtx.cpp)
- Register verification and block connection/disconnection hooks for domain transaction types:
  - `CheckSpecialTx`: Route to `CheckDomainRegisterTx`, `CheckDomainUpdateTx`, and `CheckDomainTransferTx`.
  - `ProcessSpecialTx` / `UndoSpecialTx`: Route database commits/deletions and maintain block reorganizations.

#### [MODIFY] [validation.h](file:///g:/Raptoreum/src/validation.h) & [validation.cpp](file:///g:/Raptoreum/src/validation.cpp)
- Declare global database pointer: `extern std::unique_ptr<CDomainDB> pdomaindb;`.
- Register the `TRANSACTION_DOMAIN_*` transaction types in type validation checks inside `CheckTransaction`.
- Integrate mempool collision checking using `existsDomainTxConflict` in `AcceptToMemoryPool`.
- Handle database connection/disconnection inside `ConnectBlock` and `DisconnectBlock`.

#### [MODIFY] [init.cpp](file:///g:/Raptoreum/src/init.cpp)
- Initialize the LevelDB folder `domain` on application startup.
- Cleanly reset/release the database handle `pdomaindb` during shutdown.

---

### QT Wallet Integration (RNS Resolution)

#### [MODIFY] [bitcoinaddressvalidator.cpp](file:///g:/Raptoreum/src/qt/bitcoinaddressvalidator.cpp)
- Update `BitcoinAddressEntryValidator::validate` to permit typing domain characters (alphanumeric, dot, and hyphen) without immediately flagging keystrokes as invalid.
- Update `BitcoinAddressCheckValidator::validate` to query `pdomaindb` and validate active/grace period RNS domains on focus out.

#### [MODIFY] [walletmodel.h](file:///g:/Raptoreum/src/qt/walletmodel.h) & [walletmodel.cpp](file:///g:/Raptoreum/src/qt/walletmodel.cpp)
- Add `QString resolvedAddress;` metadata member to `SendCoinsRecipient` struct.
- Update `WalletModel::validateAddress` to support validating registered RNS domains by checking the `pdomaindb` state.
- Update `WalletModel::prepareTransaction` and `prepareAssetTransaction` to:
  - Retrieve the RNS domain from `rcp.address` and resolve it to its primary target address using `pdomaindb`.
  - Populates `rcp.resolvedAddress` and creates the script pubkey (`scriptPubKey`) using the resolved base58 destination.

#### [MODIFY] [walletmodeltransaction.h](file:///g:/Raptoreum/src/qt/walletmodeltransaction.h) & [walletmodeltransaction.cpp](file:///g:/Raptoreum/src/qt/walletmodeltransaction.cpp)
- Return a mutable reference from `getRecipients` to allow `prepareTransaction` and other helpers to save resolved address metadata in the transaction instance.
- Modify `reassignAmounts` to resolve fee deduction offsets using `resolvedAddress` if present.

#### [MODIFY] [sendcoinsdialog.cpp](file:///g:/Raptoreum/src/qt/sendcoinsdialog.cpp) & [sendassetsdialog.cpp](file:///g:/Raptoreum/src/qt/sendassetsdialog.cpp)
- Update the confirmation dialog display to show the original domain name alongside the resolved RTM address: e.g. `y3ti.rtm (resolved to R9coYU9tBj...1Bm6hY)`.

---

### RPC Interface & JSON Output formatting

#### [MODIFY] [core_write.cpp](file:///g:/Raptoreum/src/core_write.cpp) & [rawtransaction.cpp](file:///g:/Raptoreum/src/rpc/rawtransaction.cpp)
- Format `domainRegisterTx`, `domainUpdateTx`, and `domainTransferTx` payloads in the JSON representation of raw transactions.

#### [MODIFY] [register.h](file:///g:/Raptoreum/src/rpc/register.h)
- Declare `RegisterDomainRPCCommands` and call it inside `RegisterAllCoreRPCCommands`.

#### [NEW] [domain.cpp](file:///g:/Raptoreum/src/rpc/domain.cpp)
- Implement native RPC commands:
  - `resolvename <name> [type]`: Retrieves resolved destination address, IPFS CID, and record JSON from LevelDB.
  - `reverseresolve <address>`: Resolves primary domain from an RTM address.
  - `registerdomain <name> <owner_address>`: Drafts, funds, signs, and broadcasts a `TRANSACTION_DOMAIN_REGISTER` transaction.
  - `updatedomain <name> <resolver_address> <ipfs_cid> <records_json>`: Drafts and sends a `TRANSACTION_DOMAIN_UPDATE` transaction.
  - `transferdomain <name> <new_owner_address>`: Drafts and sends a `TRANSACTION_DOMAIN_TRANSFER` transaction.

#### [MODIFY] [Makefile.am](file:///g:/Raptoreum/src/Makefile.am)
- Add new source/header files to the compilation list.

---

## Verification Plan

### Automated Tests
- Build codebase using standard tools: `./autogen.sh && ./configure && make`.
- Run domain database unit tests.
- Test RPC queries on regtest network via python scripts or `raptoreum-cli`.

### Manual Verification
- Launch `raptoreumd` in regtest/testnet mode.
- Execute `registerdomain` command, verifying correct fee split outputs.
- Verify resolution of registered names (`resolvename` and `reverseresolve`).
- Perform domain metadata update and transfer, checking signature authorization constraints.
- Trigger block reorganization, verifying LevelDB state rollback correctness.
- Launch `raptoreum-qt` and test typing `y3ti.rtm` in Send Coins dialog, verifying correct validation, on-the-fly resolution, and final fee confirmation screens.
