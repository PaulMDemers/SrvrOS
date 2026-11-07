# Idle Broadwell blitter disable and remaining readers

The native engine census found the firmware BCS ring enabled with empty sampled
queues. The next separate diagnostic can disable that exact already-idle command
front-end. This does not retire all graphics memory readers or grant a managed
GPU interval. Ordinary boot, wake-only and read-only census images cannot enter
this writer.

## Register authority and eligibility

Intel's [Broadwell PRM index](https://www.intel.com/content/www/us/en/docs/graphics-for-linux/developer-reference/1-0/broadwell.html)
links [Volume2c, November2015](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`.
Printed pages161–162 define BCS MI_MODE at `0x2209c`, including read-only parser
idle bit9 and Stop Ring bit8. Pages1345–1348 define ring enable and the state
invalidation condition when a disabled ring has equal head/tail. The additional
idle-messaging sequence there is explicitly Render CS; this trial never writes
Render CS controls. Stop Ring, reset and submission are absent from this path.

The new **cooperative boot BCS submission-domain policy** explicitly assumes
trusted conforming firmware yields BCS command submission after the pinned
EFI64/Limine boot handoff and does not submit during this isolated trial. This
is additional to the wake-requester policy, not inferred from empty queues or
idle samples. It does not prove opaque SMM absence. A dedicated compiled image,
exact14-token recipe, exact Air7,2 profile, CPU0/IRQ exclusion and all current
native wake/firmware/cache/backing prerequisites must hold. The caller also
requires a complete stable five-engine census, enabled mask16 and pending mask0.

The core independently requires the handoff grant and a live, identity-bound
awake parent lease. Two fresh eight-DWORD BCS snapshots must match the captured
legacy ring: head/tail0, start `0x03eef000`, control `0x0000f001`, execlist low/
high1/0 and MI_MODE low16 equal `0x0200`. BLT_MODE at `0x2229c` (pages177–179) must show
legacy/GGTT mode: its low16 may contain only the privilege-check-disable bit;
execlist, PPGTT,64-bit and interrupt-steering modes refuse. This register is
never written. All words must be nonfloating and both
snapshots agree. Other states refuse; the code does not force a busy parser idle.

The only permitted engine-control store is `0x0000f000` to `0x2203c`, once.
It clears enable while preserving the verified length and other control bits;
it writes no W1C wait bit, head/tail, submission, mode, reset or restore register.
UC ordering fences and subsequent reads provide posting/readback. Two final
snapshots must match and differ from the initial snapshots only in the control
enable bit. The live requester/ACK/C0 and resource guards remain mandatory.
The result is zero-initialized and consumed once; retrying it or reusing a copy
cannot clear uncertainty or reissue a store. The adapter is also boot-one-shot.
There is no retry or automatic re-enable.

The BCS budget is100ms, including a fresh pre-store deadline check. Zero,
backward, frozen or expired clocks cannot establish success. The parent wake
phase100ms/total300ms and full500ms bracket remain unchanged. A definitely
unissued control write can close the valid wake lease cleanly and report
incomplete. Any possibly issued write with uncertain readback, ownership, clock
or subsequent reader state retains wake, mappings and CPU exclusion until reset.
A successful disable leaves the firmware ring disabled for the rest of this
isolated diagnostic boot; it never releases or overwrites its backing memory.

## Already-disabled observation branch

Firmware state varies across boots. A complete stable engine census with
**enabled0/pending0** selects a separate observation function. Its BCS endpoint
must have tail/head/start/control0, execlist low/high 1/0, parser idle and the
same legacy-mode restrictions as the write branch. It requires the same
explicit cooperative submission handoff, identity-bound parent lease,
guards, one-use result and 100ms deadline. Four eight-DWORD snapshots must
match exactly. A nonzero start, enabled control, nonidle parser or changed
state refuses; there is no fallback to the write branch.

The observation API requires a **NULL write callback**, reports zero BCS
attempts/stores and never re-enables or restores a ring. Version2 BCS summary
adds `entry_disabled`; existing row/reader receipts stay version1. Decoders
retain version1 summary support and bind version2 branch selection to the
native engine census and all raw BCS words. `frontend_disabled` describes the
verified endpoint state; `frontend_disable_performed` and
`entry_disabled_verified` distinguish the operation from observation. Neither
is memory authority. Failed read-only observations can close the wake lease
cleanly; existing post-store uncertainty quarantine remains unchanged.

Both accepted BCS branches bracket the same read-only plane/config audit;
full completion still requires its two matching passes, native inherited-state
and UC proofs, persistence and safe closeout. Zero control bits alone do not
prove engine, reader, DMA, translation or cache retirement. The enabled16
branch retains its exact captured f001/start03eef000 eligibility and single
f000 store; ordinary and older diagnostic recipes remain independent.

## Read-only reader coverage

Prepare all11 UC aliases before the first wake store: the earlier8 plus three
plane pages. Only the request page and BCS page are writable in this new ELF;
all plane pages are RO/NX. The older engine ELF keeps BCS RO. Each page separately
requires exact identity/resource binding and fresh unchanged CPU cache context
with MTRR/effective UC. Existing observer allowlists remain unchanged.

Two27-DWORD plane passes bracket BCS disable. They cover primary control,
programmed/live surface; cursor control, programmed/live base; sprite control,
programmed/live surface for each of the three pipes. The new offsets are audited
against PRM pages1221 onward (live surfaces),1425 (sprite control) and1454
(sprite surface); cursor/primary programmed offsets retain the earlier capture.
No display register is written. Raw values, enabled masks and equality survive
in receipts; shared primary/cursor words must also match the inherited-display
capture in the native decoder. Reader timing, including the BCS operation,
is bounded to150ms and remains inside the parent lease.

Display HDA coverage is **conventional PCI config only**, for this unit's captured
00:03.0 device8086:160c. Both snapshots retain ID/class, command, PM validity/state
and capability-parse outcome. The diagnostic does not map HDA, change D-state, start/stop
streams, reset a controller or acknowledge status. Missing/malformed/D3 endpoints
remain explicit limitations, not DMA-retirement proof. Analog audio and other DMA
remain separate. Stereo/left-eye state, pending flips, translation/cache retirement
and other GPU readers still require their own handoff audit.

Complete means a bounded verified front-end disable plus stable observed planes
and unchanged config endpoints. `engine_retirement`, `reader_retirement`,
`managed_range` and physical tear-free remain false. The populated16-page
candidate, full inherited display fence and all firmware allocations stay protected.
The known BCS start lies beyond that display fence; neither the fence nor empty
sampled queues inventories all firmware memory.

## Automated trial

```sh
make -j2 all test-intel-blitter build/intel-blitter/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_VOLUME --intel-blitter-trial
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use this unit's UUID and actual MBR/GPT table. This distinct recipe adds
`srvros.intel-blitter=retire` to the complete engine recipe and selects its own
ELF. Follow the [Ubuntu guide](ubuntu-device-agent-setup.md) for exact staging,
durable payload/recovery, run record, install, one-shot and collection. No input
is needed; volume1%, audio off. Save a verified preflight before any store,
persist final diagnostics and automatically return after handled save/unmount.
Missing persistence halts visibly. Q35 must refuse before writes and cannot
accept the native register path. Native acceptance requires all receipts,
matching saved preflight, inherited-state/engine binding, independent UC proofs,
clean closeout, prior-file preservation and offline media/recovery checks.

## Air7,2 readiness checkpoint

The current installed candidate is `32859f5868aedd46`, source `47a0db4e`.
It supports both exact enabled-ring disable and separately verified zero-geometry
already-disabled observation. Host/core/profile checks, fatal sanitizers (27
adapter/25 core cases), 22 compiled negative controls, 269 collector/staging tests,
exact-stage Q35 persistence/refusal checks and prior diagnostic/VirtIO regressions
pass. Installed-root/89 installer cases are retained with exact unchanged-source
and log-hash proofs; all other targets reran after the main-branch futex fixture
was adapted to its architecture macro. The candidate is not armed and has no
native acceptance. Recovery, previous trial and accepted engine census remain
retained; the accepted wake image has a verified durable/archive copy.
See the [unit record](machines/macbookair7-2.md#already-disabled-reader-candidate-ready-native-acceptance-pending)
for local payloads, media and evidence paths. The next step is an automated
native trial and matching return collection. No managed-memory authority follows
from host/VM evidence, zero controls or a front-end disable alone.

The first native trial returned with all five engines disabled at entry:
`session-00065fa4adf643e3-00`, release `16824812ea718910`. The strict enabled16
gate correctly skipped BCS and reader work, with zero BCS stores and clean
wake closeout. Persistence/media/recovery checks pass; combined acceptance
remains incomplete. An already-disabled branch needs its own explicit
validation before it can run the remaining reader audit. The earlier enabled
BCS snapshot remains historical evidence, not a guaranteed boot precondition.
