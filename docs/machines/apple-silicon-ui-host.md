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
| Attended pointer interaction | User confirmed held slider drag and button release outside the window both work correctly on November 3 |
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
shared-SDL diagnostic app was used for the attended pointer check.

The user confirmed “Both work correctly” for held slider drag and pressing
“Try action” then releasing outside the window, with no stuck pressed state.
This completes the remaining attended host acceptance. The native automation's
trace showed DOWN, UP, then unheld motion; the human confirmation is separate
from that tool limitation and from the passing injected capture/cancellation
fixtures. No physical SrvrOS boot is armed and the original review VM is untouched.

Linux/Windows, full IME preedit, shaping/grapheme layout, native accessibility and
notarized distribution remain outside this milestone. The macOS showcase milestone
is complete and accepted. Published implementation revisions remain SrvrUI
`d65c93f` and SrvrOS `7fa07c9a`; this closeout changes documentation only.
[SrvrUI PR2](https://github.com/PaulMDemers/SrvrUI/pull/2) merged as
`76deeb30c583a98cf47ffd9bdada34c15b37890c`; its tree exactly matches the accepted
`d65c93f` implementation. The OS pin now names that merged revision. Rebuilding
that merged child passes all 12 CTests and the native Cocoa showcase smoke.

[SrvrOS PR289](https://github.com/PaulMDemers/SrvrOS/pull/289) integrates main
`105077d70fe35c8f85fd7954c0f8118a98c2710b`, preserving the newer Air6,1 read-only
HDA diagnostics. The combined tree passes the full image build, 110 desktop/I/O
PASS records, theme preview, 789-link docs check, package/platform audit and CPU
receipt tests. Merge verification logs live in `build/review/ui-host/merge/`.
The shared checkout and its review VM remain untouched. No additional feature
work is required for this milestone.
