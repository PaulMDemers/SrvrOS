# Architecture

Current overview, 2026-10-25. SrvrOS is a freestanding C/x86_64 kernel with
assembly entry points and ring-3 applications. It uses Limine for boot and
selected upstream libraries for ports, decoding, fonts and ACPI. The system is
still small, with explicit resource ceilings and incomplete isolation.

## Boot and storage layout

1. Limine loads the higher-half kernel and USTAR initramfs.
2. The kernel establishes memory management, descriptor tables, interrupts,
   scheduler, clock, framebuffer console and enabled device paths.
3. VFS exposes the initramfs and mounts the generated exFAT image at `/fat`,
   from an attached supported disk or the RAM-backed image in initramfs.
4. System init runs startup scripts/services; the monitor can launch programs
   such as `/fat/bin/sh` or `/fat/bin/deskd`.

The regular ISO and staged physical profiles have different command lines.
Physical laptop trials use the [hardware workflow](ubuntu-device-agent-setup.md),
with a RAM boot image, a separately identified exFAT USB workspace and a retained
Ubuntu recovery path. They do not install SrvrOS over the laptop's internal disk.
Boot diagnostics remain available through serial, `dmesg` and persisted logs;
quiet framebuffer output does not mean diagnostics were discarded.

## Kernel and process model

Physical frames come from the Limine memory map. The heap manages kernel
allocations; processes have their own page tables, ELF mappings, stacks and
file descriptors. Syscalls validate user buffers against mapped access rights.
Private anonymous/file-snapshot mappings exist; demand paging, general shared
file mappings and a production security model do not.

The scheduler preempts ring-3 processes and kernel threads. Wait queues and
bounded workers let eligible tasks sleep and resume. FPU/SSE state is preserved
across supported transitions. The current scheduler and synchronization work is
CPU0-oriented; it is not a claim of full multicore operation. Frame/heap/process
counters and lifetime regressions help detect leaks and stale ownership.

The `int 0x80` ABI and common structures live in `shared/include/`. Version/size
headers permit bounded copy-back for structured results. Changes to those
structures must update both kernel and userspace and preserve truncation tests.
See [executable format](executable-format.md) and [porting](ports.md).

## Filesystem and I/O ownership

The VFS and block registry sit above memory-backed images, AHCI and USB mass
storage. exFAT implements file/directory traversal and mutation, allocation,
flush, mount/unmount and consistency checks. It is not journaled and does not
provide crash-atomic transactions.

Storage operations retain explicit ownership while bounded service windows allow
eligible input and console work. This improves responsiveness without claiming
general parallel filesystem mutation. USB transfer errors have bounded deadlines
and quarantine/diagnostic paths. Tests independently read back persisted content;
a successful write return alone is not the hardware acceptance criterion.

Desktop applications delegate blocking file/image work to separate workers.
They report completion back to their event loops instead of waiting synchronously
inside input handlers. Exit/cancellation must preserve ownership until in-flight
operations can safely complete or be refused.

## Native desktop

`userspace/deskd/` is the current compositor. It owns global menus, dock, launcher,
window frames, focus, arrangement, pointer routing, clipboard, notifications and
presentation. Applications own their content and send frames through the desktop
client protocol and kernel-managed surfaces. The compositor caches accepted
frames and presents damage to the linear framebuffer in software.

`userspace/widgets/` provides shared models/controllers and rendering primitives:
buttons, edits, lists/tables, scrollbars, menus, dialogs and other reusable
controls. App code builds workflows from those controls; global desktop behavior
belongs in the compositor. See the [toolkit guide](../userspace/widgets/README.md)
and [native desktop guide](native-desktop.md).

The native apps are Files, Text, Terminal, Preview, Sketch, Find Files, Calculator,
System, Network Utility, Settings, Clock and Help. Terminal uses a PTY-backed
shell. Preview delegates bounded PNG/JPEG/BMP decoding to an image worker;
Sketch and screenshot saves also use workers. Preferences and documents can be
saved on the persistent USB workspace.

Input queues, client response deadlines and surface generations are bounded.
An interrupted pointer interaction sends cancellation rather than a normal click
release; cancellation must preserve unrelated keyboard packets. Pending input
and control packets get a delivery attempt before response timeouts are applied.
These contracts have host/VM regressions; the machine record distinguishes the
accepted native interactions from cases covered only by synthetic tests.

