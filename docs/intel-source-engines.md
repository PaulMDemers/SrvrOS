# Original-source five-engine readiness hold

The separate v34 `intel-source-engines` diagnostic registers one original
engine-control lifetime beneath the [v33 source requester](intel-source-requester.md).
The v33 source child runs after the outer reset protocol cancels its readiness
requests. A requester-inhibited PCI function is not an engine hold; this new
child explicitly requests readiness again and retains all five acknowledgements
through its own child, before reverse cancellation and requester restoration.
It adds no reset, submission, GPU translation or display operation.

## Exact control and ownership

The profile stays the captured Air7,2/8086:1626 GT3 firmware/resource policy.
Use only the five existing RW/NX/UC RESET_CTL words at
020d0/120d0/1a0d0/1c0d0/220d0; GDRST at0941c remains observation-only for this
new operation. Before the first request, capture all six original DWORDs and
require request/ready low bits zero, reserved low bits clear and GDRST zero.
Request each engine in that order with mask/request value00010001. Validate
its fresh pre-store control and original registration, take the final original
clock, then issue exactly one fenced DWORD store and a newly admitted first
acknowledgement read. Poll only for request/ready11, with the unchanged
10ms/256-poll per-phase cap inside the original deadlines.

A complete all-held snapshot precedes the child. The original requester,
transfer, source/route/reset epoch, both full private RAM holds, root and CPU
exclusion remain registered throughout. Actual parent-return and RAM-release
interlocks refuse while the child is live. The current child exercises that
registration; it performs no GPU memory operation. A second full held snapshot
must match after the child. Cancel in reverse order with exact masked value
00010000, acknowledge request/ready00 and compare a final six-word snapshot
with the original. Only then return the original engine registration; the
requester must revalidate its inhibited profile, restore Command7 and finish
the full closing hardware guard before source/RAM/root return.

Every target access retains original source/map/requester/clock admission.
The final store also checks phase, step, target index, sample/ack/write counts,
issued prefix, child completion and fresh control admission after its final
clock, using pure checks only. Each phase retains an independently captured
start across callbacks; completed raw profiles and final read/poll bookkeeping
must remain exact. Closed one-use receipts cannot be restarted or mutated by
a later deadline call. Provider callbacks and result objects are sealed
to their originals. Copies, re-entry, changed callbacks, lost registration,
partial reads, reserved/floating controls, source loss, expired/stalled clocks,
unknown stores and failed children retain the ownership chain. There is no
failure-path forced cancellation, replay or optimistic requester restoration.

The same preparation100ms, GT300ms, HDA400ms and outer500ms origins apply; no
budget restarts. The v33 observation left only1,116us of preparation margin,
which is not a promise that this additional operation fits. Local models do
not establish native timing. Any timing work must preserve final admissions,
full opening/closing validation and the unchanged deadlines.

## Scope and references

Intel's [Broadwell PRM Volume2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
November2015, printed pages165/288/1704/1749 define the mask/request/ready fields;
its pinned SHA256 is
`09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`.
Linux v6.12 [engine preparation/cancellation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c)
is a protocol reference. This diagnostic imports no catastrophic-error bypass,
non-ready reset, retry, full reset or forced failure cleanup.

Readiness is control state, not proof of internal graphics/DMA/prefetch,
pipeline/cache/translation or display-reader retirement. Command3 and AF pending
clear do not supply those proofs either. All255 completion obligations,
zero positive assurances/managed GPU pages and native interval refusal remain.
This lifetime is a prerequisite for later reviewed witnesses in the
[managed-memory plan](intel-managed-memory-plan.md), not a transfer token.

## Image, receipt and qualification

Use `--intel-source-engines --laptop-probe-volume THIS_UNIT_UUID`, the dedicated
`build/intel-source-engines/srvros.elf`, own firmware request, exact15-token
`source-engines-v1` recipe and `source-engines-preinit`. Ordinary and older
recipes cannot enter it. The v34 wake receipt and `intel_source_engines` collector
field remain separate; older fields stay empty. Internal checked shared-schema
views never change saved raw bytes or relabel older native evidence.

The receipt preserves all four six-word prefixes and timestamps, all ten
first/final acknowledgements, fresh pre-store controls, poll counts and phase/
final-store clocks in canonical624-byte binary/base64 data. Its summary binds
the exact original epoch/route/origins, counts and registration/child/return
state. Full completion requires the complete enclosing native requester,
transfer/source/RAM/reset/HDA/outer chain and its closing hardware guard.
A completed engine suboperation with a failed outer guard remains incomplete.

