# Original-owner enabled-ring transition

This next diagnostic consumes the actual enabled BCS state observed in the
[disabled-entry trial](intel-entry-reset-composition.md#native-result-enabled-entry-refused).
Native acceptance is pending. It is separate from the zero-geometry reset and
previous shutdown images; their recipes, gates and evidence remain unchanged.

The original static HDA owner performs 6→2 before GT wake. The 14 original joint
aliases retain their addresses; only the original wake request and BCS control
pages may be writable. GDRST, the other four engine pages, plane and GGTT pages
remain read-only. A full original 92-word completion census must match both
hardware samples and the exact captured enabled 16, pending 0, parser-idle 3 state.
No observation of a quiet register replaces the explicit cooperative submission
handoff policy.

A sealed original GT child pins its source/provider and blocks early close.
Four BCS 8 groups use the existing idle shutdown core: two exact source-bound
initial groups, at most one fenced `0xf000` DWORD at `0x2203c`, and two matching
readbacks. Advance the parent's engine epoch before a possibly issued store,
even a known unissued result. The old census then becomes historical data only.
A fresh full 92-word census at the new epoch must retain every word/context
except the BCS enable bit. The ring start and disabled length/control remain
nonzero; no address or geometry clearing is allowed. Two plane/HDA config
observations preserve all 54 plane DWORDs and the full 10-field audio endpoint,
including primary/cursor binding to the original display capture and Command 2.

Keep BCS 100 ms, original-source admission age 100 ms, transition including borrow
return 150 ms, readers 150 ms, GT 300 ms, HDA 400 ms and outer 500 ms. Keep the first failed
clock sample and original origins. A known unissued store may retire only with
intact original ownership and no possible hardware change. A possibly issued
store requires complete postconditions; otherwise retain the child/root and
exclusion. If a final clock fails after logical child return, retain the original
GT/HDA root; do not retry or reconstruct the returned borrow. All retained
objects/providers outlive reporting frames.

Completion qualifies a command-submission transition and preservation in that
one lifetime. It does not authorize a later boot, turn nonzero geometry into
zero-geometry eligibility, or grant reset, flush/cache/translation/DMA/firmware
retirement or managed memory. The native test ends here, before any reset.
Future reset from this state requires an explicit new ownership/postcondition
contract with fresh observations while the owner remains live.

A separate image, firmware request, exact pre-init recipe and wire version bind
all source/store/fresh-census/reader/retirement evidence. Ordinary startup gains
no optional AML or GPU action. Host faults must cover source/census/provider
copies and mutations, every read/guard/store/clock/postcondition/return boundary,
static retention, terminal clock failures and false completion. Qualify all
image isolation, actual native access widths/permissions, 2 KiB frames, 64 KiB tail
capacity, exact staged VM save/unmount/automatic safe return and physical recovery
before staging a native test. Safe saved outcomes return automatically;
retained uncertainty halts visibly. Do not force enabled entry for coverage.

## Candidate interface and evidence

The isolated `build/intel-ring-transition/srvros.elf` uses wake version 24 and
`tools/hardware_boot.py stage --laptop-probe-volume <local-UUID>
--intel-ring-transition`. Its exact 15-token recipe includes
`srvros.intel-ring=handoff-v1` and the existing completion/read-only prerequisites.
The pre-init marker is `ring-transition-preinit`; no user services start. The
ordinary kernel and earlier diagnostic images do not gain this writer.

`intel-ring-summary` binds the original source timestamps, both epochs, registered
borrow, all four BCS observations, actual final store clock, fresh census, both
reader observations and logical return. `intel-ring-post-*` carries the complete
fresh vector, including context words. `intel-ring-hda` and `intel-ring-owner`
retain the original HDA lifetime receipts. The collector requires the complete
chain and the exact saved preflight; a summary alone cannot pass. A zero-store
already-disabled source is a safe refusal rather than enabled-writer acceptance.
A completed child followed by failed HDA restoration is also an overall failure.

The guest announces progress and needs no keyboard input. Safe saved and
unmounted outcomes return automatically to Ubuntu. Uncertain ownership retains
the original static objects and halts visibly. Native acceptance remains pending
until the matching physical release/session is collected; host faults and QEMU's
unsupported-device refusal cannot establish it.
