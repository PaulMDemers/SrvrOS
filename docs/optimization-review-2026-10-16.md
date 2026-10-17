# SrvrOS architecture review and optimization plan

The next milestone is readable mixed-case text and responsive output on the
MacBookAir7,2. Keep the working USB keyboard and boot path as the baseline, then
improve the console and timing before adding more hardware features. GUI work
is deferred at the user's request. Several costs are structural and can be removed within the existing
software renderer.

This review covers the major kernel, userspace, graphics, storage, networking,
porting and build paths. Findings below distinguish reproduced defects, code
observations and performance hypotheses. It is an architectural review with
focused probes, not an exhaustive correctness or security audit of every file.
Hardware timings still need to be captured in SrvrOS; Ubuntu and QEMU timings
cannot establish the laptop framebuffer's performance.

Implementation status and test instructions are in the
[non-GUI implementation report](console-optimization-2026-10-16.md). Findings
below describe the pre-implementation baseline.

## Baseline and evidence

Release `c02ec72ad1c8f3d4` boots on the laptop and accepts built-in keyboard
input. The physical `xhci` photograph confirms Apple `05ac:0291`, interface 1,
DCI 5, with 27 successful interrupt completions and 27 HID reports. Preserve that
release while changing rendering. That source is now committed as `b79c559` on
`codex/linux-development-baseline`; an independent copy of the known-good boot
payload is retained outside installer rotation.

The project has useful foundations: separate process address spaces, user-buffer
validation, scheduler wait queues, a shell/libc layer, writable exFAT, networking,
USB input, a compositor, and substantial QEMU regression coverage. Historical
validation includes 479 CLI commands, ABI checks, shell editing and displayd
smokes. Those results do not measure physical rendering latency or prove every
POSIX/Node compatibility claim in older documents.

Three host probes compiled the current implementation directly:

- Console glyphs: all 26 lowercase/uppercase pairs are identical. Rendering
  `aA` into a RAM framebuffer produces identical cells, although serial receives
  the distinct bytes. `{` and `}` have no console glyphs.
- GUI glyphs: all 26 lowercase/uppercase pairs have identical glyph rows.
- USB HID decoding: unshifted `a` and Shift+A produce the correct distinct bytes.

Probe sources, commands, output and source hashes are under
`build/review/optimization-audit/`. These are correctness probes, not hardware
speed measurements. No kernel, boot image or installed release was changed for
this review.

## Findings that explain the reported symptoms

### Both font implementations discard visual case

[console.c](../kernel/src/console.c), `glyph_for`, converts `a`–`z` to `A`–`Z`.
[ui.c](../userspace/lib/src/ui.c), `glyph_row`, does the same. Lowercase shapes
are absent. The two independent font definitions also have different punctuation
coverage. This explains why text can look uppercase even when the shell receives
lowercase commands correctly.

Use one shared, indexed bitmap font with all 95 printable ASCII characters,
including distinct lowercase glyphs. Preserve the original input/output bytes.
Give the font explicit cell metrics and provenance/license if importing artwork.
Unicode, shaping and proportional fonts are a later extension.

[keyboard.c](../kernel/src/arch/x86_64/keyboard.c) already has shifted and
unshifted maps. Caps Lock is not implemented, and the USB report path skips keys
that were already held, so it does not implement software key repeat. Add those
behaviors with modifier/release tests rather than treating them as font changes.

### Each console scroll reads and writes almost the whole display

`scroll_one_row` in [console.c](../kernel/src/console.c) reads pixels directly
from the framebuffer and writes them one row higher, using individual volatile
32-bit accesses. Every bottom-of-screen newline repeats this operation.

At an example 1440 × 900 mode with the current 8 × 16 cells, one scroll copies
1440 × 880 pixels: 5,068,800 bytes read plus the same amount written. Clearing
the bottom text row adds 92,160 bytes. That is **10,229,760 bytes of framebuffer
access per scroll**, excluding glyphs and cursor. This is calculated traffic,
not a measured bus bandwidth or elapsed time. It closely matches the reported
symptom of slow successive lines once the screen fills.

