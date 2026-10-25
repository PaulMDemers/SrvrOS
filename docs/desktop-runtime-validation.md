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
| D4a | One stack sort per frame; conservative opaque-frame/shadow occlusion | Exact reference pixels and identical-workload host benchmarks; application/scale QEMU regression gates |

All pools retain independent finite memory/process/descriptor budgets. “Dynamic”
does not mean every requested workload fits. Partially covered windows still use
normal painting. Parented dialogs, shared chooser/activation services,
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
reboot. Put these instructions in the on-screen trial guide for the staged round:

1. Open several normal apps, then use Text's **Ctrl-Shift-N / File → New Window**
   to cross sixteen total windows. Use independent document contents and names;
   Ctrl-N alone replaces the current document after its unsaved-work guard.
2. Find early, middle and late windows through Window and the launcher. Minimize,
   restore, tile, maximize, drag and resize overlapping windows. Check the exposed
   shadows, titlebars and content for stale drawing.
3. Copy/paste between documents and apps; type, scroll and move the pointer while
   saving. Cancel an edited document's close, then save it. Closing one Text window
   must leave its siblings usable.
4. Save several distinct files, close and reopen them, and confirm their contents.
   Save screenshots. Exit normally and reopen the desktop to check cleanup.
5. Return to the monitor, follow the trial's workspace-unmount instruction, then
   reboot to Ubuntu. Collect the matching session and verify files/allocation
   before accepting the exact release.

Report any refusal separately from corruption or a hang: budgets can legitimately
refuse further windows while retaining existing edits. Native higher-count
acceptance, gesture behavior, input latency and power measurements remain pending
until the matching physical evidence is collected. Do not automatically start
this trial when the overnight no-reboot window ends.
