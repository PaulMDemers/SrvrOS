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

Air6,1 release `bae0678ed7e9b38b` physically completes this audit: all 32 short
rounds observe package C2 (about 99.54% of their wider brackets) alongside positive
core C3, while package C3 remains zero. Paired IRTL is unchanged at `0x8842`
(67.584 us), and all 25 PCI snapshots are stable. This narrows the evidence to
remaining package-entry conditions without identifying a specific blocker.
See the [unit record](machines/macbookair6-1.md) for scope and recovery evidence.

## Optional graphics and device-latency audit

Add `--ap-platform-audit` alongside `--ap-package-audit` to select the separate
`ram-usb-v1-ap-platform` profile. It inherits exact native CPU/topology admission
and adds the existing Air6,1 GPU/PCH/DSDT identity gate. No diagnostic access runs
in ordinary boot, old package profiles, native refusal or VM simulation.

Version-1–3 snapshots outside the measured rounds read Haswell graphics status
`0x138060` and RC6 residency `0x138108` only. BAR0 must be an already-enabled,
4 MiB-aligned 64-bit memory aperture and the GPU must already be D0. One page is
mapped read-only, uncacheable and non-executable; it stays retained until reset.
Identity, aperture and decoding are rechecked before the final snapshot.
The 32-bit RC6 counter uses 1.28 us ticks; the parser handles one wrap but refuses
an interval long enough to make the wrap count ambiguous. Status is an instant
observation and the counter interval spans the capture, not only the short C3
rounds. Zero residency is valid and is not proof that RC6 is disabled.

The [Linux Haswell forcewake table](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c)
limits its render forcewake domain to `0x00000–0x3ffff`; these two uncore reads
are outside it. The [graphics register definitions](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_gt_regs.h)
and [RC6 reader](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_rc6.c)
provide the offsets and counter unit. `RC_CONTROL` needs the forcewake path and
is explicitly unavailable: no forcewake request, PCODE mailbox command, graphics
initialization or control-register write is added.

For enumerated PCIe functions, an ECAM-only read interface traverses the extended
capability chain within 4 KiB. It refuses legacy fallback, alignment/range errors,
nonzero/ambiguous segment mappings, loops, overlapping LTR data and duplicate or
unsupported LTR versions. At most 960 aligned headers can be visited. The new
reader cannot truncate an extended offset into conventional configuration.
The standard LTR capability records Max Snoop/No-Snoop values and scales using
[PCI definitions](https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/pci_regs.h).
Reserved encodings remain unavailable. Capability absence differs from a valid
zero; configured maximum latency is neither measured device response latency
nor proof of a currently asserted request. No capability or decode bit is changed.

`ap-platform` receipts pair the graphics observations, PCI identities/LTR fields
and unchanged E2 policy evidence. A direct platform grant signal remains
`permission=unavailable reason=no-validated-grant-register`; the firmware limit,
PCI D0, ASPM and LTR fields must not be relabeled as that signal. This diagnostic
narrows the remaining evidence without manufacturing a grant/denial or causal
blocker. Original round counts, 20 ms deadlines, fallback, ownership checks,
log limits and automatic idle/load/recovery/save/return remain unchanged.

