# Roadmap

The goal is a pleasant, useful native desktop on the MacBook Air, with a solid
shell and server foundation underneath. The reference machine is MacBookAir7,2;
additional Macs have independent bring-up records. See [current status](status.md)
for what is already implemented and tested.

## 1. Close the current input and reliability milestone

Physically accept secondary click, tap gestures and pointer cancellation while
preserving primary dragging, two-finger scrolling and simultaneous typing.
Exercise System and other apps during file/network work with the delivery-deadline
fixes. Save logs and screenshots, complete the clean return path, and investigate
any remaining unexpected closure rather than attributing it to an unproven cause.

**Done when:** a matching hardware session accepts those interactions, retains
its artifacts, and reports no input/control timeout or unexplained client loss.

## 2. Measure power before changing policy

Collect the opt-in read-only package-energy and active-frequency sample on the
Air. Compare matched display brightness, workload, adapter state and duration
with scheduler occupancy. Keep Linux CPU policy inventory alongside the baseline.
Then evaluate less polling, timer behavior and deeper idle entry separately.

**Done when:** measurements are valid and repeatable, and wake/input/storage
regressions pass. Temperature or idle percentage alone is not an energy result.

## 3. Complete laptop services and device acceptance

Use the tested worker/IRQ/EC foundations to develop a bounded native ACPI runtime.
Resolve early EC binding, module-level AML, transaction ownership, permitted
regions, interrupt/query handling and teardown before exposing battery/AC state.
Keep the existing restricted probe separate from ordinary startup. Suspend/resume
needs its own recovery design and device reinitialization work.

Physically accept PCM playback and headphone routes. Test actual USB Ethernet
adapters and extend drivers from captured descriptors, not product guesses.
Wi-Fi, Bluetooth and additional storage controllers are later device projects.

**Done when:** each service has fault containment, current diagnostics and its
own on-device evidence; no sibling's acceptance is copied.

## 4. Make the desktop less constrained

Replace the eight preallocated window-cache pairs with demand-sized buffers and
coordinate process, queue and surface budgets. Handle allocation failure without
losing the displayed frame or input ownership before raising the window limit.
Continue reusable controls, keyboard accessibility, consistent app behavior,
text coverage and save/recovery workflows. Improve high-density rendering after
explicit 2× scaling has physical evidence.

**Done when:** workloads beyond eight windows, resize/close/reopen, memory
pressure and cancellation preserve documents and resource stability.

## 5. Expand hardware and strengthen the base

Bring up Air7,1, Pro11,2, Pro11,3 and Mini7,1 using the
[common Ubuntu workflow](ubuntu-device-agent-setup.md) and
[hardware expansion plan](apple-hardware-expansion-plan.md). Improve filesystem
recovery, network behavior, libc coverage and process isolation as applications
need them. No date is promised for production readiness or a self-hosting system.

The [earlier roadmap](history/roadmap-2026-10-18.md) is retained as history, along
with the [original desktop plan](desktop-plan-2026-10-17.md).