Move the text model and scroll operations into normal RAM, maintain a RAM pixel
buffer, and batch visible updates. The framebuffer should be an output target,
not the source of scroll copies. A software scroll still changes almost the
whole viewport: a shadow buffer removes framebuffer reads, but it does not by
itself remove the full-screen presentation cost. Coalescing multiple lines into
one presentation is essential. Hardware panning/blitting can come later.

### Output is processed character by character

`console_write`, `console_printf` and the console branches of
[syscall.c](../kernel/src/arch/x86_64/syscall.c), `syscall_write`, ultimately call
`console_putc` for each byte. Each printable character clears a cell, draws
individual pixels, and hides/repaints the cursor. Each byte is also sent to the
boot log and synchronously to serial if the UART is available.

Introduce a length-based write API that parses a batch, updates text state,
coalesces scrolling and damage, and paints the cursor once per presentation.
Use clipped row/span operations rather than repeating coordinate checks and
color conversion for each pixel. Keep a small direct-output path for early boot
and panic reporting when allocation/scheduling is unavailable.

Serial can independently limit output: [serial.c](../kernel/src/arch/x86_64/serial.c)
polls for every transmitted byte. The loopback probe disables an absent UART,
so this is **not established as the laptop bottleneck**. Benchmark with serial
on/off, retain bounded panic output, and make normal serial logging buffered or
explicitly opt-in if measurements warrant it.

### Large writes and blits can delay interrupts

[idt.c](../kernel/src/arch/x86_64/idt.c) installs `int 0x80` as an interrupt gate.
The ordinary console write and graphics blit paths do not reenable interrupts
before doing their work. Thus a large userspace write can run the expensive
scroll loop with interrupts masked. This affects input and preemption as well
as drawing speed.

Define console ownership, serialization and buffer lifetimes, then move long
render/copy work outside short protected sections. Bound output work per batch.
Do not simply add `sti` globally: many current kernel paths depend on the
existing serialization, and user-buffer ownership must remain valid across any
new blocking or preemption point.

### GUI damage tracking limits presentation more than rendering

[displayd.c](../userspace/displayd/displayd.c), `present_dirty`, computes a dirty
rectangle and presents that rectangle. But [ui.c](../userspace/lib/src/ui.c),
`ui_render_tree`, redraws the dirty root's whole surface and composites whole
child surfaces. A small change can therefore still perform substantial CPU work.
The system already has buffered surfaces and rectangle blits; retain and improve
them.

Pass a clip/damage region through drawing and composition. Preserve unchanged
pixels, redraw intersecting layers in stacking order, handle exposed regions on
window movement, and restore the cursor correctly. Use multiple bounded dirty
rectangles with a full-frame fallback when merging is cheaper. Measure both
pixels rendered and pixels presented; the latter alone hides this cost.

### Presentation adds allocation and copying costs

`syscall_gfx_blit_rect` allocates a temporary buffer for each request, validates
and copies rows from userspace in chunks of up to 64 KiB, and switches page
tables to the kernel and back for each chunk. `console_blit_rgb` then stores each
pixel separately. GUI surfaces add further copies through kernel-owned storage.
Fallback UI paths can issue a syscall per pixel or horizontal color run.

First retain the ABI and replace repeated allocation with safely owned reusable
staging buffers, optimize contiguous spans, and establish whether the
framebuffer mapping can be accessed consistently across address spaces. Preserve
user-buffer validation and process isolation. Introduce shared surfaces or a
new submission ABI only if profiling shows copies remain a dominant cost.

### The heap adds work while interrupts are masked

[heap.c](../kernel/src/memory/heap.c) validates the complete block list before
and after every allocation/free under `irq_save`, in addition to linear free
block search. Frequent graphics staging allocation compounds that cost.

Measure allocation count and maximum interrupt-disabled duration. Reuse buffers
first. Keep comprehensive validation in a diagnostic profile and retain local
bounds/integrity checks in normal operation. Change allocator structure only
when allocation traces justify it; removing checks alone is not a correctness fix.

### Boot delays and clock inaccuracies are separate from rendering

[main.c](../kernel/src/main.c) starts the LAPIC timer with a fixed count of
10,000,000, unconditionally calls `xhci_probe_input(2000)`, and then waits up to
300 ticks for init. The input probe assumes 100 ticks/second and a 3 GHz TSC for
its alternative bound. [posix_time.c](../userspace/lib/src/posix_time.c) also
assumes 100 ticks/second. These are not calibrated time guarantees.

