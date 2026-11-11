# Original-owner enabled-ring transition

This next diagnostic consumes the actual enabled BCS state observed in the
[disabled-entry trial](intel-entry-reset-composition.md#native-result-enabled-entry-refused).
Native acceptance is pending. It is separate from the zero-geometry reset and
previous shutdown images; their recipes, gates and evidence remain unchanged.

The original static HDA owner performs6→2 before GT wake. The14 original joint
aliases retain their addresses; only the original wake request and BCS control
pages may be writable. GDRST, the other four engine pages, plane and GGTT pages
remain read-only. A full original92-word completion census must match both
hardware samples and the exact captured enabled16, pending0, parser-idle3 state.
No observation of a quiet register replaces the explicit cooperative submission
handoff policy.

A sealed original GT child pins its source/provider and blocks early close.
Four BCS8 groups use the existing idle shutdown core: two exact source-bound
initial groups, at most one fenced0xf000 DWORD at0x2203c, and two matching
readbacks. Advance the parent's engine epoch before a possibly issued store,
even a known unissued result. The old census then becomes historical data only.
A fresh full92-word census at the new epoch must retain every word/context
except the BCS enable bit. The ring start and disabled length/control remain
nonzero; no address or geometry clearing is allowed. Two plane/HDA config
observations preserve all54 plane DWORDs and the full10-field audio endpoint,
including primary/cursor binding to the original display capture and Command2.

Keep BCS100ms, original-source admission age100ms, transition including borrow
return150ms, readers150ms, GT300ms, HDA400ms and outer500ms. Keep the first failed
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
image isolation, actual native access widths/permissions,2KiB frames,64KiB tail
capacity, exact staged VM save/unmount/automatic safe return and physical recovery
before staging a native test. Safe saved outcomes return automatically;
retained uncertainty halts visibly. Do not force enabled entry for coverage.
