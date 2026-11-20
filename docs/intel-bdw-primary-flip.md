# Broadwell synchronous primary-plane flips

`intel_bdw_primary_flip` implements the primary-A arm and live-surface latch
sequence for already owned buffers. It is a host-qualified protocol with an
incremental polling API. There is no native adapter, guest-image caller or new
hardware acceptance. The ordinary desktop still uses its inherited framebuffer.
This advances presentation in the [tear-free roadmap](intel-tearfree-roadmap.md);
the [Air current record](machines/macbookair7-2-current.md) owns native acceptance.

## Hardware sequence and scope

Intel's [Broadwell PRM Volume2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
defines the relevant registers:

| Register | Offset | Role |
| --- | --- | --- |
| `PIPE_FRMCNT_A` | `0x70040` | 32-bit frame count, advancing at vertical blank and wrapping; printed page1207 |
| `PRI_CTL_A` | `0x70180` | Primary format/layout; async MMIO update is bit9; pages1247–1250 |
| `PRI_STRIDE_A` | `0x70188` | Fixed admitted pitch |
| `PIPE_SRCSZ_A` | `0x6001c` | Fixed admitted dimensions |
| `PRI_OFFSET_A` | `0x701a4` | Zero panning in this path |
| `PRI_SURF_A` | `0x7019c` | Double-buffered surface arm; pages1255–1256 |
| `PRI_SURFLIVE_A` | `0x701ac` | Actual currently used surface base; pages1221–1223 |

Only synchronous primary-A, non-stereo, unrotated BGRX8888 is supported, with
unchanged admitted gamma/CSC, linear or X-tiled layout and unchanged pitch.
The sole store writes the new surface to `PRI_SURF_A`. Plane control, routes,
timing, brightness, interrupts and other planes are untouched. `PRI_SURF` bit3
records a prior ring flip source: mask it for comparisons, never copy it into
a new MMIO arm. Reserved low bits, async, rotation and unsupported layouts refuse.

Both bases have256KiB alignment. Complete disjoint mapping extents must fit
the aperture and include padded tiled rows, page rounding and the manual's
extra128-PTE display tail. The tail consumes translations, not an assertion
that128 extra RAM pages are fetched. Arithmetic supplies no mapping authority.

## Independent native prerequisites

The consumed-once source binds original owner/provider/lifetime, distinct
from/to mapping leases, root, epoch, sequence, admitted time, addresses, extent
and exact geometry. The device gate is8086:1626, not sibling acceptance.

Before every actual access, a future native provider must independently maintain:

1. The registered original device owner, root, epoch and request.
2. Both complete mappings and their pinned physical backing.
3. Completed rendering and CPU/device cache visibility before the arm.
4. The fixed enabled, non-stereo, resource/power-qualified pipe/route.
5. Firmware cooperation and exclusive plane programming, excluding competing
   MMIO and command-streamer posters throughout the pending request.
6. Independent exclusion/ownership of auxiliary readers/writers, including
   firmware/FBC/PSR, under the selected native protocol.

The sixth requirement maintains reader ownership; it does not claim that the
old front buffer's outstanding accesses have already retired. That separate
post-latch witness is mandatory before old-buffer reuse or translation teardown.
Synthetic proof callbacks, idle bits and saved samples establish none of these
native prerequisites. The first GPU mapping remains blocked on real bootstrap
DMA/cache/prefetch and firmware/display-reader ownership.

Each proof matches the exact source, mappings, original epoch/sequence, state
and a fresh bounded timestamp. An actual read/write callback must revalidate
the native access policy after the last clock immediately before ordered UC
access. The posting read is neither a live latch nor a device memory fence.

## Nonblocking lifetime and failure

`begin` checks exact geometry and matching programmed/live old addresses, then
arms one new surface. A pending/foreign flip refuses before an overwrite.
A known store requires posting readback and closing ownership admission; it
still returns pending. There is no frame-wait loop.

Each `poll` performs one finite batch: frame count, four geometry words,
programmed surface, two live reads and another frame count, then fresh ownership
admission. The caller schedules later polls outside blocking UI work. A batch
spanning more than one frame, reversed/foreign live transition, changed geometry
or lost ownership fails. A normal latch between live reads remains pending.

Completion requires both live reads at the exact new base and positive frame
progress since pre-arm capture. A frame change alone never completes a flip;
a stale matching live value without frame progress stays pending. Counter
arithmetic handles32-bit wrap, conservatively allowing up to eight frames.
Counter regression between polls, or a live surface reverting after a sampled
new target, refuses even when the total delta from the original frame is legal.

One100ms origin covers admission, store and every poll/final check, without
restarting on return to the caller. Each public batch requires positive clock
progress. Zero/backward/expired clocks refuse. At most256 polls are allowed:
the256th may complete, otherwise it fails without a257th batch. Providers must
themselves be bounded. No existing reset/preparation/HDA/outer policy changes.

Every started failure retains source and both mappings/backing, including a
definitely unissued or uncertain store. Known store counts survive callback
corruption. There is no automatic retry, restore, cancellation, unmap or free.
Source reuse, copied objects and callback reentry cannot issue another flip.

`DONE` means **surface latched**, not all-reader retirement or RAM release.
A future adapter must not call generic hardware `scanout_complete` or let the
next render reuse the old front before independent reader retirement. Timeout
or uncertainty retains both buffers and their address leases.

