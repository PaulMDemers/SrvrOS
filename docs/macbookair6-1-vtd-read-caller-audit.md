# Air6,1 guarded VT-d read-caller audit

## Status and evidence

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
