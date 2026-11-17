# Private command-memory ownership

## Full-GT composition (native qualification pending)

The separate `intel-gt-arena-v45` profile, `build/intel-gt-arena/srvros.elf` and
`srvros.intel-ring=gt-bootstrap-arena-v1` recipe bind this CPU arena to the
original full-GT reset/new-epoch/memory child. Bootstrap wire7 requires both
`intel-arena-memory` and `intel-arena-binding`; earlier wires reject them.
The [current Air record](machines/macbookair7-2-current.md) owns trial status.

Four tagged DMA pages and their WB supervisor/NX CPU aliases are allocated and
zeroed before GPU exclusion. After reset and the original remapper/GGTT child,
the arena registers against that actual GT object, original borrow, new epoch,
CPU root and enclosing clock origins. Two admissions bracket fresh mapping/cache
observations. No content accesses or allocation run inside the GPU lifetime.
Requester restoration, logical borrow return and the final guard require the
original child to have returned with its RAM still held. Cleanup independently
requires the original GT/HDA/outer lifetimes and all15 original aliases to retire;
then it verifies all2048 zero QWords, removes all4 exact CPU aliases and releases
the allocation exactly once. Unknown retirement retains the arena and fails.

Practical250ms preparation,100ms GT maintenance,500ms GT total,750ms HDA and1s
outer bounds are unchanged. CPU setup/cleanup each retain separate100ms bounds.
The command library selects its enclosing diagnostic timing aliases rather than
depending on legacy reset types; default and v37 timing values stay unchanged.
Callback snapshots seal hold identity data, including the final setup clock and
the cleanup clock before deliberately ending the hold. Parent identity is checked
between CPU-cache and alias callbacks, so identity loss stops the following call.

Before the clock callback, held samples capture the original parent's latest
sample; cleanup samples capture the actual outer-return sample and the retired
GT/HDA/outer endpoints. A backward or missing floor refuses before further
effects. The binding reports `clock`, failed `at`, `previous` and the exact
`outer_end`; the receiver independently matches the outer wake record. Scalar
comparisons add no hardware census or enclosing deadline restart. First-held and
first-cleanup regressions keep global time advancing, and a compiled omission
control must miss those faults while preserving ordinary positives.

This composes CPU-owned storage only. Zeroed context pages are not an executable
LRC, a private GPU interval or permission for PTE writes/submission. All255 wider
DMA/cache/reader obligations remain open. The profile preserves all36 prior
qualification slots and adds6 arena/legacy/image slots (42 total), plus both
exact-stage VM paths. No VM result accepts native ownership.

## Legacy source composition

