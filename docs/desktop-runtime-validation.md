# Desktop runtime validation and next native round

The October 25 desktop work removes the small fixed window-count ceiling and
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
| D4a | One stack sort per frame; conservative opaque-frame/shadow occlusion | Exact reference pixels and identical-workload host benchmarks; application/scale QEMU regression gates |

All pools retain independent finite memory/process/descriptor budgets. “Dynamic”
does not mean every requested workload fits. Partially covered windows still use
normal painting. Parented dialogs, broader chooser/activation services,
native-density fonts, text composition and accessibility remain separate work.
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

Use fresh names such as `runtime-a.txt`, `runtime-b.txt`, `runtime-c.txt` and
`runtime-notes.txt`; preserve earlier files rather than overwriting them. The
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
refuse further windows while retaining existing edits. Native higher-count
acceptance, gesture behavior, input latency and power measurements remain pending
until the matching physical evidence is collected. Do not automatically start
this trial when the overnight no-reboot window ends.
