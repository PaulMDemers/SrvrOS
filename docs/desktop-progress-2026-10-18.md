# Desktop development session, 2026-10-18

The user authorized autonomous development until **10:00 EDT (14:00 UTC)**:
advance the widget library, shared desktop UI and useful GUI applications,
adding supporting OS work as needed. Use host checks and VMs; **do not reboot
the laptop or replace the accepted hardware boot**. The visual direction stays
quiet and refined; Tiger is a reference for scope/coherence, not a pixel copy.

## Working sequence

1. Structured keyboard delivery, reusable editor/list controls and focus routing.
2. Shared client runtime; a desktop launcher, coherent window navigation and
   application lifecycle/unsaved-close behavior.
3. Files and Text: browse, open, edit, save, reopen with honest errors and durable
   data. Keep file work out of the input/render path.
4. Settings/basic system tools and Terminal as supported by available OS APIs.
5. Regression, visual review, performance and remaining supporting OS work.

Commit verified slices, preserve evidence, and carry forward explicit limitations.
The accepted hardware checkpoint remains `f05dfcc01c28dc85` (source `a870060`).
The initial separate toolkit checkpoint is `9149128`.

## Structured keyboard foundation

Two opt-in GUI-owner-only syscalls expose keyboard events. USB HID and translated
PS/2 supply usage IDs, printable codepoints, modifiers, press/release and repeat.
The stream is independent of console Escape sequences and control bytes. In event
mode Ctrl-C is a key binding, not a foreground-process interrupt. Mode changes
discard queued input; old held-key repeat cannot replay into the console. Queue
overflow and HID rollover expose cancellation. The GUI owner's exit restores
console input even when it never explicitly disables event mode.

The existing console and hardware-test desktop remain on their byte input path.
The event layout currently supports US ASCII translation; full keyboard layouts,
IME/composition and device-specific identifiers are future work. Clients will
receive events through the compositor, not access the kernel queue themselves.

Validation: full build and core host suite passed. Keyboard ASan/UBSan passed.
The QEMU compositor regression additionally exercised real structured input,
Ctrl-C isolation, arrow keys and abrupt input-owner exit, then ran the existing
study/window/scrollbar/lifecycle checks successfully. Evidence:
`build/review/keyevents-20261018/`.
