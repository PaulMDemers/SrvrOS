# Air6,1 read-only VT-d window admission design

## Status

Source audit on `4bba4b93`, after accepted boot capture `ab360d3ce2d281e4`.
The AP-only checked backend and explicit v6 admission bridge now have native
window-only qualification: release `92d9f134639f509c`, source `cca4d31f`, completed
mapping and verified retirement/claim release in77.402ms within100ms. Complete
raw30-table census and matching ecam/window/exit receipts are retained. Target
access remains absent and no register-read, global resource or ownership grant
follows. The [ECAM/root capture](macbookair6-1-ecam-mapping-design.md) remains a
separate prerequisite. A guarded register-read caller and its explicit A/D and
live-admission contract are later milestones; saved receipts cannot grant them.

## Evidence and remaining obligations

| Evidence from the exact saved boot | What remains live at use |
| --- | --- |
| Two equal raw passes over all30 managed-root tables, with zero leaves covering `0xfed90000` | Fresh complete alias admission and root/mutation exclusion for the whole operation |
| NX capability, EFER.NXE and CR0.WP observed | Fresh control/cache/firmware endpoints; saved values grant no current permission |
| Exact pinned DMAR declaration and catalog gap | Exclusive exact-page resource claim and exclusion of competing clients |
| Candidate PAT3 encoding classifies strong UC | Actual supervisor RO/NX/non-global4KiB leaf and translation/readback checks |
| Checked ECAM mapping observed | Separate VT-d publication and expected-leaf retirement protocol |

The accepted proof is pre-user-init, sole managed kernel root only. It does not
cover firmware/SMM roots, future user roots or another CPU/client lifetime.
It never authorizes walking a VT-d root/context table or changing translation.

## Current source findings

- `kernel/src/memory/pmm.c` initializes tracked frames reserved and frees only
  USABLE catalog entries; reserved-frame frees halt. This prevents an allocator
  free from being an MMIO grant, but provides no exclusive device-range token.
  The candidate is a catalog gap, not an enclosing RESERVED descriptor.
- `register_kernel_mapping` in `kernel/src/memory/vmm.c` saturates at128 rows.
  It is neither a complete reverse map nor a competing-client registry. Existing
  inherited/private roots and shared upper tables require actual-root coverage.
- `vmm_diagnostic_sole_kernel_root` checks some observational prerequisites for its CPU0 caller
  (masked IRQs, expected/current kernel root and no registered user roots).
  It does not itself validate CPU identity or parked AP state.
  Its header explicitly supplies no lease or SMP proof. A saved successful call
  cannot exclude later root creation or a different executing CPU.
- Generic `map_page_in_space` can allocate intermediate tables and its existing
  mapping path compares physical address without proving exact requested flags.
  It supplies no exclusive MMIO token or poisoned publication state.
- Generic `vmm_unmap_page_in_space` clears a present leaf and locally invalidates
  when that root is current. It takes no expected raw leaf, performs no checked
  post-clear readback and retains no uncertainty token. `protect` likewise
  operates without an expected-leaf ownership token. These are ordinary API
  semantics, not an implementation of the proposed device lifetime.
- The checked ECAM backend supplies useful publication/poison concepts but owns
  a different permanent boot mapping and is explicitly not a device-resource
  lease. Do not borrow or retire its frame/window for this operation.

## Proposed checked lifetime

1. Admit only an explicit exact-unit diagnostic epoch. Validate current CPU0,
   parked/nonexecuting AP state, sole kernel root, masked IRQs and exclusion of
   mapping/root/client mutations. Refuse other roots/clients instead of inferring
   safety from a captured root address or the public mapping list. No allocator,
   callback, scheduler or logging path may release this exclusion implicitly.
2. Acquire a token for exactly `0xfed90000` through `0xfed90fff`, tied to the
   diagnostic owner and epoch. Reject overlap, reentry, stale tokens and a
   retained/poisoned claim. Keep `0xfed91000` excluded. PMM reserved tags and a
   DMAR declaration are inputs, not this claim. All possible in-epoch clients
   must be excluded or participate in the claim mechanism; a private list that
   generic mapping clients can bypass is insufficient.
