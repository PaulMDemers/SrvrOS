# Desktop runtime validation and next native round

The October 25–26 desktop work removes the small fixed window-count ceiling and
separates windows, processes and worker ownership. It is a matched kernel/server/
client baseline, not a stable external binary ABI. Read the [capacity contract](window-capacity.md),
[connection contract](desktop-connections.md) and [API roadmap](desktop-api-evolution.md).
The [machine records](machines/README.md) own physical acceptance; a QEMU pass on
one host does not accept another Mac's input, display or firmware.

## Implemented blocks

| Block | Delivered behavior | Evidence boundary |
| --- | --- | --- |
| D0 | Clipboard state follows each admitted window, with bounded transfer/retry | Host 32 owners; actual 16 mixed clients and exact saved contents |
| D1 | Stable window identity, one connection dispatcher, acknowledged creation/retirement, independent Text documents | Actual multiwindow lifecycle, resize/clipboard and document persistence |
| D2 | Dynamic kernel queues/surfaces and compositor records; byte-budget refusal; complete Window/launcher discovery | 128 small windows in one process; pressure/rollback, replacement and complete retirement |
| D3a | Shared lazy file-worker pools for Text/Files, immutable jobs and FIFO leasing, drain/reap before release | 32 normal Text documents/33 exact files; 40 queued writes/readbacks using two workers |
| D3b | Combined GUI/nonblocking pipe/PTY wait, absolute deadline, cancellation and descriptor lifetime watches | Actual threaded ABI; typing during throttled writes; no claim of arbitrary GUI file descriptors |
| D3c | Shared asynchronous file chooser controller for Text/Preview | Host cancellation/reopen/correlation and actual legacy/pooled listing plus application VM gates |
| D3d/e | Shared bounded dispatcher/commands plus optional stable window/model ownership; Text/Files siblings, owned mutation dialogs and Terminal PTY waits | Forty-model host refusal/drain/fatal tests; twenty Files windows, accepted 8 MiB copy during close, concurrent Text saving and real-shell consumer VMs; native acceptance separate |
| D4a | One stack sort per frame; conservative opaque-frame/shadow occlusion | Exact reference pixels and identical-workload host benchmarks; application/scale QEMU regression gates |

All pools retain independent finite memory/process/descriptor budgets. “Dynamic”
does not mean every requested workload fits. Partially covered windows still use
normal painting. Protocol v15 also implements parented dialogs, cooperative
Quit and bounded existing-process document routing; see the
[application lifecycle contract](desktop-application-lifecycle.md). Broader app
services, native-density fonts, text composition and accessibility remain
separate work.
The selected [portable C/POSIX profile](posix-supported-profile.md) has its own
acceptance; this desktop work is not full POSIX or Linux compatibility.

## Repeat the local gates

Use one checkout and one build/VM harness at a time. The scripts share output
paths; archive a failed run before retrying. Preserve the source commit and
hashes of the kernel, initramfs, filesystem and ISO actually used. Do not mix an
old diagnostic kernel with new userspace. Run the current host suite and build:

```sh
tools/host-env.sh make -j2 test-desktop all
```

Then run the affected consumers serially:

```sh
tools/host-env.sh python3 tools/desktop_multiwindow_smoke.py
tools/host-env.sh python3 tools/desktop_window_growth_smoke.py
tools/host-env.sh python3 tools/desktop_capacity_smoke.py --document-growth
tools/host-env.sh python3 tools/desktop_file_pool_smoke.py
tools/host-env.sh python3 tools/desktop_wait_load_smoke.py
tools/host-env.sh python3 tools/desktop_text_windows_smoke.py
tools/host-env.sh python3 tools/desktop_files_smoke.py
tools/host-env.sh python3 tools/desktop_files_windows_smoke.py
tools/host-env.sh python3 tools/desktop_window_dock_smoke.py
tools/host-env.sh python3 tools/desktop_window_navigation_smoke.py
tools/host-env.sh python3 tools/desktop_window_commands_smoke.py
tools/host-env.sh python3 tools/desktop_terminal_smoke.py
tools/host-env.sh python3 tools/desktop_lifecycle_smoke.py --pointer
tools/host-env.sh python3 tools/desktop_lifecycle_smoke.py --shutdown
tools/host-env.sh python3 tools/desktop_lifecycle_smoke.py --crash
tools/host-env.sh python3 tools/desktop_lifecycle_smoke.py --refuse-dialogs
tools/host-env.sh python3 tools/desktop_preview_smoke.py
tools/host-env.sh python3 tools/desktop_capacity_smoke.py --clipboard
tools/host-env.sh python3 tools/desktop_apps_smoke.py
tools/host-env.sh python3 tools/desktop_compositor_smoke.py
```

