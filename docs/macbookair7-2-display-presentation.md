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