3. Revalidate pinned firmware/catalog, exact absent destination and fresh full
   root census/cache/NX/WP state. Refuse any pre-existing target alias for this
   first implementation. Hold the admission epoch through verified retirement;
   do not place a gap where another root/client can observe or mutate the leaf.
4. Stage only necessary private table backing, tracking original absent parents
   and ownership. Never allocate the MMIO target as RAM. Publish the exact
   supervisor RO/NX/non-global4KiB UC leaf under uninterrupted exclusion, verify
   every owned path/leaf and invalidate locally before exposing a pointer.
   No existing entry may be overwritten or accepted on a PA-only comparison.
5. Only after separate backend qualification may a caller perform the existing
   audited aligned32-bit sequence, with fresh guards and exact translation
   before each load. Retain the [read contract](macbookair6-1-dma-inventory.md),
   its two samples, twelve loads/sample,100ms/pass,20–100ms gap, million-iteration
   gap cap, at most two mappings and65536-byte receipts. No new offset, write,
   target-table walk, polling loop or policy change follows from this design.
6. Retire only the owned expected raw leaf/path under the original epoch, clear
   and read back, invalidate locally and verify absence before releasing claims
   or reusing frames. Define expected hardware A/D behavior explicitly; never
   mask a changed measured census or use a PA-only match as ownership proof.
   Free only private, proven-detached backing; borrowed ancestors stay owned by
   their original owner. Preserve all ordinary allocator integrity checks.
7. Before any publication, a proven clean refusal may discard detached private
   backing and release the token. Uncertainty at/after publication, readback,
   invalidation or retirement latches terminal poison and retains the claim and
   affected backing until reboot. No retry, generic-unmap fallback or success
   receipt can follow uncertain retirement.

## Qualification before a read caller

Actual-source host fixtures must cover overlap/stale-token/reentry refusal,
changed root/AP/IRQ/control/cache/firmware state, full-census refusal, unexpected
existing aliases/ancestors/leaves, allocation failure before publication,
changed parent or leaf, failed readback/invalidation/retirement and poison that
survives retries. Check exact permissions/cache and untouched borrowed mappings;
resource ownership must survive every uncertain path. Existing generic mapping,
ECAM, process and integrity regressions remain required for any shared edit.
Native proof would first qualify the window lifetime without target loads.
A later separately opted-in read profile requires its own staged VM/native
receipts. Neither host models nor the accepted historical boot prove that step.

## Offline replay

`tools/haswell_vtd_admission_audit.py` keeps legacy context-only mode and adds:

```sh
python3 tools/haswell_vtd_admission_audit.py \
  --ecam-log /absolute/path/ecam.log --exit-log /absolute/path/exit.log \
  --session session-0000000000000000-00 --output /absolute/path/audit.json
```

Use the actual matching session. Both new arguments are required together;
missing, corrupted, mismatched or incomplete raw proof refuses without fallback.
`HOST_BOOT_ADMISSION_AUDIT_BLOCKED` records observed history and receipt hashes.
Top-level `*_proven`/authorization fields remain false for live admission,
including `nx_enablement_proven`; historical NX is reported separately.


## Dormant backend implementation

[`air61_window.inc`](../kernel/src/memory/air61_window.inc) is included only by
the dedicated diagnostic VMM build. The ordinary VMM instruction/relocation
output remains identical and excludes the new entrypoints. The AP ELF retains
prepare/retire, with no boot caller. The [header](../kernel/include/srvros/air61_window.h)
requires the trusted caller already to hold CPU0/IRQ/AP/client exclusion before
entry and supplies fresh gate/RAM/clock callbacks; that native bridge is not
implemented. In particular, the software token is not a global MMIO lease
against generic clients that bypass this epoch, and PAT3 encoding alone is not
a fresh effective-UC proof. The host admission tool continues to deny live grants.

