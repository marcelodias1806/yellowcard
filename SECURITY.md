# Security Policy

## Supported Version

Security fixes are evaluated against the current public release and the latest
code on the default branch. At initial publication, the validated release is
v0.4.0.

## Reporting a Vulnerability

Use a **private GitHub Security Advisory** for vulnerabilities, accidental
secret exposure, unsafe radio behavior, or reports that require sensitive
details. In the repository, choose **Security → Report a vulnerability**.

Do not open a public issue containing:

- Passwords, Wi-Fi credentials, tokens, or API keys.
- Private SSIDs, internal hostnames, or private IP addresses.
- Private keys or certificates.
- Personal device identifiers or unredacted radio scan logs.
- Exploit details that would put users at immediate risk.

Include the affected version/commit, impact, reproduction conditions, and a
minimal proof of concept where safe. Remove unrelated third-party data.

No private security email is published by this repository. If GitHub private
reporting is unavailable, open a minimal public issue asking the maintainer to
enable or initiate a private channel; do not include vulnerability details.

## Project Security Scope

In scope:

- Memory safety or denial-of-service issues in the firmware.
- Unsafe handling or accidental disclosure of configured credentials.
- Unexpected external transmission or persistence of scan data.
- Bypass of the documented Wi-Fi/BLE scan serialization.
- Dependency or build issues that materially affect users' security.

Out of scope:

- Attacks requiring unsupported hardware modifications.
- Generic RF conditions outside firmware control.
- Requests to add offensive wireless capabilities.
- Features explicitly not provided: injection, deauthentication, packet
  capture, credential collection, active BLE/GATT operations, spoofing, or
  persistent tracking.

## Disclosure

Please allow reasonable time to validate and remediate a report before public
disclosure. The project does not offer a bug bounty or guaranteed response
time, but good-faith, responsible reports are welcome.
