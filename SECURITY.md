# SlickShot Security Policy

SlickShot is a standalone, Windows-first fork of Flameshot. This policy applies
to SlickShot only. Report issues in Flameshot upstream, not here.

## Supported Versions

Only the **latest stable SlickShot release** and the **HEAD of the master
branch** receive security fixes. Before reporting, check that the problem still
happens on one of those versions.

## Reporting a Vulnerability

Please report security issues privately so they can be fixed before details are
made public.

- **Preferred:** open a private report through
  [SlickShot's security advisories](https://github.com/khentmoba/slickshot/security/advisories).
  Use "Report a vulnerability" / "New draft security advisory" on that page; do
  not publish a draft until a fix or coordinated disclosure date is agreed.
- **If private reporting is unavailable:** contact the maintainer directly
  through [khentmoba's GitHub profile](https://github.com/khentmoba) (for
  example, a public issue asking for a private channel, without including the
  exploit details).

Do not open a public issue containing a working exploit. If you are unsure
whether something is a security flaw, use the advisory form or contact the
maintainer and ask before posting publicly.

## Report Requirements

To help triage and fix the issue quickly, include the following:

1. **CVSS vector:** a JSON file generated with the latest CVSS calculator from
   the [National Vulnerability Database](https://nvd.nist.gov/vuln-metrics/cvss).
2. **Proof of concept:** working code or a script that demonstrates the exploit.
3. **Reproduction steps:** clear, step-by-step instructions.
4. **Disclosure of tooling:** if you used software or AI to find the issue, say
   so.

**Optional but welcome:**

- A suggested fix, patch, or mitigation.

## Out of Scope

To keep volunteer effort focused, the following are out of scope unless they
show a genuinely new attack path:

- Issues that require root/administrator access or an already compromised
  operating system. At that point an attacker can do far more than a screenshot
  utility could.
- Theoretical findings and raw scanner output without a verified, working
  proof of concept.
- Vulnerabilities that exist only in unmodified upstream Flameshot, or in
  third-party packages not built from this repository.

## Response Process

1. **Acknowledgment:** private reports are acknowledged through the advisory
   thread or the channel you used.
2. **Triage:** the maintainer validates the finding against your reproduction
   steps.
3. **Fix and advisory:** if confirmed, a fix is prepared and a release date is
   coordinated before a public advisory is published.

## Responsibilities and Commitments

SlickShot is maintained by volunteers in their spare time. We commit to:

- Evaluating reported issues and validating them in good faith.
- Acting on confirmed vulnerabilities as quickly as we reasonably can.
- Being transparent about fixes and disclosure timing where appropriate.

Triage and patching may take longer than commercial software standards.

Reporters are expected to provide accurate, detailed information, avoid
exaggeration or misleading claims, and stay civil and professional throughout
the process.