Window records, GUI queues and surfaces now grow under separate byte budgets;
sixteen inline compositor records are bootstrap storage, not a count ceiling.
Client surfaces remain bounded to 1440×900 logical pixels with software rendering. Explicit 2× output expands the logical pixels without changing
app geometry. GPU discovery is not GPU rendering acceleration. The older
`gui`/`displayd`/`gui2` stack remains for historical regressions and is not the
current desktop entry point; its [archived architecture](history/architecture-before-native-desktop.md#gui)
is retained for reference.

A process connection dispatches explicit, non-reused window identities. Normal
Text owns multiple independent documents in one process; other apps can retain
the single-window wrapper. Text/Files share lazy bounded file-worker pools and
wait for GUI plus nonblocking worker pipes without adding a dispatcher per window.
Accepted file work drains before its owner or transport retires. Dynamic metadata,
pixel caches, process slots and descriptor budgets remain separate constraints.
Protocol v15 adds one-level same-process modal dialogs, cancellable application
Quit and existing-process Text/Preview file routing. Normal desktop exit preserves
owned Save/Discard/Cancel prompts while accepted work drains; emergency exit is
separate. See [application lifecycle](desktop-application-lifecycle.md),
[desktop connections](desktop-connections.md), [window capacity](window-capacity.md)
and the [API evolution plan](desktop-api-evolution.md) for implemented contracts
and remaining work. Host/QEMU acceptance includes 128 small windows and 32 normal
Text documents; physical acceptance remains scoped in the machine record.

## Networking and services

The network stack provides Ethernet/IPv4, ARP, ICMP, DHCP, DNS A lookups, UDP and
a small TCP implementation. e1000 is the standard QEMU path; CDC ECM USB
networking has separate VM coverage. User processes use network file descriptors,
readiness calls and bounded socket tables. `webd` serves static files, while
init scripts and `svscan` provide service startup, logging and restart policies.
This is useful experimental infrastructure, not an internet-facing production
network stack. The [POSIX compatibility audit](posix-compatibility.md) separates
working subsets, semantic gaps and compatibility stubs; full POSIX conformance
and Linux binary compatibility have not been achieved.

## Device and ACPI boundaries

Drivers are selected from observed model/controller/descriptor evidence. The
reference Air's USB topcase is not assumed to be an SPI device, nor are other
Macs automatically covered by the Air's register and codec gates.

Native brightness, HDA audio and temperature/fan sampling have distinct provider
and diagnostic boundaries. Read-only CPU energy telemetry is opt-in, with bounded
Air measurements recorded in the power guide. The ordinary Home battery service
and cached indicator have Air7,2 acceptance; startup remains explicitly opt-in.
This does not accept suspend/resume or calibrated battery-life estimates.

Keep these ACPI uses separate:

- A restricted, isolated resource probe with bounded execution and private
  snapshots; ordinary desktop startup omits it.
- A separate Q35-only full-uACPI VM kernel exercising real SCI, worker ownership,
  EC/SMBus/SBS fixtures, namespace initialization and teardown failures. It does
  not itself accept native firmware regions, GPEs or battery support on a laptop.
- The explicitly selected native battery runtime with captured platform/resource
  gates, cached service reads and owned lifecycle. Its machine-specific evidence
  is recorded in the [battery service contract](battery-service.md) and unit
  records; sibling profiles require their own evidence.

See the [uACPI port](../ports/srvros/uacpi/PORT.md) and
[power telemetry guide](power-telemetry.md) for exact gates and remaining work.

## Source map and validation

| Source | Responsibility |
| --- | --- |
| `kernel/src/main.c` | Bootstrap and selected diagnostic flows |
| `kernel/src/arch/x86_64/` | Traps, syscalls, CPU/platform and interrupt plumbing |
| `kernel/src/memory/` | Frames, heap and virtual memory |
| `kernel/src/process.c`, `kernel/src/scheduler.c` | Process lifetime, scheduling and waits |
| `kernel/src/drivers/` | USB, storage, input and laptop providers |
| `kernel/src/fs/exfat.c`, `kernel/src/vfs.c`, `kernel/src/block.c` | Filesystem and block paths |
| `kernel/src/gui.c`, `userspace/deskd/`, `userspace/widgets/` | Desktop transport, compositor and toolkit |
| `userspace/lib/`, `ports/srvros/` | Userspace support and upstream adapters |
| `tools/` | Host tests, QEMU harnesses and hardware evidence collection |

Host tests exercise actual portable subsystem code and injected failures. VM
harnesses exercise kernel/user transitions and workflows in disposable guests.
Physical tests validate only the matching machine, payload and observed actions.
See [testing](testing.md) and [current status](status.md). The
[earlier detailed architecture](history/architecture-before-native-desktop.md)
contains historical CLI/libc inventories; it is not the current support contract.
