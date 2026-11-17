# MacBookAir7,2 current status

Updated: 2026-11-17 UTC. Reference A1466; partial desktop/device acceptance.
The [dated journal](macbookair7-2.md) preserves exact-source history.

The inherited-framebuffer desktop remains usable, with independent evidence for
keyboard/trackpad, secondary click, window navigation, saved Home documents,
brightness, sensors, battery and earlier USB Ethernet/audio rounds. Native
tear-free scanout is still pending.

## Latest physical result: private CPU context image

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

## Latest census and next bounded scope

The first [existing-mapping census](../intel-bdw-stolen-scanout.md), source
`21bd9f7c`, release `9fbbcd3ad8e3a69a`, session
`session-0006607e6908faf0-00`, returned with an initial admission refusal:
phase1/reason4, no new mappings/metadata reads, exclusion released and no
retained alias. All three matching raw hashes and offline integrity pass; USB
is unmounted. The original collector stopped on a generic checkpoint mismatch.
The frozen run is explicitly reviewed/rejected, with its raw evidence intact.
No mapping-layout or ownership acceptance follows from that run.

The same raw CPU-cache observations expose PAT2=7 (UC-minus), PAT3=0 (UC),
MTRR=0 and effective=0. The new adapter incorrectly required PAT2 itself to be
UC0. Active scope is to reproduce that refusal using the captured PAT layout,
select actual UC slot3 while retaining strict MTRR/PAT/effective checks, record
the first failing admission operation, and collect valid incomplete diagnostics
without granting acceptance. No original-source/resource/alias check or deadline
is dropped. V48/wire2 now selects UC3/PCD|PWT and records the first refusal/cache types.
The actual adapter passes204 cases/11 compiled controls, fatal sanitizers and
2KiB native frames;205 source/transport/workflow regressions pass. The collector
independently decodes UC3 for all26 observed MMIO pages. Valid incomplete
returns collect with the refusing operation and can only be closed rejected;
they cannot be accepted or diagnosed complete. Historical v47 is read-only.
Exact frozen-source and staged-VM qualification still precede the new trial.

Base main4016abf0 remains unchanged. PR301 owns these native census/collector
changes; ordinary startup and reset/CPU-context protocols remain unchanged.

The separate [primary-A flip protocol](../intel-bdw-primary-flip.md) now
implements original-source admission, one synchronous arm, actual live-surface
latch interpretation and incremental bounded polling. Its host qualification
covers10,000 cases,18 compiled weakening controls, fatal sanitizers, native2KiB
frames and image isolation. Queued surface/frame progress alone cannot complete
a request; cross-poll counter/live regressions and missing old-reader retirement
remain distinct. Native buffers/mappings/visibility and the ordered adapter are
still absent. Ordinary startup, the accepted reset graph, installed payloads
and other device profiles are unchanged. Base main4016abf0 and draft PR301 remain;
open PR19 Wi-Fi and PR379 WebCore do not overlap. No new physical trial is
warranted by host qualification alone.

Resolve the real bootstrap outstanding-memory/cache/prefetch and firmware/
display-reader witness before a private GPU interval can be transferred. Then
bind exact mappings, visibility, actual mode/workaround/coherency policy and
queue/context retirement to the same original owner. A constructed image,
reset ACK, narrower remapper operation or stable saved sample is not that
witness. Keep the first-GPU-write refusal and original backing pinned through
all readers; do not repeat the completed CPU image trial without a new question.
Shared scope remains the original-owner adapter, memory/mapping contracts and
isolated image/recipe/collector. The v47 census was staged and run; its incomplete result is recorded above.

Integration main4016abf0 was fetched unchanged; draft PR301 owns this work.
Independent PR19 (Pro Wi-Fi) and PR379 (WebCore audit) remain untouched. No agents
or other chats were contacted and no GitHub Actions were requested.

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
