# Air6,1 read-only VT-d window admission design

## Status

Source audit on `4bba4b93`, after accepted boot capture `ab360d3ce2d281e4`.
This is a design and host audit, with no kernel implementation or target access.
The [ECAM/root capture](macbookair6-1-ecam-mapping-design.md) removes a diagnostic
blocker; it does not grant a device resource. The next implementation should be
a dormant checked resource/mapping backend with actual-source failure tests,
before adding any native register-read caller.

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
