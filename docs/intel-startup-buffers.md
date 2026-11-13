# Original-source private scanout buffer binding

The opt-in v31 `intel-startup-buffers` image joins two private padded buffer
allocations to the actual original startup reset/source lifetime. It is the
buffer ownership prerequisite for managed GPU memory. It does not transfer a
GGTT interval, expose RAM to a GPU, invalidate a translation or publish a plane.
Native acceptance is pending; the inherited framebuffer remains the desktop.

## Lifetime

Before entering the original bounded GPU lifetime, the adapter allocates two
disjoint private RAM spans below 4 GiB using the existing tagged allocator.
Each span contains all 1,356 pages of the 1440×900, pitch-6144, 904 padded-row
layout (5,554,176 bytes). Every page must be usable RAM, carry the exact DMA
allocation tag and have its expected CPU translation. The tag identifies the
allocation; it is not DMA/cache authority. Full patterns initialize and verify
all allocated bytes, including padding, through a CPU borrow. No imported or
inherited backing becomes allocatable.

The original-address `scanout_memory_hold` then registers on each memory owner.
It captures the allocation/token/epoch, serial, layout, extent and allocator
identity. While held, CPU-borrow admission, GPU preparation and RAM close/free
are blocked. Closing stops new admission and waits for the original hold to
return. Copies, stale tokens, callback drift and failed backing validation
cannot return the hold or free backing. An enclosing native-root pin also blocks
hold return until that actual root has retired cleanly; timeout is not retirement.

The separate source-observer API runs a registered buffer child after the six
original GGTT DWORD observations and before original source return. It captures
both original RAM holds, their enclosing root pin and the original source,
entry kind, reset epoch, post-census, provider and time origins. Every clock and
pure guard validates their continuity. The child blocks early source return and
both RAM hold returns. Its two one-page Broadwell encodings are alternative
plans for the same observed candidate, not two granted mappings or a grant for
either full buffer. Exact original entry and private physical addresses remain
bound through the final guard. A real transfer attempt with zero assurances and
255 open obligations must refuse before any provider callback or arena change.

The buffer child returns to its original source, then reset, HDA/GT owners and
CPU exclusion retire. RAM holds remain pinned through that entire outer lifetime.
Only a verified safe outer result allows their return, fresh CPU borrows for
full pattern verification, and exactly one release of each allocation. A partial
initial allocation may unwind only as known-private unexposed RAM; uncertain
source/root/backing/retirement retains all affected holds and storage until reset.
No forced cleanup, replay or optimistic restore follows uncertainty.
A backing failure found after clean GPU retirement quarantines private RAM;
it does not relabel or reacquire the already-retired GPU root.

## Limits and isolation

The source callback shares the existing preparation 100 ms and original GT
300 ms origins. Entry 150 ms, HDA 400 ms, outer 500 ms and all request/reset
phase limits remain unchanged. Allocation, full pattern work and allocator
validation occur outside these hardware lifetimes. They never restart them.
The new child performs pure identity/codec checks and same-origin clock samples;
the enclosing source's full native guards bracket it. Its returned observations
are historical data and cannot authorize a later live operation.

Build `build/intel-startup-buffers/srvros.elf` and stage with
`python3 tools/hardware_boot.py stage --laptop-probe-volume LOCAL_UUID --intel-startup-buffers`.
The separate image has its own firmware request, exact 15-token recipe
`srvros.intel-ring=startup-buffers-v1`, version-31 wake identity and
`startup-buffers-preinit`. No user services start. Old v28/v29/v30 receivers,
images and exact recipes remain distinct and refuse raw v31 receipts. The
source-observer fields/API exist only in this compile variant; ordinary source
runs never invoke its observer. The shared RAM hold API supplies no native grant.

## Collection and trial

The collector field is `intel_startup_buffers`. Two `intel-source-buffer` rows
capture complete private extents, memory epochs/hold serials, release counts and
alternative one-page encodings. A distinct `intel-source-buffers` row binds the
original source/kind/epoch/times, registration/return, allocation/pattern counts,
transfer refusal and retained/success outcome. The strict receiver requires the
complete original reset/source and enclosing native prerequisites, preservation,
retirement and checkpoint identity before native completion.

Shared receipt schemas use a checked internal parsing view: only an exact v31
image/recipe/pre-init can select it, raw words/times/counts stay unchanged, and
its returned data reports the actual version 31. It is never written as raw
native evidence or exported as v30 acceptance. The full collector saves original
bytes before parsing and leaves the v30 result field empty. Missing/changed
rows, aliases, source/epoch splicing, partial children and safe refusals remain
nonpasses. All 255 obligations, zero hardware assurances and zero managed GPU
pages remain; this trial cannot accept native tear-free presentation.

Use the [Ubuntu device workflow](ubuntu-device-agent-setup.md): immutable exact
stage VM validation, independent payload/evidence banks, offline media/recovery
checks and recorded one-shot installation. The automated trial needs no input,
keeps volume 1%, shows progress and persists results. Proved safe saved/unmounted
results return automatically; retained uncertainty halts visibly. Collect the
matching session before another run and accept only the startup route observed.

## Local qualification and current trial boundary

Frozen native `876b500e` passes the full 36-image/core/USB/host/docs gate, eight
actual-adapter tests under fatal ASan/UBSan, fatal RAM-hold tests, nine historical
native replays and image/receipt isolation. Both original routes, every original
reset/source clock point, new child clocks and original token/object/owner-ID,
backing, provider, registration, plan and return faults are covered. Native object
and staged writer/alias/static-lifetime checks and 2 KiB frames pass. Saved tail
is 60,134/65,536 bytes including 1,536 reserve; no deadline or log limit changed.

Host-only `7c3a7531` registers the profile-name allowlist and adds a positive
actual staging regression after the first staging attempt refused. All 111
staging tests pass and every native image hash remains exactly the qualified
`876b500e` artifact. The qualification bridge records these distinct sources;
it does not pretend the broad/sanitizer commands ran on a later commit.
Release `c154f8b4a6534d11` passes its exact staged EFI/MBR VM persistence/unmount/
automatic return, own compiled recipe/pre-init and missing-workspace halt checks.
Foreign-board VM refusal performs no GPU stores or private buffer allocations.

Local USB integrity, offline media, installed recovery/default/unarmed one-shot,
independent payload/evidence banks and candidate plus 32 MiB EFI margin verify.
Only an unreferenced older unaccepted diagnostic's EFI duplicate was retired;
its verified payload and raw refusal remain durable. Current accepted v30 and
recovery remain installed. This candidate is banked, **not installed or armed**.
Native binding and timing remain pending for each observed route. Host/VM
success does not grant managed GPU pages or accept native tear-free presentation.
