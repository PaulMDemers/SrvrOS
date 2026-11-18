# MacBookAir7,2 current status

Updated: 2026-11-18 (America/New_York). Reference A1466; partial desktop/device acceptance.
The [dated journal](macbookair7-2.md) preserves exact-source history.

The inherited-framebuffer desktop remains usable, with independent evidence for
keyboard/trackpad, secondary click, window navigation, saved Home documents,
brightness, sensors, battery and earlier USB Ethernet/audio rounds. Native
tear-free scanout is still pending.

## Active scope: OS-allocated scanout backing

On main `4016abf0`, PR301 will qualify the backing and translation lifetime
for private scanout buffers using the existing scanout memory, space and
publication cores. The Broadwell surface needs an additional 128 translations
kept with each mapping through verified retirement. The first checkpoint is
host validation of that lifecycle and its refusal paths. The native GGTT
interval owner, cache/DMA completion and first live plane transition remain
dependencies. The Wi-Fi and WebCore PRs do not touch this scope.

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
10,000 cases/18 controls, bounded incremental polling and strict live-latch/reader
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
No native image calls these submission/completion cores.

The [CPU image codec](../intel-bdw-bcs-context.md) passes4,463 decode/all-word-
corruption/input cases and11 controls. The actual context adapter passes72 new
and292 original retained-prefix faults on two modeled routes, three native
controls, fatal sanitizers and2KiB frames. The complete PCI matrix passes8,237
fault cases. All42 prior frozen slots remain among the46 qualified slots.
Earlier failed candidates and staging/collector history remain in the
[GPU qualification history](macbookair7-2-gpu-history.md) and dated journal.
