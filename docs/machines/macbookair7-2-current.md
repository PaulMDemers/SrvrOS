# MacBookAir7,2 current status

Updated 2026-11-28 (America/New_York). Reference A1466 with partial desktop/device acceptance.
The [dated journal](macbookair7-2.md) preserves exact-source history.

The inherited-framebuffer desktop remains usable, with independent evidence for
keyboard/trackpad, secondary click, window navigation, saved Home documents,
brightness, sensors, battery and earlier USB Ethernet/audio rounds. Native
tear-free scanout is still pending.

## Current Air7,2 work on PR 301

Physical release `611f57c3cf89ae16` at `b9238469` completed the idle
configuration comparison in session `session-0006610d19691536-00`.
The four GT timer registers matched the three Ubuntu/i915 reference samples
both before and after reset. Wake-rate limit was 0x280000, evaluation interval
125000, idle hysteresis 25 and sleep 0.

| MAX_IDLE register | Ubuntu/i915 | SrvrOS before reset | SrvrOS after reset |
| --- | ---: | ---: | ---: |
| RCS, VCS0, VECS and BCS (each) | 10 | 10 | 64 |
| VCS1 | 10 | 64 | 64 |

The before- and after-reset register reads took 19 and 26 µs. Across the
eight panel observations, forcewake ACK was clear, the programmed and live
surfaces matched EFI, and RC6 residency stayed at 1,553,895 ticks. The last
idle interval after display-audio release was 102.735 ms. All seven original
command-slot translations matched, all 19 EFI-reserved pages verified and
the offline exFAT allocation check found zero errors.

Release `0de8f1147c7d3102` at `1a0b2213` completed the MAX_IDLE trial
in session `session-0006610fd7bb5311-00`. All five engines started at 64
after reset, read back 10 immediately and again after the measurement, and
read back 64 after restoration. With display audio held and forcewake
released, RC6 residency advanced by 19,876 ticks over the 22.716 ms window
and by 84,585 ticks over the full 105.436 ms window. After restoration
and display-audio release, it advanced by zero ticks over 102.735 ms.
All nine samples showed clear forcewake ACK and the original enabled
primary plane, programmed surface and live surface. Ubuntu collected the
matching report and the offline exFAT allocation check found zero errors.

Release `5f166567f6a39e43` at `85aeb4ba` saved the software park trial
in `session-0006611073f4eda2-00`. The open-state guard read
`GO_GAM_REQ=0x7fff`, `MSG_GO_GAM=0x7f` and `RSTFCTLMSG=0`. The
reset-reason bits appeared across the first GT reset pulse. They remained
set at park admission, while GAM still permitted all six clients. The driver
refused the request before writing either park register. All five MAX_IDLE
values returned to 64, the temporary alias retired and the guest returned its
display and forcewake leases. Ubuntu verified the session and offline USB
integrity. This run closed as rejected. Device-wide traffic blocking and
fresh engine/L3 acknowledgements remain unverified.

The Air7,2 pre-reset park trial runs after the first traffic sample, while the
display and GT leases are held and PCI bus mastering is inhibited. It restores
the original park policy and verifies open traffic before the blitter stop,
five-engine preparation and first GT reset. Each powered sample checks the
enabled primary plane and its programmed and live surface on both sides of
the traffic reads. The version 2 receipt records the park return and the start
of preparation on the same guest clock. A completed observation can report
that traffic stayed open. Only new acknowledgements from all five engines, a
fresh L3 pulse with WiDi closed, and two subsequent held samples establish a
powered block during this interval.

Release `5c3e73bb4e1a2aed` at `6548205e` completed this diagnostic in
`session-00066110f8945ca4-00`. Both park writes read back as requested.
All eight polls over 155 µs saw `GO_GAM_REQ=0x00ff` and `MSG_GO_GAM=0x7f`.
All five engine acknowledgements stayed clear, WiDi continued to permit
traffic and the L3 flush acknowledgement stayed low. All 20 plane checks
matched the original enabled scanout. The driver restored both policy
registers and retired the temporary alias at 6,029,553 µs. Preparation
started at 6,029,554 µs and the subsequent reset sequence completed.
Ubuntu verified the matching logs and clean exFAT return. Device-wide
traffic exclusion and tear-free scanout remain pending. The eight polls
occupied only 155 µs of the existing 10 ms budget with forcewake held.

Release `0ae4269ba6159861` at `7f276f8c` completed the spaced pre-reset
park trial in `session-000661117c203eb8-00`. The eight polls started at
0, 1,251, 2,501, 3,750, 5,001, 6,251, 7,502 and 8,751 µs. The last
read finished at 8,764 µs within the 10 ms window. Every poll saw
`GO_GAM_REQ=0x00ff`, `MSG_GO_GAM=0x7f` and WiDi open. None of the
five engines acknowledged and the L3 flush acknowledgement stayed low.
All 20 display checks matched the enabled Pipe A plane and original
programmed and live surface. Both park registers were restored and the
temporary alias retired before engine preparation. Ubuntu replayed
the saved session and the offline exFAT check found zero errors.

Forcewake was held throughout park polling. In the separate post-reset
MAX_IDLE=10 measurement with forcewake released, RC6 residency advanced
by 84,605 ticks over 105,435 µs. A follow-up can use a bounded forcewake
release during the pre-reset park and measure residency across it. That
measurement alone cannot establish a device-wide traffic block.

The Air diagnostic pauses the original forcewake request with the
display and HDA leases pinned and PCI bus mastering inhibited. It samples
the always-on RC6 counter with request and ACK clear at the start and
again at 5 ms. It then reacquires the same request before checking
powered engine and L3 status at 7.5 and 8.75 ms. The park receipt binds
both samples and reacquisition to the existing 10 ms interval.

Release `b273f44d500cb3f3` at `fce8e91c` saved two incomplete sessions,
`session-000661126088712d-00` and `session-000661371c93d279-00`.
Both retained the same failure. The software park writes read back, and
the driver issued the masked forcewake release. The last recorded
request and ACK were clear, but release verification timed out before
either RC6 sample. Neither run attempted reacquisition, policy restoration,
engine preparation or a GT reset. The initial enabled Pipe A plane and
original live surface matched. The saved records end before a final
display or ownership return.

Ubuntu verified both saved checkpoints, launch recipes and log hashes.
The offline exFAT check found zero errors. The run closed as rejected after
reviewing both sessions. The release path needs separate timestamps for the
store, the first clear ACK and the closing guard to determine where the
10 ms budget was spent.

Release `77a21050a4a1632c` at `6b456a1b` saved the measured failure in
`session-000661378d68ce57-00/exit.log`. Relative to the start of the
10 ms window, the driver entered the release write at 4.080 ms and
issued it at 5.441 ms. The first verified clear request and ACK came
at 10.885 ms, and the closing guard finished at 12.245 ms. The measured
interval includes the guarded reads required to observe the ACK.
The deadline expired before RC6 sampling or engine preparation.
Ubuntu collected the report and verified exFAT integrity.

The Air park profile now saves the launch options, forcewake timings,
RC6 samples and result to `park-timing.log` in the USB session directory.
It verifies and flushes this file before writing `exit.log`. A stall
inside the probe leaves the earlier `session-a.log` checkpoint available.

Physical release `04f8f381c448b72c` at `9b997ecd` verified the compact
report in `session-0006613942583cf0-00`. Ubuntu read back all 1,199 bytes
from exFAT, matched them byte for byte against the selected exit log rows,
and verified the raw hashes, preflight checkpoint and offline USB integrity.
The compact report captured `completed=NO`. The release store occurred at
5.447 ms, the first verified clear request and ACK at 10.904 ms, and the
closing guard finished at 12.265 ms. Times are measured from the start of
the 10 ms park window. The repeat missed the deadline during ACK sampling
and ended before RC6 sampling, engine preparation or GT reset. The run is
closed as an incomplete GPU trial with verified physical log persistence.

The Air park probe groups the two always-on forcewake reads under one fresh
adapter admission. The separate closing guard, original panel, HDA lease,
PCI checks and 10 ms deadline remain in force. Physical release
`20f421f22c817cf1` at `8ae06d45` passed exact-image VM and host qualification.
Native session `session-00066139bd55b6e9-00` issued the release store at
5.444 ms, verified the clear request and ACK at 6.804 ms, and passed the
closing guard at 8.165 ms. The paired reads moved both verification points
4.100 ms earlier than the preceding physical run.

The remaining 1.835 ms did not yield a complete RC6 sample. The report does
not identify which check stopped the first sample. Forcewake reacquired at
24.509 ms and both park registers were restored, with the temporary alias
retired. The later diagnostic reset completed two writes, but park evidence
contained no fresh engine or L3 acknowledgements. Ubuntu matched the
1,217-byte USB `park-timing.log` to the exit log and verified the raw hashes,
checkpoint and exFAT integrity. The run is closed as an incomplete GPU trial.

Release `16baf15f52d96abe` at `af822833` kept the park and powered poll deadlines
and the original display/HDA/PCI checks. Native session
`session-0006613a3f1825c6-00` verified release ACK at 5.445 ms and finished the
independent closing guard at 6.805 ms. The first RC6 sample finished at
9.529 ms. Its register reads took 1 µs between two 1.361 ms guards. The next
entry guard finished at 10.893 ms, so the second RC6 register read was skipped.
The full park result remains incomplete, with no fresh engine or L3
acknowledgement in this interval.

Forcewake reacquisition verified its ACK and thread C0 at 14.992 ms and
finished its closing guard at 16.350 ms. The preceding run finished at
24.509 ms. All times are relative to the same point in each park window.
The driver restored both park registers and retired the temporary alias.
Ubuntu verified all 1,602 bytes of USB `park-timing.log` against the exit log,
replayed the native checkpoint and passed the offline exFAT checks. The run
is closed as an incomplete GPU trial.

Release `adac45682bb62e5f` at `84d038a6` verified the reduced guard overhead
in native session `session-0006613ab13c9770-00`. The full inherited GGTT and
fence census runs at park entry and at the independent release and
reacquisition checks. Inside the interval, each operation still rechecks the
live GPU and HDA endpoints, CPU exclusion, aliases, firmware registers, SMI
count and original panel. The window starts after the two park policy stores
and ends at 10 ms.

| Check | Time from park window start |
| --- | ---: |
| Forcewake release ACK clear | 1.500 ms |
| First RC6 sample completed | 2.949 ms |
| Second RC6 sample completed | 5.072 ms |
| Reacquisition ACK and thread C0 | 5.177 ms |
| Reacquisition closing guard completed | 6.540 ms |
| Park policy restored and temporary alias retired | 9.249 ms |

The timed interval recorded 13 live admissions and zero fallbacks. The two
RC6 reads returned the same residency count of 1,570,942 ticks. Neither
powered poll observed a device-wide traffic block, and the fresh engine mask
and L3 acknowledgement remained zero. The original display was preserved and
the later diagnostic reset completed both writes. This closes the timing
diagnostic. Native GPU ownership, submission and tear-free scanout remain open.
The larger diagnostic still used the practical timing profile. The earlier
system-wide timing targets remain unqualified.
The Air returned to Ubuntu, which matched the 1,805-byte USB `park-timing.log`
against the full exit log and verified the checkpoint, raw hashes and offline
exFAT integrity. All 19 source-bound host checks and four exact-image QEMU
cases passed before the physical trial.

