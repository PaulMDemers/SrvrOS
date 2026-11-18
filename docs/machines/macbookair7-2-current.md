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
The first exact-stage VM caught a host verifier NameError; that uninstalled
candidate was cancelled. Its guest had correctly refused/saved/unmounted.
The corrected verifier consumes the actual manifest profile through a tested
wire-specific helper; unsupported, native-active and mixed-wire inputs are
covered. The native startup line now identifies wire2/UC3. The replacement
still requires fresh frozen-source and both exact-stage VM gates.

The qualified UC3 retry, source `b4d8569b`, release `6816e9b06874f4f3`,
session `session-0006607f33b0cd71-00`, stopped after the census startup line.
Ubuntu collected exactly two matching logs (boot and prerequisite checkpoint),
with no census summary or exit snapshot. The run is reviewed and REJECTED as
incomplete; neither the photo nor saved preflight identifies the failing call.
All prior USB files and boot region preserve, offline integrity passes, USB is
unmounted and installed/banked recovery/default/consumed one-shot checks pass.
The raw logs, photo and negative review are sealed outside build. Current is
6816 under recovery9aa8. Both exact-stage VM paths and seven frozen host slots
passed before boot; those results do not establish native progress or cleanup.
Active scope is bounded diagnostic progress around admission, source
revalidation and first MMIO access: the isolated stolen adapter, paused workspace
logger, raw collector and local regressions. The implemented separate4KiB
progress file preserves pending operations without overwriting session-a.log or
writing through a live diagnostic MMIO alias. The16-entry RAM history supplies
finer operation edges at the next safe save. Inside-pass I/O consumes the original
2-second budget, checked immediately after each save. First-refusal27 identifies
progress-save failure. New frozen manifests require this fourth file; historical
ones keep their three-file contract. Missing exits still fail after the pending
operation JSON is saved. No framebuffer-exclusion bypass or GPU authority is
added. The actual adapter passes213 modeled cases/12 compiled controls and fatal
sanitizers/native frame checks; broad local regression passes252 tests. Staging
also binds the progress contract, and VM dispatch checks unsupported-profile
progress before any acquisition. Fresh frozen qualification and exact-stage VM
remain required before the next unattended1% native trial. No GPU mapping,
ownership or tear-free acceptance follows.

The progress-enabled native retry, source `7c89b656`, release
`91e478bb7b14aefa`, session `session-00066080b658c5ac-00`, again lacks a
summary/exit, but its new recipe-bound journal is valid. It records steps1–9:
CPU display exclusion acquired, initial cache context captured and original
source revalidation returned before the metadata boundary. No census alias
was mapped at that saved boundary. Subsequent PCI/cache/map/alias/MMIO-read
and cleanup state is unknown; this does not identify a specific blocked call.
The run is reviewed/REJECTED with three raw logs, photo and negative proof
sealed outside build. Prior USB files, boot region, offline integrity and fresh
installed/banked recovery/default/consumed-one-shot checks pass; USB is unmounted.
Current91e4/recovery9aa8 remain, with no new test armed. Next isolate metadata's
first page/cache/mapping admission from actual alias validation and register
access; do not infer a safe read or retirement from the journal. Existing budgets,
source/cache/alias gates and no-GPU-write policy remain. Main4016abf0 is unchanged.

Active implementation is the first-page CPU-alias preflight in the isolated
census adapter and versioned recipe/collector/tests. Under the same original
exclusion and2-second pass, save pending cache policy (step16), admit strict UC3,
save pending mapping (step15), map/check/unmap the first metadata page without
a register load, then save retired preflight (step18). The real load remaps with
every original check; no retired observation is reused as authority. Wire3/v49
requires preflight before any metadata/vector reads and accounts for53 mappings,
53 retirements,107 cache checks and70,770 alias checks. Nineteen safe journal
saves fit the unchanged32-update cap. Historical wire1/2 remain readable but
cannot create new trials. No live-alias storage, GPU writer, broader model gate
or deadline increase. Adapter213 cases/14 compiled controls, fatal sanitizers/
2KiB frames and final16 current-module tests pass; focused dispatch179 and broad
regression253 pass. Fresh frozen7-slot and exact-stage VM gates remain pending.
Review found an additional precondition gap: the actual alias observer requires
sole kernel root/zero registered alternate roots, while initial admission only
checked current==kernel. The adapter now invokes the existing IRQ-masked sole-root
diagnostic before exclusion and every new mapping, restores incoming IRQ state
and keeps all later leaf checks/retention. Root-context fault qualification now
passes218 cases/15 controls and16 current-module tests. This is a precondition
fix, not proof of the native stop's cause. The first sourcefbaa146d/release5cf8
candidate passed all7 slots/both VMs but was cancelled before installation; its
payload was losslessly archived and byte-verified outside build to make room
for the corrected source. Fresh qualification remains required.
Only equal inactive6816 EFI/stage and GUI f0a8 generated-stage duplicates were
removed against independent banks; the installed GUI/current/recovery/protected
payloads and all banks remain. PR19 Wi-Fi and PR379 WebCore are untouched.

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

