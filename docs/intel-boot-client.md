# Intel boot-client exclusion

`intel_boot_claim` selects one immutable software policy immediately after PCI
enumeration, before Intel initialization. Previously, skipping `intel_gfx_init`
in one `main` branch did not prevent a later caller from initializing the same
device. The initializer and Air observer now enforce the selection themselves.
This is a kernel boot-client contract, not native GPU ownership or authority to
replace inherited translations.

| Policy | Participating client |
| --- | --- |
| Legacy (1) | Existing Intel discovery/PCI-command/MMIO initializer |
| Observer (2) | Exact-gated Air7,2 read-only display/backing diagnostics |
| Blocked (3) | Neither; used by the independent Haswell capture paths |
| Unselected (0) | Neither; fail closed before a participating initializer |

Selection succeeds once. A claim copies PCI BDF, vendor/device, revision,
class/subclass/interface, header type and all six BAR values. Repeated claims
must match both the selected client and that snapshot. Unknown clients, changed
resources, foreign devices and policy changes refuse. Production has no release,
handoff, reset or mutable state accessor. Failure after a partial legacy MMIO
map cannot make the device available to another client.

The legacy initializer claims before its first PCI-command read or write and
writable mapping. Repeated calls do not clear state or repeat I/O; reentry during
initialization reports not ready. Its existing discovery result still does not
promise acceleration when mapping fails. Air observer identities, maps and reads
recheck their claim. Unmap remains available after a check fails, so changed
identity does not itself strand an owned mapping. Existing CR3/address/retirement
checks still apply to cleanup.

This is serialized boot-CPU code, before competing clients start. It supplies no
runtime SMP locking, PCI hardware lock or protection against arbitrary kernel
code. IRQ handlers do not acquire these claims. Haswell's independent adapter is
not made an Air claimant: its blocked policy prevents the participating legacy
initializer; its own read-only resource and firmware gates remain unchanged.

## Receipt and limits

Boot logs contain `intel-boot-claim: v=1 policy=P client=C immutable=1
hardware_ownership=0 managed_pages=0`. Client0 means no device was claimed,
including a Q35 guest without the matching Intel GPU. Native software-client
acceptance requires a matching source/payload and actual native receipt;
host/VM evidence alone is insufficient. Old releases keep their older readout
acceptance only.

This claim does **not** exclude firmware/SMM, console framebuffer writers,
display HDA, GT engines, device DMA or another adapter's direct accesses. It
does not pin private RAM, change PAT/MTRRs, encode/invalidate GGTT entries or
grant a native `scanout_space` range. It sets none of those assurances. Actual
handoff/admission must establish them separately before enabling a writer.

The [native window survey](intel-ggtt-window.md#reference-native-result) found
all16 entries beyond the full34MiB inherited fence already populated. Index8704
must not become scratch space because this kernel has one participating client.
A mapping experiment requires an explicitly transferred range and exclusion of
its readers, followed by exact restore and generation-specific translation
retirement. Both [reservation](native-scanout-reservations.md) and
[publication](scanout-publication.md) remain disconnected from a native writer.

`make test-intel-boot-claim` executes the real legacy initializer with side-effect
counters: forbidden policies, resource mutations, repeated/reentrant calls and
each partial-map failure. Actual Air adapter tests exercise competing policies
and cleanup after identity changes. Host tests use disposable boot state
directly; production exports no reset function.
