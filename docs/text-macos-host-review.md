# Text on macOS: dependency review

## Result and scope

The bounded dependency review is complete. A single-window macOS Text application
is feasible using the accepted SrvrUI SDL3 renderer and widgets. Text is not yet
a standalone application: compilation succeeds with the matched OS headers, but
window, file, chooser, clipboard and lifecycle services still require host
implementations. This review changes documentation only.

Closeout on November 4: this research milestone is finished. The implementation
proposal below remains optional, unstarted future work. The subsequent merged
density work is recorded in the [host status](machines/apple-silicon-ui-host.md);
it does not imply that Text itself has been ported or accepted on macOS.

Reviewed November 3, 2026 in the isolated `SrvrOS-ui-host` checkout:

- SrvrOS main: `585ad8e5ea3ec6fedb109a7a237346b26533f6e5`.
- SrvrApps: `ae56bf7175ed7df6ba6a8593aa86da95d93830be`.
- SrvrUI: `76deeb30c583a98cf47ffd9bdada34c15b37890c`.

The review refreshed main and matched submodules after the showcase merge, so
it includes the merged application registry/Open With and Files recovery work.
PR252 process/I/O lifetimes and PR19 USB Wi-Fi were still open when checked;
no SrvrApps PRs were open. The shared checkout and its review VM were untouched.
The completed [showcase acceptance](machines/apple-silicon-ui-host.md) remains
separate from future Text acceptance.

## What can be shared and what needs a host boundary

| Area | Source evidence | macOS implication |
| --- | --- | --- |
| Text model and drawing | SrvrUI text, edit, history, searchbar, dialog and renderer sources already belong to the standalone library | Reuse these implementations and the existing themes. |
| Document transitions | [text_app.c](../userspace/apps/desktools/text_app.c) handles modified state, New/Open/Save, pending close, conflicts and completion | Preserve one shared controller; isolate file submission, path defaults, status/layout and lifecycle hooks. |
| Search and replace | [search.c](../userspace/apps/desktools/search.c) uses shared text/search widgets but accesses `struct tool` and UI/status hooks | Extract Text-specific application state without copying the search algorithm. |
| Application state | [app.h](../userspace/apps/desktools/app.h) embeds desktop windows, chooser/IPC jobs and state for Files, Calculator, System and Settings | Introduce a narrow Text model/view boundary in SrvrApps; avoid requiring all desktop-tool state in the host package. |
| Layout and input order | [main.c](../userspace/apps/desktools/main.c) combines Text layout/drawing with command, modal, search and role dispatch | Share Text layout and action policy; keep SDL event pumping in a host shell. |
| Commands | [menus.c](../userspace/apps/desktools/menus.c) builds enabled snapshots and routes keys/buttons/menus; New window currently tests a desktop connection | Preserve guard parity. Represent host capabilities explicitly; a fake desktop connection would be the wrong dependency. |
| Windows and lifetime | [text_runtime.c](../userspace/apps/desktools/text_runtime.c) owns the multiwindow set, GUI connection, surfaces, worker pool and retirement | Keep the SrvrOS implementation. A one-window SDL shell can own the host window and document directly. |
| File work | [desk_file_job.c](../userspace/lib/src/desk_file_job.c) spawns `/fat/bin/deskfs`, uses SrvrOS pipes/waits and owns immutable in-flight data | Supply a bounded host worker and completion queue behind document operations; keep application state on the UI thread. |
| Save semantics | [deskfs.c](../userspace/deskfs/deskfs.c) checks expected contents, calls `srv_fs_replace`, syncs and reads bytes back | Define and test host replacement, conflict and verification behavior. Linking the worker against host libc alone does not supply `srv_fs_replace`. |
| Path selection | [picker.c](../userspace/apps/desktools/picker.c) uses `desk_chooser` and directory jobs | A native host picker can return selected paths to the shared controller, with cancellation and late-result ownership preserved. |
| Clipboard | [clipboard.c](../userspace/apps/desktools/clipboard.c) uses desktop packet transfers and guards selection/revision changes | Use host clipboard handling once; do not replay SDL-handled copy/cut/paste through the OS IPC client. |
| Defaults and diagnostics | [desk_paths.h](../userspace/lib/include/srvros/desk_paths.h) accepts only OS Home paths; `timing.h` calls scheduler tracing and OS clocks/logging | Supply host path defaults and diagnostic hooks. No syscall compatibility layer is needed for the document model. |