## Native wire3 alias trial refuses before exclusion

Source `87922b9f`, release `7c8aa2e4c47b3626`, session
`session-000660813db3e74d-00` returned with its exact recipe, four raw files,
valid progress journal and matching exit snapshot. The first refusal is code5
(root), phase0: preflight0, acquired0, retained0, maps0, retired0 and metadata
reads0. Journal steps1–3 end before root/PCI admission. The new sole-root guard
therefore prevents this trial from entering display exclusion or creating a
census alias; it does not explain which of that guard's subconditions failed,
nor prove the cause of the earlier silent stop. The census remains incomplete.

The seven frozen qualification slots and both exact-stage VM routes passed
before this trial. Post-return collection verifies unchanged prior USB contents
and boot region, exactly four added session files, clean offline fsck/allocation/
writable/headroom, installed/banked recovery, Ubuntu default and consumed
one-shot. The run is reviewed/rejected; all four raw logs, frozen manifest,
report, progress and review are independently sealed outside build. Current7c8a,
recovery9aa8 and accepted GUI/protected banks remain; USB is unmounted and no
new trial is armed. Main4016abf0 remains unchanged. Next expose the original
sole-root predicate's failure details without a second probe, changing its
requirements or mapping on a known-invalid context. No native census, GPU-memory
ownership or tear-free acceptance follows from this return.

Active bounded scope: expose the existing VMM sole-root guard's original failure
observation to the isolated census; shared VMM predicates/order remain unchanged.
Wire4/v50 binds value-only root phase/reason/detail, including original CR3
non-address bits on a current-root refusal, without a second probe. Touched
subsystems are VMM observation API, isolated census/recipe/collector and tests.
No root normalization, registry mutation, mapping on refused entry, new writer
or deadline increase. Main4016abf0 and other PR19/379 scopes are unchanged.

Wire4 implementation records the preliminary zero/kernel-root refusals as well
as the existing sole-root observer, using only values from the original checks.
No second probe, root normalization, registry change or predicate removal.
Current local suite185 passes before the final two preliminary-root fixture cases;
actual VMM observation/sanitizer and three compiled controls pass. The frozen
candidate must rerun all gates, including those final cases, before installation.
Only independently verified91e4 inactive EFI/stage duplicates were removed for
capacity; its bank and negative evidence, current7c8a/recovery9aa8, accepted GUI
and protected banks survive. No test is installed/armed for wire4 yet.

## Native wire4 identifies registered-root startup conflict

Source `08e93b36`, release `f71826d1d791bb36`, session
`session-0006608248062abb-00` returned with four exact recipe-bound raw files,
valid journal and matching exit. Wire4 records root phase1/reason7/detail2:
the original guard found two registered nonkernel address spaces. Exclusion,
census mappings, retirements and metadata reads are all zero, with no retained
alias. The run is reviewed/rejected as an incomplete census. Prior contents,
expected four additions, boot region, offline fsck/allocation/writable/headroom,
installed/banked recovery, Ubuntu default and consumed one-shot pass. Raw logs,
report/checkpoint and review are independently sealed outside build.

Source review finds the image links ordinary `main.c.o`; its boot initialization
starts `/init` before the census. The matching native log confirms init startup
and readiness. Existing terminal GT diagnostics defer user services, but the
census lacks that isolated boot entry/deferral. The count is established; the
two individual root identities are not. Next provide a separate census main
and exact-recipe terminal pre-init gate, with ordinary/malformed-recipe isolation
tests and explicit exact-stage VM deferral evidence. Do not delete live roots,
normalize CR3 or relax the sole-root/alias/resource/cache checks or deadlines.
This also gives a concrete reason the earlier alias observer could refuse;
previous silent stops still have no proven instruction-level diagnosis.

All seven frozen steps,23 census/VMM/receipt methods,197 source/return methods,
220 adapter cases/15 controls, actual VMM tests/three controls and both exact-stage
VM paths passed before this run. Currentf718/recovery9aa8 remain, USB is unmounted
and no new trial is armed. Native census, GPU memory/reader ownership and
tear-free acceptance remain open. Main4016abf0 and other PR19/379 scopes are
unchanged; source/recipe/startup isolation is the next bounded scope.
