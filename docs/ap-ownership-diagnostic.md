# Bounded secondary-CPU ownership diagnostic

This experimental payload establishes AP ownership before any coordinated deeper
idle experiment. It is not SMP scheduling or a permanent idle policy. Bounded native
ownership is accepted on MacBookAir6,1 in release `53b2c4c90f65e647`; no sibling
machine is accepted. See the [unit result](machines/macbookair6-1.md#ap-ownership--native-acceptance-2026-10-28) for measurements and limits.

## Isolation and lifetime

`make ap-probe-kernel` builds `build/ap-probe/srvros.elf` with separate main and
Limine request objects. The ordinary kernel has no MP request. The diagnostic
requires `srvros.capture=ap-ownership` or the explicit coordinated CPU-load tokens
below; without either it halts before storage
initialization. Limine processes the MP request before SrvrOS parses the command
line, so a command-line toggle on the ordinary image would not provide isolation.

Limine initializes AP architectural state, including APIC mode and MTRR
synchronization. The diagnostic itself installs per-AP GDT/TSS, a 16 KiB stack,
a separate 16 KiB emergency IST stack and a private IDT. These are architectural
setup operations; `policy_writes=0` means no CPU **power-policy** MSR writes.
The AP assembly never calls the scheduler, allocator, console or BSP handlers.
Every exception/NMI terminates that AP in a retained fault state; it never resumes
the interrupted diagnostic. Early entry still depends on Limine's valid mapping
and handoff contract, before the private descriptors are installed.

All slots, stacks and descriptors are static. The current PMM allocates only
`LIMINE_MEMMAP_USABLE`, retaining bootloader-reclaimable memory, including the
boot page tables and Limine's unclaimed AP stacks/park code. No timeout, final
acknowledgement or successful receipt authorizes reclamation. Memory stays
reserved until reset. There is no second diagnostic epoch or AP reuse in a boot.

## Protocol and bounds

BSP admission checks the MP response, unique APIC IDs and BSP membership before
publishing any entry address. Native admission requires Intel signature `0x40651`,
four CPUs with IDs 0–3/BSP 0 and the captured 64-byte MONITOR/C1 capability.
CPUID hypervisor guests are identified as `mode=vm`, allow two or four CPUs,
and may use PAUSE when MONITOR is unavailable. VM results do not accept native
MONITOR behavior. This profile is installed only after local Air6,1 inventory
verification; it is not a general Apple model detector.

Each AP independently reports its CPUID signature/APIC ID. A dedicated cache
line holds BSP commands; AP receipts occupy another line. Commands 1–32 are
strictly ordered. APs arm MONITOR, recheck the command and use only C1 MWAIT
(EAX=0, ECX=0, IF clear), with the BSP store as wake source. A one-millisecond
settle interval precedes each round. A counter records attempts to wait, not
proof of physical C1 residency or a guaranteed sleep on every exchange.

The BSP starts measurement before publishing each entry/command. Startup,
each acknowledgement and terminal retirement use the existing 20,000 us limit
and an independent finite spin bound for a stopped reference. IRQ masking keeps
these measurements out of scheduler-preemption time. No threshold changes.
Stale/duplicate acknowledgements keep waiting; future, late or faulted replies
fail. STOP is distinct from sequence numbers and always requests terminal
`cli; hlt` parking. A retirement receipt precedes the final HLT instruction and
is not evidence that storage is reclaimable. Failure still sends STOP to all
launched APs and reports each resulting state. The BSP saves logs, unmounts and
returns through the existing automated recovery path. A halt or absent completion
receipt remains failed/incomplete evidence.

## Validation and physical sequence

Host tests exercise sequence/deadline classification and strict saved-log parsing,
including duplicate/missing/corrupt receipts. Multi-CPU Q35 rehearsals check two
and four CPUs, single/over-capacity refusal, missing-start acknowledgement and a
real `UD2` exception on one AP. Injections are honored only in hypervisor guests;
injected configurations are temporary VM copies, never installed payloads.

Stage with `python3 tools/hardware_boot.py stage --laptop-probe-volume UUID
--ap-ownership-test` after building matched ordinary and diagnostic artifacts.
Use the [Ubuntu guide](ubuntu-device-agent-setup.md) for exact-stage rehearsal,
USB integrity/headroom checks, durable backup, install, one-shot and collection.
The harness defaults to four CPUs for this profile; `--ap-cpus 1|2|4|6` and
`--ap-inject missing|fault` exercise directed VM cases. `hardware_ap_owner.py`
requires a complete successful native receipt separately from a VM result.

No C3 hint, power measurement or energy-saving claim is part of this test.
With bounded native ownership accepted, a separately reviewed coordinated C3 experiment
may reuse the existing 32-attempt, 20 ms and BSP HLT-fallback protections.


## Coordinated C3 candidate

The separate `--cpu-load-capture --ap-coordinated-c3` laptop profile uses the
same diagnostic ELF and accepted 32-round C1 ownership handshake. Air6,1 release
`57274dfaccfdc0b9` accepts bounded coordinated core C3 with clean wake/fallback
and retirement; package C3 remains valid zero. See the [native result](machines/macbookair6-1.md#coordinated-c3--native-acceptance-2026-10-28).
Native admission additionally requires invariant TSC,
MONITOR C3 support, ARAT, the calibrated live BSP timer and a permissive package
C-state limit. Every peer independently reports C3 support. No power-policy
register is written. Ordinary boot still has no MP request or coordinator hooks.

During each of at most 32 scheduler C3 attempts, the BSP first checks readiness,
then commands every peer to arm its private monitored mailbox. All three must
report the current sequence and unchanged package configuration before the BSP
uses hint 0x10 with its existing timer wake. Each AP executes at most one C3
MWAIT for that round, with IF clear and the mailbox store as its wake source.
The BSP wakes all peers before checking its own counter/deadline results and
requires matching completion and resume acknowledgements. The entire masked
coordination round, including peer readiness and wake acknowledgement, retains
the existing 20,000 us ceiling. A separate finite spin bound remains in force.
Early peer return, stale/future sequences, epoch mismatch, faults, counter/clock
regression and configuration drift fail the trial. Failure sends STOP to all
peers immediately without another masked wait; storage remains retained.

The original readiness recheck, 32-attempt cap, timer check and mandatory 32 HLT
fallback receipts remain unchanged. Startup and final terminal retirement keep
the ownership diagnostic's separate per-peer 20 ms bounds. APs use C1 between
rounds and through the remainder of the original 30-second idle/load/recovery
phases, then retire. This is a short coordinated entry experiment within a
power capture, not sustained C3 policy or an energy comparison.

`ap-c3` and `ap-c3-peer` summarize coordination and retirement. Exactly 32
`ap-c3-round` rows retain the masked duration and BSP/peer TSC and core-C3 deltas,
plus package-C3 deltas on the BSP. An armed receipt proves protocol readiness;
only nonzero validated residency deltas demonstrate entry. Zero remains valid
but unobserved. Peer brackets include readiness/wake handshakes and are wider
than the BSP bracket; do not compare their durations directly or add physical
core counters from SMT siblings. The CPU0 service's existing idle-audit fields
still describe its own scheduler view; explicit peer ownership is proved by
the separate AP receipts. `hardware_ap_c3.py` cross-checks BSP totals against
the existing MWAIT receipt, epoch, configuration, limits and retirement.

The VM harness uses four CPUs. The exact native stage must refuse unsupported
hardware and save/return normally. `--ap-c3-sim` adds a temporary VM-only token
that exercises the actual mailbox protocol using PAUSE, without C3 or residency
MSR reads. Its receipt says `vm-simulation` and cannot establish native entry.
`--ap-inject c3-missing|c3-early|c3-fault` checks missing readiness, early return
and an AP exception. These injections are ignored on native hardware and never
appear in the installed configuration. Host scheduler tests cover readiness,
peer failure, counter failure with peer release, attempt caps and HLT fallback;
parser tests reject missing, duplicated, over-limit or contradictory evidence.

Follow the usual matched build, exact-stage VM, offline USB checks, durable
backup, one-shot recovery and collection sequence. No human action is required;
keep AC and USB connected. The guest shows phase progress and saves results
before automatic return. A missing completion or failed coordination is not a
pass. Require the original CPU-load/power/residency proof, the new native AP
proof and independent USB preservation/allocation checks before acceptance.

## Read-only package audit

Add `--ap-package-audit` to `--cpu-load-capture --ap-coordinated-c3` for the
separate `ram-usb-v1-ap-package` diagnostic. The original C3 hint, 32 attempts,
20 ms bounds, readiness checks, AP ownership/retirement and HLT fallback remain
unchanged. Ordinary kernels and existing profiles do not take the new reads.

Each native round adds a BSP TSC-bracketed package-C2 (`0x60d`) delta. Its bracket
starts after peer readiness and ends after peer release, enclosing the original
BSP C3 measurement. Counter/clock regression fails coordination; zero is valid
but unobserved. No PCI access, printing or allocation occurs in the round.
The extra fixed MSR reads count toward the original whole-round bound.

Two snapshots around the full capture retain `MSR_PKGC3_IRTL` (`0x60a`) and
standard PCI configuration for already enumerated functions. IRTL drift fails
the audit; unsupported/reserved encodings remain raw with unavailable decoding.
The sampler reads only Command, class/header, the conventional capability chain,
PMCSR, PCIe Link Capabilities/Control/Status and version-2 Device Capabilities/
Control 2. It walks at most 48 distinct aligned capability headers inside the
256-byte configuration space, refusing malformed/duplicate/cyclic chains or
fields past the boundary. RC-integrated endpoints have no Link-register read.
Absent capabilities are marked absent rather than reported as zero-valued state.
No BAR sizing, extended-capability traversal, GPU/PCH MMIO, D-state transition,
ASPM/LTR update or status acknowledgement is performed. Legacy config-address
selector writes are access mechanics, not configuration-data writes.

`hardware_package.py` requires the existing complete native coordination proof,
32 ordered C2 brackets, paired PCI identities/capabilities and unchanged IRTL.
PCI command/power/link changes remain reported observations, not automatic
failures or causal blocker diagnoses. Graphics RC6, actual device LTR values
and platform permission remain explicitly unknown. VM simulation reports the
package audit unavailable and performs no package-register or PCI reads; host
fixtures exercise the config reader, boundary/failure cases and strict parser.
The same automatic phase/save/return and independent USB checks apply.

The [Intel U/Y datasheet](https://cdrdv2-public.intel.com/329001/4th-gen-core-family-mobile-u-y-processor-lines-vol-1-datasheet.pdf)
sections 4.2.5–4.2.6 describes package entry as dependent on core state and
platform permission, with graphics, display, timer and device latency constraints.
Consequently, core C3 alone cannot establish package C3 or identify a blocker.
The [Intel SDM volume 4](https://www.intel.com/content/dam/develop/external/us/en/documents-tps/335592-sdm-vol-41.pdf)
Haswell/06_45 inherited register tables define `0x60a`: valid bit 15, limit bits
9:0 and unit bits 12:10 (1, 32, 1024, 32768, 1048576 or 33554432 ns). The package-C2
counter uses TSC units. Standard PCI offsets/flags are cross-checked against
[Linux's PCI register definitions](https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/pci_regs.h).
These sources describe possible constraints; the native capture must establish
this unit's actual values before proposing any policy or device-state change.