The SDL3 adapter currently binds one `desk_ui` and delivers keys directly to it.
Text also routes input to a dialog and a separate searchbar UI. The host needs
application-command interception and explicit active-input ownership. Preserve
the adapter's UTF-8 commit, clipboard, capture and Retina behavior while adding
the smallest routing extension needed by this consumer. A change of UI owner
must cancel old capture/composition even when both owners have focus index zero.
Test that search/dialog input cannot edit the document behind it.

The pinned SDL 3.4.12 headers provide asynchronous Open and Save dialogs. Their
callbacks may run on another thread and returned paths expire when the callback
returns. Copy the result into owned storage and deliver it to the UI thread;
retain request context through cancellation/window closure. This is a source
review of the bundled API, not an exercised native-picker implementation.

## Evidence and limits

Probes are saved under `build/review/text-host/` in this isolated checkout:

| Probe | Result |
| --- | --- |
| Compile `text_app.c`, `search.c`, `menus.c`, `main.c`, `text_runtime.c` using native ARM64 Clang, warnings as errors and matched headers | All five compile. Undefined-symbol listings expose the remaining application/OS service imports. |
| Compile Text with only standalone SrvrUI headers | Expected failure at missing `srvros/desk_paths.h`; the current app is not an independent SrvrUI consumer. |
| Native ARM64 `test_tool_text_history.c` | Pass: reserved/owned journals, independent undo/redo, allocation and attachment failures. |
| Native ARM64 `test_text_replace.c` | Pass: UTF-8, replacement limits, atomic rejection and 10,000 undo/redo comparisons. |
| Native ARM64 `test_tool_commands.c` | Pass: key/button/menu guard parity, busy-save refusal and close/quit policy. |

`compile-probes.json` and `test-probes.json` retain exact compiler commands;
`source.json` identifies the matched source. `file` reports Mach-O ARM64 test
executables. These tests exercise selected code with existing fixture stubs and
linker removal of unused sections. They do not link or run the complete Text
application, perform host document I/O, or accept a native picker/lifecycle.

## Optional future implementation — not started

Deliver one macOS ARM64 Text window with Open, New, Save, Save As, editing,
undo/redo, find/replace, clipboard and guarded Close/Quit. Keep the existing
Silver/Aero/Aqua presentation and the documented 64 KiB document capacity.
Multiwindow parity, Finder document registration, full IME preedit, shaping,
accessibility and notarization can follow as separately accepted work.

Implement in this order within that milestone:

1. In SrvrApps, separate Text-specific state/controller and layout from the
   multi-role shell. Keep the existing SrvrOS runtime calling shared logic.
   Expose only operations required by this application: file request/completion,
   path selection, close intent, host capabilities and status/diagnostics.
2. In SrvrUI, add the bounded input-routing support required for application
   shortcuts, search and modal focus. Retain the current showcase API/behavior.
3. In SrvrApps, add a standalone CMake target and SDL macOS shell, asynchronous
   file worker, picker handoff and application bundle. Use the accepted static
   SDL recipe and package linkage. Do not fabricate desktop service success.
4. Integrate matched child revisions into SrvrOS and run affected host and
   representative Text lifecycle/file VM checks in the isolated checkout.

File completion must preserve current policy: an accepted save owns its byte
snapshot; newer edits stay modified; a pending close waits for completion and
rechecks modification; failed reads/writes retain the document; existing or
externally changed destinations require an explicit decision. Verify exact
saved bytes and errors before announcing success. Define host replacement and
concurrent-writer limits before implementing that backend; a CRC comparison
alone does not make check-and-replace indivisible.

The current OS request path is only 160 bytes including its terminator. The host
boundary should own host paths separately, with explicit length checks and no
silent truncation, while the OS adapter retains its existing wire limit. Do not
change the shared desktop ABI just to fit macOS paths. A single-window first
release should expose only supported window commands.

Acceptance must include a standalone build outside the OS tree; real temporary
file Open/Edit/Save/reopen with byte equality; unsaved Save/Discard/Cancel;
replacement confirmation and externally changed destination refusal; invalid
UTF-8, oversize, long-path and I/O failures; edits during delayed save and close
during accepted work; picker cancellation/late callback cleanup; command/modal/
search focus routing; clipboard, dead-key input, resizing and Retina; matched
SrvrOS regressions. Use a throwaway directory for file tests. Preserve the
existing OS multiwindow and worker ownership behavior during extraction.

This defines a possible future scope. The authorized dependency review is
finished; no application implementation or new runtime acceptance is claimed.
