# Air7,2 native display presentation

Initial scope: 2026-11-03, merged baseline `eaf11488`. The local branch is
`device/macbookair7-2/tearfree`. This is an implementation plan and host reference,
not a native scanout driver or physical tear-free acceptance. Read the shared
[presentation contract](display-presentation.md) and the separate
[Air6,1 ownership design](macbookair6-1-display-ownership.md).

## Starting point

Local Ubuntu identifies GPU 00:02.0 as Broadwell HD Graphics 6000, `8086:1626`,
revision09, Apple subsystem `106b:011b`, bound to i915. Read-only sysfs/debugfs
capture observes connected/enabled eDP, 1440×900 at60Hz, active pipeA, primaryA
and cursorA; pipesB/C are inactive. Linux's primary uses XR30 with pitch6144.
These are Linux-owned reference values, not the next Limine/SrvrOS boot state.
Do not infer a native format, active route or safe write set from them. Raw
snapshots and hashes stay in local `build/air-tearfree-20261103/`.

SrvrOS's Intel stub identifies the GPU, enables PCI memory/bus-master access and
maps only four MMIO pages. It does not own display resources or flip scanout.
The desktop composes in RAM; bounded graphics copies update the live inherited
framebuffer. Existing `display_present.c` leases and begin/frame/end syscalls are
currently wired to VirtIO. VirtIO command fences establish its implemented
resource ordering, not physical display vblank. A second software canvas or
waiting for vblank before a long CPU copy cannot alone establish atomic scanout.

## Bounded implementation order

| Milestone | Implementation and required proof |
| --- | --- |
| Reference and integration | Collect the prior native session; consume merged OS/UI/Apps/Ports pins; run local host/core/build and relevant desktop/VirtIO checks. Capture Ubuntu state without changing its device. |
| Native inherited-state capture | Add an opt-in, exact Air7,2/GPU/subsystem-gated diagnostic. Establish BAR/aperture bounds without destructive sizing, boot framebuffer address/format/stride, active pipe/plane/port, surface/GGTT relationships, timing/vblank observations and relevant display-power dependencies. Validate access with host fixtures and refusal cases. Capture/return automatically when no human input is needed. No plane, GTT, interrupt, clock, power or PCI command writes in this new diagnostic path. Account separately for the existing stub's PCI writes. |
| Adopted route and buffer lifetime | Design one boot-scoped Intel owner, compatible with GUI lease/cancellation. Pin the inherited route and backing; establish exclusively owned GGTT mappings and two scanout-compatible buffers without reusing firmware allocations. CPU mappings and arbitrary physical RAM are not proof of display addressability. Preserve the existing panel link, timings, backlight and power state. Refuse unsupported formats/routes or unknown ownership. |
| Opt-in flip backend | Generalize the existing kernel presentation dispatch without changing its userspace contract or VirtIO behavior. Compose/copy only into an inactive buffer. Arm a supported vblank-synchronized plane flip and observe its matching completion before retiring the former front buffer. Add no per-frame allocation. Preserve bounded checkpoints, explicit timeouts, exclusive owner/epoch and retained-fault behavior. Do not substitute an MMIO readback or fixed sleep for completion. |
| Recovery and physical acceptance | Prove ordinary exit, owner death, delayed/missing completion, failed restore, reacquisition and console recovery. Uncertain scanout retains memory and ownership. Camera-observed drag/scroll under load plus flip/timing/input receipts are required; a completed screenshot is insufficient. Retain the inherited framebuffer fallback for unsupported admission and known-good Ubuntu/recovery payloads. |

The first mutating milestone needs its own reviewed exact register allowlist,
ordering, access/domain prerequisites and restoration target. If the initial
inherited route cannot be adopted safely, stop at diagnosis rather than perform
a speculative modeset. No display well release, display-HDA takeover, RC6 policy,
panel power sequence, render engine or blitter enablement is part of the initial
flip milestone. Air6,1's Haswell work retains its own owner and evidence; its
register gates do not authorize this Broadwell profile.

## Shared API and testing boundaries

Keep clients and widgets independent of the GPU. Preserve the compositor's
sealed-surface snapshot/release semantics; those acknowledge copying, not physical
scanout. A presentation backend owns scanout memory until hardware completion,
even if its client exits. Keep short state/queue locks separate from waits and
copy/checkpoint work. Preserve service progress, cancellation and quarantine
rather than freeing uncertain device-owned memory after a timeout.

Host tests should first cover profile/refusal, range/overflow checks, observed
state decoding, buffer/epoch transitions, completion identity and timeout/restore
ownership. Existing VirtIO presentation and desktop lifecycle tests remain
regressions when dispatch changes. Native timing/visual acceptance is separate
from VM correctness. Record copy/transfer bytes, present duration, vblank/flip
completion, UI/input latency and recovery on the same workload; preserve the
existing UI/lifetime bounds rather than relax them to accommodate a driver.