The fixed path is PML4[384] / PDPT[7] / PD[409] / PT[80]. Existing nonzero
PDPT[7] refuses, even if it describes an empty subtree or absent metadata. A
fresh guarded census retains the original64-table/32768-entry/4096-byte and
shared100ms bounds, refuses target aliases and records all existing table
frames. Allocation must return two distinct tagged RAM frames outside that
graph. Both VT-d pages are explicitly rejected as table sources or allocations.
Private PD/PT pages are zeroed and checked before one atomic expected-zero
parent publication. The exact leaf is `0x80000000fed90019`: supervisor, RO, XD,
non-global, PAT slot3. Effective UC/NX/WP and firmware/CPU permission remain
obligations of the fresh bridge. No target pointer or load operation is exposed.

The separate mapper budget remains65536 checked RAM words; the census keeps
its existing entry accounting, including its own checked HHDM paths. The
100ms whole window lifetime also bounds the nested census and retirement;
there is no clock restart. A host fixture's successful lifetime uses60451
mapper words; its synthetic clock does not measure native performance.

Preparation verifies borrowed root/PDPT contents, owned frames and every private
word. Publication and unlink use expected-entry compare/exchange, immediate
readback, fenced local invalidation and full path/private-page verification.
Retirement unlinks only the exact owned parent, checks absence and frees only
proven-detached owned pages through normal PMM integrity checks. Target reads
are unsupported: any unexpected A/D change in the owned path/leaf poisons the
operation. A future read caller needs its own explicit hardware-A/D contract;
this backend does not silently mask it.

One owner/epoch/root token and one attempt are allowed per boot, including after
clean refusal or successful retirement. Invalid/stale tokens do not disturb the
active owner. Before publication, a proven clean failure frees private frames
and releases the local claim. An attempted uncertain publication, invalidation,
readback or retirement latches first-failure poison, retains the claim and any
still-owned backing, and rejects later prepare/retire calls without more I/O.
Reentry is terminal poison. If the first detached free succeeds and the second
fails, the remaining frame and claim stay retained; no duplicate free is tried.

`make test-air61-window` includes the actual VMM and census with injected
physical RAM/CAS/invalidation/free primitives. It covers exact translation and
borrowed preservation, allocation/source/clock/word-cap failures, raw census
changes, existing slots/aliases, borrowed/duplicate/MMIO allocations, stale
tokens, failures before/after parent writes and clearing, readback corruption,
changed A/D/permissions/physical entries, root/IRQ/frame ownership changes,
reentry and partial cleanup with poison that survives retry. The fixture is
also checked under ASan/UBSan and compiled for ARM64 with assertions enabled.
Native lifetime, gate and caller qualification remain pending.


## Explicit v6 window-only integration

`--haswell-window` requires all v5 firmware/cache/pre-init/ECAM flags and automatic
return. It adds only `srvros.vtd-window=air61-no-read`. The dispatcher holds CPU0
IRQ exclusion across the existing pre-user-init context capture and the new
window operation, before creating the first user root. The bridge uses a distinct
instance of the existing exact-unit gate implementation, preserving early ECAM
evidence. It rechecks Intel40651/39-bit/BSP0, CR4=0x620, WP/NXE, four unlaunched
AP records, pinned DMAR/MCFG and complete catalog, exact adjacent gap resources
and strong UC for the target with PAT3. Legacy CF8 selection is restored; no
configuration-data or VT-d register write/read is added.

All possible ordinary CPU clients are excluded for this pre-user epoch: CPU0
IRQs stay masked, AP launch pointers stay null, the sole-root guard excludes user
roots, and selected validators/PMM operations do not yield or invoke other mapping
clients. The local claim is not exported as a general lease after this epoch.
A clean refusal restores IRQ state and saves failure; poison retains IRQ
exclusion/claim/backing and halts before user init or USB reuse. Its screen shows
latched gate/backend/census progress for a photograph and Ubuntu recovery.