These are disposable VM workloads, not commands to run against a physical drive.
Read each receipt: opening windows alone is insufficient. Require exact saved
contents, matching lifecycle/worker retirement, a clean process baseline and the
collector's relevant filesystem/allocation checks. Inspect screenshots for clipped
menus, stale pixels and unchanged document contents. A guest success string does
not override a failed or incomplete collector.

`build/tests/desktop-overdraw-host` compares the optimized and unculled renderer
on identical scenes and clips, then reports writes and timing distributions. Its
reference deliberately shares drawing primitives and uses the same prepared
stack. It isolates painting savings, not sorting, IPC, framebuffer copies, scanout
or physical input latency. Alternating execution order reduces systematic warmup
bias; timings remain descriptive host results, not universal thresholds.

## Next Air7,2 physical acceptance

The overnight work did not install or reboot a payload. Keep Ubuntu default and
the existing known-good recovery. Before any later physical trial, follow the
[Ubuntu device setup sequence](ubuntu-device-agent-setup.md): collect the prior
session, confirm this host/model and USB UUID, validate the exact staged image,
retain recovery, and write the on-disk run record. Do not copy another Mac's
profile, volume identity or acceptance.

This next round requires a person and should be announced as interactive before
reboot. The on-screen **Runtime trial** Help topic contains the checklist and
profile-specific exit instructions. Open Help, press Ctrl-L, search `Runtime trial`
and press Return. The general Laptop trial page points to it; older tap/device
recipes are optional reference material, not additional requirements for this round.
The combined Home profile starts Text, so open Help through the launcher.

The checklist covers:

1. Open several normal apps, then use Text's **Ctrl-Shift-N / File → New Window**
   to cross sixteen total windows. Use independent document contents and names;
   Ctrl-N alone replaces the current document after its unsaved-work guard.
2. Find early, middle and late windows through Window and the launcher. Minimize,
   restore, tile, maximize, drag and resize overlapping windows. Check the exposed
   shadows, titlebars and content for stale drawing.
3. Copy/paste between documents and apps; type, scroll and move the pointer while
   saving. Cancel an edited document's close, then save it. Closing one Text window
   must leave its siblings usable.
4. In Text and Preview, browse in the file picker, cancel, then reopen. Wait for
   any pending listing to drain; cancellation must retain the current document or
   image. Save several distinct files, close and reopen them, and confirm their contents.
   Save screenshots. Exit normally and reopen the desktop to check cleanup.
5. Choose SrvrOS → Return to console. With the combined battery/Home profile,
   Home stays mounted: use `desktop battery` to reopen and check the files, then
   return to the monitor and use `reboot` for the flushed, unmounted Ubuntu return.
   A basic trial unmounts on desktop exit; wait for `USB unmounted=yes`, then
   `reboot`. Collect both matching sessions and verify files/allocation before
   accepting the exact release. Immediate Ctrl-Alt-Escape is a stuck-session
   escape, not the normal completion path.

Use fresh names such as `runtime24-a.txt`, `runtime24-b.txt`, `runtime24-c.txt` and
`runtime24-notes.txt`; preserve earlier files rather than overwriting them. The
notes should name completed/skipped checks and any input pauses or drawing
artifacts. Capture many windows and the reopened documents with Command-Shift-3,
and wait for save confirmation. Keep expected document text alongside the
collected receipts so Ubuntu comparison checks contents, not just file presence.

`tools/host-env.sh python3 tools/desktop_help_smoke.py` rehearses topic discovery,
scrolling to the return instructions and compact layout in a disposable VM;
its `help-runtime-start` and `help-runtime-return` screenshots are readability
evidence, not physical acceptance. Exact-stage VM validation and this host's
recovery/USB preflight remain required before installation.

