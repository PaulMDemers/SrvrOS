# Air6,1 guarded VT-d read-caller audit

## Status and evidence

Native follow-up (2026-11-07): PR410 runtime `f8e16123`, payload
`517661c4c5a9627f`, completes the separate two-pass A-preseeded zero-load
qualification in82.610/82.379ms, with identical raw borrowed graphs and proven
retirement/claim release. The [machine record](machines/macbookair6-1.md)
owns its exact evidence and narrow acceptance. No target register was read;
the guarded read caller described below remains unimplemented. All existing
limits and raw comparisons remain unchanged. The original host audit follows.

Base `eefc5bfd`, following the accepted [window-only qualification](macbookair6-1-vtd-read-window-design.md).
Source `cca4d31f` / release `92d9f134639f509c` completed its single no-load window
in77.402ms, including a54.524ms census, with strict raw proof and verified
retirement. This is historical mapping evidence. It grants no current register
access or ownership. This audit changes host tools/models only; no kernel,
installed payload, boot recipe or native target-access API changes.

The [host audit and models](../tools/haswell_vtd_read_lifetime_audit.py) strictly
replay matching window/ecam/exit files and retain their hashes. All native,
physical-acceptance, cache/alias permission and ownership flags stay false.
Synthetic path/sample/timeline validation never stands in for native admission.

## Register and paging reference

