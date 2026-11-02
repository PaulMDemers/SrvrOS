# Built-in applications and document opening

The shared descriptors in `shared/include/srvros/desktop_apps.h` supply launcher,
dock and executable discovery, desktools role lookup, document handlers and
compositor service admission. Existing numeric application IDs and protocol v15
are unchanged. Stable keys such as `srvros.text` identify apps independently of
display titles; program/role pairs remain an executable whitelist.

## Descriptor and matching contract

Each built-in entry names its title, role, program, dock position, stable key,
space-separated ASCII extensions, fallback policy and permitted service bits.
`desk_application_by_key` and `desk_application_for_role` return zero for an
unknown key or program/role. The registry contains the shipped applications;
external manifests, installation and user association persistence are future work.

`desk_application_for_path` selects an extension match before a generic fallback.
Only the last filename suffix is examined, case insensitively. Preview declares
PNG/JPG/JPEG/BMP; Text is the generic candidate and still validates UTF-8, size and
actual contents. A candidate is not proof that decoding will succeed. Null,
empty and directory-ending paths have no document candidate.

`desk_application_handlers` returns the total candidate count, preferred first,
while storing no more than the supplied capacity. Null/zero-capacity queries
perform no writes. These functions allocate nothing and perform no I/O.
`desk_application_services_valid` permits only each registered app's declared
service bits. Text and Preview may offer OPEN_DOCUMENT; other built-ins cannot
acquire that capability by sending a request. Existing registration ownership,
parent-window restrictions and routing admission still apply.

## Files interaction and ownership

Select a regular file and use File > Open With, its contextual menu, or
Ctrl-Shift-O. The owned chooser offers the preferred app and other registered
candidates, including Text for an image suffix. Enter selects the focused choice;
Escape, Cancel, parent Close or cooperative Quit cancels without launching.
The choice applies to this opening only. Folder/busy/modal selections refuse
this command. The captured path remains fixed until the chooser resolves.

The current button dialog fits Cancel plus the two shipped document handlers.
If a future registry supplies more candidates, Files refuses this presentation
explicitly; the registry itself still enumerates all candidates. A larger
selection control should replace this presentation before adding more handlers.
This has no relation to window/process capacity.

Accepted choices use the existing asynchronous launch/document service. Text
creates a sibling model in its existing process; Preview follows its existing
open/dirty-document negotiation. Service refusal retains the existing new-process
fallback. Opening another document does not implicitly save/discard unsaved work.
Worker jobs, window retirement and save-on-close rules are unchanged. There is
no path execution, synchronous filesystem lookup or content decoding in the
registry or compositor input loop.

## Validation

`make test-app-registry test-desktop` covers descriptor/key/dock consistency,
matching precedence and bounds, per-app services, Files chooser candidates and
captured path, cancellation, unsupported choices, busy/folder refusal, menu and
keyboard guards. The actual-consumer fixture is:

```sh
python3 tools/desktop_lifecycle_smoke.py --open-with
python3 tools/desktop_lifecycle_smoke.py --refuse-dialogs --open-with
python3 tools/desktop_files_windows_smoke.py
```

The directed lifecycle fixture checks cancellation, existing-process Text/Preview
routing, explicit Text selection for a text document with an image suffix, exact
saved bytes, unsaved sibling retention, failed-save recovery and process baseline.
The refusal variant checks embedded modal fallback. Results must name the exact
source and remain distinct from physical acceptance in the machine record.