Measure boot stages separately from drawing. Make the long input probe an
explicit diagnostic option, preserve the required USB connection/reset recovery
intervals, and replace the init delay with a readiness signal. Establish a
calibrated monotonic clock before using kernel ticks for performance claims,
key repeat, animation cadence or driver deadlines. Wall-clock time needs a
separate RTC/boot-time origin; current `time()` derives seconds from uptime.

### Framebuffer cache behavior must be inspected before changing it

The kernel adopts Limine's active page tables in `vmm_init`. The pinned
[Limine protocol](https://github.com/Limine-Bootloader/limine-protocol/blob/3a0526b700e356f0eac1b71a77697b3fd1c707a3/PROTOCOL.md)
already specifies write-combining framebuffer mappings when PAT is supported;
its local copy is `build/limine-protocol/PROTOCOL.md`. Consequently, absence of a
custom PAT setup in the kernel does **not** establish an uncached framebuffer.

Add diagnostics for the actual framebuffer physical range, pitch/format, page
size, page attributes, PAT and relevant MTRRs. Preserve compatible aliases
across kernel/process mappings. Linux's [PAT documentation](https://www.kernel.org/doc/html/next/x86/pat.html)
explains the aliasing constraints, and its [device I/O documentation](https://www.kernel.org/doc/html/latest/driver-api/device-io.html)
describes write-combining and bulk device-memory access. Verify the inherited
mapping before adding any remapping; keep register MMIO separate from pixel
memory. Any optimized copy routine must respect the framebuffer's memory type
and ordering requirements.

## Review across the remaining system

| Area | Current evidence and gap | Required work |
| --- | --- | --- |
| Boot and build | Linux toolchain, pinned boot dependencies, stage manifests and current/previous GRUB entries work. Changes are still local; older roadmaps describe obsolete Windows/SPI assumptions. | Commit reviewed logical changes, preserve the tested release outside automatic two-release rotation, verify clean-checkout reproduction, and label old milestones as historical. |
| Memory and processes | Separate address spaces, user validation, NX flags and resource cleanup exist. Fixed capacities and manual mapping propagation need stress coverage. | Exercise allocation exhaustion, failed ELF loads, map/unmap, process churn and cleanup. Audit overflow/permissions at ABI boundaries. Separate process, fd and VM code by responsibility after behavior is covered. |
| Scheduler and timers | Preemption/wait queues exist; timing depends on an uncalibrated LAPIC count. Long interrupt-disabled operations undermine responsiveness. | Calibrate time, record maximum critical-section latency, verify wakeup/lost-wakeup behavior and cancellation under output load. Defer SMP until locking invariants are explicit. |
| USB and input | Physical keyboard works. One selected input interface per USB device limits the composite Apple device; hotplug and USB key repeat are absent. | Mixed-case/modifier/repeat behavior first; then multiple HID interfaces, trackpad decoding, disconnect/reconnect and endpoint recovery. Preserve the tested reset behavior. |
| Console and shell | CLI and line editing have useful tests, but serial success cannot catch a visually uppercase font. One console termios state remains. | Share font/text behavior, add image checks, verify ANSI cursor/erase/wrap, and define separate PTYs/terminal ownership before promising independent GUI terminals. |
| GUI and applications | Retained surfaces, displayd, damage and window ownership exist, with whole-surface redraw and multiple copies. Intel graphics code reports planned acceleration and maps MMIO; it does not submit GPU drawing commands. | Fix the CPU rendering pipeline first, profile representative apps, then consider GPU blits or shared surfaces against measured needs. |
| Block I/O and AHCI | AHCI `issue_command` polls command completion without a deadline while interrupts are masked. The current laptop entry skips AHCI. | Add bounded completion/error recovery and minimize masked time before enabling laptop storage. Validate against disposable QEMU disks and read-only hardware discovery first. |
| Filesystem durability | exFAT writes and a write-through block cache exist. `block_flush_all()` returns true without a device flush callback; AHCI has no flush command path. Cache contents are changed before a backing write succeeds. | Define flush/durability semantics, add device-cache flush, invalidate/restore cache entries after failed writes, and test interrupted metadata updates and recovery. Keep the RAM-backed hardware entry until this is addressed. |
| Network | e1000 and a small IPv4/TCP/UDP stack work in QEMU; the laptop's Broadcom Wi-Fi is not supported. Timer assumptions affect retransmits/timeouts. | Packet-boundary/failure tests, loss/reorder/backpressure and connection cleanup; select a physical networking path separately. Do not treat QEMU e1000 success as Wi-Fi support. |
| libc and ports | Broad API coverage includes compatibility shims and explicit unsupported operations. The restored Node checkout differs from the missing original port commit. | Classify APIs as implemented/shimmed/unsupported with behavioral tests. Reproduce the full Node build/runtime before relying on historical demo claims. Prioritize apps that exercise OS fundamentals. |
| Security boundaries | User-pointer checks exist, but whole-system isolation and malformed-input behavior are not established by smoke tests. The random syscall can fall back to timestamp/state mixing. | Audit syscall/ELF/USB/filesystem/network inputs, privilege boundaries and random-source guarantees before security-sensitive applications; include hardware-RNG failure behavior. |
| Laptop operation | Boot and keyboard are confirmed; trackpad, persistent storage, wireless, suspend/resume, battery/thermal reporting and controlled power transitions are not established. | Maintain an explicit hardware support matrix, then implement these as separate validated milestones after the rendering/timing foundation. |
| Tests and diagnostics | Extensive serial/QMP smoke tooling and a local CI definition exist; the workflow has not been verified remotely in this session. | Add visual correctness, rendering counters, performance budgets, malformed-input/failure injection and repeat physical boots. Keep emulation and hardware evidence separate. |

## Implementation sequence and completion criteria

### Phase 0 Preserve the baseline and measure the hot paths

Save a reviewed source checkpoint and immutable copy of the known boot payload.
Add a small render benchmark and diagnostics for console writes, scroll count,
framebuffer bytes read/written, glyphs, presents, allocation count, page-table
switches and maximum interrupt-disabled duration. Record time through a
calibrated reference, with instrumentation output outside the timed operation.

Completion: reproducible results at a fixed resolution/build configuration for
an empty-screen line, a bottom-of-screen line, 1,000 lines of 80 characters,
full-screen clear, large/small blits, GUI text editing and window movement.
Compare serial enabled/disabled and normal/diagnostic boot. Keep separate QEMU
and laptop result sets. Do not require another full boot suite merely to collect
source-level audit evidence.

### Phase 1 Correct text and keyboard semantics

Add the shared printable-ASCII font and remove case folding in both renderers.
Implement Caps Lock with Shift interaction, per-device modifier state and USB
key repeat driven by calibrated time; suppress duplicate presses correctly.
Retain the existing byte-based shell/ABI initially.

Completion: `aAzZ`, all printable punctuation, mixed-case command arguments and
file contents display correctly in the monitor, shell and GUI widgets. Tests
verify both decoded bytes and rendered pixels. Cover both Shift keys, Caps Lock,
Shift+Caps Lock, press/hold/release and command editing. No USB boot regression.

### Phase 2 Make console output efficient

Separate text/ANSI state from raster output, use RAM for scrolling, add batched
writes and cursor painting, and present at bounded intervals while always
flushing a prompt/interactive update promptly. Add row/span primitives and an
allocation-free early/panic fallback. Serialize normal writers without holding
interrupts off through arbitrary-length output.

Completion: zero framebuffer reads for ordinary scroll, correct final screen
across different write chunk sizes, bounded work under output floods, responsive
keyboard input during scrolling, and preserved panic/boot diagnostics. Match
wrap, backspace, tabs, carriage return, ANSI erase/cursor movement and odd pitches
against a reference text model. A full-screen write may still be required after
scroll; avoid claiming bottom-row-only damage in that case.

### Phase 3 Reduce GUI rendering and presentation work

Propagate damage clips through drawing/composition, reuse validated staging
storage, reduce redundant copies and address-space switches, and use an efficient
format-aware framebuffer writer. Keep redraw correctness for overlapping,
transparent, moved/resized windows and cursor restoration.

Completion: counters show that a small text edit no longer redraws an unrelated
full desktop. Run existing displayd/client-exit/resolution tests plus screenshot
comparisons at multiple resolutions and pixel formats. Measure ordinary and
fallback presentation paths independently.

### Phase 4 Finish timing and kernel latency work

The clock prerequisite begins in Phase 0; this phase completes LAPIC calibration,
consistent deadlines, wall-clock separation, idle waits and driver timeout
conversion. Make input probing opt-in and use init readiness rather than fixed
sleep. Reduce long protected sections in allocation, GUI, block and network
paths with explicit ownership and locking tests.

Completion: one-second sleeps agree with a reference clock, timeout behavior
survives CPU/QEMU speed differences, output load does not stall input, and boot
measurements identify time spent in firmware, initialization, diagnostics and
init separately. Do not shorten USB debounce/recovery to obtain a faster boot.

### Phase 5 Harden storage and expand hardware support

Fix AHCI timeout/flush and block-cache failure behavior, then test exFAT failure
recovery. Add the Apple trackpad through multiple interfaces while preserving
keyboard input. Follow with physical networking and laptop power management as
separate features. Keep rollback payloads for every physical test.

Completion: error injection returns a bounded failure instead of hanging; flush
semantics match the documented contract; filesystem recovery tests pass; each
new hardware feature has its own laptop evidence. The filesystem stays in RAM
until persistent-storage validation is complete.

### Phase 6 Consolidate interfaces and larger ports

Refactor large files around tested ownership boundaries, reconcile old roadmaps,
and publish implementation/compatibility status per API. Reproduce Node/libuv
builds and runtime behavior. Consider GPU acceleration, shared-buffer protocols,
Unicode, SMP and demand paging only when their prerequisites and workloads are
clear.

## Performance acceptance targets

These are initial engineering targets, not measured results or promised gains:

- On the laptop at its recorded native mode, aim for at least a tenfold reduction
  in the 1,000-line console workload relative to the captured baseline.
- Aim for p95 key-to-visible-update latency below 50 ms during continuous output,
  without dropped input. Use external capture for the physical end-to-end check
  and internal timestamps to locate delay within the OS.
- Aim for p95 presentation time below 33 ms for ordinary text edits and window
  movement. Record render, copy and presentation separately, with median, p95
  and maximum values; throughput alone can conceal long pauses.
- No framebuffer reads in the ordinary software scroll path; no allocation per
  glyph or per console character; bounded normal-output batches.
- A normal boot must not include the intentional input-probe delay. Establish
  the absolute boot budget after stage timings are available.

Run each workload repeatedly on the same hardware, resolution, output length,
build profile and serial settings. QEMU is a correctness/relative-regression
check, not a substitute for laptop throughput. Promote these targets to CI
budgets only once measurement variability and the applicable environment are
known.

The immediate implementation slice is **shared mixed-case glyphs plus measured,
batched console scrolling**. That directly addresses both reported problems;
timing/latency work supports it, and the wider subsystem plan preserves a clear
path beyond the first performance improvements.

## Progress update: 2026-10-17

The physical laptop now has a validated mixed-case console, faster rendering,
built-in keyboard input, and USB exFAT file writes with independent Ubuntu
readback and clean offline checks. The expanded automatic suite's hardware
result is recorded in [USB file workload](usb-file-workload-2026-10-17.md).

The next candidate adds an [opt-in persistent USB workspace](persistent-workspace-2026-10-17.md)
for shell documents/scripts, explicit flush/unmount and controlled reboot. Its
QEMU validation includes persistence across boots and failed-write refusal;
physical workspace run `2f1a6035` subsequently passed exact Ubuntu readback and
a clean offline filesystem check on release `385b670049974e1c`. `/fat` stays in RAM. GUI work stays
deferred; physical networking and laptop power management remain separate future
milestones, not implied by the storage results.

The next increment adds a [persistent shell environment and unattended editor
tests](persistent-shell-2026-10-17.md): one-command workspace startup, saved
history/settings, checked editor saves, and independent host verification of
real shell workloads. QEMU key-echo measurements also expose application stalls
during slow filesystem writes, despite continued keyboard service. Finer
filesystem synchronization is the next responsiveness target; this increment's
physical validation is still pending.