Release `ca0caeabad167f12` at `98c44e49` completed the VCS1-only MAX_IDLE
diagnostic in `session-0006613b397dc42a-00`. The other four engines already
read 10 before reset. VCS1 read 64 at admission, read 10 after programming
and again before restoration, and read 64 after restoration. Restoration
finished at 9.322 ms, before the temporary alias retired at 9.427 ms and
engine preparation started at 9.428 ms. The original panel and HDA
exclusions remained in force. Shared baseline `c523e036` was inspected.
[Broadwell i915 RC6 initialization](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_rc6.c#L216-L234)

Both cold samples reported core status `0x10800303`. The RC6 counter
advanced by 1,676 ticks, from 1,543,470 to 1,545,146. The samples completed
at 2.929 and 5.072 ms, forcewake ACK and thread C0 verified at 5.210 ms,
and the closing guard finished at 6.573 ms. All timings use the unchanged
10 ms window origin. The powered polls at 7.502 and 8.751 ms still saw
GAM request and response at `0xff/0x7f`, WiDi open at `0x1f`, and zero
fresh engine and L3 acknowledgements. The diagnostic recorded RC6 entry
and safe restoration. A sustained device-side traffic block and native
GPU ownership remain unverified.

The Air returned to Ubuntu at 1% volume. Ubuntu matched the 1,897-byte
USB `park-timing.log` to the full exit log and verified the checkpoint,
raw hashes, and offline exFAT integrity. All 19 source-bound host checks
and four exact-image QEMU cases passed before this run. The result is
closed as a diagnostic, with tear-free scanout still pending.

Release `cd860d3427dd926a` at `a3bc6c09` completed the early forcewake
observation in `session-0006613bc5fb0e56-00`. The 13-DWORD packet ran from
34 to 85 µs after verified ACK/C0 and preceded the independent full closing
census. The packet and both scheduled polls saw GAM request/response
`0xff/0x7f`, WiDi open at `0x1f`, and clear engine and L3 flush
acknowledgements. VECS idle status varied between the early and scheduled
samples while its GO indication stayed open. The early packet establishes
the sampled state from 34 µs after ACK/C0. The RC6 interval and the wake
transition before that point remain outside this packet's coverage.

RC6 residency advanced by 1,678 ticks. Both cold samples completed at
2.936 and 5.072 ms, ACK/C0 at 5.210 ms, and the closing guard at 6.655 ms.
The scheduled polls began at 7.500 and 8.751 ms. VCS1 returned to 64 at
9.322 ms and the temporary alias retired by 9.424 ms before engine
preparation. These times use the unchanged 10 ms window origin. The
original panel was preserved. The Air returned to Ubuntu at 1% volume.
Ubuntu matched all 2,031 bytes of the USB timing report to the exit log
and verified the checkpoint, raw hashes, and offline exFAT integrity.
All 19 source-bound host checks and four exact-image QEMU cases passed.
The run is closed as a diagnostic. A sustained traffic block, GPU
ownership and tear-free scanout remain open. Shared baseline `93e10a44`
was inspected for this run.

PR 301 release `9850d46d65e6dad9` (source `9b6d31c8`, session
`session-0006613c9cbc8051-00`) saved its full exit log and 2,030-byte park
timing report to USB. Ubuntu verified the checkpoint, raw hashes and offline
exFAT integrity. The original Pipe A surface was sampled before PCI reopen
at frame 880, with a 6,144-byte stride and both surface registers at zero.
Both GT resets completed and PCI command readback was `7,3,3,7`. The loan
stopped at stage 3, after reopening and before the second engine queue
sample completed. The final Pipe A sample and writer return were absent.
There were zero display register stores and zero GPU page-table writes.
The run is closed as incomplete. Shared baseline `37de62ed` was inspected.

The second queue census revalidates the inherited scanout through a
temporary Pipe A mapping. The VMM rejects that second alias while the writer
pin is held. The host fixture reproduces the same stage-3 stop when it
enforces the native VMM rule. The corrected sequence samples and returns
the writer immediately after the PCI readback, then runs the unchanged
queue and scanout checks under the remaining owners. The writer deadline
and the VMM exclusivity rule are unchanged.

Physical retry `1407ff34e228d690` (source `5f45432b`, session
`session-0006613d404ab7c3-00`) completed the Pipe A lease and both
five-engine queue checks. PCI command readback was `7,3,3,7`. The Pipe A
samples were 1.334 ms apart on frame 911, with the same stride and programmed
and live surfaces. The alias returned to read-only 2.200 ms after the first
sample. The parent reset and retirement both completed, with zero display
register stores and zero GPU page-table writes.

The Air returned automatically to Ubuntu at 1% volume. The collector verified
the checkpoint, all four raw file hashes, the 2,031-byte timing report, and the
offline exFAT allocation with zero errors. Fifteen source-bound workflow
checks, four supplemental checks and four exact-image QEMU cases passed
before boot. The native park sample again showed RC6 residency and no fresh
engine or L3 acknowledgements. This run closes the Pipe A CPU register lease
diagnostic. Sustained traffic blocking, GPU ownership and tear-free scanout
remain open. Shared baseline `54632d47` was inspected for this run.

Physical release `25ba8230eafc61f4` (source `c9d74941`, session
`session-0006614506b36a3d-00`) acquired and returned the VMM guard across
reset and PCI reopening. It protected 1,356 inherited image pages and the
128-entry fetch tail at `0x8c001000–0x8c5cd000` (end exclusive), plus the
candidate backing and seven firmware command pages. Both engine queue
checks matched. PCI command readback was `7,3,3,7`, the Pipe A mapping
returned to read-only 2.181 ms after its first sample, and the original
framebuffer stayed programmed. The run recorded zero display register
stores and zero GPU page-table writes.

The full guarded reset sequence took 227.216 ms under the wider diagnostic
policy. The Air returned automatically to Ubuntu at 1% volume. The four raw
file hashes, 2,031-byte timing report, and offline exFAT allocation verified.
All 15 source-bound checks and four exact-image QEMU cases passed before
boot. This closes the inherited CPU backing guard diagnostic. The
device-side traffic block and fresh engine and L3 acknowledgements remain
unverified, so the first native translation store is still gated.

Physical RC1e release `0b8aeb4fce7c1c8d` (source `6ec3a815`, session
`session-00066145876393dc-00`) read back the software target `0x20000` with
forcewake held before GT reset. Across eight polls, ending 8.751 ms into
the 10 ms window, GAM request and response stayed at `0xff` and `0x7f`,
WiDi stayed open at `0x1f`, and the L3 acknowledgement stayed at zero.
The fresh engine acknowledgement mask was zero. Native `MISC_CTRL0` was
`0xc5280000` before the request and after rollback. The firmware lock was
set and the CPD GO-message enable bit was clear. The run restored both RC
control words and confirmed the original open state 9.257 ms after the poll
origin.

Pipe A kept the inherited framebuffer across reset and PCI reopening.
The backing guard covered 1,484 inherited pages and seven firmware command
pages. PCI command readback was `7,3,3,7`, both engine queue checks matched,
and the Pipe A mapping returned to read-only in 2.136 ms. The run recorded
zero display register stores and zero GPU page-table writes. All 15
source-bound checks and four exact-image QEMU cases passed before boot.
The Air returned automatically to Ubuntu at 1% volume. The collector
verified the USB checkpoint, all four file hashes, the 1,266-byte park
timing report and offline exFAT integrity. The run is closed as a diagnostic.
Device-wide traffic stayed open, so the first native translation store
remains gated. Shared baseline `0d764d9f` was inspected.

The shared Broadwell verifier now requires L3 to read low after GAM and WiDi
close and to pulse on a later guarded read under the same block. It brackets
the L3 register with WiDi GO reads for the mapping, submission and PCI-reset
paths. The Air park recipe uses the same verifier and keeps its existing wire
format and deadlines. This scope covers the traffic verifier and its modeled
consumers. The inherited-slot loan still needs a documented way to hold the
request while observing the acknowledgements. The locked CPD policy survives
PCI function reset, and the WiDi-only soft-reset request clears on completion.
Main `d29abcf1` was inspected and contains separate Air6,1 SSD work.

The diagnostic image built and all five local suites passed. Q35 passed 44
one-page and 33 blitter cases. Removing either the closed-low requirement or
the WiDi read immediately after L3 reproduced the corresponding regression.
The saved RC1e hardware receipt still replays with zero fresh acknowledgements.

The architecture review traced i915's startup reset, firmware framebuffer
adoption, GGTT binding and per-mapping retirement. Ubuntu's current boot on
this Air logged `simpledrm` followed by i915 (`8086:1626`) with eDP enabled at
1440 × 900. For native presentation, the next SrvrOS milestone is a boot-scoped
owner that pins the live framebuffer and qualifies a separate one-page bind
and rollback. The first display path will use CPU-rendered scanout buffers.
The seven command mappings and inherited-slot experiments remain separate
work. The review used branch `86004738` and inspected main `5cb57eab`.
[Boot ownership decision](../intel-tearfree-roadmap.md#boot-ownership-decision)

The one-page milestone covers a boot-scoped range owner, the Broadwell GGTT
bind and rollback transaction, and host/Q35 fault coverage. It uses the
original display exclusion, range allocator and publication lifetime. The
native producer must demonstrate range-specific exclusion for firmware,
engines, DMA and display readers before a write-capable Air image is armed.
The scope starts at `eb038ff0` with main `5cb57eab` reviewed.

The boot-owned one-page transaction now runs against modeled registers in
the host suite and Q35. It binds the source to the same driver, CPU root and
reset epoch throughout the trial and locks the exact interval. The fixture
uses slot 12292, outside the boot fence and the inherited-slot trial at 12291.
It checks the original, private and restored contents through the aperture.
A stale translation or a lost alias after the rollback store retains the
range, backing and display pin. The test covers 17 new cases. Across the full
suites, 282 host cases and 61 Q35 cases passed.

The Air adapter still needs to supply the source. Its integration point is
the sealed five-engine queue boundary after GT reset and remapper maintenance,
before PCI bus mastering reopens. That boundary already supplies engine and
CPU-writer exclusion. It has no established way to exclude firmware and the
display proxy for the exact page throughout the operation or to attest that
earlier device access to that page has retired. Native PTE and aperture access
must also remain pinned as the page is bound and restored. No hardware image
was armed for the modeled result.

The current work starts from `290d84ed` with main `94a65bc9` reviewed. It
covers the native handoff at the five-engine queue boundary before PCI bus
mastering reopens, using the existing GT reset, remapper, VMM and display
guards. The next qualification must distinguish exact-page retirement and
maintained firmware/proxy exclusion from read-only observations. The
original active surface stays pinned throughout.

The GT flush diagnostic now takes an exact-page preflight at this boundary.
With the engine and WiDi programming pages guarded, it records two samples
of the WIN clock request and reply, WiDi `GO`, and the original Pipe A
surface. Between them it captures both WiDi translation-valid vectors,
`WTCR` on each side, and the unchanged slot 12292 entry. It checks the boot
CPU's SMI count on both sides of the sample and retires all four temporary
register aliases before continuing. A changed PTE, display, SMI count or
unsafe alias stops the diagnostic before PCI bus mastering reopens. The
collector always reports `authority=none` for this checkpoint.

Gated WIN clocks and empty translation vectors can coexist with `GO=1`.
Intel requires the WiDi pipeline to be drained before software initiates its
TLB invalidation. The remapper and System Agent invalidations do not provide
that engine/proxy drain or an exclusion held through a store and rollback.
The native build contains no one-page writer. [Broadwell register reference,
pp. 234–237, 815–816, 1607, 1620 and 1823](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

Review release `f7f826b7bb1f815f` passed four exact-image QEMU boots using
this unit's Ubuntu shim and GRUB. The normal case saved diagnostics,
verified the exFAT files and reset. Missing workspace, missing ACPI protocol,
and missing ACPI vector refused the trial. The review image was staged
locally. Ubuntu and installed recovery `9aa8dbf6fbda8252` were left in place
with an empty GRUB one-shot entry.

Physical release `48e671599eb9867e` (source `fd722b07`, session
`session-000661470813bda0-00`) completed the pre-reopen checkpoint in
910 µs. All 19 reads and four alias retirements verified. PCI command read
back `7,3,3,7` across the full trial, and all five engine queues matched on
both sides of reopening. Slot 12292 stayed at `0x8f005001`. Pipe A stayed
on the original programmed and live surface, with the frame counter
advancing from `0x38f` at the checkpoint to `0x390` before reopening. The
boot CPU's SMI count remained 12. The run issued zero GGTT entry writes.

Both WiDi translation-valid vectors and both `WTCR` reads were zero. The
WIN clock-gate request and its acknowledgement were set on both sides of
the sample, even as the raw clock words changed from `0x30` to `0x7f`.
WiDi `GO` remained open (`0x1f`) and GAM returned `0x7f`, with both GAM
request samples at `0x7fff`. The GPU engines and L3 had zero fresh flush
acknowledgements. The completed VT-d drain and System Agent invalidation
therefore leave the on-chip drain and continuing exclusion unverified.
The display WD fuse and disabled wireless audio route still cover their
respective CPU-programmed producers under the same HDA and VMM guards.

The Air returned automatically to Ubuntu at 1% volume. Collection verified
one new session, all four raw file hashes, clean offline exFAT and recovery
`9aa8dbf6fbda8252`. This closes the read-only checkpoint as a diagnostic.
The occupied slot stays outside the writable allocator until its native
owner can prove the [drain and rollback lifetime](../intel-tearfree-roadmap.md#native-rollback-admission-after-the-air-checkpoint).
Main `dc4a48d3` was reviewed for this trial.

The follow-up starts at `baff806d` with main `dc4a48d3` reviewed. It adds
an owner qualification without a GGTT store before PCI bus mastering
reopens. The diagnostic will exercise acquisition, fresh drain witnesses,
original-slot and Pipe A preservation, and release through the existing
Broadwell traffic verifier. Local models include the Air's captured
pre-reopen state. A physical run requires a documented request provider.

The recorded GAM trace sampled `0xff` before reset and `0x7fff` after
the first reset pulse. Across 844 reads after the second pulse, from
3,034,147 through 3,172,683 µs of boot time, it still read `0x7fff`.
The client request bits stayed open in those samples, and no clear
reset-reason state was observed at the pre-reopen boundary. The
no-store fixture replays the final native request, GAM reply and WiDi
reply and refuses before issuing a request.

The same fixture exercises a provider with a fresh drain, separate
existing 10 ms acquisition and reopening budgets, identical slot and
Pipe A readbacks, and a clean return. Uncertain request or reply keeps
the interval and display exclusion pinned. All 299 host cases and 78
QEMU cases passed, including 17 new cases. The Air stays in Ubuntu at
1% volume while the native request provider is pending.

The native provider follow-up starts at `d588f83a` with main `dc4a48d3`
reviewed. It checks the real GPM policy under the original pre-reopen
owner and keeps that evidence bound to the PTE, Pipe A and WiDi sample.
The first no-store request needs a documented request source and a
clear opening state before it can run on this Air.

The pre-reopen receipt now brackets the PTE, WiDi and Pipe A reads with
two reads of `MISC_CTRL0` through the original guarded wake mapping.
A changed or unreadable value stops before PCI bus mastering reopens.
The collector reports whether firmware locked CPD GO messaging off,
and continues to replay version 1 receipts with their original scope.
The existing physical RC1e packet read `0xc5280000` before and after the
request. The same boot's post-reset trace recorded `0x7fff` for every
sample after the second reset. The published Broadwell interface assigns
GO requests to GPM and reports the engine and L3 responses separately.
The Air firmware leaves the documented CPD messaging path locked off.
The native diagnostic image built, all 44 host tests passed, and the
saved Air version 1 report replayed with identical decoded evidence.
The no-store hardware trial still requires a native source of a sustained
request. Ubuntu and the existing recovery remain installed.
[Broadwell register reference, pp. 587–592, 606–607 and 859](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf)

The provider review at `a1d7de5b` checked main `dc4a48d3`, the Broadwell
graphics initialization in coreboot and U-Boot, and the published PCODE
mailbox commands. It found no maintained GO request available to the OS
with the observed firmware lock. The [rollback plan](../intel-tearfree-roadmap.md#native-rollback-admission-after-the-air-checkpoint)
records what could unblock the occupied-slot test and the acceptance
requirements for a driver-owned interval. No new hardware run was armed.

The driver-owned range work starts at `e83f19e0` with main `dc4a48d3`
reviewed. It covers the Broadwell boot page transaction, its range claim
and retirement, and the shared host and Q35 fixture. The Air6,1 Haswell
observer in PR 567 is independent. Qualify a one-page bind and rollback
with the original plane pinned and keep the occupied-slot protocol intact.
Physical staging depends on a same-boot source of authority for the range.

The driver range now lends one page from its own pinned scratch mapping and
refuses copied, foreign, overlapping or firmware-owned claims. It keeps the
parent range and the original Pipe A surface pinned through both translation
invalidations. All 312 host transfer cases and 91 Q35 cases passed, including
13 new range cases. The Air's earlier aperture census found all 53,760
eligible entries populated. The EFI handoff supplies zero range grants, so
no native test was installed or armed. Ubuntu is running at 1% volume.
[Driver range qualification](../intel-tearfree-roadmap.md#driver-owned-range-qualification)

The follow-up starts at `502394bc` with main `dc4a48d3` reviewed. It
covers first scratch publication in the Broadwell boot page transaction,
range retention in the shared allocator, and the host and Q35 fixture.
The checkpoint is an explicitly granted vacant page, its first scratch
mapping, and a private bind and rollback while the original plane stays
live. The Air6,1 work in PR 567 is independent.

The scratch transaction passed all 8 new host and Q35 cases. They cover
first publication followed by bind and rollback, missing ownership, an
occupied PTE, stale translation, uncertain write, and changes to the
original plane or closing PTE. The complete runs passed 320 host transfer
cases and 99 Q35 cases. The saved Air EFI record still has `grants=0`, and
the native aperture census supplies no vacant, granted entry. No physical
trial was installed or scheduled. [Scratch publication and native prerequisite](../intel-tearfree-roadmap.md#driver-owned-range-qualification)

The full display takeover starts from `3e241194` with main `dc4a48d3`
reviewed. The user approved a brief boot blank. This block joins the
Broadwell eDP shutdown and recovery, the existing reset completion gate,
and the first driver-owned GGTT mapping under one display lease. Work is
limited to the recovery and page diagnostic cores and their host and Q35
fixtures. The Haswell observer in PR 567 remains independent.

The joined takeover passed 11 host and Q35 cases and the existing eDP
recovery and baseline suites. The positive case exercises shutdown, the
actual reset/drain cores against modeled hardware, fixed-mode recovery
with both CRCs, and the scratch/private/rollback sequence. It restores the
original display after a clean reset refusal. Replay of the Air's open
traffic prevents reset, and an absent or stale post-reset grant prevents
all GGTT stores. The native diagnostic image still builds for the existing
eDP profiles. The native takeover needs a real GPM request and a range
grant from the post-reset owner. The Air stayed in Ubuntu at 1% volume.
[Full takeover sequence](../intel-bdw-edp-recovery.md#full-takeover-with-a-boot-blank)

The native-hold follow-up starts at `9edfbe67` with main `dc4a48d3`
reviewed. This work checks the all-bank L3 flush policy in the Broadwell
PCI reset lifetime, including selector retirement and the existing hold
deadline. It covers the reset core and the joined eDP host/Q35 fixtures.
The Air6,1 Haswell scope in PR 567 is independent.

The reset core now verifies all eight L3 banks before requesting the
traffic hold and again before closing PCI decode. It records the fresh
five-engine mask, L3 pulse, policy checks and selector retirement. The
existing 10 ms deadline covers both sweeps. The host suite checks each
bank in both phases and injects loss of slice power and uncertain,
unposted or late selector writes. All 14 joined Q35 cases passed. The
native Air receipt `ee4cf3b2ed98c61a` establishes the earlier bank steering
and open-state policy only. The Air still needs a real maintained GPM
request and post-reset address range before a takeover can run. No
hardware test was armed. [Reset admission](../intel-bdw-edp-recovery.md#joined-pci-reset-and-panel-recovery-fixture)

The panel-recovery checkpoint starts from `5857dc46` with main
`dc4a48d3` reviewed. It extends the existing Broadwell PCI decode hold
and collector to sample slot 12292 and its neighbors before blanking,
with memory decode reopened and bus mastering off, and after native
panel recovery. Release `51b86bfdd5df5da0` (source `26e3f621`)
completed this isolated test on the Air. The panel recovered with both
original CRCs and all three occupied GGTT entries matched during the cycle.
USB integrity and automatic Ubuntu return passed. The real GPM traffic
hold and a driver-owned range after reset remain open. The independent
Haswell scope in PR 567 is unaffected.
[Native panel result](../intel-bdw-edp-recovery.md#protected-translation-evidence-across-panel-recovery)

The next takeover checkpoint starts at `555a8de5`; main `24feebdd`
and the independent Haswell PR 567 were reviewed. This block binds the
post-reset range to the completed remapper drain and restored PCI BARs,
and reserves the exact page through native-style panel recovery in the
Broadwell host and Q35 fixtures. All 16 joined cases passed, including a
competing loan and parent release during recovery, a mismatched PCI BAR,
and a failed panel CRC that retains the reservation with zero PTE stores.
Native reset and mapping require the on-device traffic provider and range
grant. Ubuntu stayed at 1% volume. [Range handoff](../intel-bdw-edp-recovery.md#joined-pci-reset-and-panel-recovery-fixture)

The traffic-owner follow-up starts at `b17d5441` with main `24feebdd`
reviewed. It covers the Broadwell PCI reset core and its host and Q35
fixtures. The same registered remapper source, result and callbacks must
remain in place from the initial traffic request through PCI reopen.
The Haswell work in PR 567 has a separate hardware lifetime.
All 20 joined cases passed on the host and in Q35. The new cases reject
a replaced I/O callback, a changed source sequence, lost drain completion,
and a changed result root. None reaches the reset store or a range grant.
A replay of the saved Air RC1e run found zero fresh engine and L3
acknowledgements in eight powered samples. The pre-reopen EFI record still
has zero range grants. Ubuntu remained at 1% volume with no trial armed.
[Traffic and remapper owner lifetime](../intel-bdw-edp-recovery.md#display-owner-block-lifetime)

The private-backing checkpoint starts at `99dbb8e2` with main `24feebdd`
and Haswell PR 567 at `1b582efe` reviewed. It applies the Air's existing
graphics DMAR reservation to the post-reset Broadwell range offer. The
current Ubuntu DMAR read matches the exact native profile hash. Its IGD
reservation covers the three physical pages sampled in slots 12291–12293
by release `51b86bfdd5df5da0`. All 22 host and Q35 takeover cases passed.
Both reserved-backing offers restored the modeled panel with zero PTE
stores. A maintained native traffic hold and the GGTT range grant remain
open. [Physical backing and DMAR result](../intel-bdw-edp-recovery.md#protected-translation-evidence-across-panel-recovery)

The separate-range traffic checkpoint starts at `efc06d12` with main
`24feebdd` and the independent Haswell requester audit `d7ecead3` reviewed.
It extends the Broadwell no-store traffic trial to an explicitly granted
vacant page. The test holds an exact loan from the driver range while
checking the fresh engine and L3 responses and through clean release.
All 327 host cases and 106 Q35 cases passed, including seven new
driver-range cases. The modeled roundtrip recorded all five engine
acknowledgements and a fresh L3 acknowledgement while the candidate stayed
vacant and the inherited plane stayed unchanged. The Air still lacks a
native request source and matching range grant. Ubuntu stayed at 1% volume.
[Separate-range traffic trial](../intel-tearfree-roadmap.md#native-rollback-admission-after-the-air-checkpoint)

The held-bind checkpoint starts at `06c3f58e` with main `24feebdd`
reviewed. It composes the Broadwell driver-range traffic test with the
existing scratch and one-page writers under the same ticket and loan.
The 331-case host matrix and 110-case Q35 matrix passed, as did the 22
joined eDP Q35 takeover cases. The new fixture covers scratch publication,
private bind and rollback, missing L3 before any write, ticket loss after
the private store, and stale translation after rollback. The checkout
still has no native provider for the maintained hold and range grant.
[Held bind fixture](../intel-tearfree-roadmap.md#native-rollback-admission-after-the-air-checkpoint)

The November 27 Ubuntu/i915 comparison recorded eight powered IDI and
GAM samples. IDI was awake and unblocked, GAM remained open, all five
engine flush acknowledgements and both L3 bits were clear, and the firmware
policy stayed locked at `0xc5280000`. Raw register data was saved in local
diagnostic evidence. The one-page hardware test remains unarmed and Ubuntu
is running at 1% volume.
[IDI register scope and comparison](../intel-tearfree-roadmap.md#native-rollback-admission-after-the-air-checkpoint)

The boot-owned range work starts at `5b34e270` with main `24feebdd`
reviewed. The first target is GGTT slot 16385, disjoint from the
inherited display fence and known firmware command reservations. Scope
is a read-only pre-reopen sample under the original native reset owner,
the collector and its fixtures. A stable zero is vacancy evidence only.

Source `1fd41a81`, stage `41b5fe53adafdcee`, samples slot 16385 twice
with bus mastering off after same-epoch memory maintenance and before PCI
reopen. It verifies read-only alias retirement, the original Pipe A surface,
and the closing SMI and owner guards within the existing phase deadline. The
45-test host suite passed, including occupied and drifting entries and guarded
refusal. Four exact-stage OVMF runs using the unit's Ubuntu shim and GRUB passed
the save/return, missing workspace, missing ACPI protocol and missing ACPI
protocol with EFI vector cases.

The physical retry at source `bb459b98`, release `22098cdc89f8d227`,
saved `session-0006614d4ff914ba-00` and returned automatically to Ubuntu.
Both reads of slot 16385 returned `0x8c001001` in a 478 µs bracket. The
same-boot backing guard identifies physical `0x8c001000` as the first page
of the active framebuffer. PCI bus mastering was off during both reads,
both MMIO aliases retired, and Pipe A's programmed and live surface matched
the original. All five engine queues matched before and after bus mastering
reopened. The diagnostic issued zero GGTT writes. Offline collection
verified all four saved files, a clean unmount and zero allocation errors.
Slot 16385 remains reserved because it aliases live backing. Native ownership
of a writable range remains pending. Ubuntu is at 1% volume with no one-shot
boot armed.
[Driver-owned range qualification](../intel-tearfree-roadmap.md#driver-owned-range-qualification)

The takeover range block starts at `3b575888`, with main `24feebdd` and
the separate Haswell work in PR 567 reviewed. Scope is the Broadwell
takeover core, its guarded one-page publisher and the paired host/Q35
fixtures. The takeover will register the exact post-reset candidate and
keep the same reservation through panel recovery, bind and rollback. The
native prerequisite remains a verified traffic retirement and a range
handoff that excludes firmware and the display proxy.

The joined takeover now registers one exact page and keeps the same loan
through dark-state vacancy, both original CRC windows, scratch publication,
private bind and rollback. It also rejects scratch or private frames inside
the inherited physical fetch extent. All 27 takeover cases passed both on
the host and in the SrvrOS Q35 guest. The saved Air framebuffer alias was
refused as an occupied entry both at admission and when injected after
panel recovery. An uncertain rollback kept the existing loan and backing
pinned. The current Air boot path supplies neither the sustained drain needed
by the takeover reset nor the exclusive post-reset grant, so no write-capable
trial was armed.
[Joined takeover and recovery](../intel-bdw-edp-recovery.md#full-takeover-with-a-boot-blank)

The next native checkpoint starts at `3f05f022` with main `24feebdd`
reviewed. It extends the qualified eDP PCI hold to census the CPU-visible
GGTT outside the inherited fence while the panel is dark and hardware
reports memory decode on and bus mastering off. Scope is the Air eDP
adapter and receipt collector. The original framebuffer and the normal
panel recovery path stay pinned. The census makes no PTE writes and
grants no ownership.

Release `96bc0d488b190eac` saved `session-0006614d9cec73fb-00` and
returned automatically. The original framebuffer verified across dark
recovery, both CRC checks matched, slot 12292 stayed unchanged, and the
offline filesystem and allocation checks passed. The census itself stopped
before its first map. Its guard still required one completed pixel sample
after the dark framebuffer comparison had recorded the second sample.
The retry starts from `b189f81a`, binds the census to that verified second
sample, and records the precise admission or survey failure without changing
the PCI, SMI or time guards. Main `24feebdd` and PR 567 were reviewed again.

Release `ffe69ac65cad6101` saved `session-0006614e8d9b8471-00` to USB
and returned automatically. The panel recovered with both CRCs matching,
slot 12292 stayed unchanged and offline filesystem and allocation checks
passed. At PCI Command `0x0402` the census mapped and retired its first
register page and stopped at alias verification before reading any entries.
The next retry masks local interrupts for each sole-root page-table walk
and restores the prior interrupt state before the census reads. It retains
the exact alias attribute checks, 500 ms budget and zero PTE writes.

Release `c8e7e1b94465f1aa` saved `session-0006614f0bd9ad6f-00` to USB
and returned automatically. Both restored CRCs and all three framebuffer
digests matched, slot 12292 stayed unchanged, and offline filesystem and
allocation checks passed. Even with local interrupts masked, the census
stopped at the first alias check after 6 µs. It retired the mapping, read
zero entries and made zero GGTT PTE writes. The log shows PID 1 started
before the census. System init stays resident, and its registered address
space conflicts with the verifier's sole-kernel-root requirement. The next
checkpoint is to defer user init only for this terminal capture and record
the VMM refusal reason or verified alias attributes in the USB receipt.
This retry touches the dedicated eDP boot recipe, alias receipt and collector.
It uses main `24feebdd`. Air6,1 PR 567 at `654ad9d9` records a separate
Haswell census and supplies no Broadwell ownership evidence.

Release `a73eb19a306f716e` completed the native read-only census in
`session-0006614fc04615c1-00`. Before user init, PCI Command `0x0402` kept
memory decode enabled and bus mastering off while the panel was dark. The
driver read all 56,832 entries in `[8704, 65536)` in 38.954 ms. Every entry
was nonzero, and 49,153 matched the first framebuffer PTE exactly. All
111 CPU mappings were verified and retired, with zero GGTT writes. The
original framebuffer digests and restored CRCs matched, slot 12292 was
unchanged, and the USB save, offline checks and return to Ubuntu passed.
A reversible PCI decode hold therefore leaves the entire surveyed aperture
populated. The next ownership test must establish an explicit handoff for a
specific occupied entry or initialize a driver-owned range after a qualified
reset. The existing occupied-entry refusal remains in effect.

The occupied-slot work starts at `6a4ebc43` with main `24feebdd` and Air6,1
PR 567 at `4b8fcf94` reviewed. It joins the Broadwell traffic, interval
and one-page writer cores for a single occupied-slot loan while Pipe A stays
live. The physical prerequisite is a native provider that can exclude firmware
and WiDi and hold fresh engine and L3 completion through both PTE stores.
The native census establishes occupancy only. The Air's earlier request
samples do not supply that provider.

The occupied-slot core now has a no-write drain trial and a two-store bind
and rollback trial using the same exact firmware loan. The host transfer
suite passed 341 cases and the Q35 suite passed 120, including ten occupied
slot cases. Both check fresh acknowledgements before writing and retention
on lost ownership or stale rollback. Open WiDi, missing L3, and missing
firmware exclusion prevent the first write. The saved Air baseline refuses
before requesting the hold. The branch leaves Ubuntu at 1% volume with
recovery `9aa8dbf6fbda8252` and no occupied-slot trial armed. The native
provider for the on-chip traffic hold is still required for a hardware boot.

The native traffic-hold review uses `c07bd825`, main `24feebdd` and the
separate Haswell PR 567 at `0220fb23`. The current collector replays the
RC1e, early RC6 wake, slot 12292 and slot 16385 USB captures. All four raw
file sets and their timing packets match. Each replay reports zero fresh
engine and L3 acknowledgements. The pre-reset observations leave the
clients open, while both pre-reopen slot samples read `GO_GAM_REQ=0x7fff`
with WiDi open. The latest sample brackets the slot with `MISC_CTRL0`
`0xc5280000`, confirming the CPD GO control is locked off in the exact
owner lifetime. The Broadwell register descriptions scope the FIFO ACK to
register traffic and the soft-reset request to a self-clearing operation.
The outstanding prerequisite is an OS-accessible or firmware-provided
request that remains held while the acknowledgement registers are readable
and the original display runs. The native occupied-slot trial stays unarmed.
[Broadwell register reference, pp. 469, 587–592, 606–610 and 859](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf)

Release `025f89dd3986e36b` at `e69ae6fa` finished the full GPU-only GGTT
census in session `session-000661507bbafc2c-00`. The scan read all 983,040
entries in `[65536, 1048576)` in 677.324 ms. Every word matched the first
framebuffer PTE. There were zero empty entries and all 1,920 read-only
aliases retired. Pipe A advanced 40 frames while the programmed and live
surface, framebuffer digest and PCI Command stayed fixed. SrvrOS made no
PTE stores or resets. Ubuntu returned automatically and collected the USB
receipt. The offline exFAT check found no leaked allocations or errors.

Together with the completed CPU-aperture census, the Air has zero empty
entries across `[8704, 1048576)` in these two captures. The next checkpoint
uses a boot-scoped driver range while preserving the inherited scanout and
its fetch tail. It needs proof of inherited engine, firmware and display
proxy retirement before qualifying one backed page under a new owner.
The occupied-slot publisher remains gated on a native traffic provider with
fresh engine and L3 acknowledgements. Main `24feebdd` and the separate
Haswell PR 567 at `91a11411` were reviewed for this checkpoint.
[Takeover contract](../intel-bdw-edp-recovery.md#full-takeover-with-a-boot-blank)

The occupied driver-range checkpoint starts at `2cbb749e` with main
`24feebdd` and Haswell PR 567 `91a11411` reviewed. Scope is the Broadwell
boot drain and one-page publisher, their host and Q35 fixtures, and the
Air pre-reopen read-only receipt for slot 16385. The new driver loan will
restore the exact inherited alias under the same traffic ticket while the
protected scanout remains pinned. Native writes require a qualified ticket
and proof of backing, firmware and proxy exclusion in the same boot.

The occupied driver loan passed all 351 host cases and 130 Q35 guest cases,
including exact rollback while the original framebuffer was pinned. Physical
release `82e4c4dc7d57534b` at `9827235e` saved four files in session
`session-00066151336e7118-00`. Both raw reads of slot 16385 were
`0x8c001001`, matching the first page of the pinned scanout. Both powered
packets recorded `GO_GAM_REQ=0x7fff`, reply `0x7f`, zero engine and L3
acknowledgements and WiDi GO still set. All six temporary read-only aliases
retired. SrvrOS issued zero PTE stores.

The closing slot check did not pass after the 17.594 ms bracket, so bus
mastering stayed inhibited and the boot owner was retained. The receipt
records no timer or SMI fault and does not identify the exact closing
subcheck. Ubuntu recovered the incomplete run and verified all four
file hashes and a clean offline exFAT check. The next diagnostic must
record each closing check separately. Native drain and write ownership
remain open.

The closing-check retry starts at `e0d6dfc4` with main `24feebdd` and
Air6,1 PR 567 `b9121084` reviewed. It covers the Broadwell slot 16385
closing checks, their USB receipt and the paired parser and host fixture.
It uses the same pinned scanout and read-only traffic observations. The
report distinguishes explicit open-client evidence from an unverified drain.

Release `d5c1ff8a3445cc4c` at `a6f88f77` saved the closing checks and
timing report in USB session `session-000661527aa07a8d-00`. The first 23
checks passed. The final phase limit failed at 18.567 ms against 10 ms, with
17.650 ms inside the driver-slot bracket. The original scanout stayed
pinned at `0x8c001000`, Pipe A advanced one frame with stable surface
registers, and both reads of slot 16385 returned `0x8c001001`. The six
temporary aliases retired. PCI bus mastering stayed inhibited and the
bootstrap owner was retained. SrvrOS wrote no GPU PTEs.

Both powered packets showed open GAM and engine requests, WiDi GO set
and zero engine or L3 flush acknowledgements. Inherited GPU traffic drain
remains unverified. Ubuntu verified a clean offline exFAT check and all
four file hashes. The first collection used the 100 ms GT budget for the
closing check. The corrected parser uses the 10 ms limit from the
validated receipt and can replay the same raw bytes without a reboot.

The timing retry starts at `04d4fb19`. Shared main `117a0294` and Air6,1
PR 567 `6c42e738` were reviewed. Scope is the Broadwell powered traffic
sampler and its receipt and host fixtures. Each powered sample uses a
single fresh wake census and rechecks request and acknowledgement before
reading the powered thread register. The retry measures each traffic packet,
the PTE pair and closing separately within the same 10 ms phase. The
Air's original scanout, PCI requester hold and read-only scope are required.

Release `651b1269f6de3d14` at `a4caecc7` passed all 24 closing checks on
the Air in USB session `session-000661537e1f2677-00`. The full phase took
7.647 ms against 10 ms, down from 18.567 ms. Inside it, each traffic
packet took 3.126 ms, the PTE reads took 384 µs and closing took 94 µs.
The four wake censuses accounted for 5.444 ms of the two traffic packets.
Both reads returned `0x8c001001`, matching the pinned framebuffer at
`0x8c001000`. The panel registers held, all six temporary aliases retired,
and PCI bus mastering was restored after the read-only checks. SrvrOS
issued zero GPU PTE writes.

Both traffic packets still showed open GAM and engine requests and WiDi
GO, with zero engine and L3 flush acknowledgements. No device-wide drain
or native write authority was established. Ubuntu collected and verified
all four USB files, and the run closed as a diagnostic with a clean offline
exFAT check. This qualifies one run under the unchanged 10 ms slot limit.
The full sequence still used the wider diagnostic timing policy.

The native traffic-hold follow-up starts at `4556ffdf` with main
`117a0294` and independent Air6,1 PR 567 `35e76b8b` reviewed. Scope is
the no-store Broadwell drain verifier for a driver interval whose exact
inherited PTE aliases the pinned framebuffer. The same reset-free hold
must exclude WiDi and competing resets, witness fresh engine and L3
acknowledgements and preserve the original PTE across release. Native
execution depends on a provider for the hardware request.

The new no-store entry point passed all 359 host cases and 138 Q35
guest cases. The inherited-slot cases cover clean release, wrong
backing, the saved open-GAM baseline, unavailable request, open WiDi,
missing L3 and two foreign-reset timings. A reset reason observed
after the fresh engine and L3 acknowledgements still retains the
ticket and framebuffer pin. All eight cases leave the original PTE
intact with zero stores.

Replaying the verified v4 Air files again produced `GO_GAM_REQ=0x7fff`,
`MSG_GO_GAM=0x7f`, WiDi `0x1f` and zero engine and L3 acknowledgements
in both packets. The Intel reference designates GPM as the request
originator. Its message fields and EventBus vectors do not specify an
OS acquire-and-release protocol, and the Air's CPD policy remains
locked with GO messaging disabled. The no-store hardware trial remains
unarmed until a native provider can keep the actual request live with
the panel running and the acknowledgement registers powered.
[Broadwell register reference, pp. 426–427, 587–592, 606–607 and 859](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf)

The firmware-interface checkpoint starts at `4b1aa8fb`. Main
`117a0294` and separate Air6,1 PR 567 `273a9b7d` were reviewed. Scope is
the Air UEFI handoff record, protected import and collector. The probe
will inventory protocols on the matched GOP and its child and on the
bound PCI and CoreEG2 driver handles. This keeps the original panel
active and records what firmware exposes without opening new interfaces
or changing the controller binding.

Release `7508b67b55c47747` returned to GRUB on the Air. The offline USB
check passed and found zero new sessions. The same release failed to reach
SrvrOS in OVMF using this Air's installed Ubuntu shim and GRUB. The repaired
GRUB entry runs the EFI probe, then chainloads the same Limine binary used by
recovery. With the Ubuntu binaries the guest recovered all 19 EFI pages,
saved the report and restarted.

Physical retry `7878ec2039051e30` reached SrvrOS and saved session
`session-000660f87d2503b3-00`. Ubuntu verified a clean unmount and zero
allocation errors. The kernel recorded `intel-efi-boot: imported=0 backing=0`.
The framebuffer snapshot was preserved. Admission stopped before GT wake or
any GPU page-table write. All seven command slots had zero observations. This
confirms the direct GRUB boot repair. The EFI handoff prerequisite failed.

Physical release `7ad8a1e15a360b58`, session `session-000660f90e1a5366-00`,
located the failure. The matching ESP receipt records probe entry and capture
state 2, step 6. The helper sampled all seven translations twice and matched
the active framebuffer. Locating the EFI ACPI table protocol then returned
`EFI_NOT_FOUND` (`0x800000000000000e`) before allocation or publication.
GRUB recorded `start-failed` and successfully booted SrvrOS. The kernel saw
17 ACPI tables and zero OEMG records. The original framebuffer was preserved,
GPU PTE writes remained zero and the offline USB check passed. The missing
protocol QEMU case reproduced this sequence.

Release `4ee4688f2965deff`, session `session-000660f99ec3d386-00`, physically
verified the EFI configuration-table handoff. GRUB returned from the helper.
SrvrOS imported all seven original PTEs at slots 11744–11750 and verified all
19 reserved pages at `0x8aced000`. One EFI driver had the PCI controller open,
with firmware file GUID `0cb4e51b-a3ca-4afe-b5b6-cb387ed1aee3`. The three
command-slot samples agreed across GT reset and PCI reopening. The diagnostic
retired all seven temporary mappings and returned the backing reservation.
The original display was preserved, GPU PTE writes and device grants stayed
at zero, and Ubuntu verified a clean USB volume. Four release-bound Q35
boots covered save/return, missing workspace, and the configuration-table
fallback with both the production helper and a seven-word test fixture.

Release `ec83b32f33ae4388` on source `f633b954`, session
`session-000660fb262080ec-00`, verified the CPU mapping loan for the blitter
registers at BAR0+0x22000 on the Air. With PCI bus mastering off and four peer
ports protected, SrvrOS acquired the loan, read HWS_PGA = 0 and
HWSTAM = 0xffffffff, and returned it. Both queue samples matched across
PCI reopening. All five register mappings were restored. SrvrOS verified
19 reserved EFI pages, retained the seven original translations at slots
11744–11750, released the temporary mappings and preserved the original
display. Ubuntu verified the saved session and clean USB return.

The four sampled windows showed idle GPU memory arbitration and empty media
queues. None showed a device-wide traffic block or a fresh flush acknowledgement.
The diagnostic issued zero GPU page-table writes and submitted zero commands.
Preparation took 207 ms and the GT sequence took 344 ms, within the diagnostic
limits but above the original timing targets.

The Air reset census and command driver now share a VMM check for the four
non-blitter submission ports. The driver checks their write protection before
each blitter descriptor store. The host fixture withheld the fourth word when
a competing port became writable after the third and retained all seven pages.
All 53 blitter cases passed on the host and in the SrvrOS Q35 guest. The
native adapter suite also rejected loss of each of the four ports on either
side of PCI bus-master reopening. The release above exercised the successful
protection and restoration path on hardware.

The host and Q35 command tests now cover loss of the device or WiDi lease on
the final clock read before submission. In both cases the driver withheld the
fourth word and retained the seven command mappings. The driver also checks
both WiDi translation-valid registers after publication and rollback. On this Air
under Ubuntu/i915, three blitter requests each verified 4,096 bytes. All six
boundary samples showed zero valid vectors and zero WTCR with WiDi GO open.
An earlier attempt stopped at the baseline because the compositor had a pending
flip. The completed comparisons preserved the active primary surface within
each run. Raw comparisons and regression logs are in `build/air-trigger-leases/`.

The Air's read-only BIOS MTD identified the PCI-bound firmware file above as
the **Broadwell EG2 Graphics Driver**. Its PE subsystem is EFI boot-service
driver (11). The private device protocol GUID on that PCI handle,
`53cfeb0d-9b9e-4d0a-90b6-024a79906a58`, also occurs in the CoreEG2 image,
file `4ddeafa4-f480-4b9d-889b-1e3ae6605bd6`. CoreEG2 contains the standard
GOP GUID as well.

On the Air, release `0aa8d40a0735e90d` at `3b9095f5` verified that
**CoreEG2 had the Broadwell driver's private protocol open**. Of eight open
records, one was BY_DRIVER and resolved to the CoreEG2 firmware file, six
were BY_CHILD_CONTROLLER, and none requested exclusive access. The probe
left both drivers attached. Session `session-000660fd04cfe476-00` confirmed
the seven inherited entries at slots 11744–11750, all 19 reserved pages,
the original framebuffer and a clean USB return to Ubuntu.

All four samples still permitted GPU traffic (mask `0x5f`). The driver
observed zero fresh engine or L3 flush acknowledgements and submitted no
command.

Release `4424637c0edb47cd` at `42a6fd03` completed the direct child check
on the Air. Session `session-000660fd9968899a-00` found six child records
and zero child handles exposing the Graphics Output Protocol (GOP). The
global GOP search still found the active 5,529,600-byte framebuffer at
`0xb0000000`. Pipe A and all seven translations were preserved across the
query, all 19 reserved pages verified, and Ubuntu found the USB volume clean.

Ubuntu identifies the connected eDP panel as `IGPU.DD1F`. On this unit the
76-byte EFI `ConOut` path passes through PCI device 2, function 0, then two
vendor messaging nodes. The path has no ACPI display-address node. Its
FNV-1a 64-bit hash including the end node is `a9167657801a6afb`.

Release `f4e57e160cd7022a` at `8ad1c753` traced the console and all six
EG2 children between two Pipe A snapshots. In physical session
`session-000660fe2599690c-00`, two GOP handles exposed the active framebuffer.
One was the system console handle and had no device path. The other's four
nodes matched the boot-time `ConOut` path byte for byte. Child 0 supplied
its first three nodes. The active path continued one vendor node beyond it.
Children 1 through 5 shared the two-node PCI prefix and diverged at the
third node. Child numbers refer to this boot's open-record order.

The original framebuffer remained active at `0xb0000000`, all seven
translations matched, all 19 reserved pages verified, and the USB volume
returned clean. All four samples still allowed GPU traffic and none showed
a fresh engine or L3 flush acknowledgement. The display handoff can now
identify the branch to preserve by matching the active GOP and console paths.

Release `57ccbd10d68e0d48` at `989ebe3f`, session
`session-000660ff0a10772f-00`, verified that selection on the Air. EFI
matched the four-node GOP path to the console and selected its unique
three-node EG2 ancestor out of six children. The kernel admitted GT preparation
with token `0xf85dfc5100000201`. Ubuntu independently reproduced the token
from the saved device paths. Pipe A kept its original framebuffer, the seven
command-slot translations matched across three observations, and all 19
reserved pages verified. The session returned with zero USB allocation errors.

The five samples taken immediately after each engine prepared all saw the
same request (`0xff`) and arbiter reply (`0x7f`) at both ends of the sample.
The permitted-client mask was `0x5f`, WiDi reported `0x1f`, and L3 reported
zero each time. Engine status changed during preparation, but none of the five
samples recorded a fresh flush acknowledgement. SrvrOS submitted no blitter
command. The next prerequisite is for the traffic owner to withdraw the GO
requests while preserving this display branch, then verify closure and fresh
engine and L3 acknowledgements before publishing command pages.

Release `9a95e69f4ea72e97` at `4b385108`, physical session
`session-000660ffcddd5548-00`, isolated the blitter's contribution. One
masked Stop Ring write changed BCS_MI_MODE from `0x200` to `0x300` on the
idle, disabled ring. It still read `0x300` immediately before the first GT
reset and returned to `0x200` after reset. Immediately after the stop and
again after each of the five engine preparations, both request reads were
`0xff`, both arbiter reads were `0x7f`, WiDi was `0x1f` and L3 was zero.
All six samples lacked a fresh engine or L3 flush acknowledgement.

Pipe A kept its original framebuffer, the seven command-slot translations
matched and the kernel verified all 19 EFI-reserved pages. The guest completed and
Ubuntu found zero USB allocation errors. Preparation took 211 ms and GT
ownership lasted 348 ms, both within this diagnostic's timing limits.
The driver can now stop the blitter parser. With global GO still asserted,
the device-wide drain and independent WiDi exclusion remain prerequisites
for replacing the seven mappings and issuing the first native command.

Release `77a7fbfaab1db2dd` at `fb582f40`, session
`session-0006610072e95c7a-00`, sampled the original Pipe A surface and
always-on GT power registers twice before forcewake acquisition and twice
after release. Both pairs had a 2 ms gap between read windows. All four
samples showed RC0 (`0x40800000`), a clear forcewake acknowledgement and
the same RC6 residency count (1,582,670 ticks). The programmed and live
surface matched EFI throughout the samples. The blitter stop verified,
all 19 EFI-reserved pages verified, and the USB allocation check passed.
All six powered observations still showed GO open and no fresh engine or
L3 flush acknowledgement.

In a separate Ubuntu/i915 comparison on this Air, 14 of 19 trials with
matching panel surfaces showed RC6 and a clear forcewake acknowledgement
about 2 ms after release. All 20 acquisitions showed RC0. SrvrOS sampled
after GT release while the display-audio owner was still held. The Linux
manual park sequence disables hardware timers before writing the RC6 target.
[i915 RC6 park and unpark](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_rc6.c#L681-L718)

Release `380e9dc159c30a3d` at `ee3b159b`, session
`session-000661023c32d543-00`, completed all eight active-panel samples.
RC6 residency stayed at 1,527,378 ticks. After forcewake release, the
measured idle interval with display audio held was 22.723 ms. After display
audio released, the intervals were 20.005 ms and 102.713 ms. Forcewake ACK
was clear at both ends of each read, and every programmed and live surface
matched the EFI panel. The blitter stop verified, all seven original
translations matched and all 19 EFI pages verified. Ubuntu collected the
completed session and the offline exFAT check found zero allocation errors.
The engine and L3 flush observations still lacked a fresh acknowledgement.

After return, the Ubuntu i915 residency counter on this Air advanced by
723, 626 and 779 ms across three successive one-second intervals. For the
next comparison, the Gen8 i915 initialization supplies concrete reference
settings for the per-engine idle limit, GT idle hysteresis, evaluation interval,
wake-rate limit and sleep register. The inherited 625-tick threshold already
matches that reference.
[Gen8 RC6 initialization](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_rc6.c#L216-L236)

The Ubuntu installer now recognizes completed EFI traces with appended GOP
paths. It validates the existing trace before copying the next release. The
installer suite and a replay of this Air's 1,014-byte trace cover the repair.

Current scope is the Broadwell blitter handoff between GGTT publication and
first submission. It depends on the original display exclusion, the seven-page
transfer, and independent WiDi exclusion when global GO reopens. The native reset
owner binds the captured wireless routes and their VMM guard to these
seven pages. The command driver now checks distinct engine and L3 flush
acknowledgements for acquisition and rollback. The driver suite includes an uncertain release after
all seven original translations were restored. Three i915 comparisons kept
Pipe A on its original surface. Each saw a gated WiDi clock and open WiDi GO
together. The Air still needs a hardware owner for the GPM request and the
independent WiDi hold. [Register evidence and test](../intel-bdw-bcs-flush.md#command-submission-while-widi-stays-excluded)

The Air7,2 flush diagnostic now starts before Limine. Its EFI loader records
the graphics controller's protocols and bound driver identities when firmware
exposes them. It captures slots 11744–11750 twice against the active GOP
framebuffer and reserves 19 pages for the command image and page tables.
SrvrOS checks the EFI record, page contents, original panel and seven mappings
before using the reservation. The backing stays reserved across reset and
PCI reopening and returns to the boot reservation on clean retirement.

The host suite passed all 23 tests. The first OVMF trial recovered all 19 pages.
The Air now has physical evidence for kernel import of the driver inventory,
raw translations and backing reservation.
The first native command still depends on a device-wide drain and
an independent WiDi hold. [EFI handoff](../intel-bdw-bcs-flush.md#efi-handoff-on-the-air72)

The blitter can promote that exact reservation when the original owner
supplies live DMA, cache and reader assurances. It then installs seven PTEs,
invalidates the GGTT and reads a distinct temporary marker through each
aperture page before allowing the first submission. On this Air under
Ubuntu/i915, all seven markers and restored words matched in three runs.
The original eDP CRTC advanced in each run. SrvrOS still needs the native
device exclusion and drain required to promote the reservation. The most
recent physical reset report recorded WiDi at `0x1f` and L3 at zero.
Hardware acceptance of the new SrvrOS path remains pending.
[Command-page handoff and qualification](../intel-bdw-bcs-flush.md#seven-page-command-handoff)

The Air driver can now initialize a cold blitter queue under its own full GT
reset. It takes the original submission port on loan, installs the private
status page, initializes both completion cursors, and posts the first command.
The owner keeps the port and mappings pinned after command retirement.
Rollback requires the next full reset on the same device, an idle queue, a
detached status page and an independent reader drain.

All 33 driver cases passed on the host and in the SrvrOS Q35 guest.
The same cases passed under the host address and undefined-behavior
sanitizers. Changing the reset epoch after the first PTE was restored
prevented the second store and held the FIFO and backing for recovery.
Raw results are in `build/air-cold-queue/`.

The last physical reset capture recorded an idle blitter with MI_MODE
`0x200`, BLT_MODE zero and execution-list status one on both sides of PCI
bus-master reopening. The new initialization and rollback still need physical
qualification. The Air6,1 and Pro11,2 paths are independent.
[Cold queue lifetime](../intel-bdw-bcs-flush.md#cold-entry-after-a-full-gt-reset)

The Broadwell blitter driver now prepares a standalone request for the Air.
The kernel mapping path takes a transferred GPU interval, installs seven
global entries, selects the request’s engine status page and posts the
context descriptor. It waits for a fresh completion for that context, checks
both result cache lines and all padding, restores the original status page
and returns the original translations. A lost completion keeps the DMA
allocation and address interval pinned.

All 18 driver cases passed on the host and in a SrvrOS Q35 guest with modeled
registers. These include exact rollback of all seven entries, refusal of a
pre-existing GPU alias, and retaining the mapping when completion is lost.
The host cases also passed with address and undefined-behavior sanitizers.
Local build, guest and hardware comparison logs are in `build/air-first-marker/`.

On the Air under Ubuntu/i915, three paired runs executed both the direct
batch and its SrvrOS ring dispatch instructions. All six verified 4,096 bytes
and advanced the original eDP CRTC. A separate read-only device comparison
captured three fresh blitter context completions. The status-buffer write
cursor advanced by two each time, including a wrap from slot 5 to slot 1.
The native submission diagnostic uses the new image builder. The new cold
entry can initialize the blitter once the device owner transfers the command interval. The original panel and slot
12291 remain under their existing owners.
[Driver and qualification procedure](../intel-bdw-bcs-flush.md#first-native-blitter-request)

The Air executed the new SrvrOS render batch under Ubuntu/i915. RCS copied
the complete 64-bit post-flush markers from BCS, both video engines and VECS
before publishing its final PIPE_CONTROL marker. Three consecutive trials
checked those four values before replacing slot 12291, after replacement and
after exact rollback. Five competing requests stayed pending at all three
checkpoints and completed after release. Slot 8704 and the programmed/live
surface were preserved. The panel advanced and all 1,024 original words
were verified on each return.

At the held checkpoints GAM request/reply read `0xff/0x7f`, WiDi read `0x1f`,
and the L3 message read zero. These results establish visibility of the
sampled cache lines with i915 owning the contexts and mappings. Cold-boot
device exclusion and native submission still precede the first SrvrOS slot
write. An initial comparator run rejected the post-release render value
because it compared against the initial data word. The corrected comparison
uses the complement of the request’s marker. Raw runs are in
`build/air-gpu-peer-markers/`.
[Private command image and hardware comparison](../intel-bdw-bcs-flush.md#private-command-pages-on-broadwell)

The shared Broadwell writer now checks the WiDi reset-flow acknowledgement
during acquisition, both changes to slot 12291, and release. All 42 Q35
owned-page cases passed, including six new reset-flow faults. On this Air under
Ubuntu/i915, five competing GPU requests stayed pending through replacement
and rollback, then all five completed. The original PTE and all 1,024 words
returned, the protected slot 8704 stayed intact, and eDP advanced.

In the separate five-engine hold trace, all 560 register sweeps saw WiDi GO
open and both reset-ack samples clear. The held interval contained 443 sweeps
with a maximum sampling gap of 35 µs. The four rollback checkpoints also had
WiDi GO open. The i915 result validates rebind under an established driver
owner. SrvrOS still needs a native device-side handoff before its first store.
Raw host logs and the Q35 output are in `build/air-widi-reset-flow/`.

Release `7aa6d9b2ca48e720` (source `937b8a24`, session
`session-000660ec70119182-00`) held all five CPU submission-register pages
read-only while PCI bus mastering reopened. Both 46-word queue snapshots
matched. Slot 12291 read back as `0x8f004001`, with zero GPU page-table writes.
The driver restored write access to all five pages and returned the reservation.
The original panel stayed active. Preparation and final return took 197.850 ms
against the 250 ms limit.

The frozen manifest expected reopened-slot receipt v2 while this image emitted
v3. A collection-only review checked the version against the staged source,
recollected the same USB session, verified the original snapshot and raw hashes,
and closed the diagnostic as `DIAGNOSED`. Both offline exFAT checks passed.
All four observations still lacked a fresh engine/L3 flush acknowledgement or
device-side WiDi exclusion. These remain prerequisites for the slot write trial.

Release `3221b5025d194aa1` (source `6782d5c0`, session
`session-000660ebf03315b7-00`) completed preparation and the final return in
191.927 ms on the Air, leaving 58.073 ms under the existing 250 ms limit.
Both runs performed 1,206 guards. After segmenting the firmware identity check,
total framebuffer/endpoint validation fell from 87.095 to 33.427 ms. CPU cache
validation took 57.185 ms, compared with 57.401 ms previously. The workflow
closed as `DIAGNOSED` with the matching exit report and a clean offline exFAT
check.

The original display stayed active. Both 46-word queue/context samples for the
five engines matched across PCI bus-master reopening. The independent slot
12291 reservation returned with its original PTE `0x8f004001` and zero PTE
writes. The CPU-side WiDi programming guard closed. Device-side WiDi exclusion
and fresh engine/L3 flush acknowledgements remain prerequisites for replacing
the entry and exercising rollback.

Release `f1d4509e69f7f50a` (source `6a0ce5be`, session
`session-000660eb6b5b1fca-00`) matched all 46 engine and context words
before and after PCI bus-master reopening to the same post-reset sample.
The slot 12291 PTE read back as `0x8f004001`. The driver released the
reservation and WiDi programming guard with zero PTE writes. All four
engine and L3 observations lacked a fresh flush acknowledgement.

The run was rejected at the final timing check after returning the GT
borrow. The clock was at 248.600 ms entering the return and 251.782 ms
at the failing check against the 250 ms preparation limit. The exit log
was collected and the offline USB integrity check passed. Across 1,206
guard calls, framebuffer/endpoint validation consumed 87.095 ms and CPU
cache validation consumed 57.401 ms. These are full-run totals.

Release `31cf9ad2837cdcb6` (source `b7ce1048`, session
`session-000660eaf9bfabb7-00`) held all three WiDi programming pages
and the slot 12291 reservation across GT reset and PCI requester reopening.
The original display remained active. After bus mastering reopened, three
reads confirmed the original PTE `0x8f004001`. SrvrOS issued zero PTE
writes and returned the reservation. The exit report and offline USB
integrity checks passed.

Video and audio producers were disabled, but the device-side WiDi window
read `0x1f` and reset flow read zero in all four samples. There was no
fresh engine acknowledgment or L3 flush acknowledgment. Replacing and
restoring this entry on hardware still requires a native submission path
that retires inherited accesses and holds device-side exclusion through rollback.
[Trial and receipt](../device-workflow.md)

On Ubuntu/i915, four trials on this Air held a blitter request for a private
GPU address. An independent render request completed, while a competing
submission for that same address waited inside the kernel. Releasing the
first request allowed the second binding to complete. Both backing pages
contained their expected results and the original eDP frame count advanced
in each run. The address belonged to the process's private page table.

SrvrOS now pins the candidate's published range through the return flip
and original-image CRC check. The Q35 fixture exercises successful return,
wrong candidate pixels with clean return, and an uncertain return that keeps
the mapping pinned. The remaining native dependency is a cold-boot owner
that can retire inherited accesses and exclude every user of slot 12291,
including WiDi, through rollback.
[Mapping lifetime and first native ownership](../intel-bdw-primary-flip.md#mapping-lifetime-and-first-native-ownership)

Release `51a95ee689defdf0` (source `88767a7e`, session
`session-000660ea03769bd3-00`) found all 53,760 eligible entries in the
CPU-visible aperture occupied. Immediately above BAR2, all 1,484 entries
starting at slot 65536 read `0x8c001001` on both passes. The independent
display-backing capture identifies physical page `0x8c001000` as the first
page of the active boot framebuffer. Writing pixels through any of those
GPU addresses would therefore alias the same live page.

The allocator left the candidate unclaimed and issued zero PTE writes.
The original Pipe A frame counter advanced from 1160 to 1165. Ubuntu
collected the matching session and the offline exFAT check passed.
Further progress on rebinding slot 12291 requires control of new GPU
requests, fresh engine and L3 completion evidence, and WiDi exclusion
through rollback. This read-only trial exercised none of those operations.

## Driver-owned one-page trial

The slot 12291 verifier now samples GAM request and response on both sides
of each acquisition poll. It credits a fresh L3 flush acknowledgement only
when all six clients are closed throughout those observations. In the Q35
guest, a pulse before WiDi closed previously admitted a PTE write. The
corrected writer refused that sequence, a pulse that stayed high through
closure, a late WiDi response and a reopening during acquisition. A new pulse
after closure allowed replacement and rollback. All 33 one-page cases passed.

On the reference Air, the live Intel OpRegion is version 2.0 with mailbox mask
`0x1f`. The DSDT's IGPU device-specific method handles the Apple `hda-gfx`
property. The eleven loaded SSDTs examined on Ubuntu contained no additional
IGPU or GFX0 scope. These interfaces provided no identified callable GAM hold.
Broadwell's WiDi TLB invalidate at `0x4278` also requires an already-drained
pipeline. The hardware slot-write trial remains pending a driver-controlled
request that can stay asserted through both invalidations.
[Intel Broadwell register reference, pp. 587–592, 859 and 1823](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf)

Three Air7,2 acquisition-only runs under Ubuntu/i915 parked and released all
five queues and verified the five scratch markers. Across 4,086 register sweeps
the GAM request/response stayed at `0xff/0x7f`. WiDi remained permitted and
no engine flush or L3 block acknowledgement was sampled. Each run observed an
advancing eDP frame counter and preserved the primary surface and both PTEs.
Maximum gaps across the three traces were 54 µs during acquisition, 67 µs
during the hold and 33 µs during release. Shorter pulses remain unresolved.

On the Air, SrvrOS release `5a6a18d4a1777ca7` (source `a4e4085e`,
session `session-000660e9697155cb-00`) completed the five-engine prepare,
hold and cancellation sequence in 93.015 ms with PCI bus mastering enabled.
All five controls went from 0 to 3, stayed at 3 for both held snapshots, then
returned to 0. The GAM request read `0xff` on both sides of every snapshot.
All six clients, including WiDi, still had permission at those observations.
Engine preparation alone therefore cannot authorize a write to slot 12291.
This run measured the GAM request and engine controls. It did not measure
L3 acknowledgements. Both display snapshots matched, the driver issued zero
reset or PTE writes, and the returned USB session passed the offline check.

On November 22 the i915 comparison queued one independent request on each of
the five engines while two lower-priority contexts held the queues. The kernel
reported priorities −1023, −1023 and 0. None of the five scratch markers or
fences completed before replacement, after replacement or after rollback of
slot 12291. All five then completed after release. All 1,024 original words
returned, slot 8704 retained its PTE, and eDP CRTC 52 advanced from 126288 to
126289. At each checkpoint GAM request/response were `0xff/0x7f`, WiDi was
`0x1f` and the L3 message was zero. Queue exclusion worked under i915 while
the power manager still allowed traffic. Two attempts with 40 MiB allocations
were rejected when i915 placed the replacement across slot 8704. The completed
priority-readback trial used 16 MiB allocations on the text VT.

The SrvrOS hardware-window command mapper now claims the same blitter submit
port as the one-page writer. Both sample the arbiter on either side of engine
and L3 checks. The Q35 guest passed 28 one-page cases, including rejection of
the permissive Air sample and retention of the private page if the block opens
during rollback. The native slot-write trial still needs a device-side owner
that closes all six clients and witnesses fresh replies with the panel active.
[One-page qualification](../intel-bdw-primary-flip.md)

Release `6c37f322e69fefa6` (source `f34f8a09`, session
`session-000660e88ddc04f8-00`) held a private 4 KiB page and the blitter
submit port through GT reset and PCI bus-master reopening. All 512 marker
words in the page at `0xc47000` passed all three checks. The proposed PTE for
slot 12291 was `0xc47001`. Both 132-entry hardware snapshots retained the
original `0x8f004001` there and `0x8e201001` at slot 8704. The primary
display registers matched the original capture. After verified reopening
the driver released both reservations, the page and the submit port.

The guest made zero PTE writes. All four GPU samples saw the client mask
at `0x5f`, with zero engine or L3 flush acknowledgements. The next physical
milestone is to demonstrate the device-side traffic block and fresh flush
acknowledgements before installing and rolling back the private translation.
The Ubuntu collector initially rejected the v4 range vector because it
counted the three independent-slot entries in the primary footprint.
Collector `a5d0343f` separates those entries and checks the full vector length.
Replay of the original USB session completed with offline integrity verified.
Preparation took 239.991 ms within the 250 ms diagnostic budget. Preparation,
GT and HDA exceeded their original timing targets.

On PR 301 the Broadwell blitter submitter and one-page GGTT writer now share
a CPU-side claim on the submit port. An incomplete descriptor keeps the port
claimed. The page writer holds it through rollback and device reopening. The
host queue tests passed under all three timing policies, and all 25 Q35
one-page scenarios passed. [Blitter submission protocol](../intel-bdw-bcs-submit.md)

On November 22 the Ubuntu i915 comparison on this Air observed blitter status
`0x00054032` and context `0x20` at all three held checkpoints. Five engine
receipts preceded the final render flush. Slot 12291 changed and returned to
its original PTE, slot 8704 stayed protected, and the original panel advanced
from frame 68718 to 68721. All five engines resumed. An earlier attempt
refused an overlapping i915 allocation before the held trial. Both attempts
returned to the original VT. For native command submission the cold-boot driver
still needs GPU-visible private ring, context, status and completion storage
and an independent device-side drain covering cache and display readers.

Release `418d18e891298e1b` (source `f72b78e2`, session
`session-000660e7c9767b13-00`) held both GGTT reservations through PCI
bus-master inhibit, five-engine preparation, GT reset, translation maintenance
and verified requester reopening. Both reservations returned. The two 132-entry
samples matched, including slot 8704 (`0x8e201001`) and slots 12290–12292
(`0x8f003001`, `0x8f004001`, `0x8f005001`). The original primary surface
passed preservation checks. The run made zero PTE writes and the USB report
passed offline integrity verification.

In all four samples the GPU reported idle with client mask `0x5f` still
allowed. The five engine flush acknowledgement bits and the L3 flush
acknowledgement stayed clear. This closes the reservation-lifetime trial.
The one-page replacement still needs a native submission owner that can
block new work and demonstrate completion through rollback. Preparation
took 240.784 ms under the 250 ms diagnostic budget. The preparation, GT
and HDA intervals exceeded their original timing budgets.

The shared scanout publisher and Broadwell one-page writer on PR 301 now
hold the address and backing RAM until the device reopens after rollback.
The Q35 guest checks slot 12291 changing before release and during reopening,
plus a timeout or a lost transfer owner during release. Each fault leaves the address quarantined and
the RAM pinned. The physical write still requires a native traffic provider.

Release `7f7c88c0fa69ae38` (source `389a992d`, session
`session-000660e6b02f39c7-00`) completed the read-only slot 12291 trial.
Both samples showed wireless video fused off and wireless audio disabled
on the powered codec `0x80862808`. Pipe A kept its primary surface and
advanced from frame 1085 to 1086. All 52 display register comparisons
matched. Slot 12291 retained `0x8f004001`, both neighbours were preserved
and slot 8704 retained `0x8e201001`. All 94 temporary mappings retired.
The saved USB session passed offline integrity checks. Firmware transfer is
an unimplemented prerequisite in this read-only image. Native replacement
and rollback still require a driver-held traffic block and fresh engine
and L3 acknowledgements. This trial made zero GPU page-table writes.

Release `163e5b4eb7ce6403` (source `2b9d307e`, session
`session-000660e6539eafa9-00`) qualified CPU-side display-reader exclusion
on the Air. The VMM held the Pipe B and C programming pages through return
of slot 12291. All 52 register comparisons matched. Pipe A kept its original
primary surface with cursor and sprite disabled and advanced from frame 1165
to 1166. The driver refused both competing slot reservations and passed all
three guard checks. Slots 12290–12292 retained `0x8f003001`, `0x8f004001`
and `0x8f005001`. Slot 8704 retained `0x8e201001`. All 84 temporary mappings
retired, and the returned exFAT volume passed offline checks. The trial made
zero GPU page-table writes. The next hardware milestone is a driver-held
traffic block with fresh engine and L3 acknowledgements through rollback.

The Air7,2 objective is to map one driver-owned GGTT page beyond both
firmware imports, keep slot 8704 reserved and Pipe A active, then restore
the exact PTE and retire its translation. The mapping core ran inside
SrvrOS under Q35 with an explicit model grant for slot 12291. It restored
both tested originals exactly, waited for readers and retained backing
after a failed rollback invalidation. The existing Air census found
nonzero entries throughout the eligible CPU-visible aperture. The cold-boot
driver still lacks a hardware write grant for any of them.
[Mapping, hardware prerequisites and QEMU reproduction](../intel-bdw-primary-flip.md#driver-owned-second-surface-on-air72)

Release `ec94f04d158f8a13` (source `34d5123f`, session
`session-000660e530bf89ae-00`) reserved and returned slot 12291 on the Air.
Both samples read the same three PTEs at slots 12290–12292,
`0x8f003001`, `0x8f004001`, `0x8f005001`. They point into graphics-reserved
RAM beyond all three measured display buffers. Their addresses fit the same
linear mapping as slot 8704, whose original entry stayed `0x8e201001`.
The allocator refused both competing claims to slot 12291. Pipe A kept its
original surface and advanced from frame 1111 to 1112. All 80 temporary
mappings retired, and the returned USB report passed offline integrity checks.
The remaining requirement for a native replacement is exclusive control of
GPU access to that address through both translation invalidations.

Release `7b0c251bb4317d66` (source `013e72a5`, session
`session-000660e59735fe0c-00`) verified the CPU mapping guard on the Air.
The same guard covered the target aperture at `0xb3003000`, its GGTT
backing at `0x8b818000`, the inherited aperture and table backing, and the
eight engine and WiDi submission ports. All three guard checks passed
through return of slot 12291. The driver verified the uncacheable policy
for register page `0xc0818000`. Slot 12291, both neighbours and slot 8704
kept their original entries. Pipe A advanced from frame 1112 to 1113.
All 80 mappings retired and the offline exFAT check passed. The guarded
Intel writer exercised replacement and rollback in QEMU. The physical run
performed zero GPU page-table writes.

The native writer now verifies the guarded GGTT backing for both slots before
requesting traffic exclusion and throughout rollback. It also waits for both
L3 message bits to settle before returning the CPU guard after release.

On November 22, three Ubuntu i915 trials replaced and restored slot 12291
on this Air. i915 rebound 40 MiB objects containing the tested page.
In each run a separate occupied interval kept slot 8704 outside the loan. The original PTE and all 1,024 data words returned. The protected
PTE matched at all seven checks and the trace recorded no binding changes at
8704 during the loan. All five engine fences completed after the rollback.
The text console kept its original primary surface while CRTC 52 advanced
66563–66564, 66618–66619 and 66674–66676. At the nine held samples the GAM
request/response read 0xff/0x7f, all five engine flush acknowledgement bits
were clear, WiDi reported 0x1f and the L3 message was zero. Ubuntu owned
submission and the GGTT throughout. The cold-boot SrvrOS writer still needs
a device owner that excludes inherited GPU readers and accounts for outstanding
cache traffic. [Reproduce the independent slot comparison](../intel-bdw-primary-flip.md#independent-slot-comparison-under-i915)

## Reset-free primary plane flip

The Air7,2 work covers native GGTT register access for the blitter
context and the device-wide traffic block needed to change slot 8704.
The qualification keeps the original Pipe A scanout active.

On November 22, 60 alternating Ubuntu trials started in RC6 with forcewake
clear and advancing residency. Acquiring i915 forcewake took the GPU to RC0
in all 30 trials. At the subsequent reads all six GAM clients were permitted,
WiDi GO was open and the five engine flush bits and L3 flush bit were clear.
Across the 30 control intervals without forcewake, 26 remained in RC6 at
both subsequent samples. Slot 8704 read the same throughout the experiment.
Ubuntu changed the panel surface in four forcewake trials; the other 26
showed the same outcome with an unchanged panel surface at all four samples.
This rules out forcewake as a way to inspect an ongoing RC6 traffic block
on this unit. The cold-state register FIFO is configured to return zeros
when blocked. The separate CPD transition has GAM GO messaging disabled
by locked firmware policy (`MISC_CTRL0=0xc5280000`).
[BCS context and firmware policy](../intel-bdw-bcs-context.md)

The rollback verifier samples the hardware request before counting replies.
It rejects a client acknowledging closure while its own request line is
still open, including the distinct blitter, VEBox and WiDi bits. An L3
pulse while every request line is open also disqualifies the transaction.
The synchronous host fixture covers each of these seven sequences and
a staggered positive handshake. An independently sustained device request
remains outstanding. This qualification leaves the SrvrOS slot write unarmed.

Three runs under Ubuntu i915 held all five queues on the Air. Both RC6
control words read `0x40000` during each hold. GAM still allowed all six
clients, all five engine flush acknowledgements were clear, and the L3
flush acknowledgement was clear. Each run verified all five scratch
and resume markers while the internal panel advanced. A separate Linux
trace recorded 731 ms of RC6 residency in each of two one-second idle
intervals. The driver called its park and unpark routines repeatedly.
These observations rule out using the RC6 target readback as a held
traffic block. MGSR acknowledges a blocked **register FIFO**. The clock
arbitration messages report exchanges between hardware units.
[Register definitions, pp. 469, 546, 587–593, 1042](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

The Air can access its global translation table while the render engine is
parked. Under Ubuntu, 330 of 600 one-millisecond intervals bracketed reads
of slot 8704 with RC6 at both ends, forcewake clear, advancing RC6 residency,
a stable PTE and the same live panel surface. In 57 intervals the engine
started in RC6 and ended outside it.

In a second experiment, a Linux process mapped 30 newly allocated private
GEM pages through the aperture. The i915 trace recorded one GGTT insertion
and one invalidation in each first-page fault. In 28 cases both power samples
showed RC6, forcewake was clear, residency advanced and the programmed and
live panel surfaces matched throughout the sampled endpoints. The median
interval in those 28 cases was 42 µs. i915 chose the locations. This gives
SrvrOS a way to observe translation-table access and render sleep together.
The native writer still needs to prevent a wake during the update and to
witness the engine and L3 handshakes. On Broadwell, i915 uses render forcewake
to read the handshake registers. [i915 register access](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/intel_uncore.c#L1229-L1234)

With the display-audio controller awake under Ubuntu runtime PM, nine
samples across three more runs returned codec `0x80862808` and wireless
audio control `0x540`. Audio output to wireless memory was disabled at
all nine boundaries. The well read zero when Ubuntu suspended the
display-audio controller, returned to `0xc0000000` on resume and read zero
again after autosuspend. WiDi GO remained open during the queue holds. The
SrvrOS submission diagnostic now verifies the powered codec around each
wireless audio control read and excludes its MMIO page during the loan.
The CPU guard now excludes the five engine submission pages, the three
WiDi programming pages and the five aperture pages at slots 8704–8708
together. The slot writer checks the submission pages before requesting
the device block and keeps checking them through rollback. Physical acceptance
still requires an independently sustained device request and fresh engine
and L3 acknowledgements on the Air.
[Wireless audio control, p. 117](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

On PR 301 the native submission diagnostic allocates 19 DMA frames and
populates a Broadwell private GPU address space. The blitter context points
at all four private page directories. Its ring selects a batch in that
address space. Three readbacks span the loan of slot 8704 and its return.
Each checks all seven proposed global bindings, the context, private tables,
commands and scratch contents. The driver frees the frames after returning
the command interval. The global context binding is still outstanding.

Three November 22 i915 runs captured the blitter ring at GGTT address
`0xfffaa000` and the engine hardware status page at `0xfffe7000` while
all five queues waited. Pipe A kept scanning surface `0x00600000` and its
frame counter advanced on all three runs. All five scratch markers and resumes
verified. In each held sample all five engine flush acknowledgements were
clear, WiDi GO was set and the L3 flush acknowledgement was clear. Receipt
v7 now checks a dedicated engine status page in addition to the context's
per-process status page.

The host fixture binds command pages 8705–8708, replaces and restores slot
8704 and retires both ranges under one device request. It checks all five
engine acknowledgements, a fresh L3 flush, WiDi exclusion and the active
panel throughout. The physical GPU still needs an owner that can issue and
sustain that request. The fixture's adjacent test interval and the native
diagnostic's firmware tail at 11744 are separate. These three captures left
slot 8704 untouched. [Mapping and rollback](../intel-command-mapping.md#one-hold-for-the-command-pages-and-slot-8704)

On November 22 the Air executed the exact private command page three times
under Ubuntu i915. Each run produced five distinct drain markers, a subsequent
render flush marker, five pending fences and five resume markers. All 512
words of each 64-bit scratch array matched. Internal CRTC 52 advanced from
frame 2886718 to 2886719, 2886719 to 2886720 and 2886720 to 2886721 in
the three holds. i915 owned the active page tables in these runs. The
SrvrOS device-wide WiDi hold and coherent L3 acknowledgement remain open.

An instrumented fourth run sampled GAM, WiDi, all five engine messages and
L3 together after the private flush and before the common semaphore release.
The five command streamers reported GO open and flush ACK clear (low six
bits `0x10`). WiDi returned `0x1f`, GAM request/response returned
`0xff/0x7f` and L3 returned zero. Private PAT entry 3 was zero in all
three snapshots. The Ubuntu compositor changed the panel surface before the
held sample. The panel continued scanning. This comparison made zero
changes to the inherited slot.
[Private command layout and hardware comparison](../intel-bdw-bcs-flush.md#private-command-pages-on-broadwell)

The native slot writer checks the live primary plane, WiDi and engine
traffic immediately after each store to slot 8704 and again after the
System Agent invalidation posts.

On November 22 the Ubuntu comparison completed a render-context switch
after fresh markers from all five engines. The programmed render indirect
pointer and length read `0xffffd002` at all three held checkpoints. i915
replaced `0x118874001` with `0x1239d7001` at slot 8704 and restored the
original PTE and all 1,024 words. Pipe A stayed on surface `0x1700000` at
each sample and advanced from frame 2752202 to 2752203. Fourteen other
attempts with the indirect-pointer check stopped at the panel guard.
The five private queues resumed after rollback. GAM still permitted all
six clients, WiDi GO remained set and the sampled L3 message was zero.

The next native dependency is ownership of GPU-visible command, scratch and
context storage and all issuers that can reach the inherited slot, including
the WiDi display proxy. The CPU image and Ubuntu completion receipts supply
no native device-wide hold. The SrvrOS slot-write trial remains unarmed.
[Render flush procedure](../intel-bdw-bcs-flush.md#device-side-queue-hold-through-slot-8704-rollback)

Release `ee4cf3b2ed98c61a` (source `05f62fd0`, session
`session-000660d7f63163aa-00`) completed the native eight-bank check.
After all five engines were prepared and before the first GT reset, the
driver read `L3SQCREG4=0x40400000` and `SCRATCH1=0x1` on every bank.
Both slices reported power around each pair of reads. Eight selector writes
covered the sweep and restoration to `0x10000000`. The driver retired both
MMIO mappings before reset. The pipeline flush policy covers noncoherent
lines only. The power-manager policy permits flushing both line types.

All four engine and L3 acknowledgement samples were clear. Clients remained
permitted during maintenance (mask `0x5f`). The driver returned slot 8704
with zero PTE writes. Ubuntu collected the matching report and the offline
exFAT check passed. The next milestone is a driver-held block on new GPU
requests with a fresh engine and L3 completion acknowledgement. The block
must remain in force through replacement and rollback of slot 8704.

SrvrOS release `653a13d92f2e78f2` (source `5f53e4a0`, session
`session-000660d723ab302b-00`) completed the slice-power diagnostic on
November 22. Both available slices reported power at the beginning and end
of all four samples around GPU engine preparation and reset (mask `0x3`).
All four engine-ack masks were zero and the L3 flush acknowledgement stayed
clear at the sampled endpoints. The maintenance interval still allowed GPU
clients (mask `0x5f`). The driver rechecked and returned the reservation
of slot 8704 with zero PTE writes. Ubuntu collected the report and the
offline exFAT check passed. The result uses the diagnostic timing limits.

In the November 22 Ubuntu comparison, i915 reported two available slices
and zero powered slices at idle. With all five private queues held, both
slices were powered before and after each of six policy reads. MCR stayed
at `0x10000000`. Slot 8704 changed from `0x6f77f001` to `0x1ee3a1001`
and back, with the 1,024 original words restored and a frame advance from
291009 to 291010. Two other attempts stopped on the panel check. These
reads cover the selected bank. The publisher now steers each of the eight
banks in its host fixture and checks live slice power around every read.

SrvrOS release `9f2a7089ffb12693` (source `1329b823`, session
`session-000660d5b2987ebf-00`) qualified a software reservation of slots
11744–11775 on the Air. These 32 pages start at GPU address `0x02de0000`,
20 pages beyond the second imported buffer’s tiled image and fetch allowance.
The driver built a private 16 KiB blitter image in CPU memory for these
addresses. It verified the image and all 32 original translations twice
during the slot 8704 loan and once after its return. All three competing
borrow requests were refused. The command interval was then returned.
The original command-interval PTE started at `0x8ede1001` and the context
descriptor was `0x102de100d`.

Both reads of slot 8704 returned `0x8e201001`. The original Pipe A frame
counter advanced from 1159 to 1160 during the range check. The diagnostic
issued zero PTE writes. Ubuntu collected the completed session and the
offline exFAT check passed. The next step is establishing device-side
submission and cache ownership before publishing the private command
backing or changing the mapping at slot 8704.

In a separate Ubuntu i915 comparison at slot 11744, one of four attempts
replaced and restored a private mapping, verified all 1,024 original words
and resumed all five queues. Three attempts stopped at the original-panel
guard. The successful Linux run used i915-owned pages and queues.

Current PR 301 work covers native command memory and exclusion of the
wireless-display requester while Pipe A keeps scanning. The display fuse,
HD-audio input path and per-bank L3 policy have separate effects on this
handoff. Three reads with the Ubuntu i915 driver found `FUSE_STRAP=0x20100042`
(WiDi video fused off) and `TRANS_WD_FUNC_CTL=0`, while GT WiDi GO remained
open. On the separate display-audio controller at 00:03.0, two GET_HVV
reads on codec 0x80862808 returned 3 (WiDi audio input disabled).
The Ubuntu comparison also observed the fuse set and GET_HVV equal to 3
at all six checks across an i915-owned replacement and rollback. All five
private queues resumed and the 1,024 original words matched. Native audio
exclusion and coherent flushing across all active L3 banks remain open.

SrvrOS release `a3b6a5ce1f663e68` (source `f63cecfd`, session
`session-000660d549554360-00`) confirmed the same video-disable fuse at
both boundaries of the slot 8704 loan. Wireless video capture stayed
disabled. Both reads of slot 8704 returned `0x8e201001`. The driver
returned the loan with zero PTE writes, and the original Pipe A frame
counter advanced from 1101 to 1102. Ubuntu collected the report and the
offline exFAT check passed.

All 32 entries in each GPU-only window starting at slots `0x10000`,
`0x80000` and `0xffe00` were nonzero. This run claimed no command pages.
The occupancy masks do not identify the backing or its owner. The later
command-interval trial above used the imported buffer’s padding. i915 uses its address allocator to distinguish
reserved ranges from holes when initializing the GGTT.
[Linux GGTT initialization](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c#L785-L849)
· [SrvrOS trial guide](../device-workflow.md).

On November 21 the Air ran the SrvrOS command encoder on five i915-owned
hardware queues. Each engine drained its preceding batch and parked. Only
then did RCS issue the final render flush and park again. i915 rebound a
private GGTT allocation covering slot 8704 and restored it before the CPU
released the five successors. In one completed run the slot contained
`0x14cd5a001`, then `0x14d123001`, then exactly `0x14cd5a001`. All 1,024
original words matched and Pipe A advanced from 90301 to 90302 on the same
surface. Two of eight final trials completed. The other six refused when
the original-plane check failed. Both hardware controls with an omitted
wait refused before replacement. [Reproduction and scope](../intel-bdw-bcs-flush.md#device-side-queue-hold-through-slot-8704-rollback)

The hardware also identified the boundary of the private-queue result.
At every held check of the completed run the sampled L3 bank had coherent
pipeline flushing disabled (`L3SQCREG4=0x40400000`). The arbiter and
WiDi still allowed traffic (`GO_GAM_REQ=0xff`, `MSG_IDLE_WIN=0x1f`).
The SrvrOS writer for the inherited entry still needs a native submission
owner, a full coherent/noncoherent cache drain, and exclusion of the
independent display proxy. Original Pipe A scanout must continue across
the entire loan. The native write trial remains unarmed.

PR 301 adds a System Agent GGTT invalidation after each PTE store and
readback in the one-page publisher. Replacement and rollback use the same
traffic hold. The 193 host transfer cases cover missing, uncertain, late and
unposted requests. The native traffic provider is still outstanding.

On Ubuntu, i915 bound a 40 MiB object at `0x1700000`. The probe's bind of
that object called `gen8_ggtt_insert_entries` then `gen8_ggtt_invalidate`.
At slot 8704 it verified blitter completion and restored all 1,024 words.
The matching GGTT unbind followed verification. The eDP panel stayed enabled.
The register trace did not report an individual MMIO write.

Release `2bb7163ce847d9f5` (source `22163267`, session
`session-000660d49023a911-00`) excluded CPU mappings of seven register
pages throughout the slot 8704 loan and return. These cover the five command
engines, the WiDi queue and the wireless-display capture registers. The
capture surface, tail configuration and status read `0`, `0x10` and `0`
at both ends of the loan. Pipe A advanced from frame 1083 to 1084 on the
original framebuffer. Both PTE reads returned `0x8e201001`, and all three
competing claims were refused. All 78 temporary mappings retired. Ubuntu
collected the matching report and the offline exFAT check passed.

The trial issued zero GPU page-table writes. Before replacing slot 8704,
the native display driver still needs to hold off new device-side requests
and observe fresh engine and L3 flush acknowledgements under that hold.
The capture register samples alone cannot establish that drain.

Release `d407fda266aee3db` (source `eafdd4d3`, session
`session-000660d42be2ac76-00`) kept all five command-streamer register
pages and the WiDi queue page unmapped in the boot CPU address space
through the slot 8704 loan and return. The guard closed cleanly. Both
reads of slot 8704 returned `0x8e201001`, and the allocator refused all
three competing claims. The original Pipe A frame counter advanced from
1083 to 1084. All 76 temporary mappings retired and the offline exFAT
check passed after Ubuntu collected the report. The trial issued zero GPU
page-table writes. Device-side submission, including the display proxy,
and outstanding engine and cache traffic still need an owner before the
first replacement.

Release `a773b6292dc27551` (source `71af1064`, session
`session-000660d3b60bcb64-00`) imported both 6 MiB firmware buffers into
the SrvrOS GPU address allocator. The allocator lent slot 8704 to the driver
and accepted its return. It refused a competing claim before, during and
after the loan. Both reads of the slot returned `0x8e201001`.

Pipe A stayed on the inherited framebuffer and advanced from frame 1151
to 1152. The trial checked 1,484 candidate entries, completed 42 closing
reader checks and retired all 76 temporary CPU mappings. Ubuntu collected
the matching report and the offline exFAT integrity check passed.

The trial issued zero GPU page-table writes. The remaining prerequisite
for replacing the slot is a native graphics owner that can block new GPU
and display-proxy requests to it, witness outstanding engine and L3 work
finish, and hold that exclusion through rollback while Pipe A keeps scanning.

Release `5d1fa7400ba472cc` (source `bccf2409`, session
`session-000660d30f73afdb-00`) completed the independent-range diagnostic.
All 53,760 entries in slots 11,776 through 65,535 (46–256 MiB) contained
nonzero values. The search thus exhausted the tested aperture with zero
private pages claimed. The receipt counts nonzero entries without identifying
their target pages or owners.

The original Pipe A frame counter advanced from 1165 to 1168. The diagnostic
rechecked all 1,484 entries of the older candidate at slot 8704 and released
its software reservation. All 179 temporary CPU mappings retired, all 42
closing reader observations matched, GPU writes were zero, and the offline
exFAT integrity check passed after the return to Ubuntu.

For comparison, Ubuntu i915 on this unit reported the console framebuffer
pinned at GGTT offset zero. Two 6 MiB desktop surfaces appeared at offsets
`0x01080000` and `0x01680000`, inside the 34 MiB fence SrvrOS currently
preserves. These are allocations under Linux's ownership. The next PR301
milestone is to establish the SrvrOS graphics address-space owner, keep the
inherited display and its dependencies pinned, then bind and restore a single
private page under that owner. [Implementation sequence](../intel-bdw-primary-flip.md#driver-owned-second-surface-on-air72)

Release `e6fb694136606470` (source `9771e874`, session
`000660d2bf57904a-00`) returned to Ubuntu with intact USB evidence.
The CPU guard closed, all 64 temporary mappings retired, and GPU writes
were zero. The first frame counter was 1100. The probe refused before
checking the aperture page or scanning any private entries. On this path
the aperture-guard query requires IRQs masked and its caller did not mask
them. The retry fixes the caller, models the IRQ contract locally, and
records a distinct refusal if the aperture page is still unprotected.

The version 4 context diagnostic checks the CPU path to the wireless-display
(WiDi) queue tail registers while inspecting slot 8704. It requires their
register page to be absent at both held boundaries. Local qualification also
injects a premature block acknowledgement on each of the five command engines.
The display proxy can write the WiDi queues independently of the CPU. The
physical result below used version 3. [Diagnostic coverage](../intel-bdw-bcs-context.md#widi-submission-coverage)

PR301 also covers per-bank L3 policy in the slot writer and
per-engine completion packets shared with the Ubuntu hardware reference.
It depends on exclusive native admission for any GGTT publication.

The bootstrap task on PR301 is to keep the private blitter ring,
context and completion address independent of slot 8704. It touches the
isolated context-image driver, the shared GPU interval registry as a caller,
and the corresponding collector. The original panel remains on its inherited
surface. Native command submission depends on a separate hardware grant.

Release `9a8fbc4f973644df` (source `fa41b54e`, session
`session-000660c464c6780d-00`) verified the CPU submission block on this
Air after the GT reset. The driver made all five command-streamer register
pages read-only, completed six alias walks and restored all five mappings
after 30 GGTT reads covering slots 8704–8708. Both samples returned
`GO_GAM_REQ=0x00007fff`, with all six client-permission bits set
(`0x005f`). The two video engines each reported zero outstanding reference
and prefetch reads at both samples. The display register samples matched,
the driver issued zero GPU page-table writes, and the return and offline
USB integrity checks passed.

That run physically verified CPU write exclusion on the five command-streamer
pages. The later slot-loan trial above also covered the WiDi CPU mapping.
The post-reset version 4 context trial has separate acceptance.
The next native ownership step is to acquire and hold a GPU-side traffic block with
Pipe A active, witness fresh engine and L3 acknowledgements, and keep that
block through replacement and rollback of slot 8704.

The one-page writer on PR301 now samples an open arbiter before asking
for a block, then witnesses the five engine flush acknowledgements and a
fresh L3 flush acknowledgement under that request. It checks the cache
flush policy on all four banks in each of the two enabled slices, keeps the same block through both
stores and translation retirement, and verifies that requests reopen
before returning the register and display exclusion. Host tests cover a
delayed fifth engine, a lost block during rollback, a pending plane flip
and an uncertain release. Both programmed and live plane addresses must
match the original enabled plane through the final unblock.

Ubuntu's i915 topology query on this unit returned slice mask `0x03`
with eDP enabled before and after the query. The SrvrOS hardware test still
needs a driver operation that acquires and holds the block with Pipe A
active. The latest physical packet still permits all six clients. The
one-page replacement and rollback have only run in the host fixture.

On this Air under Ubuntu, one private i915 context submitted two batches
to each of RCS, BCS, VCS0, VCS1 and VECS. The first batch on each engine
flushed and wrote a distinct completion marker. One unsignalled fence
held all five successor batches while the CPU checked the five markers.
After release all five successors wrote their own markers. The eDP
connector was enabled before and after. This provides a hardware reference
for explicit per-engine barriers once SrvrOS owns submission. It covers
the private queues only. Device-wide admission and L3 coverage under
the same owner remain open.

Three subsequent Ubuntu trials executed the SrvrOS private-address barrier
encoder on all five engines. The CPU observed an earlier GPU write on a
separate cache line when each fresh post-sync marker arrived. The five
successors still had zero payloads, zero markers and pending fences as the
internal panel advanced a frame on CRTC 52. Releasing the gate let each
successor write and signal. The result covers 30 private submissions.
Two on-device negative controls also failed as expected. Omitting the
successor dependency exposed early execution, and changing the payload
exposed an incorrect earlier write despite a valid post-sync marker.

Separately, with i915 holding forcewake, the Air read `GO_GAM_REQ=0xff`,
`MSG_GO_GAM=0x7f`, `MSG_IDLE_WIN=0x1f`, L3 message `0`, and
`FUSE2=0x46000280`. WiDi was idle with GO acknowledged open, and both
slices were enabled. The reset-free writer now requires WiDi to
acknowledge closure and rejects a new reset reason before either PTE
store. The physical SrvrOS slot trial is pending a native request owner
and verified per-bank policy access. [Engine trial](../intel-bdw-bcs-flush.md#five-engine-private-queue-test)

Current Air7,2 work on PR301 covers exclusive access to the first candidate
GGTT slot during publication and rollback, with the inherited panel scanning
out. The VMM and Intel scanout adapter share this scope. The CPU aperture
address and original stolen RAM are distinct paths to the same slot. Native
GPU admission also depends on the firmware, engine, DMA, cache and reader
handoff. The aperture diagnostic holds the candidate CPU address across two
advancing primary-A frame counts and checks the inherited surface addresses.
It saves the aperture address and both frame counts. A live firmware/GT
range transfer is still required before publishing or restoring a PTE.
Current implementation work binds the aperture exclusion to the same one-page
publication through restoration, translation completion and reader retirement.
This depends on the VMM guard and the Broadwell page-store adapter.
On PR301 the GT range and flush trials hold the same reservation object the
one-page publisher can promote. The display exclusion stays pinned across
promotion. Both page-table writes require a single continuously held traffic
block and fresh closing checks. This path has passed host qualification.

On this Air, the GGTT table page containing slot 8704 (`0x8b811000`) and
its inherited backing page (`0x8e201000`) both fall in the EFI runtime
descriptor `[0x8b000000, 0x90000000)`, type `EfiReservedMemoryType`.
This classifies the physical memory. Authority to reassign the GPU address
must come from the graphics owner. The captured IGPU methods and OpRegion
driver-ready flags provide no identified per-slot loan. [UEFI memory map](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html#memory-allocation-services) · [i915 OpRegion driver notification](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/intel_opregion.c#L1109-L1144)

Replaying the four native traffic packets from release `78276fdd559a38cd`
through the page writer's closing check refused each at the render engine
(`0x8000`). Media counters were empty, all six request permissions were
open, and the engine and L3 flush acknowledgements were absent. The next
physical write needs a native graphics owner that can transfer this slot
with the boot scanout pinned and keep new requests blocked through both
writes, translation invalidation and reader retirement. The automated
replacement and rollback are pending.

On November 21 the native range adapter began excluding the physical GGTT
table page along with the CPU aperture. The writer pins the only writable
MMIO alias for slot 8704. Its original traffic block and alias must survive
the final reader-retirement callback before memory can be reused.

Under Ubuntu's i915 on this unit, a 40 MiB private GEM object occupied GGTT
`[0x01200000, 0x03a00000)`. Its page at offset `0x01000000` was slot
8704. A private blitter request wrote data and a post-sync marker on that
page. After GEM wait, both the independent GEM read and the aperture view
matched. All 1,024 words also matched after a CPU write and restoration.
The i915 trace showed a single binding through verification, followed by
the matching unbind. The internal panel was enabled before and after.
This verifies the exact address and one blitter request under Linux. Native
SrvrOS still needs a device-wide drain with the original panel active.

The independent CPU context layout reserves slots 8705–8708 and rechecks
them along with 8704 under the post-reset arena hold. Saved native windows
from November 17 and 20 both put these five pages at physical addresses
`0x8e201000` through `0x8e205000`. Release `9a8fbc4f973644df` verified the reservation and
held reads on this Air. Under Ubuntu on November 21, i915 submitted a private
batch to each physical engine (render, copy, two video engines, and video
enhance). All five individual sync-file fences signaled success, with the
panel enabled before and after. Those batches used per-process addresses
from `0x04000000` to `0x04040000`. Their completion does not cover work left
by firmware at SrvrOS boot. The remaining prerequisite is an independent
native graphics-address-space takeover that can install the snoopable
command backing with inherited submission excluded.

Broadwell's render `MI_MODE` programming notes rule out using STOP_RING
as a reversible pause during an inherited batch. Resuming can use the
wrong memory access type. The next driver step is to retire inherited
submission and bring up an owned command context for explicit completion
fences. A completion on one engine still leaves the other engines, caches
and GPU readers to account for. [Intel Broadwell register reference, p. 1322](https://cdrdv2-public.intel.com/690789/intel-gfx-prm-osrc-bdw-vol-02c-commandreference-registers.pdf#page=1364)

Release `f3961550b358c858` (source `7e3af989`, session
`session-000660c0d078f7b5-00`) completed the one-page reservation trial.
The GT requester held slot 8704 across the GT-only reset, rechecked it while
PCI bus mastering was disabled, and returned it before bus mastering resumed.
All 129 sampled translations matched, including the slot's original
`0x8e201001` entry. The programmed display state was preserved, the trial
issued zero GPU page-table writes, and offline USB integrity passed.
Preparation took 221.213 ms under the 250 ms diagnostic limit. The original
preparation, GT and HDA timing limits remain unqualified. The next mapping
step requires a live handoff from the firmware owner and proof that outstanding
GPU accesses have drained before replacing and restoring the entry.

Release `88b98be2420eb2dc` (source `0eaf94b3`, session
`session-000660c05d0472e9-00`) completed the aperture diagnostic on this Air.
The CPU guard excluded BAR2 page `0xb2200000` for slot 8704 and closed
cleanly after 66.552 ms. Pipe A advanced from frame 1095 to 1096 on the
inherited surface. All 1,484 candidate translations matched and the
reservation returned. The USB integrity and Ubuntu recovery checks passed.
The first unmet condition was `firmware-transfer`. The GPU page table
received zero writes. Single-page replacement and rollback still await
a live device owner.

The range diagnostic now registers the entire inherited 34 MiB fence for the
rest of the boot. Both kernel handoff APIs reject a conflicting framebuffer
description even after the candidate lease closes. The one-page publisher
checks this independent registration before preparing or writing the mapping.
The version 11 receipt records the number of protected pages. GPU ownership
of the second range and a physical page flip remain pending.

Source `aef29e75` passed 65 transfer cases and the 336-case native adapter
fixture under fatal ASan/UBSan. Release `b05a2ddf2ec897ad` booted in QEMU,
recorded zero protected pages and zero writes on the unsupported GPU, saved
the report to exFAT and reset. The same image halted on missing workspace.

PR301 on main baseline `4016abf0` now exercises a synchronous flip to a second
surface and restores the original using the same display owner. The local
fixture runs the plane and primary CRC cores together at the Air's captured
geometry. It checks a distinct alternate CRC and exact restoration, including
rollback after an incorrect alternate image. A timed-out or uncertain return
retains both pins. Neither leg requires a PCI reset.

The physical flip trial remains pending. The native capture on release
`42cdc13b60340ddd` found both 6 MiB candidate spans firmware-reserved. The
OS-allocated alternative needs an owned GPU address interval and visible
X-tiled backing before the native adapter can arm the first flip.
[Trial contract and local coverage](../intel-bdw-primary-flip.md#reset-free-hardware-trial)

PR301 now takes OS-allocated X-tiled backing through Broadwell GGTT encoding,
publication, a synchronous flip, rollback and mapping retirement in the local
fixture. At 6144 bytes per row the padded image occupies 1,356 pages. The
128-page translation tail brings the lease to 1,484 pages. A one-pixel fault
in the back buffer fails the image check and the original still returns.

The earlier physical session `session-000660ba79e6f52e-00` records firmware
framebuffer memory at `0xb0000000` with length 5,529,600 bytes (1,350 pages).
That is exactly one 1440 × 900 image at this pitch. The same fields match
in all 128 saved exit logs containing this endpoint in the latest collection.
The native source transfer currently describes one GGTT entry and supplies
zero ownership assurances. A native test needs a live transfer covering the
whole second surface, plus the ordered publication and display adapters.
[Private scanout mapping](../intel-bdw-primary-flip.md#private-backing-in-a-transferred-interval)

PR301 now checks the second surface’s exact 1,484 original GGTT entries
under the native display and CPU mapping guards, bracketed by two reads of
all 32 hardware fences. Local coverage includes a changed last padding entry
and a changed last fence on return. The shared transfer admission still
requires five hardware assurances. The source offers none, so the
reset-free hardware flip remains pending.
[Native range observation](../intel-bdw-primary-flip.md#native-reset-free-range-observation)

The exact staged diagnostic from `748f615f`, release `a6262aa628bf2e85`,
booted in QEMU. With the workspace attached it saved its unsupported-GPU
result to exFAT and reset. The missing-workspace run halted. On the Air the
cooperative EFI lease covers the boot GT requester. It does not transfer
these mappings. The next implementation needs a native graphics owner that
can reserve them across publication, both flips and restoration, or receive
that reservation from a preceding owner. The physical flip is pending.


The native range adapter now reserves all 1,484 candidate translations across
both kernel handoff APIs before it checks them against hardware. Version 10
records the reservation generation and clean return. A conflicting reservation
cannot enter while a flip, rollback or reader retirement holds the pages. The
native source still needs control of the firmware mappings, GPU traffic, caches
and display readers before it can promote this reservation and publish a second
image.
[Reservation lifetime](../scanout-transfer.md#reserving-the-second-framebuffer-interval)

The exact staged kernel from `5134dc1e`, release `c75840e6674be3a6`, passed
both QEMU boot paths. With the USB workspace it saved a version 10
unsupported-device receipt and reset. Without the workspace it displayed
the expected halt. The QEMU receipt records zero reservations and writes.
The full 1,484-page flip, rollback, competing-owner and delayed-reader cases
run in the host fixture. A physical flip remains pending.

The Broadwell one-page publisher now exercises its aligned 64-bit store and
exact restoration in the host fixture at GGTT index 8,704. A simulated stale
device translation after a successful store keeps the range and RAM pinned.
The fixture restores both the captured Air entry `0x8e201001` and an
original with a nonzero upper word. All 63 transfer cases passed under fatal
ASan/UBSan. The write primitive
compiled with the freestanding kernel flags and is absent from the ordinary
kernel and read-only range diagnostic.

On this Air under Ubuntu kernel `7.0.0-43-generic`, i915 bound one 4 KiB
page at GPU offset `0x554000`. All 1,024 words matched an independent GEM
read after both changing and restoring them. The trace recorded the matching
unbind. This confirms the physical aperture works under i915 ownership.
SrvrOS still needs a native owner to exclude prior GPU users and maintain
completed translation invalidation and reader retirement. The next native
trial publishes and restores one entry with the original display pinned.
The 1,484-page flip follows that qualification.
[One-page implementation and Ubuntu reference](../scanout-transfer.md#broadwell-one-page-checkpoint)

## Native display recovery after PCI decode reopens

Release `40de8de2891163f4` (source `7e227182`) passed all 47 checks on this
Air. With bus mastering inhibited and BIOS keeping the well powered, the
driver released and reacquired its display power request. The complete PCI
hold took 197.912 ms. All 5,529,600 framebuffer bytes matched and the restored
primary produced the reference CRC in both windows. The USB return passed
offline verification. The run is closed.

The next prerequisite for a PCI function reset is proving completion of
outstanding GPU traffic in the same ownership lifetime. The native trial
exercised the BIOS-powered well path. Cold-well recovery, post-reset display
restoration and native tear-free scanout remain pending.
[Hardware evidence](../intel-bdw-edp-recovery.md#native-resource-recovery-under-inhibited-pci-bus-mastering)

PR301 now joins the graphics VT-d read/write drain to the PCI function reset
in the host recovery fixture on main `4016abf0`. The fixture checks loss of
requester ownership, a missing drain acknowledgement, changes in the PCI
configuration and failure to retire the remapper mapping. The native Air
still needs the same dark-display owner to control the remapper and account
for remaining GPU pipeline, cache and prefetch work. The Wi-Fi and WebCore
PRs have separate scopes.

Release `8e5b856650ecf306` (source `240f0c8d`) completed the GT3 media
counter diagnostic on this Air. All four reference and prefetch counters
read zero at each of three checkpoints. The arbiter continued to allow
requests and the engine and L3 flush acknowledgements stayed clear.
The USB evidence and Ubuntu recovery checks passed. The next reset milestone
is to exclude new GPU requests and verify outstanding writes and cache
completion while the same owner holds the display off.
[Media counter measurements](../intel-completion-prerequisites.md#macbookair72-media-counter-result)

PR301's host recovery fixture now exercises the request to block GPU
memory traffic before PCI memory decode closes. Loss of an engine or L3
acknowledgement, a busy video queue or a reopened arbiter blocks reset.
A physical trial needs a native provider that can initiate and maintain
the power manager's request under the dark-display hold.
[Reset ordering and dependencies](../intel-bdw-edp-recovery.md#joined-pci-reset-and-panel-recovery-fixture)

As an Ubuntu reference, i915 reported RC6 enabled on this unit's
`8086:1626`. Its GT0 residency counter advanced 679 ms over a 1.000 s
interval. Upstream i915 has a software park operation for an initialized
RC6 context. A candidate SrvrOS provider must account for that context,
the GT wake held to read the acknowledgements and whether the block remains
observable before PCI decode closes. Residency alone cannot establish any
of those conditions.
[i915 RC6 park](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_rc6.c#L643-L668)

A second Ubuntu comparison measured 764 ms of RC6 residency over one
second before opening i915's forcewake handle, 0 ms while the handle was
held, and 753 ms in the second after release. Each interval followed a
200 ms settling period. This motivates sampling with the guest's GT wake
released.

Release `c49a10f84b3db492` (source `efcf3ff7`) completed the physical
dark-display PCI hold. With bus mastering inhibited, both samples showed
RC0 and a clear forcewake acknowledgement. The RC6 residency counter
stayed at 1,594,584 ticks across the 2.004 ms between sample starts. All
47 reopen checks passed, the framebuffer bytes matched, and both restored
frames produced the reference CRC. Offline USB verification passed.

Release `9eb54abb15203e80` (source `4888fb6d`) confirmed that the inherited
policy already enables RC6, selects timeout mode and targets RC6 with an
800 µs promotion timer. The controls matched before engine preparation,
after preparation and after GT reset. All six memory clients retained
permission in those snapshots. After GT reset the power manager also reported
reset reasons `0x7f00`. A host replay of that sample stopped before requesting
a fresh block. The physical diagnostic closed with verified USB evidence.
[Native samples and reset-guard replay](../intel-completion-prerequisites.md#air72-inherited-rc6-controls-and-reset-reasons)

Release `78276fdd559a38cd` (source `814eb242`) traced the request through
both GT reset pulses. The seven reason bits first appeared across pulse one,
within a 724 µs interval, and were still set at the last read 171.4 ms later.
All six client permission bits were set in every one of 1,077 guarded reads.
The four engine/cache packets showed no flush acknowledgement. The 83.615 ms
maintenance interval therefore ran with sampled permissions open. The result
closed as `DIAGNOSED` after USB integrity and Ubuntu recovery verification.
[Physical trace and coverage](../intel-completion-prerequisites.md#air72-two-pulse-request-trace)

The next dependency is a native mechanism for the display owner to request
and hold a traffic block across the drain. To locate any later reason-bit
clearing, a follow-up observation needs to span wake release with a documented
register-access path and an independently held display/requester guard.
[Dark-display samples and recovery](../intel-bdw-edp-recovery.md#gt-power-state-during-the-dark-pci-hold)

PR301 now binds the modeled block to the display owner's original reset
attempt. An observed client reopening or loss of an engine acknowledgement
invalidates the transaction before decode closure. On this Air, an Ubuntu
i915 trace recorded 60 park calls before obtaining the forcewake handle,
zero during the hold and 49 after release. The engine and L3 message registers
used for verification are in the render-forcewake range. The native PCI reset
trial still needs an independently observable, maintained block.
[Owner checks and Ubuntu trace](../intel-bdw-edp-recovery.md#display-owner-block-lifetime)

## Native eDP recovery scope

PR301 owns the Broadwell port-A AUX transport and fixed-mode eDP link
training in `kernel/diagnostics`, with host tests in `tools`. The first
checkpoint is bounded AUX traffic and link-training state transitions using
this unit's Ubuntu display reference. Native use depends on verified panel
power and clock sequencing, an owned display route, and a known-good scanout
backing. GGTT writes and reset-based takeover remain downstream. The base is
main `4016abf0`. The open Pro11,2 Wi-Fi and WebCore PRs have separate scopes.

The [eDP recovery core](../intel-bdw-edp-recovery.md) now uses the single
enhanced-framing 2.7 Gb/s lane and panel power delays observed for this
unit's 1440×900 mode through Ubuntu i915. The host fixture covers dark panel power-up,
AUX link training, live-primary verification and backlight admission. The
native clock, PHY, primary restore and recovery provider is the next
checkpoint. There is no new release or physical acceptance.

PR301's next checkpoint is the source-side DDI A and pipe A restore in
`kernel/diagnostics`, using Intel's ordering and the Air7,2 reference mode.
It joins the AUX/PPS sequence to actual source register programming and live
primary readback in a host fixture. Native use requires a separate authority
for the powered display wells, restored CDCLK/LCPLL, and the original
framebuffer translation lifetime. Main is `4016abf0`. The open Wi-Fi and
WebCore work has no shared display-driver changes.

The source adapter joins LCPLL 1350 selection and PHY replay to the
1440×900 timing, M/N, watermark, and original tiled primary programming.
The same register sequence runs through PPS and AUX in the host fixture.
Backlight permission requires advancing frames, a clean underrun latch,
and independent fetch evidence. The native reset and restore adapter
still needs to supply that evidence after reinitializing the display.

PR301 now owns the inherited-route qualification for pre-reset DDI PHY,
the original framebuffer pin and its primary-plane CRC. Work is limited to
the Broadwell display diagnostic, its host/native checks and the boot recipe
that carries them. The first trial keeps the inherited display running and
leaves PCI reset and translation writes out of the sequence. It requires an
exclusive CPU display lease, verified primary mappings, and independent
CRC completion and retirement. Main remains `4016abf0`; Wi-Fi and WebCore
have separate scopes.

The first native trial, release `752c58ee0fb72b7e`, captured all nine DDI A
PHY entries and verified the 1,356 visible PTEs and 128-entry fetch tail
around the CRC window. The first window accepted zero CRC completions and
restored its control state. It saved logs, unmounted, and returned to
Ubuntu automatically. The retry unmasks only CRC-done and records the
raw poll status. The reason for the original CRC failure remains unconfirmed.

The retry, release `5d36f1891358bf06`, closed as
`EDP_PHY_CRC_BASELINE_COMPLETE`. Both windows preserved all nine PHY
entries and all 1,484 backing translations. Each produced four fresh
completions, and the stable CRC changed with the console content. The
probe restored the CRC control and interrupt mask, saved its report, and
returned to Ubuntu. The next step is to reproduce a known frame during
native display restoration and verify its CRC across reset.
[Pre-reset trial result](../intel-bdw-edp-recovery.md#physical-pre-reset-result).

Release `43785d3ecb432ae1` physically qualified the fixed A–B–A reference
sequence and passed Ubuntu return verification. The restore adapter now
requires two fresh CRC windows matching the reference before enabling
the backlight. Host fixtures cover missing and mismatched CRCs,
backing changes and uncertain retirement. On main `4016abf0`, the open
Pro11,2 Wi-Fi and WebCore PRs have separate scopes. The remaining native
checkpoint is continuous ownership of powered MMIO, clocks and the
original framebuffer across reset.

Release `d68c99c294da7d5c` physically verified the Broadwell driver
well request and release around the final reference CRC in 94.452 ms.
The firmware request remained asserted, and the pinned backing, clock,
panel sequencer and PWM state held steady. USB and Ubuntu return checks
passed. The native restore still needs to carry the backing pin across
reset and reestablish the sampled clock, panel sequencer and PWM state.
[Display-well trial result](../intel-bdw-edp-recovery.md#display-well-reservation-trial).

The PR301 host fixture now connects ordered display disable to the fixed-mode
restore adapter. It keeps PCI and LCPLL running and proves the original
backing again with the eDP pipe off. The isolated native preflight exercises
the route-independent backing census and records the live shutdown
prerequisites. Main remains `4016abf0`, with separate Wi-Fi and WebCore scopes.

Release `81cfc92b023e51c5` verified the same framebuffer translation and
fence digest through the independent native census and repeated the
reference CRC. It returned automatically and passed offline USB checks.
The preflight identified the inherited Pipe A FIFO underrun mask as the
only blocker in its shutdown prerequisite census.
[Display-cycle preflight result](../intel-bdw-edp-recovery.md#display-disable-and-restore-rehearsal).

Release `f9ac1a127a36316f` physically verified the temporary underrun
unmask and exact mask restoration in two sessions. Both stopped after
the panel-off request, before the dark backing check or any restore
writes. The inactive display remained leased and the USB reports
survived.
[Native cycle result](../intel-bdw-edp-recovery.md#first-native-display-cycle).

Release `e3e5a1cc22c5e3a6` completed the isolated native shutdown and
restore. The PPS reached panel-off in 324.117 ms with T12 still active.
The full dark backing digest held and both restored primary CRCs matched
the reference before the panel lit. The underrun mask, driver-well
request and display lease retired cleanly. The single session returned
to Ubuntu with verified USB integrity.
[Panel-off timing retry](../intel-bdw-edp-recovery.md#panel-off-timing-retry).

## PCI reset needs independent display recovery

The Air7,2 advertises PCI AF FLR, which Intel documents as resetting the
display engines and audio codec. PR301 now checks that the exact Intel
diagnostic can record a refused probe and reset in QEMU without a
framebuffer. It also verifies a serial halt without a workspace and
rejects a VM that unexpectedly has a framebuffer. The isolated native
eDP cycle now passes. Carrying the display owner and original backing
through PCI AF FLR, reestablishing the reset display resources, and an
independent memory-completion witness remain open before reset-based
GGTT takeover.
[Reset decision](../intel-completion-prerequisites.md#reset-and-display-recovery-decision).

PR301 owns the PCI AF FLR recovery work on main `4016abf0`, in the
Broadwell display diagnostic and its host and native trial tools.
Release `373cffdfd77b05c3` physically verified a 5,529,600-byte CPU-owned
framebuffer copy and identical PCI snapshots before shutdown, while the
panel was off and after restore. Both restored primary-plane CRCs
matched the reference before the backlight came on. The trial saved its
result, returned to Ubuntu and passed the offline USB integrity check
with zero reset writes. The PCI reset and configuration recovery state
machine passed its fault-injecting host fixture. Native reset requires
proof of requester and memory retirement and a provider that can
reestablish the reset display resources. Wi-Fi and WebCore are separate.
[PCI function reset preflight](../intel-bdw-edp-recovery.md#pci-function-reset-preparation).

Release `2d343940a84a3f33` verified release and reclaim of the driver
well request in 35 µs with the panel dark and firmware holding the well
powered. All framebuffer, PCI and restored CRC comparisons passed.
The one-shot returned to Ubuntu with verified USB integrity and zero reset
writes. Pipe A interrupt mask recovery after reset passed in the host
fixture. A native AF reset still depends on independent requester and
memory-retirement proof and recovery of any display resources that lose
power or state.
[Driver well recovery result](../intel-bdw-edp-recovery.md#pci-function-reset-preparation).

Release `e897744dfc0800ec` verified PCI decode close and memory reopen
with bus mastering inhibited and the panel dark. The CPU-owned copy
retained its reference digest while decode was closed. The first live
validation after reopen failed, so SrvrOS retained the display lease,
left bus mastering inhibited, and halted before panel restore. The
collected result passed offline USB integrity and was rejected as
incomplete. The host restore fixture covers the documented 450 MHz
CDCLK reset default. Native FLR and whole-device memory completion
remain open. [Decode-hold trial](../intel-bdw-edp-recovery.md#pci-decode-hold-and-reset-default-clock).

Release `8f3ddab113407403` traced the refusal to mapping the fence page
at MMIO offset `0x100000`. The display lease, all fourteen platform
registers, and the live PCI endpoint checks passed at Command `0x0402`.
The run saved its diagnostic and halted before invoking reset. USB
integrity passed after the manual return.

Release `c05862653e0d1140` passed the PCI decode hold with cache
admission bound to the original display lease and verified Command
`0x0402`. All 45 reopen checks passed. Both restored CRCs matched
before backlight. The run selected return after save and unmount, and
Ubuntu collected the session with verified USB integrity. The run
closed as `DIAGNOSED` with zero retained state and zero PCI reset
writes. [Decode-hold result](../intel-bdw-edp-recovery.md#pci-decode-hold-and-reset-default-clock).

Release `a112eede6e6eb39c` passed the native dark-panel PWM rehearsal.
It restored the captured period, duty and override with five verified
stores in 111 µs, and both frame CRCs matched before backlight enable.
PCI configuration and the framebuffer were preserved across the cycle.
The run saved and unmounted its USB workspace, returned to Ubuntu and
closed as `DIAGNOSED` with offline USB integrity `PASS`. Native PCI
function reset and whole-GPU memory retirement remain open.
[Backlight PWM result](../intel-bdw-edp-recovery.md#backlight-pwm-recovery).

### Panel power-sequencer recovery checkpoint

Release `b97772bb1e3c7432` passed the native panel timing rehearsal.
After a 501.202 ms T12 wait, five verified stores took 111 µs and
restored the original timing words. The framebuffer, PCI state and
both reference CRCs matched. The run saved, unmounted and rebooted
into Ubuntu, closing as `DIAGNOSED` with offline USB integrity `PASS`.
Native PCI function reset and whole-GPU memory retirement remain open.
[Panel timing result](../intel-bdw-edp-recovery.md#panel-power-sequencer-timing-recovery).

### Joined PCI reset recovery checkpoint

PR301 on main `4016abf0` owns joining the verified PPS and PWM
recovery to the modeled Air7,2 PCI reset and eDP source cycle. Work
covers the Broadwell recovery core and its host fixture. The same
display owner and PCI memory decode must survive into resource
verification, and captured timings and duty must agree before the
panel can light. A native AFCTL write still depends on independent
GPU memory retirement and native recovery of all reset resources.
The open Wi-Fi and WebCore PRs have distinct scopes.

The joined host fixture now models AF reset, T12, PPS and PWM
reprogramming, and both reference CRCs under the original owner.
The source refuses panel power if the restored resources are missing
or changed, and keeps the backlight dark on duty drift detected after
the final CRC. Native hardware evidence covers the isolated rehearsals.
[Joined fixture](../intel-bdw-edp-recovery.md#joined-pci-reset-and-panel-recovery-fixture).

### Display power and clock recovery after decode reopen

PR301 on main `4016abf0` owns the bounded Broadwell display-well
and clock-register recovery in `kernel/diagnostics` and its joined
host fixture. The Air's captured 675 MHz LCPLL selection should
survive Device 2 FLR. The provider will require that exact locked
state, own the original display exclusion with memory decode
reopened, reacquire only the driver well request and restore the
known timing encoding and LP clock gate before PPS/PWM recovery.
The joined fixture now exercises both a cold well with default timing
registers and a firmware-powered well with its timing intact. It requires
the original PLL and verifies the driver request and clock readbacks
before PPS/PWM writes. Native requester retirement, an adapter for the
reopened hardware resources, and a physical FLR remain open.
Wi-Fi and WebCore have distinct scopes.

## Owned blitter completion observed on Ubuntu

On the Air7,2 Broadwell GPU, the i915 probe observed 20 fresh 64-bit post-sync
markers and their earlier writes through private PPGTT mappings before calling
the userspace wait ioctl. Ten controls submitted the earlier write alone and
kept the marker at zero. SrvrOS still needs a native provider for its private
command mappings, completion and retirement.
[Ubuntu blitter check](../intel-bdw-bcs-flush.md#physical-ubuntu-reference-probe).

## Native graphics memory arbitration diagnostic complete

PR301 release `9b727e958b7591d3` returned automatically and closed as
`DIAGNOSED`. The graphics memory arbiter reported all six documented
request groups allowed and idle in all three samples. The sample after
all five engines reached Ready for Reset showed the same permission mask
as the one before preparation. Both display packets and all 129 checked
translations were preserved. USB integrity, recovery and the consumed
one-shot verified. An independent completion witness is still required
before a writable GGTT interval can be granted.
[Native arbitration result](../intel-completion-prerequisites.md#native-graphics-memory-arbitration-result).

## Firmware interference detection added to the held GT owner

On main `4016abf0`, PR301 adds a read-only SMI counter check before the
Air7,2 full-GT bootstrap stores and borrow retirement. Ubuntu read the
counter on all four logical CPUs on this unit. The host fixture injects a
count change at every checkpoint and verifies that the original GT owner
stays pinned. Release `fd9743c1fe226cd4` passed both exact-stage QEMU cases.
QEMU refused the native bootstrap before sampling the counter. It saved and
reset with a workspace, and halted visibly when the workspace was missing.
The later native arbitration run sampled the counter 19 times through
retirement and observed a stable count of 12. This covers the boot CPU
checkpoints in that run. GGTT access and GPU completion still need their own proof.
[Firmware interference check](../intel-completion-prerequisites.md#firmware-interference-check-inside-the-original-gt-owner).

## First-page physical disjointness checked

PR301 now joins the candidate's original target to the inherited primary image
and translation tail. The latest archived runs agree that the original points
into firmware-reserved stolen memory and is physically distinct from the 1,484
sampled display pages. The new kernel and collector checks refuse a candidate
that aliases a display page. Host qualification and replay cover the new
check. A live firmware and GPU ownership provider is still required before
the first write. [Physical backing check](../intel-bdw-primary-range.md#first-candidates-original-physical-backing).

## Native GT pipeline and L3 acknowledgement observation complete

PR301 release `9d2b6c6ea286892a` captured three message snapshots and returned
to Ubuntu automatically. All five Prepare for Reset requests reached Ready
for Reset. The samples showed no engine flush-and-block acknowledgement or L3
flush acknowledgement. All 18 reads completed, the three temporary aliases
retired, and USB integrity and recovery checks passed. The run closed as
`DIAGNOSED` under the wider timing policy. Outstanding GPU accesses, prefetch,
and cache completion still need an independent hardware witness before GPU
ownership can be granted.
[Native wire12 result](../intel-completion-prerequisites.md#native-wire12-result).

## Native auxiliary display-reader observation complete

PR301 release `19e263fec39500ce` captured matching control and status words
for VGA, framebuffer compression and eDP panel self-refresh while the
original requester hold was in force. Both samples showed all three paths
inactive. All eight temporary auxiliary aliases retired, all 129 checked
translations matched, and the Mac returned to Ubuntu automatically. The
USB integrity and recovery checks passed. The run closed as `DIAGNOSED` under
the wider diagnostic timing policy. The next ownership checkpoint needs
evidence that outstanding GPU accesses and prefetched reads have drained and
that cache effects have completed before a writable GGTT interval can be granted.
[Native wire11 result](../intel-bdw-primary-range.md#native-wire11-result).

## Native post-maintenance primary display footprint

PR301 release `2945aece351aa904` captured two matching live primary geometry,
reader and display-well samples after reset and memory maintenance, while the
requester was still inhibited. The 112 DWORD reads matched the original
display records and all ten temporary aliases retired. The laptop returned to
Ubuntu, and automatic collection passed. The wire11 follow-up added the
VGA, FBC and PSR register observations in the same held window.
[Native wire10 result](../intel-bdw-primary-range.md#native-wire10-result).

## Native preservation of the inherited display translation tail

PR301 release `12d3110bd495bbc2` completed the wire9 trial and returned to
Ubuntu automatically. The 128 inherited display-tail PTEs and one prospective
GGTT slot matched before and after reset and memory maintenance. Both
exact-image VM checks and all nine local validation jobs passed. The initial
collection needed a read-limit correction for wire9. The reviewed collection
verified the raw checkpoint and USB integrity and closed as `DIAGNOSED`.
Pipeline, cache and display-reader completion remain prerequisites for a PTE
write. See the [native result](../intel-bdw-gt-bootstrap.md#native-wire9-result).

## Bind GGTT publication to the transferred entries

On main `4016abf0`, PR301 binds the original PTE vector in
`scanout_transfer` to every child publication and restore. The one-page
fixture refuses drift before a write and retains exposed backing if its
restore vector no longer matches. The two-buffer fixture verifies that a
conflict in the second buffer's translation tail also blocks the first
buffer from publishing. The first physical write still requires a live
native GGTT owner with all requester, cache and display-reader prerequisites.
See the [transfer contract](../scanout-transfer.md#capture-and-maintained-ownership).

## OS-allocated scanout backing

On main `4016abf0`, PR301 adds an explicit translation tail to the existing
private scanout allocation, mapping and retirement path. The Air7,2 linear
surface has 1,350 image pages and 128 tail pages per buffer. The host fixture
uses two independently backed buffers with aligned GPU bases and verifies
exact restoration across the full interval before releasing the owner.
The native GGTT interval owner, cache/DMA completion and first live plane
transition remain dependencies. See the
[private backing contract](../scanout-memory.md#broadwell-translation-tail).

## Native scanout ownership gate diagnosed

PR301 on main `4016abf0` covers the isolated ownership gate. The native v55
trial from `c767ee23`, release `42cdc13b60340ddd`, closed as `DIAGNOSED` in
session `session-00066087a8a5365f-00`. Both PCI samples agreed, with bus
mastering enabled and transaction pending clear. The 42 fresh display-reader
words matched the earlier sample. The first unmet condition was an explicit
firmware transfer for the two 6 MiB spans. The CPU guard closed and the display
exclusion unpinned in 35.624 ms, with all 64 temporary aliases retired and
zero GPU control or pixel writes.

The matching session, prior USB files, offline allocation and installed
recovery checks passed. Current `42cdc13b60340ddd` and recovery
`9aa8dbf6fbda8252` verified after the return to Ubuntu. See the
[native gate result](../intel-bdw-stolen-scanout.md#native-v55-ownership-gate-result).
The next design step is to establish a source for live scanout ownership.
The [roadmap](../intel-tearfree-roadmap.md#existing-mapping-reservation-bridge)
records what must be proven before a first flip.

## Native CPU mapping guard lifetime complete

Source `e4d7772e`, release `7daeada549485c7e`, v54/wire7/configuration8,
completed the CPU mapping guard diagnostic in session
`session-00066087356fa4c7-00`. The original display exclusion stayed pinned
through guard acquire, check and close, then unpinned cleanly. Each whole-root
walk checked 110 table paths, 56,320 entries and 47,472 leaves. The guard took
24.119 ms and both full passes stayed within their original two-second limits.
All 59 temporary aliases retired. See the [native guard result](../intel-bdw-cpu-backing.md#native-guard-result).

The run is `DIAGNOSED`. Raw receipts, media preservation, all 1,621 prior USB
files apart from expected metadata, four new logs, offline integrity and the
installed recovery and consumed one-shot checks passed. At that checkpoint the release was
`7daeada549485c7e` with recovery `9aa8dbf6fbda8252`. Firmware, DMA, cache and
display-reader ownership remain prerequisites for writable scanout buffers.

## Native CPU backing admission diagnostic complete

On baseline4016abf0, PR301 adds the native provider's CPU backing guard:
firmware-map/PMM reservation checks for both complete candidate spans and a
bounded whole-root search for any existing CPU alias. Scope is the VMM
read-only reverse walker, isolated stolen probe, actual-core fixtures and its
versioned collector/workflow. Existing unique-alias callers keep their original
contract. No target RAM is read or written, no mapping is changed and no
firmware/DMA/cache/display-reader right is issued.

Source `bc8af8e1`, release `6771b4c6bb8713cc`, v53/wire6/config7, run
`air-stolen-cpu-backing-20261117`, session `session-000660858468bf7b-00`
[completes the CPU backing diagnostic](../intel-bdw-cpu-backing.md#native-result).
Both complete6MiB candidates have1,536 reserved firmware carriers and reserved
PMM tags, with no uncovered/other/framebuffer page. The actual whole-root walk
checks110 table paths /56,320 entries /47,470 leaves and finds no intersecting
CPU alias. The CPU phase takes22,924us; both original passes660,826/705,920us
remain below2s. All59 metadata aliases retire with no writes or retained state.

All7 frozen jobs and both exact-stage VMs passed. Matching raw/checkpoint/source,
all1,617 prior USB files apart from expected metadata, four exact new logs,
offline integrity and fresh recovery/default/consumed-one-shot checks verify.
The run is `DIAGNOSED`;19 native evidence files are independently sealed outside
build. At that checkpoint the installed pair was 6771/9aa8. The CPU observation
establishes backing eligibility for the maintained VMM guard described above.

After collection, the standalone generated USB image was losslessly archived
and verified before removing its build copy. The inactive1070 EFI copy was
removed only after full equality with its independent bank and current/recovery/
GUI/protected checks. Banks, raw evidence and the current6771 stage remain.
Root/EFI free space was 644/207 MiB at that checkpoint.

## Native reader/cache preflight complete

Source `2d7b164b`, release `10703e82a2760453`, v52/wire5/config6, run
`air-stolen-owner-final-20261117`, session `session-00066083dd93046c-00`
[completes the live reader/cache diagnostic](../intel-bdw-stolen-scanout.md#native-v52-reader-and-cache-observations-complete).
Both full census passes finish in830,295/406,653us below the unchanged2s bound;
all59 aliases retire, with zero writes/refusals/retained resources. Only primary
A is enabled; cursor/sprite readers are disabled, programmed/live addresses
agree and no sampled starting address hits a candidate. Both complete6MiB
physical spans have MTRR UC0 and effective WC1 at the existing PAT slot5.
Other PAT slots resolve UC0. This is hypothetical CPU policy, not a writable
alias, GPU cache/visibility guarantee or maintained reader retirement.

All7 frozen jobs/both VMs, matching raw/checkpoint/source, USB preservation,
offline integrity and fresh recovery/default/consumed-one-shot checks pass.
The run is `DIAGNOSED`;19 native evidence files are independently sealed outside
build. At that checkpoint current1070/recovery9aa8 and accepted GUI/banks remained.
The v53 result above supersedes the current installation. Next implement the
original live firmware/DMA/cache/all-reader ownership
provider and reservation before the first writable CPU alias. Do not repeat
this completed snapshot or turn saved packets into live authority.

## Earlier private CPU context image milestone

Source `4ce676bd`, release `08ae0d63f7b821c4`, profile `intel-bcs-context-v46`
/wire8, run `air-bcs-context-isolation-20261117`, session
`session-0006607c842689c0-00` completes one original kind2 epoch0→1
[CPU context-image lifetime](../intel-bdw-bcs-context.md#native-result-one-cpu-image-lifetime-complete).
All46 frozen validation slots and both exact-stage VM paths passed before boot.
The matching raw session and original checkpoint reparse strictly; the run is
`DIAGNOSED` and732 independently verified evidence files are banked outside build.

The complete16KiB ring/HWSP/register/completion candidate is constructed and
verified before GPU exclusion, checked at both original held-source guards,
then checked and erased only after actual GT/HDA/outer retirement. All four
CPU aliases retire,2048 zero QWords verify and the original four pages release
once, with DMA57→57. The two temporary UC aliases and15 original GPU aliases
also retire. Proposed GPU addresses remain unmapped; no command is submitted.

| Clock | Observed | Existing practical limit |
| --- | ---: | ---: |
| Preparation including guarded return | 219.532 ms | 250 ms |
| Largest original GT maintenance phase | 77.210 ms | 100 ms |
| Original GT total | 379.409 ms | 500 ms |
| HDA total | 399.125 ms | 750 ms |
| Outer total | 451.007 ms | 1000 ms |
| CPU arena setup / held / cleanup | 87 / 401 / 82 us | Existing parent and100 ms setup/cleanup |
| Image construction / erasure | 9 / 2 us | Inside the original CPU arena bounds |

The same owner completes both reset pulses/five cancellations, all32 fences and
inherited display/backing preservation. Its memory child completes remapper DR/DW
in36.987ms and System Agent GGTT invalidation in2.321ms (83.119ms child).
All1,590 prior USB files preserve apart from expected metadata; three exact logs
yield1,593files/187sessions. Boot/preflight/exit are9,204/39,096/74,581bytes,
within96KiB. Offline fsck/allocation/writable/headroom, unchanged boot region,
installed/banked recovery, Ubuntu default and consumed one-shot pass.

At the accepted CPU-image checkpoint current08ae/recovery9aa8, acceptedf0a8
and protected installed releases remained. The later census installs9fbb as
current under the unchanged9aa8 recovery; independent accepted banks remain.
Prior a382 remains independently banked; only its equal inactive EFI/staged
copies were pruned to make room for the next read-only image.
USB is unmounted, host volume is1%, this profile is silent and no reboot is armed.
Review current root/EFI capacity before staging; generated copies are retired
only after full comparison with the independent bank.
Only inactive installed/staged434e duplicates were removed after full equality
with its independently verified external bank; bank/raw evidence remain. Preserve
dirty `build/repro-7032f10` and all accepted payload/evidence banks.

This accepts CPU image construction/preservation/retirement in one original
hardware lifetime under the explicit practical policy. It does not accept the
historical stricter preparation/GT limits, actual executable LRC/mode/cache
policy, GPU mappings, DMA/cache/prefetch/firmware/display-reader ownership or
native tear-free. All eight obligations (mask255) remain, with zero managed GPU pages/PTE
writes/submissions. Enabled entry for this source and repeatability remain open.
The prior zero-arena result remains in the [dated journal](macbookair7-2.md#native-full-gt-private-cpu-arena-completes).

## Earlier native layout census

Source `aad02bb5` / release `aa2c4bed41ec15f4` / v51/wire4 / session
`session-00066082b1847677-00` completes the
[read-only existing-mapping census](../intel-bdw-stolen-scanout.md).
An isolated main defers user init for the exact terminal recipe; the unchanged
sole-root and alias checks now pass. Both complete11,776-entry vectors agree.
The passes take479.746ms and232.507ms within their original2-second bounds.
All53 CPU mappings retire, exclusion releases, and there is no retained alias,
refusal, clock failure or GPU write. Strict UC3,107 cache checks,70,770 alias
checks,23,552 translation checks and8 metadata reads verify.

| Range | GPU interval (exclusive end) | Physical interval (exclusive end) |
| --- | --- | --- |
| Complete inherited fence | 0–0x2200000 (34MiB) | 0x8c001000–0x8e201000 |
| First6MiB candidate | 0x2200000–0x2800000 | 0x8e201000–0x8e801000 |
| Second6MiB candidate | 0x2800000–0x2e00000 | 0x8e801000–0x8ee01000 |

These are observed existing translations within sampled stolen-memory bounds,
not allocated/free buffers or transferred firmware memory. Sampled FBC/PSR gates
are clear; maintained DMA/cache/prefetch/display-reader ownership remains open.
The candidate cold alias preflight retires before any metadata load.

All7 frozen steps,27 census/boot/VMM/receipt methods,197 source/return methods,
22 additional legacy boot dispatchers and both exact-stage VM paths passed.
Exact four raw files/recipe/journal/exit, prior USB contents, boot region,
offline integrity and fresh installed/banked recovery/default/consumed-one-shot
checks pass. Raw hashes/checkpoints are reparsed before the run closes DIAGNOSED;
self-contained native evidence is independently sealed outside build. At that
checkpoint currentaa2c,
recovery9aa8 and accepted GUI/protected banks remained; USB was unmounted and no
trial was armed. Earlier refusals and startup-conflict evidence remain in the
[dated journal](macbookair7-2.md#native-pre-init-read-only-census-completes).

The next bounded milestone is explicit original-lifetime borrowing of these
candidate spans, including firmware reservation and maintained engine/DMA/cache/
prefetch/display-reader exclusion before any RAM store. Then bind exact CPU
aliases/visibility, the first inherited-X-tiled-to-linear primary transition,
actual live latch and independent old-reader retirement, followed by compositor,
restoration and endurance qualification. Keep the first-write refusal until the
required live ownership witness exists. A stable layout or CPU image is not that
witness; native tear-free remains unaccepted.

The [primary-A flip core](../intel-bdw-primary-flip.md) is host-qualified for
10,000 single-leg cases plus rollback/CRC trials, bounded polling and live-latch/reader
handling, but has no native caller. All broader bootstrap ownership obligations
remain open. Ordinary desktop startup is unchanged; this exact terminal image
has its own recipe-gated pre-init dispatch.

Integration main4016abf0 and draft PR301 own this scope. PR19 Wi-Fi and PR379
WebCore do not overlap; no agents/chats were contacted or Actions requested.

## Existing-mapping borrow backend: host-qualified

The [reservation backend](../intel-bdw-stolen-borrow.md) now binds an original
live source/provider/root/epoch and the exact classified pair to the existing
scanout_memory lifecycle. It pins real CPU display exclusion, registers children
before callbacks and stops new CPU borrows while original users drain. Returning
a child never frees firmware RAM or returns the outer reservation; final return
requires fresh provider proof. Missing rights refuses before admission; later
uncertainty retains the original source, backing and exclusion.

The actual backend/memory/hold/exclusion pass7,360 cases,16 runtime-detected
compiled controls, fatal sanitizers and native2KiB frames, with the existing
memory/hold/transfer/handoff regressions passing. It is excluded from ordinary
image linkage and has no native ownership producer, CPU alias or RAM/PTE writer.
No new trial is armed. Next supply the live reservation/firmware/DMA/cache/reader
provider specified in the contract; the completed census is historical data,
not that live source. PR301 owns the software bridge; other device scopes remain
unchanged and no Actions/other agents were used.

## Qualified software prerequisites

The [mapping transaction](../intel-managed-memory-plan.md) supports the existing
practical250/500/750/1000ms aliases, retaining both legacy policies and nested
100ms transfer/publication caps. Three policies pass26,712 composed cases,
25 compiled controls, fatal sanitizers and2KiB native compiler frames.

The [primary-range classifier](../intel-bdw-primary-range.md) passes18,216 cases,
12 controls, fatal sanitizers and native frames/image isolation. It includes tile
padding,256KiB alignment,128-PTE display tail and full protected-fence separation.
Saved-Air replay is offline geometry, not a live reader/ownership token.

The [primary flip protocol](../intel-bdw-primary-flip.md) has no native caller.
Its finite9-read polling batch and100ms original operation bound cover arm and
live-latch checking without a frame-wait loop. It supplies no old-reader
retirement, GPU mapping or native tear-free grant. Related full-fence geometry,
100,000 buffer-lifetime interleavings and publication/restore regressions pass.

The [submit port](../intel-bdw-bcs-submit.md) and actual joined flush/nonce cores
pass2,519 port,2,326 flush and35 joined cases per policy,24 new plus24 existing
controls, fatal sanitizers and native frames. Port issuance is separate from
command completion and retirement; all partial/uncertain effects retain backing.
The new blitter driver uses the submit port. Calling it during a physical
SrvrOS boot requires the live range and exclusive engine owner described above.

The [CPU image codec](../intel-bdw-bcs-context.md) passes4,463 decode/all-word-
corruption/input cases and11 controls. The actual context adapter passes72 new
and292 original retained-prefix faults on two modeled routes, three native
controls, fatal sanitizers and2KiB frames. The complete PCI matrix passes8,237
fault cases. All42 prior frozen slots remain among the46 qualified slots.
Earlier failed candidates and staging/collector history remain in the
[GPU qualification history](macbookair7-2-gpu-history.md) and dated journal.