The [existing read contract](macbookair6-1-dma-inventory.md#vt-d-readresource-contract-audit-2026-11-04)
uses Intel [VT-d4.1](https://cdrdv2-public.intel.com/774206/vt-directed-io-spec%20.pdf)
sections11.1–11.2 and11.4.1–11.4.5: aligned32-bit accesses to VER/CAP/ECAP/GSTS/
RTADDR only. Preserve GSTS, VER, CAP high/low/high, ECAP high/low/high, RTADDR
high/low/high, GSTS: exactly12 DWORDs/pass, twice. No locked register operation,
command/fault acknowledgement, write or RTADDR dereference. Preserve all raw
words; unavailable, torn or changed brackets/samples refuse. Matching brackets
are not full-interval stability, and RTADDR is not loaded-table proof.

Intel [SDM3A](https://cdrdv2-public.intel.com/812386/253668-sdm-vol-3a.pdf#page=147)
section4.8 describes processor Accessed updates on translation and Dirty updates
on writes. Therefore the existing strict no-load path cannot simply acquire a
load caller: its initially clear A bits can change on translation. Broad A/D
masking would invalidate its raw-comparison contract.

## Proposed exact A-preseeded path

Design inference from that architecture: initialize A only in newly owned
entries, before publication. Use owned parent flags `0x23` and leaf
`0x80000000fed90039`; only bit5 differs from the qualified no-load leaf. RO,
supervisor, NX, nonglobal and PAT3 attributes are unchanged. Require the borrowed
PML4 entry already to have A set in the fresh admission; never set or clear a
borrowed A bit. The accepted raw root satisfies that historical prerequisite,
but every future pass must observe it again.

Compare every complete expected word exactly before and after each DWORD. Dirty
must remain zero. No adaptive A/D mask, A clearing after publication, hidden
priming target load or PA-only permission match. Pre-set A is not evidence of a
read: retain explicit raw load counts/sequence instead. Full private-page and
borrowed-page comparisons remain around the batch and retirement. The proposed
encoding and actual read path still need their own backend qualification.

## Two independent bounded lifetimes

The accepted backend is one attempt per boot, including after successful
retirement. Never reset it to NEW or extend its100ms deadline to insert a second
sample. Implement a separate bounded two-context series: one attempt per pass,
no third mapping, and no retry after refusal/poison. Hold CPU0 IRQ/AP/root/client
exclusion throughout the series and gap; neither logging nor scheduling may
silently release it. Each pass redoes complete current firmware/catalog/PCI,
CPU/cache/NX/WP and raw alias admission. Neither historical receipts nor pass1
observations are pass2 admission.

The interval20–100ms is measured from completion of pass1's last DWORD to the
start of pass2's first DWORD. It includes pass1 teardown and pass2 fresh
preparation, rather than adding an unconditional20ms sleep afterward. First
retirement must complete before second preparation/reuse. Any remaining minimum
interval wait occurs while the target remains unpublished, retains the million-
iteration stop and counts against pass2's100ms budget if that claim has begun.
Long preparation does not extend either pass or the100ms maximum interval.

Keep both full raw receipts, each within the existing65536-byte file tail.
First-pass evidence must survive second-pass preparation; hashes alone are not a
replacement. Maximum producer/retention tests must account for actual buffers,
BSS/root-graph changes and unchanged64-table/32768-entry/4096-byte census bounds.
Uncertain publication, readback, observed path/control change or retirement
retains its claim/backing and prevents the second pass or ordinary continuation.

## Guard budget conflict

Calling the current whole-window verifier before every DWORD is unsuitable:
it would repeat complete private-page scans and can exceed the65536-word cap.
Per-load validation needs the exact four-entry path plus fresh live admission
checks, while retaining the complete page scans at the existing lifecycle
boundaries. This is a new backend obligation, not permission to skip checks.

The existing complete window charges1840 firmware bytes. A full stable-source
check copies136 DMAR bytes plus60 MCFG bytes. Twelve additional pre-read checks
would charge4192 bytes, exceeding4096. Full pre/post checks would exceed it
further. The proposed arithmetic fits3996 bytes only after coalescing one
adjacent duplicate source check already covered by the immediately preceding
full phase1 guard under uninterrupted exclusion. No coalescing is implemented
or qualified here. The actual refactor must retain source comparisons and bind
any one-use result to that adjacent operation/owner/epoch; a general cached
source grant is prohibited. Post-read path checks and the lifecycle's full
post-batch source verification remain required.

The host upper-bound model uses worst unaligned136/60-byte copies (18+9 RAM
words), five physical RAM loads per checked word, and four-entry pre/post paths
for all12 reads. Starting from the actual fixture's61621 mapper words, the
proposal estimates63586 words. This is budget arithmetic, not proof about a new
implementation. Actual-source counters/fault tests must establish both bounds;
no byte, word, time, map, read or log cap is increased.

## Next implementation checkpoint

Implement and fault-test a dormant two-context, A-preseeded backend and bounded
raw-retention/guard mechanism first. Exercise stale/reentrant tokens, every raw
path bit change, wrong roots/CPU/AP/IRQ/cache/firmware/catalog, delayed/stopped
clocks, per-read limits, partial reads, unavailable/torn/changed samples,
publication/clear/invalidation/free uncertainty and poisoned state surviving
reuse attempts. Verify actual maximum source-byte and mapper-word accounting.

Then prepare a distinct native two-window/A-preseed qualification with zero
target loads, preserving the current accepted no-load payload and all refusal
paths. A separately opted-in register-read adapter and strict raw collector come
after that qualification, with matched builds, VM refusal/save/return/missing-
workspace checks. No read-enabled boot is prepared by this audit. Ordinary
native desktop regression for the shared occupancy change remains independent.

## Dormant backend qualification implementation

`SRVROS_AIR61_READ_WINDOW` additionally compiles the separate VMM engine in
`air61_read_window.inc`; `make air61-read-window-object` is compile-only. There
is no production gate bridge, boot option, device-load callback or target-read
API. The existing accepted profile cannot reset into this engine, and compiling
both profiles enforces mutual exclusion. Normal and existing AP builds do not
select the new flag.

The engine owns two independent contexts with separate 100ms deadlines and
one attempt each. Owned parent entries are exactly `frame|0x23`, the leaf is
exactly `0x80000000fed90039`, and borrowed root A must already be set. It checks
all raw bits, including A/D; it never masks hardware changes. Lifecycle checks
retain full borrowed/private pages and the three-pass alias census. Each of the
12 zero-load slots has a fresh source guard and exact four-word pre/post path.
The offset markers are qualification evidence, not fabricated register data.

One token bound to owner, epoch and root consumes only the immediately preceding
phase-1 source guard; all remaining full guards execute. Two actual instances
of the existing gate charge 3996 firmware bytes per pass. The fixture charges
63476 mapper words per pass, within the unchanged 65536 cap, including a
128-row catalog. A bounded wait before pass-1 publication measures the gap from
pass-0's last slot; the first pass-1 marker rechecks the 20–100ms interval.
Preparation and retirement count toward that interval and their own deadlines.

Both complete raw census/page/path/counter views remain in bounded RAM. The
second pass cannot update the first view. Callback-only RAM copies are current-
pass and fresh-check scoped and cannot access either target page. Uncertain
publication or cleanup poisons the context and retains ownership; no retry or
best-effort cleanup is permitted. The trusted future bridge must maintain
CPU0/IRQ/AP/root/client exclusion across both passes and the gap, and compare
fresh source/control identity at second admission. Host fixtures exercise that
obligation with actual gates; they do not accept native latency or ownership.

Full raw serialization, a fresh native admission bridge and the separate
no-target-load two-window physical qualification remain prerequisites to any
register-reading adapter. The accepted installed payload remains unchanged.

## Fresh bridge and lossless receipt

`make air61-read-qualification-objects` compiles the native bridge, its two
independent production gate instances, the VMM backend and receipt encoder.
No existing kernel target selects these objects, and there is no boot caller.
The host fixture compiles the same gate-instance source with hardware hooks.
Native instances have no reset entry point. The bridge permits one call, requires
current ECAM ACTIVE/single adoption, disables interrupts for the whole series,
and runs the unchanged exact-unit predicates afresh for each admission. Pass1
admission also compares raw source/catalog/MCFG and cache/control observations
against pass0. It restores the prior IRQ state only after a clean result. Poison
prints both passes' progress and halts with ownership and IRQ exclusion retained.

The RAM-only encoder is also one-shot after a clean complete series. It samples
no clock, register, port or device. Its closed `R1PASS61` records preserve each
pass's full borrowed/private page views, both raw census traversals, firmware/
cache/control snapshots, cost counters, indexed-classifier work and all12 exact
pre/post paths. Sparse page deltas refer to complete retained raw baselines;
zero runs reconstruct bytes exactly. There are no hashes or padding/pointers
in place of raw observations. Old W1 decoders refuse these distinct records.

The outer raw format is `R1AIR610`,27 ASCII session bytes, six little-endian
u64 values (owner,epoch,root,begin,end,gap), then three u32 counters
(started,retired,gap-spins). Each pass has a u32 extent and its raw pass record,
then slots/coalescings (u32), first/last slot times (u64), seven u32 cost counters,
three u64 phase times, a u64 RAM-classifier step count, and12 repetitions of a
u32 offset plus four u64 pre-path and four u64 post-path values. The raw cap is
70000, with each existing-style pass body still bounded to33000 bytes.

`Z1AIR610` wraps the raw length as a u32, followed by u16 run tags. Bit15 denotes
zeros; bits0–14 give a positive length. Zero runs must contain at least four
bytes; literal runs contain their following bytes. The canonical producer emits
the longest next zero or literal run up to32767 bytes. The strict decoder
requires exact canonical recompression, declared extents and no trailing bytes.
Packed output is capped at45000 bytes, leaving base64/framing room in the
unchanged65536-byte tail. Overflow yields no valid receipt and cannot retry.
The eventual boot wrapper must also bound the entire saved envelope.

`tools/hardware_air61_read_qualification.py` checks both complete raw proofs,
independent100ms lifetimes,20–100ms marker gap including retirement/preparation,
owner/root/epoch/session identity, source/cache/control stability, exact A/D/path
bits, budgets, costs and clean retirement. It also requires identical borrowed
census graphs across passes; any raw change refuses. This is an offline retained
observation only: source-completed/native-accepted, register-read authorization,
hardware authority and ownership remain false. A boot caller and strict joint
ECAM/exit/session persistence binding are still required before physical
qualification can be accepted. No target load adapter exists.

The final offline decoder also mirrors the native borrowed-root allowed-bit
mask (`address|NX|0x3b`), independently of raw graph/path equality. Coherently
repeating a forbidden flag in every raw copy is still invalid.29 flag variants
and512 individual path-bit variants are refused by real-producer regressions.
Before native boot integration, audit first-touch RAM/A-D effects against the
strict retained graph comparisons. Preserve existing bounded preparation and
all exact comparisons; do not hide differences with masks, timing exclusions
or larger budgets. This compile-only qualification does not accept native
startup, latency, source persistence or register access.

The separate v7 boot integration uses `--haswell-read-qualification` and its own
ELF, refusing mixed/legacy opt-ins. Next-pass private context/gate RAM is first-
touched inside pass0's original deadline before its census, with per-page live
clock/root guards; pass1 still performs all of its own bounded preparation.
Neither existing lifetime is reset or shortened by excluding work. The saved
R1/Z1 `window.log`, `ecam.log` and `exit.log` are validated jointly. Inside each
claim and across the two claimed snapshots all raw bits remain exact. The
existing W1 source-to-claim one-way hardware A/D relation between distinct
epochs is retained without expanding it; R1 explicitly returns every raw delta.
This does not create an A/D mask or a live permission from historical evidence.

## Separate guarded register sampler implementation

The explicit v8 `--haswell-register-sample` profile selects its own ELF,
`air61-sample-kernel`, and requires `srvros.vtd-window=air61-two-pass-read-only`.
The normal, AP and accepted v7 images reject this opt-in and have no sample
entry point. The new image rejects legacy/qualification tokens. Every native
load is a fixed aligned volatile32-bit access bracketed by compiler barriers
and LFENCE at one of the12 audited offsets. There is no caller-supplied target
address, callback or pointer-return API in native builds.

The existing fresh per-slot source guard and exact four-entry pre/post path
remain intact. Raw value, load timestamps and attempt/completion counts are
retained in the already-primed private pass contexts. Partial/unavailable
reads, zero version, torn high/low/high or GSTS brackets and changed second
samples poison the series, retaining claim/backing and IRQ exclusion; no
retry, USB reuse or ordinary continuation occurs. Both successful windows
still require complete full-page verification and proven retirement. All
clock/source/census/word/gap/iteration and receipt caps stay unchanged.

The separate `hsw-sample` envelope contains closed R2/Z2 records and
`R2PASS61` geometry with a distinct owner. It preserves each pass's full raw
proof plus12 raw DWORD/timestamp pairs and actual counters. R1 decoders refuse
R2; the isolated R2 consumer rejects R1, mixed profiles, unavailable/torn/changed
data, bad counts/times and altered path words. Shared context binding grants
no current access authority. A complete retained sample is only a historical
read-only observation: it does not prove full-interval stability, a loaded
translation table or permission to follow RTADDR, write registers or own DMA.
Native register-read acceptance remains pending matching physical evidence.

The read-enabled profile also retains13 fresh cache/NX admission snapshots per
pass: one before each fixed DWORD and one after the batch, all inside the
original claimed lifetime. Each snapshot uses the existing whitelist and
49-MSR cache-snapshot cap, plus one separately counted EFER observation.
These additional snapshots do not reuse or extend either of the two original
gate counters. Full raw values/times and per-snapshot counts fit the unchanged
33000-byte pass,70000-byte raw,45000-byte wire and65536-byte tail bounds.
No such extra MSR accesses occur in the existing zero-load profile.
