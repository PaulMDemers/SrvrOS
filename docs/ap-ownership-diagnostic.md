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