The first native platform run (`a3ac64f7cd15293c`) is failed, not accepted. It
exposes a Null Extended Capability traversal bug and a separate RC6 nominal-time
versus monotonic-time mismatch; see the [unit record](machines/macbookair6-1.md).
[PCI-SIG's Code and ID Assignment specification, table 3-1](https://pcisig.com/sites/default/files/files/PCI_Code-ID_r_1_12__v9_Jan_2020.pdf)
defines zero as a header-only Null Extended Capability with an uninterpreted
version field and permits repeated instances. Correctly following its next
pointer must retain the original address, visited-header and overlap checks.
Do not reinterpret a failed complete capture as a pass from its partial readings.

The retry fixes Null traversal (including terminal zero headers and arbitrary
Null version) while preserving the full-chain, overlap and non-Null version
checks. Version-2 platform receipts add two PM-timer/ordered-TSC brackets around
the existing graphics reads, after identity/mapping work. IRQ policy is unchanged;
interruptions widen the recorded bracket. Cached nominal MHz is context only,
not a TSC calibration. No additional GPU register is read or written.

`platform_timing_result` extracts those raw intervals even when the overall
platform result fails. It never returns acceptance: the original nominal
RC6-versus-snapshot bound and wrap refusal still govern `platform_result`.
The collector saves that diagnostic before checking acceptance, preserving a
repeat mismatch rather than clamping a ratio or inventing a clock tolerance.
Version-1 historical receipts retain their original validation.

Air6,1 retry `8c9a5a28464ffd7c` accepts the Null-chain traversal on the four
affected root ports. Its workload/capture completes, but the strict platform
collector still rejects RC6 timing: about 90.151273 s nominal RC6 versus
90.057877–90.057879 s tightly bracketed PM time. The 1 us read brackets and
closely agreeing TSC/PM observations do not explain the 0.104% discrepancy.
Keep this distinct from runtime access/traversal success and from package-C3
acceptance. Counter clock/scaling/update semantics remain to be audited; no
acceptance tolerance or power policy has changed. See the unit record for the
matching session, retained evidence and recovery state.

## RC6 scaling and update audit

[Intel Haswell PRM volume 12, register page 100](https://kiwitree.net/~lina/intel-gfx-docs/prm/hsw/intel-gfx-prm-osrc-hsw-pcie-config-registers.pdf)
specifies a 32-bit read-only RC6 accumulator at `0x138108`, cumulative since
boot, wrapping, in 1.28 us units. It does not supply a counter-clock tolerance
or publication cadence. The adjacent mailbox includes a counter-clear command;
this diagnostic never accesses that mailbox. Linux v6.12's reader uses the same
unit and 32-bit wrap handling for Haswell, not the Valleyview/Cherryview CZ-clock
or Gen9-LP conversion. There is no evidence here for changing Haswell's scale.

[IGT's pinned RC6 test source](https://chromium.googlesource.com/external/gitlab.freedesktop.org/drm/igt-gpu-tools/+/a8e2cbd2854d7980a9eccecc6e0c801d0824b88f/tests/intel/i915_pm_rc6_residency.c)
compares sysfs residency over a three-second sleep and permits a broader range;
that software test allowance is not a hardware accuracy guarantee and is not
adopted here. Its wait for RC6 entry is also not proof of the accumulator's
update cadence. Unsupported clock tolerances and update periods remain unknown.

The local PM reference accumulates modulo-24-bit ticks before converting the
total at 3,579,545 Hz; it does not round and add each polling interval. Its
existing 90-second wrap/fraction regression passes. Microsecond truncation of
that cumulative conversion cannot account for the observed roughly 93 ms
excess. PM/TSC agreement remains a cross-clock observation, not an independent
calibration of either oscillator or a guarantee of the RC6 counter clock.

Version-3 receipts add paired counter/status reads at every other existing
retained power checkpoint (at least ten seconds apart, at most 12 pairs from
the unchanged 24-checkpoint capture). Only the isolated
AP diagnostic links the sampling hook; ordinary power sampling is unchanged.
The hook runs within the existing bounded sampling critical section, performs
no allocation, waiting or logging, and uses the same read-only mapped page.
It adds no worker, timer, control access or additional wakeups. Sampling becomes
inert after capture, and storage remains static until reset. Endpoint brackets
and all original bounds remain intact.

The series parser matches each point to its retained power timestamp, verifies
ordering/counts and counter continuity including one wrap, and reports per-gap
nominal excess plus immediate-pair deltas. Raw observations survive a failed
whole-interval check. Sparse samples can distinguish distribution of the excess
but cannot establish a precise update cadence or identify a hardware clock.
The original RC6-versus-time predicate is unchanged; interval observations are
not a new acceptance tolerance. Maximum-width receipt replay must retain all
original completion proofs inside the unchanged log tail.


The Air6,1 PR249 native repeat (`0804f2944e9364d3`) retained nine valid series
points and completed the workload, but still failed the original RC6 bound.
Eight of ten intervals including endpoints exceeded their PM brackets; six of
eight internal intervals did so. Immediate paired reads were equal and counter
continuity held. This distributes the excess across the run without resolving
clock accuracy or exact publication semantics. Nominal RC6 time was
90.143014400 s versus 90.059109–90.059113 s by PM. Keep the mismatch unresolved;
no new tolerance or policy is accepted. See the unit record's October 31 review.

## Read-only Linux RC6 comparison

`tools/linux_rc6_baseline.py` observes the existing i915
`gt/gt0/rc6_residency_ms` interface on a selected DRM card. Its default three
90-second passive windows retain ten-second checkpoints with both
`CLOCK_MONOTONIC_RAW` and adjusted monotonic brackets around each sysfs read.
Output includes model/kernel/PCI/clocksource, RC6 enable/runtime state, flushed
raw rows and an observation-only summary. Existing output directories refuse
replacement. Counter regression, clock overlap, invalid brackets and incomplete
windows fail parsing; partial raw evidence remains available.

For example, after identifying the correct i915 card on the local machine:

```sh
python3 tools/linux_rc6_baseline.py --card /sys/class/drm/card1 \
  --output build/power/linux-rc6-new-run
make test-linux-rc6-baseline
```

The upstream Linux v7.0
[sysfs reader](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/gt/intel_gt_sysfs_pm.c)
acquires runtime PM and rounds cumulative residency to milliseconds. Its
[RC6 implementation](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/gt/intel_rc6.c)
still uses 32-bit counter extension and 1280 ns units for Haswell. These upstream
references do not independently verify every Ubuntu distribution patch.
The helper issues no graphics/power-control writes, direct MMIO access or GPU
workload, but Linux continues managing its own device/runtime power state and
the normal desktop workload. A two-millisecond quantization annotation covers
both rounded endpoints conservatively; it is never a SrvrOS tolerance or pass
criterion. Linux raw monotonic time on a TSC clocksource is not an independent
PM timer measurement. Residency below elapsed time cannot exclude a small
counter-scale discrepancy when GPU activity or other counter semantics could
hide it. Compare named captures without promoting this host observation to
native SrvrOS acceptance.


Air6,1's November 1 Linux comparison retained 28 samples across three 90-second
passive windows: RC6 residency was 43.73/34.97/40.67%, with no elapsed-time
excess. That lower residency cannot resolve the earlier SrvrOS near-100%
counter mismatch. Separate Ubuntu package counters observed C3 at 13.23–14.41%,
confirming this unit can enter package C3 under Linux's policies/device state.
Neither observation changes the SrvrOS acceptance predicate or identifies a
causal blocker; the unit record preserves the full comparison limits.

## Cross-OS package-C3 prerequisite comparison

The Air6,1 November 1 comparison uses the saved native release
`0804f2944e9364d3` and two Linux snapshots on the same unit. All 25 PCI functions
match by BDF, vendor/device and class. These observations narrow candidates;
they do not establish the platform's instantaneous package-entry grant.

| Field | SrvrOS | Ubuntu Linux | Interpretation |
| --- | --- | --- | --- |
| Package configuration `0xe2` | `0x408` | `0x408` on all four CPUs | Same sampled limit/configuration |
| Package-C3 IRTL `0x60a` | `0x8842` | `0x8842` | Same 67.584 us allowance |
| Misc enable `0x1a0` | `0x850089` | `0x850089` on all four CPUs | Same sampled control value |
| Core C3 request | Fixed MWAIT `0x10` trial | Enabled C3 entry described as MWAIT `0x10` | Same hint, different idle scheduling/workloads |
| PCIe ASPM, LTR enable/support, clock-PM enable/support | Captured per function | Same decoded fields | No difference in these sampled fields |
| Extended LTR | Two Broadcom values `0x10031003`; absent on others | Same state/offset/value/header counts | Configured maximum latency, not a live request measurement |
| Audio `00:03.0`, `00:1b.0` | D0 | D3hot, runtime suspended | Ownership/idle-state candidate |
| Root port `00:1c.0`, Thunderbolt `06:03.0`–`06:06.0` | D0 | D3hot, runtime suspended | Other device-state candidates |
| Common Clock Configuration | Clear at `00:1c.4`, `05:00.0`, `06:00.0`, `07:00.0` | Set at those four functions | Distinct from ASPM and clock-PM enables |
| Display well | No matching native field | Linux cached `display` use count zero | Software reference count, not hardware-off proof |

Linux also enables deeper core idle entries, while the native trial uses its
fixed C3 hint. The [U/Y datasheet](https://cdrdv2-public.intel.com/329001/4th-gen-core-family-mobile-u-y-processor-lines-vol-1-datasheet.pdf)
sections 4.2.5–4.2.6 describes core C3-or-deeper resolution and platform
permission, with memory, timer and device constraints on package entry. Positive
Linux core C7 does not by itself show that the SrvrOS C3 hint is wrong. The
[upstream idle table](https://github.com/torvalds/linux/blob/v7.0/drivers/idle/intel_idle.c)
and captured sysfs descriptors both identify the Haswell C3 request as `0x10`.

`tools/linux_package_pci.c` adapts O_RDONLY/pread configuration-file access to
the existing bounded `package_pci_read`/`package_ltr_read` implementation. The
host-only collector gates known-supported MSRs to this Air6,1/CPU/GPU/PCH profile,
records all four CPUs before/after, preserves per-device sysfs power state and
refuses output replacement. Its comparison keeps native start/end separately;
missing, unreadable or identity-mismatched devices remain unknown. No shared
runtime, native hardware allowlist or boot profile changes.

```sh
make test-linux-package-compare
sudo python3 tools/linux_package_compare.py \
  --pci-reader build/tests/linux-package-pci-host \
  --native-log PATH_TO_MATCHING_EXIT_LOG \
  --output build/power/package-comparison-new-run
```

The [Linux PCI sysfs implementation](https://github.com/torvalds/linux/blob/v7.0/drivers/pci/pci-sysfs.c)
acquires runtime PM for configuration reads. The helper issues no configuration
or power-control writes, but reads can resume a device through Linux; surrounding
states are evidence, not a guarantee of an unperturbed sample. Both captures
retained the same seven D3hot differences. Raw link/status/interrupt fields also
differ; matching selected fields does not mean all PCI configuration is equal.
The [PCI definitions](https://github.com/torvalds/linux/blob/v7.0/include/uapi/linux/pci_regs.h)
distinguish Common Clock bit 6 from clock-PM bit 8 and ASPM bits 1:0.

Only cached DRM state, connector modes, runtime/frontbuffer state and
[display power-domain reference counts](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_display_power.c)
are used for the display comparison. An exploratory Linux `drpc` read is
excluded from passive evidence: its
[implementation](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/gt/intel_gt_pm_debugfs.c)
uses runtime PM, forcewake-requiring reads and a PCODE read request on Haswell.
A debugfs file opened read-only is not necessarily a passive hardware operation.
No such path or PCODE/forcewake access is added to SrvrOS.

The bounded next native diagnostic should capture validated display/power-well
status alongside the existing C3 attempts. The current record lacks that side
of the comparison. Audit driver/DMA/interrupt/teardown ownership before proposing
any D-state or PCI link transition. Neither Linux D3hot snapshots nor common-
clock differences authorize copying control values. RC6 timing, dynamic device
latency requests and the actual package-C3 blocker remain unresolved.


## Version-4 display power-well endpoints

The exact Air6,1 platform diagnostic additionally reads `PWR_WELL_CTL1` through
`PWR_WELL_CTL4` (`0x45400`, `0x45404`, `0x45408`, `0x4540c`) at its two existing
full-capture endpoints. These are the BIOS, driver, KVM and debug request/status
views of Haswell's global display well. Bit 31 is the request and bit 30 is the
state; raw values retain reserved/unknown fields without interpreting them.
The sources are ORed to request the well. Different status observations can
reflect a transition between reads; this is not an atomic snapshot or a live
platform-grant register.

[Linux's Haswell definitions](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_display_regs.h)
and [well reader](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_display_power_well.c)
identify the offsets, requester roles and global bits. The
[Intel Haswell display PRM, page 189](https://kiwitree.net/~lina/intel-gfx-docs/prm/hsw/intel-gfx-prm-osrc-hsw-display.pdf)
describes the request OR and power-well access behavior; it does not authorize
releasing firmware requests or shutting off resources. The existing Haswell
forcewake range ends at `0x3ffff`, below these registers. No forcewake or PCODE
operation is required or added for the four fixed reads.

After the unchanged CPU/GPU/PCH/DSDT, BAR, D0 and memory-decode gates, the code
maps the `BAR0+0x45000` page beside the existing `BAR0+0x138000` page. Both maps
are read-only, uncacheable and non-executable. Occupied addresses or either map
failure refuse use; partial mappings remain retained until reset. No additional
registers in that page are read. The original RC6 brackets remain unchanged;
the display reads have their own PM brackets after them. D0 is explicitly
revalidated at both endpoints before MMIO. No read is added to
the sparse power hook or a measured C3 round, and no worker, timer, wait,
request write, D-state transition or status acknowledgement is added.

Version-4 strict parsing requires two well receipts, validates BAR/time/bracket
ordering and rejects missing, duplicate or all-ones data. The independent
`platform_display_result` returns observations even when the original RC6
predicate fails, without granting acceptance to that failure. Versions 1–3
retain their existing rules. Save the independent display observation before
an overall platform-failure assertion so an expected RC6 discrepancy cannot
hide the new evidence. Maximum-width endpoint/series/device/durability replay
must preserve the original proofs inside the unchanged 64 KiB tail.

This narrows the missing native side of the Linux comparison; it does not read
pipe/plane/port ownership or prove the well causes the package-C3 block. Neither
an enabled well nor a firmware request authorizes clearing it. A later policy
experiment requires separate resource, DMA, interrupt and display ownership
analysis. All existing attempts, watchdogs, HLT fallback, integrity checks and
RC6 timing limits remain unchanged.


### Native version-4 result and receipt-coverage failure

Air6,1 release `c6fd6d4c55a57566` recorded BIOS `0xc0000000` and
driver/KVM/debug `0x40000000` at both endpoints, with valid read brackets and
zero runtime fault. This observes an enabled display well with only the BIOS
request set. It does not prove the request causes the C3 block or permit clearing
it without display-resource and firmware ownership analysis.

The trial remains failed. RC6 still exceeds its original PM-clock bound, and
newer source's repeated I/O-owner output displaced the early workload/AP-owner
proofs from the 64 KiB exit tail. The preflight replay used an older trace that
lacked 7,701 bytes of those reports. Use source-matched complete output and
representative checkpoint verbosity for the next coverage regression; preserving
only added feature receipts is insufficient. Matching periodic/exit logs have a
verified byte overlap and allow full-stream diagnostic review, but reconstruction
is not a substitute for the existing exit-only acceptance requirement. Do not
increase the tail, suppress failures or relax strict parsers to accept this run.
See the unit record's November 1 closeout for the exact session and hashes.


### Final-only CPU and AP acceptance receipts

The CPU-load diagnostic now retains three fixed-size records of completed
phase start/end, work chunks and checksum. Live one-second progress and phase
completion remain visible under `cpu-load-progress` where applicable. Only after
all three phases and original safety checks succeed does it emit the existing
`cpu-load` version/phase receipts once, using retained sampling times rather
than reporting time. Refusal or stop keeps its original failure marker and
partial progress and emits no successful phase receipt set.

AP startup similarly freezes the validated VM/native, CPU count, BSP, mask and
MONITOR method before publishing startup. It emits live `ap-owner-progress`,
then the original admission header once at retirement before actual per-CPU
results. Failed retirement remains failed; the header describes admission and
never implies success. AP assembly, polling, ownership, limits and failure paths
are unchanged. Strict legacy parsers are unchanged and still reject progress
without full final receipts, duplicate/malformed data and all prior failures.

This places the original proofs alongside already-deferred power/idle and
C3/package/platform reports. It leaves the shared logger, worker/I/O-owner
output, 64 KiB tail, timing, workload, hardware access and RC6 predicate intact.
`check_cpu_capture_tail.py` verifies exact logger/scheduler source fingerprints
against a named trace revision before replay. Its optional final-layout model
moves only existing records and is explicitly synthetic, never physical
acceptance. The failed November 1 stream is the local integration fixture:
old exit coverage fails; final placement preserves all critical record bytes,
with 128 KiB extra pre-final chatter and 29,813 bytes of conservative receipt
width/capacity padding. Retained samples, rows and all original strict checks
remain required. Negative fixtures preserve failures. Repeat the regression
when logging source changes instead of assuming an older trace is representative.

```sh
make test-cpu-receipt-tail
python3 tools/check_cpu_capture_tail.py --log PATH_TO_FULL_TRACE   --log-source-ref TRACE_SOURCE --model-final-layout --output REPLAY_RESULT_JSON
```

Use modeling only for regression evidence. A native retry must pass its original
exit-file checks directly, without replay or joining periodic files. Keep the
independent RC6 failure separate from receipt coverage. Progress, samples and
collector errors must survive a failed retry; no acceptance limit is relaxed.


The native Air6,1 retry on release `98bced62ca7be424` accepts final-receipt
retention: all original workload/AP/power/idle/C3/package/display proofs parse
directly from its unchanged 64 KiB exit log, without reconstruction or modeling.
The overall trial remains failed on the unchanged RC6 bound (about 94.279 ms
nominal excess over 90.048 seconds). Coordinated rounds and AP retirement pass;
package C3 is still zero. See the unit record for exact source/session/hashes.
This closes receipt coverage only; it neither resolves RC6 semantics nor
establishes that the BIOS display-well request causes the C3 block.


## Display-resource ownership audit (Air6,1, 2026-11-01)

The native request receipts establish that firmware's BIOS request is set at
both endpoints. They do not describe the resources it protects. The source
audit below uses Linux **v7.0** and SrvrOS integration base `a6b9e1b7`; it adds
no native register reads, driver takeover or power transition.

### Existing SrvrOS ownership

`kernel/src/main.c` passes the first Limine framebuffer to `console_init`.
`kernel/src/console.c` and the graphics syscalls in
`kernel/src/arch/x86_64/syscall.c` present pixels through that inherited linear
buffer. The current tree has no complete Haswell pipe/plane/transcoder/DDI
state adoption, display-domain reference tracking or safe well-release sequence.
The existing brightness path and isolated read-only platform pages do not
supply that ownership model.

Pinned Limine binary `5be26a73` is release v11.4.1; its corresponding
[source tag](https://github.com/limine-bootloader/limine/tree/v11.4.1), commit
`1276c440`, publishes the framebuffer and calls `efi_exit_boot_services` before
kernel entry. That service exit is a boot-environment handoff, not evidence that
Intel display requests, scanout or DMA have been retired. No firmware call or
OpRegion/SWSCI/ASLE transaction is proposed here. SrvrOS continues using the
inherited display.

### What the Linux sequence actually establishes

[Haswell well synchronization](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_display_power_well.c)
sets the driver request before clearing a set BIOS request. This preserves the
request while transferring its source; copying only the BIOS-clear write would
omit that protection. Linux separately tracks users and releases its driver
request through its power-domain lifecycle.

[Display power initialization and sanitization](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_display_power.c)
hold an INIT reference while firmware-enabled resources are read out. Unused
wells are considered for disabling after pipe/encoder state has been sanitized
and active users have acquired references. SrvrOS does not currently reproduce
that sequence. This audit therefore does not authorize even a temporary
request transfer or a firmware-request clear.

The [Haswell global-well domain map](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_display_power_map.c)
covers pipes B/C, panel fitters A/B/C, transcoders A/B/C, DDI lanes B/C/D,
CRT/VGA and audio MMIO/playback. Pipe A, transcoder eDP and DDI A are outside
that switchable list. Thus an active internal panel need not alone explain
this well request; an enabled panel fitter or other dependent resource would
matter. This is a dependency distinction, not proof that this unit can release
the well or enter package C3.

The earlier Ubuntu cached observation shows pipe A, eDP-1, primary/cursor A,
no other active CRTC and zero cached display-well users. That observation is
post-i915 state, not SrvrOS firmware routing or a hardware-off receipt. In
particular, the eDP transcoder can select different input pipes;
[DDI hardware readout](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_ddi.c)
uses its eDP input selection and holds references to already-enabled domains
before reading dependent resources. Do not infer native pipe A from panel size
or the Ubuntu observation.

### Bounded next capture contract

The next implementation should be a separate explicit, exact-Air6,1 read-only
endpoint inventory alongside the existing platform diagnostic. Before adding
MMIO, review an exact Haswell register/page allowlist for these resource groups:

| Group | Required observation |
| --- | --- |
| Inherited framebuffer | Geometry/pitch/format; preserve existing presentation ownership |
| Transcoders A/B/C/eDP | Enable/state, DDI selection/mode and eDP input routing |
| Planes and cursors A/B/C | Enable/mode controls; no framebuffer-pixel or backing-memory dump |
| Panel fitters A/B/C | Enable and input-pipe selection |
| DDI A–E and VGA | Buffer enables, relevant routing, VGA disable state |
| Display audio and interrupts | Read-only enable/mask context where validated; no status acknowledgement |

These are candidate groups, **not a blanket register access allowlist**. Offsets,
Haswell-specific masks, read side effects and powered-domain accessibility must
be checked before implementation. In particular, the
[Gen6/7 forcewake range](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/intel_uncore.c)
ending at `0x3ffff` says nothing by itself about accessibility when a display
power domain is off. Do not wake a domain just to inspect it. Revalidate GPU
identity, BAR, D0/memory decode and required well state around bounded dependent
reads; skipped/inaccessible data must remain explicit unknowns, never decoded
as disabled. Endpoints remain non-atomic observations.

Use read-only/NX/cache-disabled private mappings and fixed storage, outside
RC6 brackets, power hooks and measured C3 rounds. Retain firmware requests and
all original phase, latency, watchdog, integrity and strict RC6 checks. Emit
versioned raw final receipts and parse the resource observation independently
of the unresolved RC6 predicate. Require malformed/missing/duplicate/all-ones,
wrong-profile, mapping-failure and off-domain refusal coverage plus current-source
64 KiB tail regression and exact-stage VM validation before native use.

Even complete endpoints would inventory possible dependencies, not prove a
causal package-C3 blocker, safe DMA teardown, physical display continuity between
reads or permission for a well/D-state/link transition. RC6 clock semantics,
package C3 and any later control experiment remain separate work.


## Version-5 bounded display-control capture

The explicit exact-Air6,1 platform recipe now retains 26 raw controls at each
of its two endpoints. This first inventory excludes display audio/interrupts,
OpRegion, pixel/backing-memory dumps and control writes. Existing boot logs
retain framebuffer geometry; controls cannot alone prove the scanout address
or safe teardown. Ordinary boot and the sparse power hook remain unchanged.

The fixed read allowlist is in `display_resource_audit.h`. Offsets come from
Linux v7.0 `intel_display_regs.h`, `i9xx_plane_regs.h`, `intel_sprite_regs.h`,
`intel_cursor_regs.h`, `intel_pfit_regs.h`, `intel_vga_regs.h`, with Haswell
pipe/transcoder/cursor offsets from `intel_display_device.c`:

| Controls | BAR0 offsets (hex) |
| --- | --- |
| Transcoder config A/B/C/eDP | 70008, 71008, 72008, 7f008 |
| Transcoder DDI function A/B/C/eDP | 60400, 61400, 62400, 6f400 |
| Primary planes A/B/C | 70180, 71180, 72180 |
| Sprite planes A/B/C | 70280, 71280, 72280 |
| Cursor controls A/B/C | 70080, 71080, 72080 |
| Panel fitters A/B/C | 68080, 68880, 69080 |
| DDI buffer controls A–E | 64000, 64100, 64200, 64300, 64400 |
| CPU VGA control | 41000 |

These are control readbacks, not interrupt status, AUX transactions or data
ports. Linux hardware-readout uses the same control classes. The Haswell display
PRM's Display Power Well description (PDF page 189) specifies zero readback for
powered-down resources. Such zeroes cannot prove disabled hardware. The capture
therefore requires all four existing well-status views enabled before dependent
reads, then checks them again and requires exact pre/post values to match.
GPU identity, BAR, memory decode and D0 are also revalidated before/after the
bounded reads. These non-atomic observations cannot exclude an intervening SMI
or firmware transition; no lock or firmware ownership is inferred.

Only unique pages containing the fixed allowlist are mapped at private addresses
starting `0xffffc001f3210000`, with the existing read-only/NX/cache-disabled flags.
Every destination is checked before any page is installed. Partial map failures
are retained unused until reset. The producer has no writable MMIO alias and
reads only the fixed controls. Off domains are explicit state 2 with zero attempted reads and zero raw
placeholders which are never decoded; identity/access changes, all-ones or changed well state are state 3. Both
make the overall capture incomplete rather than producing successful zero data.
The report preserves partial raw reads and the failed source marker.

`ap-platform-resources` carries phase, validity state, PM bracket, read count and
pre/post well controls. Fixed-order `ap-platform-resource` records bind all 26 raw values
to the versioned exact-offset table above. The independent `platform_resources_result`
requires both endpoints and rejects duplicates, missing/unlisted/out-of-order
records, bad counts, all-ones, invalid access and brackets. It reports explicit
unavailable data without decoding it as disabled. Decoding retains raw fields;
eDP input selection is separate from the port-selection bits ignored by that
transcoder. Reserved encodings remain numbers, not assumed routes.

Version-5 overall acceptance requires a valid complete resource observation in
addition to every previous strict check, including the original RC6 bound.
The independent observation can survive an RC6 failure without accepting the
platform trial. Current-source tail regression must retain all 52 raw control values in two records, both headers and every original proof in 64 KiB with maximum-width/capacity
padding and pre-final chatter. No tolerance, phase, attempt, latency, ownership
or integrity requirement changes. Native acceptance remains pending a matched
physical return; enabled controls alone cannot identify a causal C3 blocker.


### Native version-5 observation

Air6,1 source `78fbea3c`, release `6ac139e77c5d3070`, records both complete
26-control endpoints with identical raw values, valid access brackets, zero
source faults and unchanged pre/post BIOS well request. The actual 64 KiB exit
log directly retains every original proof and the new controls; no reconstruction
or synthetic data is used for this native observation.

The eDP transcoder selects pipe A, primary A and DDI A are enabled, and the
other sampled primary/sprite/cursor/fitter/DDI enable fields and VGA are disabled.
The BIOS request remains set. This inventory narrows the missing routing data;
it does not prove the switchable well unused. Display audio/interrupts, remaining
firmware dependencies and safe ownership/teardown are still unmeasured. Do not
release the request based on these controls alone.

The overall platform trial remains failed on the original RC6 bound: about
98.739 ms nominal excess over a 90.051-second PM bracket (0.1096%); package C3
remains zero despite 32 successful bounded core-C3 rounds. See the unit record
for exact session/hash and USB/recovery checks. Newly merged PR261 changes the
scheduler/logger baseline after this trial; its integration needs fresh local
validation and source-matched tail coverage before another candidate. None of
this promotes an older logging trace or sibling-machine result to new acceptance.


## Post-PR261 integration gate and remaining ownership audit (2026-11-02)

Main `95ddb8d6` includes PR261. Its logger adds `exfat_print_costs()` to every
device checkpoint, and the scheduler adds retained I/O-gate wait attribution.
The old native source `78fbea3c` does not contain those reports. The original
`check_cpu_capture_tail.py --log-source-ref` guard correctly rejects that trace
against this integration; changing the declared trace revision is not validation.
The guard now also fingerprints `exfat.c` and `exfat.h`, since cost-report
formatting/capacity changes can occur without another logger/scheduler edit.

`check_cpu_checkpoint_budget.py` separately sizes the reviewed **additional**
checkpoint output. It reads current C format strings/capacities and compares the
logger/scheduler changes with the declared historical source, refusing unknown
format/capacity changes. It adds nine gate rows (eight longest waits plus a
separately retained exit), one gate summary, four filesystem cost rows and
sixteen cost-phase rows. Numeric fields use maximum u64 width; operation labels
use the longest actual static mutation label. The historical native proofs and
original RC6 failure are unchanged. Results are explicitly synthetic, not a
source-matched new trace and never physical acceptance.

On the latest Air6,1 exit file, these additional reports require 9,056 bytes.
With the unchanged width/capacity stress and 64 KiB tail, required proofs are
lost. This is a failed conservative retention budget, not a newly observed
hardware failure. Do not stage this profile for a native retry or extend its
records until the retention gate is resolved. Preserve both the failing model
and raw native trace. Do not grow the tail, discard checkpoint diagnostics,
shorten workloads, reduce coverage or relax strict parsers to bypass it.
A later fix should keep all sample values and failure evidence while arranging
or encoding final proofs to fit. A real current-source native return remains
required after host/VM validation.

```sh
python3 tools/check_cpu_checkpoint_budget.py --log HISTORICAL_EXIT_LOG --trace-source-ref TRACE_SOURCE --output SYNTHETIC_BUDGET_JSON
make test-cpu-checkpoint-budget
```

### Display audio and interrupts are distinct owners

SrvrOS `audio_init` selects this unit's PCH `8086:9c20` controller and CS4208
profile. It does not select display HDA `8086:0a0c` at 00:03.0. The platform
recipe omits `srvros.audio=hda`, so even PCH playback setup is not invoked by
this trial. No cached inactive audio state establishes that firmware's display
controller, streams or graphics audio are disabled.

The ordinary audio initializer is unsuitable for read-only inspection: it
requests D0, enables MMIO/DMA, creates writable mappings and resets controller/
CORB/RIRB state. The existing Intel graphics stub also enables PCI memory decode
and bus mastering and maps its first four MMIO pages; it supplies no complete
display interrupt, audio, scanout or DMA teardown lifecycle. Neither initializer
should be called as a dependency probe or copied into the read-only allowlist.
This audit adds no such calls and changes no existing boot policy.

Linux v7.0 [display audio](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_audio.c)
coordinates graphics codec sequences with the HDA controller. Its Haswell codec
disable path changes timestamp/ELD/presence state and waits for vblanks; playback
power references protect the display domain. The native 26-control inventory
does not measure that ownership or permit executing the shutdown sequence.

The [Haswell audio definitions](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_audio_regs.h)
identify candidate readback `HSW_AUD_PIN_ELD_CP_VLD` at `0x650c0` and config
controls at `0x65000/0x65100/0x65200`. Output-enable/ELD/inactive fields are
per-transcoder observations, not proof that controller DMA is retired. These
are **candidate controls only**; any implementation still requires exact
access/side-effect/powered-domain review, fixed read-only pages and explicit
invalid/off-domain evidence. Exclude ELD data windows, codec verbs, CORB/RIRB
transactions and status acknowledgements from this first read-only inventory.

Linux's [display interrupt implementation](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_display_irq.c)
and [register definitions](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/i915_reg.h)
separate mask/enable controls from identity/status acknowledgement. Candidate
context is DEIMR/DEIER (`0x44004/0x4400c`) and PCH SDEIMR/SDEIER
(`0xc4004/0xc400c`); the PCH block needs its own access prerequisites, not merely
the existing GPU well check. Do not read/write IIRs as a substitute for owning
and draining the interrupt source. A masked source is not proof of an unused
power domain or stopped DMA. SrvrOS's generic IRQ dispatcher/EOI infrastructure
does not by itself provide a Haswell display interrupt owner.

These findings define the remaining read-only audit; no new registers are read
and no audio/interrupt state is changed. Resolve final-log retention first, then
add separately validated compact observations. The BIOS well request, unresolved
RC6 semantics and zero package C3 remain independent; no causal or safe power-
transition conclusion follows from the current samples.


## Lossless compact final telemetry (format 2)

Only the deferred CPU capture reporter changes encoding. Live progress, ordinary
power/status output, checkpoint data, capture capacity (24), sampling cadence,
phase lengths, AP rounds, watchdogs and semantic acceptance checks stay intact.
The retained-count/overflow receipt remains `cpu-load-power: retained=N overflow=F`.
One `cpu-capture-format: v=2 idle=I entry=E` header declares whether retained
idle and idle-entry records are present. Indexed records use `cpu-p2: i=N v=...`,
`cpu-i2: i=N v=...` and `cpu-e2: i=N v=...`. All numeric fields remain decimal
u64 values; no value, duplicate sampling timestamp or counter is omitted.

| Record | Fixed value order |
| --- | --- |
| p2 (15) | power_state, flags, energy_shift, package_mw, active_mhz, nominal_mhz, package_uj, sample_us, interval_us, samples, rejected, energy, aperf, mperf, monotonic_us |
| i2 (28) | supported, valid, rejected, sample_us, interval_us, interval_tsc, tsc_begin, tsc_end; then c0_raw,c0_bp through c9_raw,c9_bp |
| e2 (5) | sample_us, epoch, enters, returns, ready |

`cpu_capture_format.py` requires the unique known header, unchanged bounded
retained-count receipt, matching per-kind sample counts and consecutive indices.
It rejects overflow, malformed/unknown records, duplicates, missing rows, wrong
field counts, out-of-range u64 values and mixed legacy/compact encodings. Only
then does it expand to the original named records for the existing strict power,
residency, idle-entry, prerequisite, MWAIT and platform-series semantic checks.
It does not repair invalid measurements or convert a failed workload to success.
Legacy captures retain their existing rules.

Producer host tests check the actual formatter's field counts and every vararg
against the retained C snapshots. Codec fixtures compare exact canonical byte
roundtrips and identical parsed results, including malformed, unavailable and
failed measurements. The regression-only `compact_legacy` function and optional
`--model-compact-v2` budget flag never run in the physical return collector.
They preserve every field in old evidence while modeling only its encoding.

The previous native exit plus the current maximum added checkpoint fails in
legacy layout and passes with format 2 under the same 64 KiB tail, 128 KiB
pre-final chatter and maximum-width/capacity rules. The compact model includes
all 9,056 bytes of added gate/cost reports. Width padding is schema-aware and
uses the original per-field bounds; the new row index is bounded to 0–23.
Every critical compact record must remain byte-identical in the stressed tail.
No original strict RC6 limit changes; its existing failure remains a failure.

The trace-source guard now covers capture producers and main's report ordering
as well as logger/scheduler/filesystem formatting. Synthetic budget reports
also record current source and codec fingerprints. A real matching native
return must retain and decode the new header/rows directly; the lossless model
is regression evidence only, never physical acceptance.


### Native format-2 retention result

Air6,1 release `0bc5f7e0298ca886` on source `8f0c5aa6` physically accepts the
compact retention fix. Its actual 64 KiB exit file contains one format header,
15 power, 15 idle-residency and 15 idle-entry records plus every original proof.
All parse directly under the unchanged semantic requirements. No legacy model,
joined log or invented value is used. A fresh nine-source-matched replay of
that exit also preserves every critical byte under the unchanged width/capacity
stress and extra chatter, covering the PR261 reporting integration.

The overall platform test still fails the original RC6 bound (about 0.0966%
nominal excess), and package C3 remains zero. Thirty-two coordinated core-C3
rounds and terminal AP retirement pass; display controls and firmware request
remain unchanged. The encoding fix grants no display/audio/IRQ ownership or
power-policy acceptance. Continue with the audited read-only dependency
inventory; retain the new trace/source identity when checking future additions.


## Version-6 read-only display audio and IRQ controls

The exact-Air6,1 opt-in platform diagnostic adds eight fixed control readbacks
at its existing two endpoints, after the version-5 resource brackets:

| Order | Control | GPU BAR0 offset |
| --- | --- | --- |
| 0 | HSW_AUD_PIN_ELD_CP_VLD | 0x650c0 |
| 1–3 | HSW audio config A/B/C | 0x65000, 0x65100, 0x65200 |
| 4–5 | CPU DEIMR / DEIER | 0x44004, 0x4400c |
| 6–7 | PCH SDEIMR / SDEIER | 0xc4004, 0xc400c |

Linux v7.0 `intel_audio_regs.h` and `i915_reg.h` define these control offsets.
The [PCH detection](https://github.com/torvalds/linux/blob/v7.0/drivers/gpu/drm/i915/display/intel_pch.c)
identifies the `0x9c00` family as LynxPoint LP, and its display IRQ implementation
uses the SDE mask/enable block for LPT. SrvrOS admits only captured `8086:9c43`
at 00:1f.0 with live class `060100`, not the whole family. The captured DSDT,
CPU/GPU profile and existing BAR/D0/memory-decode gates remain mandatory.

The PCH block is reached through the validated GPU aperture. Its separate gate
rechecks the exact LPC identity/class/location before and after the reads; it
does not invent an LPC PM capability or request D0. GPU identity/BAR/decode/D0
and all four well status views must also validate on both sides, with unchanged
pre/post well raw values. These are non-atomic checks; an intervening firmware
change which reverts between reads remains outside the observation.

Three new private pages at `0xffffc001f3220000` map only BAR0 offsets `0x65000`,
`0x44000` and `0xc4000` with read-only/NX/cache-disabled flags. Occupied virtual
pages or a partial mapping fail closed; partial maps remain unused until reset.
Only the eight fixed offsets are read, outside RC6 brackets, sparse power hooks
and measured C3 rounds. No HDA initialization, verbs, ELD data windows, CORB/RIRB,
IIR/status acknowledgement, mask write, forcewake, power request or firmware
transaction is added. Existing sampling, policies, ownership and limits remain.

`ap-platform-deps: v=1` records each phase's state, read count, PM bracket,
pre/post PCH identity, all pre/post well values and eight raw controls. An
all-ones audio control is invalid; all-ones IRQ masks are legitimate all-masked
values and are retained. The parser accepts uint32 IRQ raw values without
claiming reserved bits or master enables imply dispatch. Its mathematical
`enable_and_not_mask` value is context only, not pending interrupt status.
Off-domain captures report explicit unavailable state, with no decoded zero
values; other access changes or partial/invalid reads fail the capture.

The independent `platform_dependencies_result` decodes A/B/C ELD-valid,
CP-ready, output-enable and inactive bits plus raw audio config and CPU/PCH
mask/enable intersections. Version-6 overall acceptance requires both complete
valid dependency endpoints in addition to all prior proofs and the unchanged
RC6 predicate. Independent observations remain available on an RC6 failure.
No mask or graphics-audio readback establishes HDA controller/stream retirement,
IRQ ownership, unused display well or a causal package-C3 blocker.

Host fixtures verify exact maps/offset order, all pre-access gates, powered-off
skips, all-ones handling, changed PCH/well state, and every reported scalar against
retained C data. Parser fixtures preserve versions 1–5 and refuse malformed,
missing/duplicate/out-of-order/invalid version-6 endpoints. Retention regression
uses the preserved native compact trace plus explicitly synthetic new endpoint
records and source-derived maximum final checkpoint shapes/widths; it is never
native acceptance. Require the actual new records directly after the retry.


Air6,1 source `4e2ff068`, release `ee85beb85b75fdca`, now supplies both actual
valid eight-control endpoints directly in its 64 KiB final log, alongside all
15 compact power/idle/entry samples and earlier proofs. The two endpoint values
are identical: pin/ELD zero, configs A/B/C `0x0070fa60`, CPU/PCH masks
`0xffffffff` and enables zero. Live access gates and unchanged well checks pass.
This accepts the bounded read-only observations and retention only. HDA/DMA
retirement, IRQ ownership and a safe well transition remain unproven.
The overall platform trial still fails the original RC6 bound (about 0.0893%
nominal excess), and package C3 remains zero. No policy or limit changes follow.


## Remaining Haswell ULT well dependencies (2026-11-02)

This source audit uses Linux v7.0 commit
`028ef9c96e96197026887c0f092424679298aae8` and the actual Air6,1
`ee85beb85b75fdca` endpoints. It changes no register access, ownership, policy,
limit or installed payload. Upstream code describes prerequisites; it does not
supply native acceptance or prove the cause of zero package C3.

### Applicability and observed coverage

The pinned [PCI IDs](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/include/drm/intel/pciids.h)
classify captured `0x0a26` as Haswell ULT GT3 mobile. The
[display device profile](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_display_device.c)
removes port D for ULT. The
[output setup](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_display.c)
excludes CRT on ULT before CRT initialization. Do not add a general Haswell
CRT/FDI probe to this unit merely because the generic domain list includes CRT.
Existing raw port-D/E slots remain recorded; zero bits in an unsupported slot
do not establish an implemented, disabled port or an ownership proof.

The [Haswell well map](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_display_power_map.c)
associates the switchable well with pipes B/C, fitters A/B/C, transcoders A/B/C,
DDI lanes B/C/D, CRT, VGA, audio MMIO/playback and initialization. Pipe A, the
eDP transcoder and DDI A are outside this switchable domain list. This software
map is context, not an exhaustive hardware permission register.

| Dependency | Actual endpoint evidence | Remaining requirement |
| --- | --- | --- |
| Pipes/transcoders B/C and other non-eDP routes | Sampled A/B/C transcoder controls disabled; eDP selects pipe A | Adopt and retain the active route, framebuffer and resource lifetime before any release |
| Fitters A/B/C | Enable bits clear; eDP DDI input field is 0 | Preserve both fitter enable and forced-through routing semantics; no inference from enable alone |
| DDI B/C and VGA | Sampled buffers disabled; VGA disable set | Supported-resource ownership and exclusion of concurrent users, not only two snapshots |
| CRT/FDI and port D | ULT source profile excludes CRT and port D | Treat as profile applicability; no speculative CRT initialization or generic register sweep |
| Display audio | A/B/C output/ELD bits clear; configs retained | Separate display-HDA controller, link and DMA context remains unmeasured |
| Display IRQs | DE/SDE enables zero; all mask bits set | Masks do not stop DMA, drain events or acquire an IRQ owner |
| Firmware/init references | BIOS well request remains set | Establish a serialized takeover/readout/release lifecycle and recovery before writes |

A subtle fitter dependency is already checkable without adding MMIO:
`hsw_get_transcoder_state()` in the pinned output source marks fitter A forced
through when the eDP input is `A_ONOFF` (encoding 4). The actual input field
is `A_ON` (0), so that specific forced-through condition is absent at both
endpoints. This is a bounded routing observation, not proof that the well is free.

### Display HDA is the next useful read-only gap

The same native PCI snapshots retain display HDA `8086:0a0c` at 00:03.0 in D0
with command `0x0006` (memory decode and bus mastering enabled) at both endpoints.
The distinct analog controller `8086:9c20` at 00:1b.0 is also D0 with command
`0x0006`. Neither fact proves active DMA; neither permits treating the controller
as retired. The ordinary `srvros.audio=hda` initializer does not own display HDA.

Pinned [HDA controller code](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/sound/hda/controllers/intel.c)
keeps Haswell display-HDA power bound to the graphics component. Its runtime
resume gets display power before controller initialization; shutdown stops the
chip, enters link reset and clears pending IRQs before releasing display power.
The [graphics component](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_audio.c)
backs its audio power reference with `POWER_DOMAIN_AUDIO_PLAYBACK`. Clear ELD or
output-enable bits cannot substitute for this controller lifecycle.

A future bounded capture should first review exact display-HDA PCI identity,
subsystem, class/BDF, BAR span, live D0/decode and powered-domain prerequisites.
Candidate controls from the pinned
[HDA definitions](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/include/sound/hda_register.h)
are GCAP (16 bits), GCTL/INTCTL (32), CORBCTL/RIRBCTL (8), DPLBASE (32), and
capability-bounded stream control fields. Read widths must exclude adjacent
status bytes; stream-count arithmetic must be validated before addressing.
These are candidates requiring access/side-effect review, not a new allowlist.
Exclude codec verbs, immediate commands, data buffers, ring pointers, W1C status,
BAR sizing writes, resets, D-state changes and bus-master changes. Unsupported,
off, inaccessible or changing state must remain explicit unknown/failure.
Even an all-stopped capture would not grant ownership or permission to power off.

### Why a well-clear experiment is premature

Pinned [power-domain initialization](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_display_power.c)
holds INIT until hardware readout has established resource references; sanitation
releases only enabled wells with no users. The
[well takeover](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_display_power_well.c)
sets the driver's request before clearing the BIOS request. Copying only the BIOS
clear would omit those protections. SrvrOS `intel_gfx_init()` enables PCI decode/
bus mastering and maps four pages; it supplies no equivalent modeset/domain
reference graph, display-HDA lifecycle or firmware-resource adoption. The Limine
framebuffer remains live and must survive console, desktop and failure reporting.

Before any future transition proposal, define exact exclusive ownership,
active-route and framebuffer invariants, domain users and teardown ordering,
bounded state verification, restoration with tested fallback, and evidence for
success and refusal paths. Read-only observations alone cannot fulfill these
requirements. Keep the unresolved RC6 clock discrepancy and package-C3 result
separate. The immediate next implementation should be narrowly scoped read-only
display-HDA context with profile-aware applicability, followed by its own matched
host/VM/native validation and unchanged final-log retention gate.


## Version-7 display-HDA read-only capture

The exact Air6,1 platform profile adds two controller snapshots after its prior
resource/audio/IRQ endpoint reads, outside RC6 brackets, sparse sampling and
measured C3 rounds. Ordinary audio startup and production display behavior are
unchanged. Captured display HDA is 00:03.0 `8086:0a0c`, subsystem `106b:011a`,
class `040300`; analog HDA remains a separate device/owner.

Live identity/class/subsystem/BDF, the captured non-prefetchable 64-bit 16 KiB
BAR shape, D0/PM capability and memory decode are required. No BAR sizing or
D-state/command write occurs. A private first-page mapping at
`0xffffc001f3230000` is read-only/NX/cache-disabled, with occupied-page refusal.
Existing GPU/PCH gates and all four display-well views must pass before/after;
BAR, command/PM and well values must remain unchanged. Observations are
non-atomic and cannot exclude a firmware change that reverts between checks.

The [Intel HDA 1.0a specification](https://www.intel.com/content/dam/www/public/us/en/documents/product-specifications/high-definition-audio-specification.pdf)
sections 3.1.2 and 3.3 define naturally aligned register accesses. The capture
reads GCAP as 16 bits, GCTL/INTCTL/DPLBASE as 32 bits and CORBCTL/RIRBCTL as
8 bits. GCAP bounds the descriptor sum to 1–30. Each stream control is read
as 16 plus 8 bits, excluding its status byte at offset +3. GCAP and GCTL are
rechecked afterward. Reset asserted skips other controls; off-domain skips MMIO.
No codec/data/ring-pointer/status-window access, reset or acknowledgement is
added. An active command/stream DMA bit is retained as an observation, not a
capture error. All-ones or changed access/control identity fails closed.

`ap-platform-hda: v=1 phase=N data=...` holds a canonical 240-character
base64 encoding of a fixed 179-byte little-endian payload. It preserves all
raw values and partial/failure rows while fitting the unchanged 64 KiB final
log; this is an encoding change, not reduced sampling or omitted diagnostics.
The strict decoder rejects wrong sizes, noncanonical padding, unknown states,
wrong counts, invalid fields and duplicate/missing/out-of-order endpoints.

| Payload order | Width |
| --- | --- |
| State, MMIO read count, stream count | Three unsigned bytes |
| Begin/end PM microseconds, BAR base | Three uint64 values |
| Pre command/PM, post command/PM | Four uint16 values |
| Before wells, after wells | Eight uint32 values |
| GCAP, GCTL, INTCTL, CORBCTL, RIRBCTL, DPLBASE, GCAP/GCTL rechecks | 2/4/4/1/1/4/2/4 bytes |
| Thirty stream-control slots | Three bytes each |

Only GCAP's count of stream slots is interpreted; unused slots must be zero.
State 1 is observed, 2 unavailable without MMIO, 3 invalid/partial, 4 reset
asserted with only GCAP/GCTL observed. No zero placeholder is decoded as stopped
hardware. Global producer completeness and final-log coverage require both
valid new snapshots; overall version-7 acceptance also retains the original
RC6 predicate and every prior proof.

Host fixtures exercise exact widths, stream bounds, identity/subsystem/BDF,
PM/decode/BAR, mapping and powered-domain refusals, reset/all-ones/change cases,
post-read identity/BAR/decode changes, active DMA observations, all emitted
scalar fields, strict decoding and mandatory tail retention. Native acceptance
remains pending its exact-source run. No HDA/link/codec readiness, DMA retirement,
ownership, safe well-clear or package-C3 causal claim follows from these controls.