No physical test is armed by this plan. Prepare a source-bound run record,
independent payload/recovery and exact-stage rehearsal before any next reboot.

## First observation implementation

The opt-in `srvros.capture=intel-display` path runs immediately after PCI
inventory, before normal Intel initialization and userspace. It uses the existing
exact Air7,2 DSDT matcher plus GPU `8086:1626` rev09, subsystem `106b:011b`,
PCH `8086:9cc3`, existing memory decode and a validated read-only D0 snapshot.
It never enables a device or transitions its power state. Ordinary boot retains
its current behavior; the diagnostic skips the old Intel stub's PCI writes.

The first MMIO prefix is bounded to2MiB and BAR0 must meet the captured16MiB
alignment and39-bit address limit. Firmware memory-map overlaps with RAM or the
framebuffer refuse admission. The address-map reference is Intel's
[Mobile 5th Generation datasheet, section3.2.10](https://cdrdv2-public.intel.com/332379/mobile-5th-gen-core-family-datasheet-vol-2.pdf).
The read allowlist uses the pinned Linux
[i9xx plane](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/i9xx_plane_regs.h)
and [display register](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_display_regs.h)
definitions. Intel's [Broadwell manual index](https://www.intel.com/content/www/us/en/docs/graphics-for-linux/developer-reference/1-0/broadwell.html)
is the architecture reference. The capture accesses no AUX, forcewake, W1C
status, GTT PTE or framebuffer contents.

Nine temporary kernel-address-space pages have no writable, user or executable
permission and never enter the global mapping registry. Only translations
created by this attempt are removed; an existing mapping refuses and is left
alone. Page-table storage retains the kernel address space's normal lifetime.
Partial mappings are cleaned up; uncertain cleanup is retained and reported.
There is one attempt per boot and no runtime API for repeating it.

Two snapshots contain58 allowlisted reads each, bracketed by four power-well
registers. Their separation targets20ms with a finite counter fallback and a
100ms clock bound. PCI identity/access is revalidated before each snapshot and
afterward. Only scanline/frame counters may differ; changed power/control/access
state refuses a complete observation. These are non-atomic observations, not a
proof excluding firmware activity. Raw source dimensions, plane/stride/surface,
DDI/transcoder/timing and cursor fields are preserved. A numerical linear
surface relation is explicitly separate from unknown GGTT backing and ownership.

```sh
make -j2 all test-intel-display-capture
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL \
  --intel-display-capture
# Verify the reported release and run this immediately in the same checkout:
python3 tools/hardware_boot_smoke.py --probe-check
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

The exact-stage Q35 check must refuse the unmatched GPU before display MMIO,
save the full refusal receipt, unmount and reset. A missing workspace must halt
with save/unmount failure instead of resetting past lost evidence. The stager
rejects combined firmware experiments and attended failure-hold overrides.
Successful observations and handled refusals both return automatically after
successful save/unmount; refusal remains a failed native observation. No tester
input is needed for this diagnostic. Use the normal recovery/run-record/install
sequence before a physical boot.

The existing native desktop collector exposes `intel_display_capture` and flags
missing, refused, malformed or incomplete reports. It validates versioned rows,
identities, fixed read counts, bounded timing, stable controls/well endpoints and
mapping retirement. Match the new session/release to its staged manifest before
accepting the observation. Neither decoder acceptance nor VM fixtures establish
native flip support or visible tear-free scanout.

## First native observation (2026-11-03)

Air7,2 release `c60576b371caaab4`, compiled source `5c8c0569`, saved a complete
version1 observation in `session-00065f6627117de5-00` and returned automatically.
Both samples contain58 reads; their durations are12us and8us and their starts
are20084us apart. PCI identity/access, well endpoints and all non-counter fields
are stable. All9 temporary pages are retired, retained mask0, capture writes0.
This accepts the bounded read-only diagnostic on this unit.

Inherited framebuffer: physical `0xb0000000`, 1440×900, pitch6144, 32-bit BGRX.
PipeA reports source `0x059f0383`, control `0xd8000400`, stride `0x1800`, surface/
offset/live0 and disabled cursor. Its scanline changes856→120 and frame count
724→726. PipesB/C have zero plane control. The eDP transcoder reports htotal
`0x063f059f`, vtotal/vblank `0x039d0383`, DDI `0x82200000`, config `0xc0000000`.
Preserve these as raw inherited observations; Linux's reference state remains
separate. The capture reports active-plane mask1 and linear-relation mask0.

Next checkpoint: decode the captured format/layout and independently establish
the inherited GGTT/backing, aperture and stolen-memory bounds using a bounded
read-only extension. Refuse unsupported or ambiguous state. Only then design
exclusive buffer ownership, inactive scanout allocation and vblank flips with
retirement/restore proofs. This snapshot accepts none of those later milestones.
Raw reports and return-integrity evidence remain in ignored
`build/air-intel-observe-native-20261103/returned/` and independent reviewed storage.