The backend retains complete observed root/PDPT before, linked and retired
arrays and verified private PD/PT words before freeing. A separate `window.log`
uses canonical base64 chunks, lossless zero/delta encoding, at most33000 raw
bytes and the unchanged65536-byte log tail. Firmware/cache/control records use
the existing strict gate layout and caps. Both complete census blobs are kept.
Reporting performs no table/device reread. `exit.log` binds its saved status,
root, epoch and session; `ecam.log` retains the independent early proof.

The collector requires all three raw proofs, final completion and exact binding
to the pre-user context. Each measured census still compares every raw bit.
Between the two distinct historical epochs, only monotonic hardware A changes
on present entries and D changes on writable present leaves are accepted; any other
graph/permission/cache/firmware change refuses. No such allowance exists inside
either two-pass census or the owned window path. A successful result is only
`OBSERVED_RETIRED_WINDOW_ONLY`, with zero target loads/writes and no register-read
or graphics/device ownership authorization. Native timing/retirement acceptance
requires the new exact-stage run, not host clocks or old accepted captures.


### HHDM effective-permission validation

The dedicated window backend verifies each complete HHDM translation before
returning a source/store pointer. Upper entries may carry U/S while supervisor
leaves exclude user access. Require cumulative writable access and a supervisor
leaf at4KiB/2MiB/1GiB; never accept an effectively user-accessible path. Presence,
reserved bits, source provenance, canonicality, large-leaf alignment, exact
physical translation and existing timing/accounting remain checked. No pointer
is approved early. This changes no owned window entry or borrowed-page raw
equality rule; post-publication U/S mutations still retain poison/backing.


### Bounded clock scopes for RAM accesses

The census still performs its fresh HAC clock check before each logical word,
then walks/validates every HHDM entry and loads that word. A one-use ticket
connects those adjacent callbacks; another read cannot reuse it. Only redundant
ACPI PM samples between at most four checked HHDM loads and one data load are
coalesced. The next HAC boundary and final clock check reject an elapsed or
stopped lifetime before census acceptance. No translation/provenance grant is
cached. Every individual physical source retains RAM and root/exclusion checks.
Native classification callbacks are bounded software-only catalog checks.

Ordinary reads check entry and completion; stores also check immediately before
mutation and afterward. A delay inside a walk cannot authorize a store, and
a deadline after publication retains poison/backing. Whole-lifetime100ms, all
accounting/census caps, raw A/D/permission/address equality and mapping attributes
remain unchanged. `hsw-window-cost` records clock/load/guard/RAM/logical counts
and gate/census timestamps already observed, including failed outcomes, without
extra measurement I/O. Older receipts remain readable with cost unavailable;
new trials require the cost record independently of strict mapping proof.


### Authoritative occupancy and source-row indexing

The VMM's64 live occupancy flags reside in one authoritative bitmap. All slot
selection, lookup, registration and removal use that source; no per-record used
flag or duplicate empty-registry cache remains. Atomic bit updates preserve the
other slots. The sole-root predicate retains every IRQ/CR3/kernel-root/count
check and reads the entire bitmap live, including count/bitmap contradictions.
Record/table capacities and lifecycle ordering are unchanged. This shared
representation update needs matched generic VMM and guest regressions.

Only the window gate indexes its already fully validated immutable private
catalog snapshot. A sorted permutation of original row indices makes the same
unique full-page/type predicate logarithmic; each call reads actual row bounds
and type. The validated nonoverlap invariant proves uniqueness. The index
caches no classification, pointer or permission; stale count/invalid indices
refuse, and fresh whole-catalog continuation comparisons remain unchanged.
Early ECAM uses its original scanner. `ram_steps` extends the optional cost
record with row comparisons; previous records remain readable without it.
Native trials require the new field along with the full raw proof. No alias,
ownership, clock, word, table or data limits change.
