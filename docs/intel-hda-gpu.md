# Joint display-HDA and GPU diagnostic lifetime

`intel_hda_gpu` composes the existing display-HDA inhibit owner and read-only
[GPU interval claim](intel-interval-claim.md) under one original CPU exclusion.
It is an implemented software core with an actual-core host fault model.
Native adapter, image, recipe and strict collector integration remain pending;
ordinary desktop startup and installed payloads do not activate this component.

Open requires the original empty GT owner, the already-held exclusion and the
original outer diagnostic clock origin. It pins that exclusion before callbacks,
opens the actual HDA owner, inhibits command6→2 and registers the original HDA
borrow plus its nested joint dependent. Successful open is active ownership,
not a completed diagnostic. Rebound copies, callback reentry and reuse refuse.
The caller must keep all objects and callback contexts alive through retirement
or retained fault. Large result storage belongs in static/heap storage rather
than a kernel stack.

Checks validate that exact HDA registration, nested counts, exclusion and GT/claim
identities, then take a fresh guarded HDA endpoint/clock check. They avoid another
full HDA census. A claim attaches once, before it opens, while the original GT
owner is awake. The actual claim still owns both six-DWORD observations and its
original re-arm child registration; the joint wrapper supplies no replacement
source, epoch or hardware authority.

The original HDA400ms and outer500ms origins never restart. Native integration
must route immediate I/O clock samples through `intel_hda_gpu_deadline`, which
advances the shared monotonic watermark and retains the first invalid raw sample
before the individual core receives refusal. `intel_hda_gpu_time` is the pure
predicate for an already-taken sample. Neither function supplies a fresh clock
or extends the original preparation100ms or GT300ms bounds. A sub-core's own
checks remain mandatory. Source failure is not relabeled as a clock failure.

Close requires the original GT owner cleanly closed (or never opened) with no
pins, borrowers or dependents. An attached claim must be cleanly closed with both
owned observations complete and bound. Only then may a separate two-pass HDA
midpoint inspection run, the nested dependent drain, HDA unpin and command2→6
restoration occur. The adapter must independently prove alias retirement before
the joint CPU pin is dropped. `gpu_complete=false` permits clean refusal cleanup;
it cannot produce a pass. An unfinished claim, lost registration, expired clock,
unknown store or failed alias retirement retains exclusion and refuses retry.

The model uses the real CPU exclusion, HDA observer/owner, GT, engine census,
takeover, preparation, held reset, re-arm and interval-claim implementations.
Its successful path has four full HDA inspections (232 reads), command6/2/2/6,
12 original-entry reads and no early restoration. Each injection uses a fresh
process because retained faults cannot be reset by a test-only production API.

```sh
make test-intel-hda-registry test-intel-hda-gpu
```

The fault matrix exercises each shared clock, HDA guard/endpoint/read and command
store position, raw drift, small cross-subsystem clock backsteps, copies, reentry,
lost dependent pointers/counts, unfinished claims and failed retirement. This is
host evidence, not native timing, DMA drain or physical tear-free acceptance.

## Remaining native boundary

A separate image/recipe must prepare and admit the root read-only UC HDA alias
before either HDA or GT stores. The same exclusion must cover the whole lifetime.
Each census can borrow that mapping logically, but reports must distinguish its
four inspection borrows from physical root-map creation/retirement. Do not reuse
the independent HDA image's four-physical-alias interpretation for this adapter.

Wire the new claim opening/closing reads into the existing sealed re-arm child.
Use cheap exact endpoint guards between full inspections, preserving full native
resource/cache/backing admission at the appropriate boundaries. Capture HDA
command phases correctly: boot6, GPU-inhibited2, restored6. Preserve every raw
engine, display, HDA and PTE word using canonical packing if needed; keep the
65,536-byte tail and all deadlines unchanged. Static/heap result storage, actual
native access widths/final clocks, strict partial-failure collection and exact
stage VM refusal/persistence must be qualified before installation.

No PTE, command submission, flush or invalidation callback is introduced here.
HDA inhibition remains a cooperative submission policy; it does not prove posted
writes, prefetch, firmware/SMM, cache/TLB or other-reader completion. All eight
GPU completion obligations remain open (mask255), assurances and managed pages
remain zero. A persistent desktop GPU service and page flipping need those
independent witnesses and their own maintained runtime lifetime.
