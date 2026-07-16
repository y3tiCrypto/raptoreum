# Raptoreum Core (Latest v2.0.3)

---

### Build Status & Development Branches

| Build Pipeline | Branch | Release Status |
|:---:|:---:|:---:|
| **[![CI Status](https://img.shields.io/badge/CI-passing-success.svg)](#)** | `master` (Stable) | **[![Stable Release](https://img.shields.io/badge/Release-v2.0.3-blue.svg)](#)** |
| **[![CI Status](https://img.shields.io/badge/CI-passing-success.svg)](#)** | `develop` (Active) | **[![Pre-release](https://img.shields.io/badge/Dev-v2.1.0--alpha-orange.svg)](#)** |

---

### Introduction

> **Etymology**: The name **Raptoreum** is derived from the Victorian term for a bird of prey (*raptor*), combined with the suffix *-ium* (a place or object pertaining to). 

The Raptoreum project is built by an experienced team with deep roots in cyber security and systems engineering. Combining robust consensus security with advanced application capabilities, Raptoreum offers a secure, decentralized smart contract and asset platform.

Initially starting on the Ravencoin codebase to enable trustless asset transfers, the project quickly evolved. Following security vulnerabilities discovered in Ravencoin's native asset code, that codebase was abandoned. Raptoreum has been completely re-architected as a **Dash code fork**, inheriting high-performance features like **ChainLocks**, **InstantSend**, and **Smartnode Quorums**, while introducing a unique custom asset layer and native protocols.

Raptoreum expands the capabilities of the Dash core architecture by integrating:
*   **A) Custom Asset Layer**: Native creation, updating, and minting of unique assets and tokens directly on-chain without the complexity of external smart contracts.
*   **B) Timelocked Transactions**: The capability to lock native coins (RTM) or assets in a special transaction until a chosen block height or timestamp is reached.
*   **C) Trustless Asset Transfers**: On-chain transfer of assets and coins managed transparently via Smart Contracts.
*   **D) Multi-Language Smart Contracts**: Developing a VM protocol allowing smart contract execution in four major programming languages (widening access beyond Solidity).
*   **E) Raptoreum Name Service (RNS)**: A native, on-chain name resolution protocol featuring cryptographic commit-reveal protection, owner/manager role delegation, on-the-fly resolution in the QT Wallet, and consensus-level fee splits.

These features extend the power of Raptoreum to a wider range of decentralized applications (dApps), providing developers with alternatives and flexibility, particularly in software language choices.

---

## Raptoreum Name Service (RNS)

RNS is integrated natively at the consensus protocol layer of Raptoreum Core. It maps human-readable domains (e.g., `example.rtm` or `developer.raptoreum`) to base58 addresses.

### Architectural Overview

RNS is powered by three main components:
1.  **Special Transaction Types**: Added to version 3 transaction types:
    *   `TRANSACTION_DOMAIN_REGISTER = 11`: Performs domain name registration, revealing salt.
    *   `TRANSACTION_DOMAIN_UPDATE = 12`: Updates domain records, subdomains, IPFS CIDs, and manager assignments.
    *   `TRANSACTION_DOMAIN_TRANSFER = 13`: Transfers primary ownership and clears delegated managers.
    *   `TRANSACTION_DOMAIN_COMMIT = 14`: Publishes cryptographic commitments to mitigate mempool sniping.
2.  **LevelDB Registry (`domaindb`)**: Custom index database stored under `[datadir]/domain/`.
    *   `d_[domain_name]`: Maps domain strings to serialized `CDomainMetaData` records.
    *   `r_[rtm_address]`: Maps addresses to primary domain names for reverse lookup.
    *   `c_[commit_hash]`: Stores commitment hashes and the block height where they were mined.
    *   `u_[block_hash]`: Stores block-undo logs (`CDomainBlockUndo`) to support correct state transitions during block disconnects (network reorganizations).
3.  **Consensus Verification Rules**:
    *   **Length-Based Fees**: Enforces registration fee brackets based on domain name character length.
    *   **50/50 Fee Split**: Consensus verifies that 50% of the registration fee goes to the Developer address and 50% goes to the Donation address (pre-configured per network).
    *   **Mempool Conflict Mapping**: Employs `mapDomainsToHash` in the transaction mempool to prevent concurrent duplicate registration broadcasts.
    *   **NFA Collision Blocking**: Prevents hijacking of existing Non-Fungible Assets (NFAs). If an asset exists on-chain with the same name, RNS registration is rejected by consensus unless signed by the NFA's owner.

---

## License

Raptoreum Core is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see https://opensource.org/licenses/MIT.

---

## Development Process

The `master` branch is meant to be stable. Development is done in separate branches. [Tags](https://github.com/raptor3um/raptoreum/tags) are created to indicate new official, stable release versions of Raptoreum Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

---

## Testing

Testing and code review is the bottleneck for development; we get more pull requests than we can review and test on short notice. Please be patient and help out by testing other people's pull requests, and remember this is a security-critical project where any mistake might cost people lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to submit new unit tests for old code. Unit tests can be compiled and run (assuming they weren't disabled in configure) with: `make check`. Further details on running and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written in Python, that are run automatically on the build server. These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the code. This is especially important for large or high-risk changes. It is useful to add a test plan to the pull request description if testing the changes is not straightforward.