Frozen source `4fd81b45` / release `90dfc6ca035e19f7` is **hardware-ready**:
39-image host/core, 117 staging/rollback tests, 12 fatal sanitizer adapter tests,
RAM-hold sanitizers, six compiled weakness controls, 12 historical raw replays,
native object/linked store and seven 2 KiB frame checks pass. All 40 saved-tail
projections fit (worst 65,364 / 65,536 bytes including 1,536 reserve + 1,024 header).
Exact-stage EFI/MBR VM persistence/refusal/unmount/return and missing-workspace
halt pass. Both interrupted gates stay nonpasses; the corrected frozen source
has its own full passing gate. Installed-root testing was explicitly omitted
because installer core is unchanged; baseline-known injected exFAT repair gaps
remain documented.

The immutable payload and complete proof bank survive build cleanup. The
[unit checkpoint](machines/macbookair7-2.md#active-integration-and-source-bound-engine-hold-work)
records recovery, media integrity and exact scope. At qualification the candidate
was uninstalled/unarmed; the subsequent native result below accepts only its
observed route. All managed memory/tear-free grants remain unaccepted. Automated trials need no human input, show progress and
save/unmount/return on safe outcomes at volume 1%; unknown ownership halts visibly.
Use the [Ubuntu workflow](ubuntu-device-agent-setup.md) and keep accepted recovery.

## Native result: original already-disabled lifetime accepted

Air7,2 source `4fd81b45` / release `90dfc6ca035e19f7`, session
`session-000660308088581c-00`, collector `b5694a5a` pass one original kind 2
already-disabled lifetime: entry epoch 0, reset 0→1/source 1. All five requests
remain held through the child, reverse cancellation restores exact controls,
and four full six-word profiles/10 stores/10 admissions/10 ACKs/44 reads verify.
Engine scope 1,352 us, child 48 us, phases 58–87 us; parent PCI 7→3→3→7 restores
before full closing validation and complete original source/RAM/root retirement.
No pending polls occur, so delayed acknowledgements remain unaccepted.

Preparation 99,793/100,000 us leaves only 207 us; this is one bounded pass, not
repeatability. GT 266,183/300,000 us, HDA 302,774/400,000 us and outer 357,288/500,000 us;
all limits unchanged. Four padded patterns, exactly-once 2,712-page release,
DMA 57→57, original borrows and 14 GPU/HDA 1 alias retirement pass. Raw logs and
all nine lack-of-authority alerts remain sealed. All 1,503 prior files preserve
apart from expected metadata; three logs yield 1,506 files/158 sessions. Offline
media/recovery/default/consumed-one-shot verification passes; no new boot armed.

This accepts the maintained readiness-control lifetime only. New v34 enabled
entry, delayed ACK/AF pending branches, repeatability and every actual managed
GPU-memory/completion/tear-free grant remain pending. All 255 obligations and
zero assurances/managed pages remain. See the
[unit result](machines/macbookair7-2.md#native-v34-original-source-five-engine-readiness-hold-accepted)
for exact evidence and the next timing/ownership boundary.

## Removing repeated admission work

Candidate `b8b8a2d1` / release `83fd1d098a221916` removes two repeated full guards
per store while preserving the fresh full check before the pre-store control
read and the separate full check immediately before the store's final clock.
The full core/first-ACK/opening/closing checks remain, as do every original
identity, registration, clock and profile test. No target access is shared
between admissions. Host full guards fall 106→86 (delayed-poll 166→146); a target
fixture rejects a missing fresh full check, including a compiled negative
control of the final store guard. Clock calls and raw controls/target counts are stable;
mock timestamps shorten 80 us because mocks charge 4 us per guard, not because any
deadline changed. Native speedup is pending.

Full 39-image/core, 117  staging,13 fatal sanitizer tests, seven compiled controls,
13 historical classifications, native object/log/frame and exact-stage VM/media/
recovery checks pass. The candidate is banked, uninstalled/unarmed and eligible
for its standing-authorized automated native trial at 1%. Current 4fd native
kind 2 acceptance above is not transferred to this revision. All 255 obligations,
zero assurances/managed pages and native tear-free remain unchanged.