The separately selected `intel-command-memory-v37` diagnostic prepares private
RAM for a Broadwell blitter ring, context storage and completion page. It owns
the backing and actual CPU mappings through the original GPU diagnostic, then
checks preservation and retirement. One original kind2 Air7,2 lifetime is
[accepted](machines/macbookair7-2.md#native-private-command-memory-accepted) on
`4864ad3a` / `838082d16f6be16d`; enabled route and repeatability remain pending.

This is the backing/mapping prerequisite for the
[BCS completion protocol](intel-bdw-bcs-flush.md). It does not initialize an
executable context, install GPU translations or submit commands. GPU address-space,
cache visibility and engine/context ownership must be established independently
before the protocol can execute.

The host-qualified [GPU mapping transaction](intel-command-mapping.md) composes
this returned CPU owner with an already transferred interval. Its optional
`consumer_begin/live/end` helpers register one dependent on the exact RAM hold
before callbacks. While registered, ordinary `returned()` and `close()` refuse,
also blocking the native parent-return gate. Only clean mapping retirement may
return that dependent. These helpers grant no GPU authority. The isolated
[native provider dry run](intel-command-ownership.md) now uses them for paired
observations; ordinary and v37 paths do not. The original zero-preservation and
parent-retirement rules remain.

## Arena and CPU mappings

One original `intel_command_memory` object owns four contiguous allocator-tagged
DMA pages below4GiB. Page0 is ring storage, pages1–2 are context storage and page3
is completion storage. The two-page context reservation follows pinned Linux
v6.12's [non-render context size](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_engine_cs.c).
Zero storage is not a valid LRC image; the generation-specific
[LRC initialization](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_lrc.c)
and [context definitions](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_lrc_reg.h)
remain future implementation inputs.

The native adapter creates four dedicated4KiB supervisor writable/NX aliases at
`0xffffc001f5160000` in the original CPU root. Each alias must resolve to its
exact backing page, use PAT index0 and independently resolve to WB under the
captured CPU cache context. The allocator tag, HHDM translation and RAM backing
are also checked. These dedicated aliases are not GPU mappings or a claim that
the kernel's ordinary direct map does not exist.

The native sole-root walker requires IRQ exclusion. Initial cache capture and
each fresh cache/alias observation briefly mask interrupts and restore the
incoming state. Preparation and final cleanup otherwise run outside the GPU's
critical section; observations inside an already masked original child leave
it masked. Allocation and content initialization do not move into that scope.

Before acquiring the RAM hold, the adapter zeros and verifies all16KiB. A CPU
borrow initializes storage; a separate consumed serial protects the held arena.
No content access occurs while the original GPU child is active. Two parent
admissions and fresh mapping/cache observations bracket the child. The original
lease binds owner, provider, lifetime, root, epoch, sequence and all enclosing
clock origins. Callback identity, registration and counters must remain intact.

## Retirement and failure

The arena remains held after its child returns. Cleanup begins only after the
enclosing original engine/requester/reset and hardware-borrow lifetimes have
retired. It checks all2048 QWords, ends the CPU borrow, removes each exact alias
and verifies the returned physical address. Immediately before allocator release,
the adapter independently rechecks parent retirement and absence of all four
aliases. One allocation must have exactly one release; DMA counts must balance.

The selected original preparation bound remains150ms, GT300ms, HDA400ms and
outer500ms. The library's default preparation policy remains100ms. Storage
initialization and final CPU cleanup each have a separate100ms software bound
outside the GPU lifetime; neither restarts an enclosing GPU deadline. Zero,
backward or frozen clocks refuse. Partial mapping, lost registration, modified
contents, changed backing/cache or uncertain retirement retains the remaining
arena and fails the run. No forced unmap/free or automatic retry is permitted.

## Image, receipt and automated trial

`--intel-command-memory` selects only
`build/intel-command-memory/srvros.elf`. Ordinary images and the earlier v35/v36
diagnostics do not link this owner. The new wrapper is `intel-vtd-drain` wire5;
it requires the prior original remapper/GGTT chain and exactly one512byte
`intel-command-memory: v=1` packet. Older wires reject that packet. The reader
checks actual alias/cache policy, original timestamps, full closing counts and
independent CPU cache derivation. Closed success requires two parent-retirement
admissions. Raw logs are collected unchanged.

The manifest profile requires actual-core faults, strict receipt mutations,
fatal ASan/UBSan, compiled weakening controls, native2KiB frame checks, native
alias flags and image isolation. Exact-stage save/return and missing-workspace
QEMU paths must pass before installation. VM evidence accepts no native resource.
The unattended Air7,2 trial uses its own USB UUID and retained Ubuntu recovery,
saves progress/results, returns automatically when safe, and halts visibly on
uncertain retirement. Volume is1%. Only matching collected native evidence may
accept this CPU-owned arena; all GPU ownership/tear-free claims remain false.

## Native Air7,2 result and log framing

The accepted session owns four CPU-mapped pages through the full original
remapper/GGTT lifetime. Initialization64us, held observation118us and closing
preservation/alias retirement65us pass; four pages release once and DMA61→61
balances. Original preparation and return take103,671/150,000us. The accepted
scope includes zeroed context storage, not an executable LRC or GPU mapping.

The raw final log reached its64KiB ring bound and began inside the initial
command line. The normal reader rejected that fragment as an unknown GT receipt.
Reviewed collection recovered only311 known preamble/command-line bytes from
the complete preflight checkpoint, with39,456 exact overlapping bytes and
terminal snapshot sequence/length checks. All hardware bytes and strict validators
remain unchanged; raw files, rejected collection state, helper/tests and accepted
proof are preserved. Changed overlap, bad/duplicate footers and hardware packet
mutations refuse. This review is recorded separately from frozen qualification.

Future workspace snapshots start at a complete line, retaining the original
64KiB limit and total-byte sequence. Exact line boundaries keep their first
record; only a partial leading line is omitted. Raw bootlog snapshot/dump APIs
retain byte-exact semantics. Missing hardware records remain failures. The new
framing has host wrap/boundary/fault and native compiler validation, without
claiming physical acceptance of a new runtime or altering past raw logs.
