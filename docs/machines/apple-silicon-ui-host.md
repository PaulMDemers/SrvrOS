# SrvrUI macOS host showcase

## Active scope — November 3, 2026

Target: standalone macOS ARM64 SDL3 widget showcase, with unchanged SrvrOS
widget behavior and Silver/Aero/Aqua styling. User authorized implementation,
standalone builds and acceptance checks. This separate checkout owns its
submodules, build output and disposable VM state. The shared integration checkout
and open theme review VM must remain untouched.

Base: SrvrOS e376669c, SrvrUI 89fb6d9 (remote main merge 453053b has identical
content), SrvrApps d245fe2, SrvrPorts 0494ae2. PR278 application descriptors,
PR252 process/I/O lifetimes and PR19 USB Wi-Fi remain separate workstreams.

Owned files: SrvrUI portable renderer/build/tests, optional SDL3 adapter and
showcase; SrvrOS renderer/build compatibility and documentation. No kernel,
driver, desktop protocol or application service behavior changes are intended.
Publish the child revision before any OS pin update. Actions remain disabled.

Acceptance: fresh standalone build; real widgets with pointer/capture, keyboard,
UTF-8 commit and clipboard, fractional scroll, resizing/Retina and live themes;
core boundary/clipping/stride/theme tests and sanitizers; matched OS host tests,
full build and isolated representative desktop/theme VM regression. Record
injected events separately from native host interaction. Linux/Windows, full
IME preedit, shaping and accessibility integration remain unaccepted.

Journey: open the showcase, try controls, edit/select/copy text, scroll the list,
switch each theme, resize and confirm state is retained. Keep primary actions
reachable at compact sizes and report capacity failures without losing text.
Existing themes and geometry are the visual direction; no redesign is needed.

## Implemented and verified

Published SrvrUI `d65c93f5153bdc07592438574950d7affb3b8019` supplies the
standalone core, renderer/font ownership, CMake installation, optional SDL3
adapter, checksum-pinned static SDL 3.4.12 recipe and macOS application bundle.
SrvrOS retains thin compatibility paths; host tests compile canonical renderer
sources and sysroot installation copies the real header rather than a relative
forwarder. Key/calendar/image limits remain byte-compared value snapshots; no
desktop wire ABI, kernel or driver behavior changes.

Verification on macOS 26.6.2 ARM64, Xcode Clang 21, CMake 4.4.0:

| Check | Result |
| --- | --- |
| Standalone CTest in matched checkout | 12 tests pass, assertions enabled in Release |
| Fresh source directory outside OS tree | 9 tests pass; no parent headers or build files |
| Installed CMake package consumer | Compile, link, window/canvas/present and cleanup pass |
| ASan/UBSan | 11 tests pass; ordinary archive purity check run separately |
| Pinned SDL3 | Archive hash verified; static source build/install passes |
| macOS bundle | System-framework-only linkage; bundled SDL/font notices |
| Showcase synthetic events | Native Cocoa and dummy smoke pass; three profiles and compact capture inspected |
| Native host interaction | Keyboard/dead-key é, Command copy/paste, undo/redo, checkbox, slider click/keys, Tab/Shift-Tab, themes and scrolling observed |
| Retina | 900×650 logical canvas presented to 1800×1300 window backing; coordinate scale 1, density/display scale 2 |
| Matched OS | Full image build, desktop/widget suite (110 PASS records), desktop I/O, theme preview and docs pass |
| Installed OS sysroot | Canonical renderer header compiles through staged include tree |
| Isolated theme VM | Live Aero/Aqua, existing/new Text selection retention, menus/dialogs, drag/Help wheel, saved Aqua and desktop restart pass |

Runtime core renderer data/behavior is retained from e376669c. Host-only fixes
after the first OS VM run do not enter the SrvrOS widget source list. The final
matched revision/build hashes are recorded in the local build manifest.

Evidence: `build/review/ui-host/`, `build/review/desktop-theme-profiles/` and
`userspace/widgets/build/review/`. The pinned deliverable is
`userspace/widgets/build-pinned/SrvrUIShowcase.app`; `build/package/` under the
widget checkout contains the installed library and application. The initial
shared-SDL diagnostic app remains open for the requested attended pointer check.

One attended check remains pending: held slider drag and release outside the
window. The native automation's trace shows DOWN, UP, then unheld motion, so it
cannot establish that acceptance. Actual adapter held-motion, outside-release,
missed-release and focus-cancellation fixtures pass. Do not turn those injected
checks into a human or native-device claim. The user has been asked to check the
open showcase. No physical boot is armed and the original review VM is untouched.

Linux/Windows, full IME preedit, shaping/grapheme layout, native accessibility and
notarized distribution remain outside this milestone. The UI and OS changes are
ready for draft review, with attended pointer acceptance still open.