Report any refusal separately from corruption or a hang: budgets can legitimately
refuse further windows while retaining existing edits. Later native higher-count
results appear below. Gesture behavior, input latency and power measurements
require their own matching physical evidence. Do not automatically start
this trial when the overnight no-reboot window ends.


## October 25 mixed-app hardware result

Air7,2 release `2750e5efa04573ba` reached fifteen application windows with the
combined device services enabled, then logged `exec: scheduler thread table full`
three times. That release has 32 global scheduler slots. The launcher is an
overlay and does not consume another window. The 128-window single-process and
32-document fixtures validate window growth, not this mixed-process workload.
That failure was addressed by the scheduler growth and visible launch-failure
feedback changes; the follow-up acceptance is recorded below. See the [Air record](machines/macbookair7-2.md)
for clean retirement, filesystem preservation and the pending checks. Do not
count the general device success as document/chooser/reopen acceptance.

The follow-up uses stable scheduler chunks under a stack/metadata byte budget
and visible app-launch refusal. Its combined-service mixed-app regression opens
24 separate processes with 2 GiB guest RAM, saves a document after growth, then
retires and reopens the session with offline filesystem checks. See
[window capacity](window-capacity.md#scheduler-storage-after-the-mixed-app-native-trial)
for the independent resource budgets and [the current unit record](machines/macbookair7-2.md)
for validation status. On October 25 native release `c9e59c1b11d5c239` completed
two automatic rounds of 24 rendered mixed app processes, with screenshots, normal
child exits and complete battery/logger retirement. Prior files were preserved
and offline filesystem/allocation checks passed. This accepts automated native
capacity/reuse; physical input and interactive document/chooser checks at higher
counts remain pending.

The subsequent attended Air7,2 return on release `80904d5e5fd6f6ac` exercised a
mid-20s window count and sensors. The user did not receive chat instructions
before the machine rebooted; do not count the entire document checklist as
completed. Future attended checklists must open automatically in the guest or
be captured by the user before reboot. Current unattended v73 capture performs
acknowledged Text edits, five fresh USB save/rename/reread checkpoints and shared
picker cancellation per round, displays progress and returns automatically.

The extended unattended round is now physically accepted on Air7,2 release
`f01d4049daae0b4a` / source `044d0b4`: both 24-process rounds completed 60
acknowledged edit/clipboard events, five exact USB save/rename/reread files,
legacy/pooled chooser cancel/reopen checks, screenshots and normal retirement.
All ten saved files were compared in Ubuntu; prior non-metadata files were
unchanged and offline fsck/allocation passed. See the current machine record for
session identities and remaining physical-input/manual-visual limits.

## Idle lifecycle and report worker waits

The compositor lifecycle worker sleeps on a latched process-scoped futex when
it has no admitted work. Allocation, launch, buffer discard, screenshot cleanup
and shutdown publish their existing release/acquire records before notifying the
worker. The reporter has a separate latch for snapshot publication and shutdown.
Clearing the latch before scanning and comparing it atomically when sleeping
covers publication before startup, during work and immediately before the wait.
Multiple publications coalesce; the work scan still visits every existing slot.
UI completion consumption and resource credit remain unchanged.

Screenshot cleanup still retries once per tick while its child is retiring;
shutdown still joins only after that ownership drains. A failed/interrupted wait
without a notification retains the old one-tick retry instead of spinning.
No queue, memory, timing, watchdog, same-process I/O or integrity limit changes.
Native battery supervision and USB HID polling are outside this change.

`make test-lifecycle-wait` exercises actual worker threads through a portable
host compare-and-sleep adapter: idle blocking, publication before creation and
between scan/sleep, launch/allocation/discard/report delivery, screenshot retry
and shutdown retention, stop races, interrupted waits and partial thread-start
rollback. Existing desktop-session tests retain cache budgets/readback and
retirement checks. The power VM exercises the real futex syscalls and normal
app lifecycles; audio/disk/app/screenshot stress also exercises the report and
capture paths. Native wakeup or energy improvement requires a separate matching
physical power trial; passing host/VM checks alone does not establish savings.
