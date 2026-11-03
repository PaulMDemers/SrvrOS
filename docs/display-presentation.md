# Desktop presentation and scanout

The desktop keeps a software canvas in RAM. A client commits a sealed surface;
the compositor copies it into a private window cache before replacing the
previous image. The protocol's release acknowledges this snapshot, not physical
scanout (`shared/include/srvros/desktop_protocol.h`). The compositor freezes the
scene and stacking order for each damage batch, composes every damaged rectangle
into the canvas, then submits those rectangles to the framebuffer. This reduces
the time spent rendering between uploads of related damage; it does not make
those uploads atomic.

The graphics syscall copies each rectangle from userspace in bounded chunks of
at most 64 KiB and checkpoints service work (`kernel/src/arch/x86_64/syscall.c`).
The console blitter writes rows directly into the inherited framebuffer
(`kernel/src/console.c`). Separate rectangles and chunks can become visible at
different times, and scanout can overlap a write. Either can contribute to a
visible discontinuity during movement or scrolling; the actual observed artifact
has not yet been isolated. There is no vblank-synchronized page flip in this
path. A second software canvas or a full-frame copy alone cannot guarantee a
tear-free display.

The existing Intel graphics code identifies the controller and maps resources;
it does not establish display ownership or implement scanout flips. The Air6,1
unit record documents the incomplete Haswell display-resource handoff. Preserve
its active diagnostic ownership and model-specific gates; do not infer that a
related model has the same firmware routes or safe registers.

## Verification and next stages

1. Compare damage coverage, tiled/full pixels, upload order, and failure behavior
   in host tests; run the desktop compositor and application VM checks. Measure
   copy time, frame/input latency, and framebuffer bytes on identical workloads.
   Retain the bounded kernel copy/checkpoint behavior.
2. Capture repeated drag and scroll observations on each target with an external
   camera or other scanout-aware instrument, alongside software timing. A
   screenshot of the completed framebuffer cannot establish whether a physical
   scanout tear occurred. QEMU results do not establish physical behavior.
3. For each target, establish controller, active pipe/plane, framebuffer format,
   inherited firmware state, display power dependencies, and a safe recovery and
   ownership handoff using model-specific evidence. Coordinate the existing
   display audit before touching controller registers. Unsupported devices retain
   the inherited framebuffer path.
4. Only after ownership and memory lifetime are validated, design scanout-capable
   front/back buffers, a bounded vblank-synchronized flip and completion event,
   and fallback/recovery for unavailable interrupts, timeouts and mode changes.
   Hold a scanout buffer until the display has stopped using it. Verify the
   transition and teardown in a suitable emulator and on each authorized target.
   The [Linux DRM KMS documentation](https://docs.kernel.org/gpu/drm-kms.html)
   describes modesetting, framebuffer, plane, vblank and atomic-state concepts;
   it is a reference for the contract, not proof of SrvrOS hardware support.
