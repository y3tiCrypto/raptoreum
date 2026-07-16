# Security Policy

We take the security of the Raptoreum network and the Raptoreum Core software very seriously. If you believe you have found a security vulnerability, please report it to us privately following the instructions below. 

**Do not open a public GitHub issue for security vulnerabilities.**

---

## Supported Versions

Only the latest stable release branch receives active security updates. Older versions are deprecated and users are strongly encouraged to upgrade to prevent exposure to known issues.

| Version | Supported | Notes |
|:---:|:---:|---|
| **v2.0.x** | :white_check_mark: Yes | Current active stable release. |
| **v1.x.x** | :x: No | Deprecated. Please upgrade immediately. |

---

## Reporting a Vulnerability

Please report security issues privately to the Raptoreum core security team.

*   **Email**: `security@raptoreum.com`
*   **PGP Key**: We recommend encrypting your email using our PGP key (available upon request or via public key servers).

To help us triage and resolve the issue quickly, please include the following details in your report:
1.  A detailed description of the vulnerability.
2.  Clear step-by-step instructions (or a proof-of-concept script) to reproduce the issue.
3.  The potential impact of the exploit (e.g., Denial of Service, double spending, private key extraction).
4.  Any proposed fix or remediation strategy if available.

---

## Our Security Response Process

Once a vulnerability report is received, the Raptoreum core team will follow a coordinated disclosure process:

1.  **Acknowledgment**: We will acknowledge receipt of your report within **48 hours**.
2.  **Triage & Validation**: We will validate and assess the severity of the issue internally.
3.  **Patch Development**: We will develop and test a security patch privately.
4.  **Coordinated Release**: We will package the patch into a new minor release. If appropriate, smartnodes and exchanges will be notified privately beforehand to coordinate network updates and protect user funds.
5.  **Advisory Disclosure**: A public security advisory detailing the vulnerability and its fix will be published alongside the release notes.
