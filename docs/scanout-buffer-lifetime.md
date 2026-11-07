# Scanout buffer ownership and retirement

This is a CPU0-serialized lifetime mechanism and an opt-in shadow publication
trial. It does not program a native plane or GGTT, provide display ownership or
advertise a new desktop capability. The ordinary presentation API and VirtIO
backend remain unchanged. Read the [Air7,2 display plan](macbookair7-2-display-presentation.md)
and [presentation contract](display-presentation.md) alongside this document.

## Resources and identity

One lifetime borrows inherited target0 and owns two disjoint page-aligned RAM
allocations, targets1/2. A non-reset boot pool assigns monotonically increasing
epochs. Tokens bind object, epoch, operation serial, target and publication kind. Every
mutable entry, including queued starts, clock/guard faults and RAM release,
authenticates the caller's epoch before changing state. Reusing a PID cannot
refresh a stale queued job's authority.
A reopened object requires both previous releases and a newer epoch; stale,
foreign and duplicate tokens cannot complete work or release RAM. Every public
transition checks the state invariant. The caller serializes access; this is
not an SMP memory-ownership primitive.

`SCANOUT_RAM_PIN` is established only after private PMM allocation. The native
adapter additionally requires unambiguous firmware-USABLE coverage below4GiB,
separate from inherited aperture/stolen memory. The profile is copied by value.
Hardware-mode admission requires separate GGTT reservation, DMA authority,
route ownership, restore and firmware-exclusion assurances. No current adapter
establishes those bits or submits a hardware-mode request. The Air6,1 diagnostic
owner and its seven unknown assurances are unchanged.

## Publication and closing

| Operation | Required ownership and resulting lifetime |
| --- | --- |
| Copy begin/end | Only an inactive owned target is writable. No copy can start while a publication is submitting/pending. Successful full copy makes it ready; canceled copy must drain before retirement. |
| Request | Reserve the target and mark SUBMITTING before entering any writer. Keep both old front and new target pinned. Serial/epoch identify this exact publication. |
| Issued | A provably zero-write rejection can roll back. Issued work becomes PENDING. Any uncertain write poisons the lifetime and retains both allocations. |
| Completion | Only the exact pending token, observed target and publication kind can advance front. The adapter must independently prove device completion/old-front retirement; a matching integer alone is not that proof. |
| Close / owner exit | Block new copies/publications. Drain an existing copy or pending request. If the logical front changed, request and complete restoration to inherited target0 before retiring. |
| Release | Claim each owned RAM release once, only in RETIRED with no copy, pending request or owned front. Never release inherited target0. |
| Fault | Changed route, uncertain publication, missing/late completion or clock rollback retain all owned RAM and the owner object. Close/reopen/release cannot bypass the fault. Recovery needs a separate audited hardware-retirement mechanism. |

Each publication has a100ms monotonic deadline, including the submitting phase.
A matching event after the deadline cannot free memory. Serial/epoch/deadline
arithmetic refuses exhaustion. Waiting, allocation, copying and hardware access
stay outside this pure transition core. A future adapter must mark uncertainty
conservatively and preserve this state across cancellation; it cannot reset the
object merely because a caller exits.

## Native shadow trial

`--intel-lifetime-capture` first runs the exact Air7,2 display/backing readouts.
It then uses actual private PMM buffers, the same transition core and a read-only
native guard. The guard checks PCI/firmware/CPU translations, inherited plane
and well endpoints and samples the real frame counter. A frame advance paces
an explicitly SHADOW completion; it is never reported as a physical page-flip
completion. No native writer exists in this path, and the GUI cannot select it.
The final full fence/PTE traversal must match the initial backing observation.

The eight cases are ordinary alternation/restore, cancel while copying, cancel
before publication, cancel while pending, owner-exit while a second target is
pending, injected uncertain publication, injected timeout and injected guard
fault. The first five retire and free ten buffers. The last three deliberately
retain six buffers until reboot, with exact expected reason/state accounting.
They are software fault injections on private RAM, not physical GPU failures.
No per-frame allocation occurs. Copy work checkpoints every64KiB inside the
existing kernel service window. Each wait has a clock deadline and finite
fallback count; the entire trial has a30-second bound.

At1440×900/pitch6144 the trial allocates21600 frames across eight lifetimes,
releases13500 and retains8100 (about31.6MiB) on the three expected faults. It
performs12 full private copies,13 logical requests,10 matched shadow completions
and3 logical restores. Copy work touches private RAM only. Ordinary console progress continues through
the inherited framebuffer; its34MiB fence and firmware allocations stay intact. Progress is printed in the guest and complete
versioned receipts are saved, including every logical completion's epoch,
serial, previous/next target, before/after frame counters and timestamps. Handled refusal/failure returns automatically only
after successful save/unmount; persistence failure halts visibly.

The separate `--intel-lifetime-fixture` profile exercises the same PMM/lifetime
code in QEMU with synthetic timing and no Intel admission. Its report must say
`kind=fixture`, `ownership=shadow`, `hardware_writes=0` and
`hardware_completion=0`. It cannot accept native timing or display ownership.
The collector checks case identities, epochs, accounting, quarantine reasons,
timing and publication kinds. A successful native shadow report additionally
requires the complete native display/backing prerequisites. Missing/truncated or
mismatched receipts do not pass.

```sh
make -j2 all test-scanout-lifetime test-intel-scanout-trial
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL \
  --intel-lifetime-fixture
python3 tools/hardware_boot_smoke.py --probe-check
# Stage a separate exact native candidate after the fixture:
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL \
  --intel-lifetime-capture
python3 tools/hardware_boot_smoke.py --probe-check
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

Host coverage includes partial allocation, firmware RAM refusal, active-front
copy exclusion, rejected/uncertain issue, delayed/stale/wrong/duplicate events,
copy/pending cancellation, restore, double-release, retained faults, epoch and
serial exhaustion and deterministic event interleavings. Actual producer/decoder
fixtures validate receipt loss and false hardware claims. The ordinary VirtIO
normal exit/owner death/reacquisition regression remains required. Native shadow
acceptance, real GGTT/plane programming and visible tear-free desktop acceptance
are distinct checkpoints; only the bounded shadow trial below is accepted.

## Air7,2 native shadow acceptance

Source `ae598610`, release `d79f3ca9a74fa201`, session
`00065f687e92a7ff-00` completes all eight cases with exact resource, token and
completion accounting. The native1440×900/pitch6144 run allocates21600 frames,
releases13500 and retains8100 on the three expected injected faults. DMA grows
57→8157, exactly matching retention. Ten shadow completions observe bounded
frame progress; the maximum wait is16635us and the full trial takes169137us.
Full inherited fence/PTE preservation, zero hardware control writes, complete
saved reports, clean unmount/automatic Ubuntu return and offline filesystem
integrity are verified. The [unit record](machines/macbookair7-2.md) owns this
physical evidence. Private buffers were never mapped to the GPU. Actual GGTT
reservation, DMA/firmware authority, native route/restore ownership and real
flip completion remain prerequisites for a hardware writer and desktop backend.

The subsequent [address reservation contract](native-scanout-reservations.md)
adds software leases for explicitly owned GPU address intervals, protects the
full inherited fence and requires independent translation/reader retirement
after exposure. It has no native managed-range importer or GGTT writer; the
accepted shadow trial remains unchanged.