## Reset-free hardware trial

Broadwell can switch an enabled primary plane at vblank by programming its
surface address. The first trial keeps format, tiling, pitch and dimensions
fixed through both directions. Linux i915 uses the same surface register in
its [primary plane update](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/display/i9xx_plane.c#L461-L472).

The isolated `intel_bdw_flip_trial` takes a reference primary CRC with both
images pinned, then asks the owner for a freshly timed forward request. It
polls the live surface, captures the primary CRC again, and compares it with
a caller-supplied alternate pattern CRC distinct from the reference. The
return request reverses the two exact ranges and mapping leases while
preserving root, owner, geometry and epoch. Its sequence must advance and its
admission time must follow or equal the forward latch. Each leg has its own
100 ms bound. Each CRC window uses the existing four-frame primary-tap
protocol and checks the two settled frames.

A clean capture with the wrong alternate image still allows one return and
records a failed trial. An uncertain latch or incomplete CRC cleanup stops
the sequence with the pins held. Final verification checks the inherited
surface latch, PHY and backing snapshot, and the original primary CRC. The
caller still owns retirement of both pins after the diagnostic.

On Air7,2, the boot interface describes a single 1440 × 900 framebuffer.
The [native census](intel-bdw-stolen-scanout.md#native-v55-ownership-gate-result)
observed two disjoint 6 MiB ranges outside the inherited fence, both in
firmware-reserved memory. It found no transfer to the OS. Using OS-allocated
RAM instead requires a live grant of GPU virtual addresses and publication of
the RAM into the GGTT under that grant. The [mapping path](scanout-transfer.md)
has host coverage. A hardware trial first needs an independently owned and
visible second image in the active X-tiled format and a native plane/CRC
adapter holding the display and both mappings across the trial.

## Private backing in a transferred interval

`intel_bdw_scanout_prepare` accepts a live transfer and an OS-allocated
surface backed by low 32-bit physical pages. It reserves a 256 KiB-aligned
GPU address and encodes every image and tail page against the transfer's
original entries. The existing publication transaction checks those originals,
writes and verifies the new entries, then restores the originals after the
provider confirms reader retirement. The provider must maintain real device
ownership, ordered MMIO and translation completion throughout.

`make test-scanout-transfer` joins that path to the round-trip diagnostic.
At the captured Air geometry the fixture initializes 904 padded X-tiled rows
plus the 128-entry tail. The simulated primary tap fetches the visible pixels
through published entries and compares their checksum with an independently
generated reference. It catches a corrupted pixel, verifies the return flip,
and holds the range and RAM while reader retirement is pending. The fixture
uses software CRC-32 and models X tiling without address swizzling. The
physical trial must use the actual pixel layout and Pipe A CRC tap.

## Qualification and remaining integration

`make test-intel-bdw-primary-flip` exercises the actual C core against independent
queued/live device state. A frame may advance while the old surface remains
live: it must still return pending. Coverage includes late/mid-sample latch,
frame wrap, stale matching live values, foreign/pending flips, geometry changes,
ownership/visibility loss, every callback identity/progress mutation, every read
failure, proof fields/bits, uncertain stores, all clock positions, exact/+1us
endpoints, frozen polling and256th-poll outcomes.

The 10,000-case matrix, six rollback scenarios and seven joined CRC trials
run under fatal ASan/UBSan. The suite checks 22 runtime-detected compiled
weakening controls. Actual native compiler flags pass the2KiB frame limit;
ordinary and accepted CPU-context image plans exclude this core. An initial
test candidate missed the removed posting-readback control. Requiring every
intentionally wrong register read to refuse exposed it; all controls pass
after that fixture correction, with runtime checks unchanged.

Next supply the genuine mapping/visibility owner and native ordered register
adapter, bind old-reader retirement to the existing [buffer lifetime](scanout-buffer-lifetime.md),
then qualify full-buffer flips, inherited restoration and desktop presentation.
Host/QEMU success cannot accept the native provider or visible tear-free behavior.


### Native reset-free range observation

The isolated Air7,2 census accepts the additional boot token
`srvros.intel-scanout=range-observation`. Its version 9 report identifies
the exact GPU interval for the second X-tiled image, including the 128 tail
entries. The driver holds the display exclusion and VMM reservation guard
while it checks all 32 hardware fences, rereads the 1,484 original entries,
and checks all 32 fences again. The 42 display-reader words, PCI requester
state and firmware endpoint must agree before and after that interval.
Each entry must match both earlier complete
samples. This runs within the original two-second census budget.

The collector checks the entry count, digest and fence counts independently.
The source submits the observed interval to the same admission check used by
the GGTT transfer. The Air boot source currently supplies zero hardware
assurances. A verified observation therefore leaves the GPU range with its
original owner. A reset-free flip requires a native owner that can exclude
firmware and competing readers, maintain engine, DMA and cache ownership,
publish the OS pages and retire them after the return flip.


Version 10 reserves the same interval across the kernel's two GGTT handoff
paths before rereading the full original vector. The range receipt records the
reservation generation and whether cleanup returned it. The native source still
offers zero hardware assurances. In the local fixture, a provider holding the
required rights promotes that same reservation and keeps it through both flips
and the original mapping's retirement.
