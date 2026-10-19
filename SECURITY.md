# Security

SrvrOS is experimental software. Its process separation, pointer validation,
network stack and filesystem are still being developed and are not a security
boundary suitable for hostile workloads. There is no supported stable release
or promised security-response SLA. Use disposable VM images for untrusted tests;
do not expose experimental services as production infrastructure.

## Reporting a vulnerability

If GitHub's **Report a vulnerability** action is available under this repository's
Security tab, use it for a private report. Private reporting availability depends
on the repository settings; this document does not claim it is enabled.
Otherwise, open a minimal issue asking the maintainer for a private reporting
channel, without exploit details or sensitive attachments. Do not post credentials,
private memory dumps or other people's data in public issues.

Include the source commit, affected component, host/guest configuration, impact,
reproduction and any proposed fix. Say whether the result was observed in a VM
or on physical hardware. Preserve relevant logs locally until a suitable channel
has been agreed. Ordinary non-sensitive crashes can use the bug-report template.
