# QEMU VirtIO baseline

The current target is QEMU presentation with less full-frame copying, followed by
input, RNG, block storage and networking. Native hardware acceptance is unchanged.
The drivers are enabled only by `srvros.virtio=on`; presentation uses the separate
`srvros.virtio-gpu=present` token. The macOS launcher generates a QEMU-only ISO,
defaults to the modern devices, and retains `--devices legacy` plus
`--display-device std` for comparison/recovery. Generic hosts can use
`make run-virtio`; `QEMU_DISPLAY=std` selects VGA.

Implementation follows the [VirtIO 1.2 specification](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html).
Only modern PCI devices are admitted: VERSION_1, split queues, no indirect or
packed rings, no IOMMU/access-platform negotiation, no MSI-X. The transport
validates capabilities, BAR bounds, feature negotiation, queue ownership, used
IDs and lengths. It maps MMIO explicitly in the kernel address space, bounds all
work, and proves reset before releasing DMA. A failed reset permanently retains
the buffers. There are at most eight transport devices, three queues per device,
and 256 descriptors per queue. These are CPU0 drivers with external serialization.

SeaBIOS leaves modern block devices initialized with status 15. The explicit
boot-only block handoff accepts that exact status, resets through validated
PCI_CFG access and observes zero before changing BARs or allocating queues. It
refuses any previously claimed function or known physical alias, and quarantines
a failed reset. Other device opens still require inactive status zero. The
kernel never reads or frees firmware queue memory. This is a QEMU boot handoff,
not general live-device takeover. See the
[SeaBIOS block initialization](https://github.com/coreboot/seabios/blob/master/src/hw/virtio-blk.c)
and VirtIO 1.2 section 4.1.4.9.

| Driver | Initial supported behavior | Limits |
| --- | --- | --- |
| GPU | Two fenced resources with independent pending damage; unchanged frames skip presentation | Full scanout/flush; command completion does not establish host vblank |
| Input | QEMU keyboard, relative mouse and absolute tablet; modifiers/releases/repeat, buttons and wheel; independent source state | Four devices; 64 events per device per poll; no LED output or host-device passthrough |
| RNG | One writable request and a private 4 KiB pool, validated partial replies, nonblocking syscall consumption | Exhaustion keeps the existing random syscall fallback; no new cryptographic guarantee |
| Block | Two disks, 512-byte sectors, 16 KiB batches, partial-sector read/modify/write, explicit flush | Media without FLUSH or with RO remain read-only; capacity changes/faults retire the device |
| Network | One RX/TX pair, MAC and optional live link status, Ethernet packets up to 1514 bytes | No offloads, mergeable buffers or live provider replacement |

Input and entropy use a bounded scheduler service once per timer tick. Network
uses the existing poll worker. Storage performs synchronous private-buffer
transactions with bounded waits and scheduler service checkpoints, restoring the
caller address space before every checkpoint. Transport/device failures retire
ownership rather than reuse potentially live DMA. These polling paths are a
usable initial baseline; interrupt-driven operation is separate work.

The default launchers select `virtio-tablet-pci`. Its signed axis bounds are read
from ABS_INFO and normalized to inclusive 0..65535. The existing mouse event ABI
carries positions under `MOUSE_ABSOLUTE`; relative sources retain signed deltas.
Consumers map positions to their logical canvas, and cancellation preserves the
current position. Both axes must be known before a tablet can emit pointer
effects. Queues keep absolute and relative events separate and retain button
edges. Cocoa's absolute-pointer mode releases capture on window exit and raises
held buttons; see [QEMU's Cocoa input handling](https://github.com/qemu/qemu/blob/v11.1.2/ui/cocoa.m).

`virtio` in the monitor prints device inventory, failure/quarantine status and
transfer counters, without exposing entropy bytes. RNG consumption can be
exercised with `run /fat/bin/nodeprobe`. Block `windows` counts offered service
checkpoints, not actual scheduler switches.

QEMU 11.1.2 leaves the non-mergeable RX header's `num_buffers` field unwritten.
The driver clears headers before submission and accepts zero or one for this
single-descriptor receive path; larger values are rejected. A captured DHCP
exchange showed one reply dropped before this compatibility fix; the corrected
path completes DHCP and ICMP traffic. See the
[QEMU receive implementation](https://github.com/qemu/qemu/blob/v11.1.2/hw/net/virtio-net.c).

## Verification

The [GPU measurement report](display-presentation.md#damage-aware-presentation--november-3-2026)
records three successful repeated Cocoa workloads, with 64–67% lower copy/transfer
volume and lower draw/presentation time. Host tests cover the transport and each
protocol, invalid completions, bounded failure, reset quarantine, and existing
keyboard/mouse/graphics/storage/scheduler behavior.

The complete matched VM acceptance passes on QEMU 11.1.2. Run
`python3 tools/virtio_baseline_smoke.py` against a fresh build; it uses a disposable
disk copy and disables PS/2, so keyboard/mouse checks cannot accidentally use the
old path. It requires modern-device boot, actual VirtIO `/fat` mounting, keyboard
typing, mouse drag/wheel, RNG consumption, network traffic, clean desktop exit,
and a written file surviving a guest-flushed reboot. Evidence is saved under
`build/review/virtio-baseline/`; missing markers or fallback mounts fail the run.

The accepted runtime is clean commit `986c16e8`; its rebuilt kernel/exFAT hashes
exactly match the two-boot test. The final protocol, existing input/storage/
graphics/network/scheduler/cancellation and desktop suites pass. The default
1440×900 Cocoa launcher reaches its first app frame in 4.61 seconds in the
recorded run; this is one startup observation, not a benchmark. The explicit
VGA/USB/AHCI/e1000 launcher also renders and returns to the console cleanly.
See the [compact acceptance receipt](assets/screenshots/virtio-baseline-manifest.json)
and [PR284](https://github.com/PaulMDemers/SrvrOS/pull/284).

### Absolute tablet verification — November 3, 2026

Clean runtime `0779dd72` passes the full build, input/pointer ABI/keyboard/desktop
host suites, and launcher tests. `python3 tools/virtio_tablet_smoke.py` passes with
PS/2 disabled at 1280×800 scale 1 and 1920×1080 scale 2 (960×540 logical): all
corners, center, edge release/re-entry, Text edit/save/drag, Help wheel input, and
clean presentation reset. The original relative-mouse baseline also passes both
boots, including networking, entropy, and flushed disk persistence.

The default 1440×900 Cocoa launcher reaches its first app frame in 4.32 seconds
in this run and reports `QEMU Virtio Tablet` as its active absolute pointer.
Automated guest input does not establish host cursor release. The attended
window-boundary check is pending. See the
[tablet receipt](assets/screenshots/virtio-tablet-manifest.json) and
[PR285](https://github.com/PaulMDemers/SrvrOS/pull/285).
