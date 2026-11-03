# Browser engine port

## Second checkpoint — November 3, 2026

User authorized explicit SrvrOS platform support, a real threaded C++ runtime,
and advancing no-JIT JavaScriptCore configuration to its next actual dependency.
Main remains `a936baeb`; PR252/19/301/306 heads were freshly fetched and unchanged.
This continues draft PR310. The clean assigned checkout and all three protected
review VMs are retained. Free storage was approximately 16 GiB at the start of this checkpoint.

Own the new SrvrPorts `srvros/webkit` and `srvros/libcxx` recipes, dedicated
configuration/guest probes, and necessary C++ ABI guard/TLS adaptation in
`userspace/lib/src/cxx_runtime.cc`. Actual PR252 diffs do not touch that C++
runtime or pthread implementation. Consume existing pthread/process semantics;
do not change the kernel, pthread library, descriptors, resolver or ownership
limits. Any discovered requirement in those owned subsystems must be published
as a dependency rather than implemented independently. Other platform tracks
retain their namespace/profile/installation scope.

Acceptance: explicit `OS(SRVROS)` without Linux impersonation; actual upstream
libc++ thread/mutex/condition-variable/once code linked and exercised in a matched
disposable guest; thread-local destruction and static initialization correctness;
documented no-exceptions failure at worker exhaustion; executable (not archive)
CMake feature checks exposing the next true Threads/ICU dependency. Do not force
thread discovery success or add missing pthread cancellation/fork-handler stubs.
No full JSC/WebCore build, browsing, JIT, physical installation or merge is in
this checkpoint. Keep experiments below 3 GiB additional storage, two compile
jobs, and one build/VM harness in this checkout. The bounded implementation and guest acceptance are complete; publication
records and the configure result follow below. SrvrPorts commit
`4d47276917a9c8738c920cd21208b04fdcd24d02` is published in
[draft SrvrPorts PR4](https://github.com/PaulMDemers/SrvrPorts/pull/4); the OS
ABI, harnesses and matching gitlink continue in
[draft SrvrOS PR310](https://github.com/PaulMDemers/SrvrOS/pull/310).
Tests ran on the recorded dirty implementation atop `3863241d`; the input
hashes identify the tested source, not a retroactively claimed clean commit.

### Implemented and measured

The SrvrPorts overlay defines `OS(SRVROS)` and generic `OS(UNIX)` without
`OS(LINUX)`. Its driver links real SDK executables for CMake checks. C++ uses
hosted language semantics for ordinary `main` linkage and implicit return zero,
while retaining the freestanding SrvrOS target, explicit SDK headers/startup and
static libraries. This does not establish a complete hosted C++ environment.

The separate threaded libc++ archive uses pinned Zig 0.15.2 upstream thread,
mutex, condition-variable, once and supporting sources. OS changes are confined
to `userspace/lib/src/cxx_runtime.cc`: synchronized static-initialization guards,
per-thread LIFO TLS destructors, bounded DSO-filtered global destructor records,
and main-thread TLS-before-global teardown. Existing pthread operations remain
the backend. No Node threading shims or success-returning error stubs are used.

Matched SDK, kernel/init, libc++ recipe and existing C++ regression builds pass.
The diskless, networkless, single-CPU Q35/TCG guest receipt is
`build/browser-runtime/guest-1550di15/receipt.json`. It records all six passing
cases: existing `cxxprobe` and `cxxstlprobe`; TLS-first registration/teardown;
threaded synchronization, call-once, concurrent local-static construction and
TLS destruction; eight-worker exhaustion with real error 11 and expected abort
status 134; and a successful fresh normal run afterward. The last run proves
worker resources were released after the aborted process. The harness stopped
its own guest. Existing review VMs 61610, 6233 and 85869 remain untouched.

The no-JIT configure passes C/C++ executable validation, explicit platform
identity and a real `std::thread`/mutex link control. CMake's broad libc pthread
check fails on undeclared `pthread_cancel`; its upstream `-pthread` fallback
successfully compiles and links actual `pthread_create`/`pthread_join` against
the SDK, producing `Found Threads: TRUE`. Nothing pre-seeds this result.
Configuration then requires target **ICU >=70.1**, with `ICU_INCLUDE_DIR`,
`ICU_LIBRARY`, `data`, `i18n` and `uc` missing. Host ICU is not substituted.
Final configure receipt: `build/browser-platform/run-7ik21jgi/receipt.json`
(41.050 seconds, pinned archive extraction 478,809,532 bytes). The probe exits
zero for the recognized dependency stop; CMake itself exits one. This is
dependency discovery, not successful configuration or engine execution.
CMake still warns that its built-in platform catalog lacks SrvrOS; the explicit
toolchain and WebKit patch supply this probe's target settings.

The SDK/runtime receipt hashes sources, SDK archive/startup/linker script,
headers and resulting ELF/archive files. Both harnesses reject stale runtime
inputs. Guest kernel/init hashes identify the artifacts actually booted;
Git HEAD is labeled an observation, not an assertion that uncommitted changes
were absent. Raw logs/receipts remain local under `build/browser-runtime/` and
`build/browser-platform/`. Failed exploratory runs remain failed evidence:
monitor echo parsing, same-translation-unit TLS fixture initialization, ordinary
C++ main linkage, and an ICU diagnostic-format mismatch were corrected before
final acceptance. No old receipt is relabeled as a pass.

### Supported slice and owner handoff

This is a real, bounded threaded runtime, not the full C++ standard library or
ABI. Eight workers, 4096-byte ELF TLS templates and 128 global destructor records
remain limits. No exceptions/unwinder, dynamic unloading, recursive static
initialization recovery or initializer-thread-death recovery is supplied.
Concurrent finalizers may execute distinct callbacks concurrently; global LIFO
completion is only established for single-caller finalization. Aborts and forced
termination intentionally skip C++ teardown.

`notify_all_at_thread_exit` and promise/future readiness at thread exit are
excluded. The existing pthread code allocates keys and drains destructors in
ascending key order; libc++ can therefore notify before user C++ TLS destruction.
Supporting these APIs needs an agreed pthread teardown-order contract with the
shared runtime owner. This finding is a dependency handoff, not authorization to
change pthread lifecycle independently. PR252's process/I/O/socket/resolver
changes, PR19 Wi-Fi, PR301 display and PR306/SrvrPorts PR2 SQLite remain owned
interfaces. No competing kernel, pthread, descriptor or resolver implementation
was added. Existing C `atexit` registration is not a general concurrent teardown
facility.

The next browser milestone is a pinned SrvrOS ICU recipe and then the next
no-JIT JSC dependency/compile failure, with runtime requirements handed back to
their owners. No full engine build, browser rendering, network enablement,
physical installation or merge is accepted by this checkpoint.

### Reproduce the second checkpoint

From the matching OS branch and published ports gitlink:

```sh
git submodule update --init ports userspace/widgets userspace/apps
sh tools/macos/build.sh -j2 srvros-sysroot
python3 ports/srvros/libcxx/build.py
sh tools/macos/build.sh -j2 build/kernel/srvros.elf build/userspace/init.elf build/limine-binary/.ready build/userspace/cxxprobe.elf build/userspace/cxxstlprobe.elf
python3 tools/browser_runtime_smoke.py
python3 tools/browser_platform_probe.py
```

The final command requires the pinned archive from the first checkpoint at
`build/browser-probe/wpewebkit-2.54.2.tar.xz` (URL and digest below). The historical
`browser_probe.py --fetch` can acquire it but also runs the old expected-gap
checks; it is not the acceptance command for this updated runtime. CMake/Ninja,
QEMU and xorriso must be installed. Configure subprocesses have 60-second
bounds; extraction is capped at 1 GiB and requires 2 GiB free. The guest uses a
private serial socket and read-only boot ISO with no writable disk or network.
Run only one build/guest harness in this checkout at a time.

Python syntax, focused document links and diff-whitespace checks pass.
Negative controls reject missing/stale runtime artifacts and unsupported driver
target/shared/C++ language modes before compiler or guest invocation. The global `check-docs` gate was
rerun and still fails on the same nine existing missing vendor-source/license
links in intentionally uninitialized nested submodules. That gate remains a
review dependency for a fully initialized integration checkout.

## Historical first checkpoint — November 3, 2026

Target: select an engine from current primary-source requirements and deliver a
bounded, pinned-source configure/compile probe against the real SrvrOS SDK.
This checkpoint does not deliver a browser or accept arbitrary web content.

Base: `a936baeb4a20ff28304a13252bf20345dee13bf1`, freshly fetched main.
Branch: `device/apple-silicon/browser-probe`, in isolated `SrvrOS-browser`.
Own only this document and browser feasibility tooling. Do not edit the
integration roadmap, machine record, kernel or libc.

Inspected live heads: PR252 `badfeb37` owns process/I/O and socket/resolver
lifetime; PR19 `993e920a` owns Wi-Fi; PR301 `4c9fc184` owns native display
readout; PR306 `5028871b` and SrvrPorts PR2 own SQLite. Their implementations
are dependencies, not permission to duplicate shared runtime work. Filesystem,
profile, SrvrApps and SrvrUI contracts retain their independent owners.

Host preflight: ARM64 macOS, 32 GiB physical RAM, approximately 31 GiB free
storage. Three existing review QEMU processes (61610, 6233, 85869) remain live
and untouched. No physical install, host reboot or VM mutation is needed.

Experiment envelope: WPE WebKit 2.54.2 release archive, 49,565,492 bytes;
SHA256 `f3284e19e47fcb5ac8c63c2e1c113dbd65b2423eb8a545f5b345707829004f56`.
At most 2 GiB of source/probe artifacts, two compilation jobs, no full engine
build. Build only a matched SDK and bounded probes; preserve all external
checkouts and images. This envelope produced the evidence below; no engine runtime was built.

Status: first feasibility checkpoint published in [draft PR310](https://github.com/PaulMDemers/SrvrOS/pull/310),
implementation commit `84b25354`. Locally verified for the bounded draft scope. Next implementation gate is explicit platform/threaded-runtime support
and a no-JIT JSC configure reaching its next dependency. Actions stay disabled.

## Engine decision

Recommend **WebKit/WPE as the first feasibility track**, starting with static,
no-JIT JavaScriptCore and then an offline WebCore software-rendering slice.
This is a conditional engineering choice, not a claim that WPE currently runs
on SrvrOS. The advantage is a documented embedder/platform boundary and an
existing C/C++ SDK. An upstreamable SrvrOS port is preferable to a permanently
outdated fork. Stop and reconsider before a full engine build if the graphics
and runtime contracts cannot be made tractable.

| Candidate | Current upstream evidence | Consequence for SrvrOS |
| --- | --- | --- |
| WebKit/WPE 2.54.2 | Release archive and SHA256 are pinned below. `Source/cmake/OptionsCommon.cmake` requires C++23; `OptionsJSCOnly.cmake` requires threads and ICU >=70.1. WPE requires GLib >=2.70, HarfBuzz >=2.7.4 with ICU, FreeType >=2.9, libsoup3, libgcrypt, libxml2, SQLite, image libraries, Epoxy and xkbcommon. | Existing Zig/Clang and C++ work provide a starting point, but the current SDK is not a complete threaded libc++/WebKit platform. A JavaScript shell is a prerequisite experiment, not HTML rendering. |
| Chromium/Blink/V8 | Official Linux build instructions require at least 100 GB free disk, recommend >16 GB RAM, and support Clang/libc++; Linux instructions assume an x86-64 host. GN/depot_tools, large dependency trees, multiprocess IPC and platform sandbox backends are involved. | No full checkout/build on this 31 GiB-free host. A macOS host build would not establish a new SrvrOS target. Existing Node/V8 historical notes do not establish a current Chromium runtime. |
| Servo | Official tree uses Rust 2024, `rust-version = 1.88.0` and currently pins Rust 1.97.1; build docs use rustup, Cargo, Python/mach and native C/C++ dependencies. WebRender/graphics, fonts, SpiderMonkey and OS integration still matter. | Viable alternative to revisit, but requires an SrvrOS Rust standard-library target and runtime validation in addition to the C/C++ dependencies. No such matched SDK is supplied here. Rust alone does not remove VM, threads, IPC or graphics requirements. |

Primary sources checked November 3, 2026:
[WPE releases](https://wpewebkit.org/releases/),
[Chromium Linux requirements](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md),
[Servo build instructions](https://book.servo.org/building/linux.html),
[Servo workspace](https://github.com/servo/servo/blob/main/Cargo.toml), and
[Servo toolchain](https://github.com/servo/servo/blob/main/rust-toolchain.toml).
Moving branch references describe the inspected date; the implemented probe uses
an immutable archive digest, not these moving branches.

WPE 2.54 makes WPEPlatform stable/default and deprecates the legacy libwpe API;
new application work should follow WPEPlatform, not start a new Cog fork.
Its Skia compositor does **not** imply framebuffer-only integration is solved.
The current documented new-API transport is DMA-BUF, with separate web-process
rendering and UI-process presentation. SrvrOS has neither that Linux transport
nor a demonstrated equivalent. Software painting and transporting/presenting the
result are separate dependencies. See the
[2.54 announcement](https://wpewebkit.org/blog/2026-09-16-wpewebkit-2.54.html) and
[graphics architecture](https://docs.webkit.org/Ports/WebKitGTK%20and%20WPE%20WebKit/Graphics.html).
Chromium's [SwiftShader](https://chromium.googlesource.com/chromium/src/+/main/docs/gpu/swiftshader.md)
is similarly not a replacement for an OS port or sandbox; its WebGL fallback
has security caveats. No candidate's guest RAM, frame time or binary size is
measured by this milestone. The 1 GiB review VM configuration is not a proven
browser memory budget.

### Maintenance and licensing

WebKit is mixed BSD/LGPL by file; a static distribution needs a concrete
license inventory and fulfillment plan for applicable source/relink obligations,
including dependencies and fonts. Servo uses MPL-2.0 for its project code, with
separately licensed dependencies. Chromium also carries many third-party
licenses; its top-level license does not cover the whole distributed bundle.
These are engineering obligations to review for the chosen build, not a blanket
license conclusion. Keep upstream notices with fetched source and patches.
See [WebKit licensing](https://docs.webkit.org/Other/Licensing.html),
[Servo license](https://github.com/servo/servo/blob/main/LICENSE), and
[Chromium third-party policy](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md).

A port owner must track releases, security fixes, dependency updates and patch
rebases continuously. WebKit explicitly distinguishes maintained upstream ports
from downstream ports that can lag; see its
[port maintenance guidance](https://docs.webkit.org/Ports/Introduction.html).
This checkpoint does not promise that maintenance capacity or upstream acceptance
already exists. No independently maintained adapter exists yet, so creating
SrvrWeb is premature. Reusable upstream recipes/patches belong in SrvrPorts;
the eventual browser shell belongs in SrvrApps.

## SDK dependency map at the first-checkpoint base

Headers and a successful static link are weaker evidence than runtime semantics.
The table separates source findings from the focused tests below. Pending PRs
are not part of the tested SDK.

| Area | Actual SrvrOS evidence | Required milestone / owner |
| --- | --- | --- |
| Toolchain and C++ | Zig 0.15.2, Clang 20.1.2, freestanding x86_64, static ELF SDK. Existing `USER_STL_CXXFLAGS` are C++20 with a no-threads compatibility setting and Node shims. WebKit needs C++23 and a real threading configuration. | Explicit SrvrOS platform registration, threaded libc++ configuration and required ABI symbols. Publish port patches through SrvrPorts; coordinate SDK changes rather than inventing a parallel libc. |
| Threads, atomics and TLS | Real pthread mutexes/condition variables, ELF TLS and futexes exist. `kernel/src/process.c` caps spawned user threads at 8/process, futex entries at 64 globally and TLS image size at 4096 bytes. Public pthread keys are capped at 128. Ordinary scheduling is CPU0. | Measure engine worker/TLS demand, atomics, parking, termination and exhausted-resource behavior in a matched guest. No SMP or unlimited capacity claim. |
| VM and JIT | Private anonymous/file mappings and page protections exist. `posix_mman.c` rejects MAP_SHARED, strips MAP_NORESERVE and accepts discard advice without performing it. Kernel MAP_FIXED requires an unused range. File mappings are private snapshots. | Test reservation/commit/guard replacement/decommit contracts against JSC allocator expectations. Keep JIT and WebAssembly disabled initially; executable mappings alone do not validate JIT or W^X policy. Kernel/VM corrections need explicit shared ownership. |
| Processes and IPC | Native spawn, pipes, socketpairs and SCM_RIGHTS exist, with four transferred FDs per message. Fork refuses live workers; pseudo-socket/adapter inheritance is restricted. `dlopen`/`dlsym` explicitly fail. | Static initial build, explicit process launcher and audited stream/FD transfer semantics. PR252 owns lifetime and rights/payload corrections; consume its reviewed integration, do not duplicate it. |
| TLS, DNS and certificates | Selected IPv4 TCP/UDP and resolver support exist; no established browser TLS/certificate/trust-store stack. `tlsprobe` means thread-local storage, not TLS networking. Kernel entropy can fall back to timestamp/PID-derived SplitMix64 if hardware/virtio entropy is unavailable. | Stay offline. Before HTTPS, require a vetted TLS provider, certificate/hostname/clock checks, trust-store updates and a cryptographic entropy contract that fails closed. Resolver/socket changes remain PR252-owned. Wi-Fi stays PR19-owned. |
| Unicode and fonts | SrvrUI's bitmap atlas and density support do not provide browser Unicode layout, shaping, bidi or general font selection. WPE's ICU/HarfBuzz/FreeType dependency graph is absent from the matched SDK. | Port pinned text libraries and fonts via SrvrPorts; validate mixed scripts, combining marks, fallback and text metrics before claiming text rendering. |
| Storage | exFAT and libc support bounded regular-file operations. Pending SrvrPorts PR2 is a single-threaded SQLite VFS, one persistent connection, with WAL and ATTACH disabled; clean reopen is not crash durability. | Begin with memory-only offline documents. Browser profile/storage design consumes filesystem/profile contracts; assess engine SQLite concurrency and transaction requirements with PR306/ports owner. |
| Graphics and input | SrvrOS copies pixel rows into owned GUI surfaces; no WPE DMA-BUF/EGL/GBM backend. SrvrUI exposes logical coordinates and 1x/2x backing density, not a browser renderer. | Start with an offscreen software buffer and explicit copy/presentation lifecycle. Consume existing SrvrUI input/density and SrvrOS surface contracts, preserve stride/clipping/budgets, and test resize, input cancellation and scale changes. PR301 display readout is not an engine backend. |
| Sandboxing | Separate address spaces exist; the architecture does not claim production isolation. Linux seccomp/namespaces and browser-specific service restrictions are not supplied by header compatibility. | Offline, trusted fixtures only. A multiprocess or no-JIT prototype is not safe for arbitrary browsing. Network enablement requires independent isolation and hostile-content review. |

Local sources: [supported POSIX profile](posix-supported-profile.md),
[architecture](architecture.md), [ports/VM contracts](ports.md),
[application runtime](desktop-application-runtime.md),
[SrvrUI density contract](../userspace/widgets/docs/render-density.md),
[libc mapping adapter](../userspace/lib/src/posix_mman.c),
[dynamic-loading adapter](../userspace/lib/src/posix_dlfcn.c),
[kernel process implementation](../kernel/src/process.c), and
[kernel entropy implementation](../kernel/src/arch/x86_64/syscall.c).
The pending SQLite contract was inspected directly at
[SrvrPorts 8cbf17b](https://github.com/PaulMDemers/SrvrPorts/blob/8cbf17b68b711ab8cf59ee2e5b98657ec9b8968c/srvros/sqlite/README.md).

## Ordered implementation gates

The bounded platform/threaded-runtime portion of gate 1 is accepted above;
ICU is the next dependency. The broader runtime and browser gates remain open.

1. **SDK/platform recognition and threaded C++ runtime.** Add an explicit SrvrOS
   cross-build description in a SrvrPorts patch series after dependency review.
   Preserve target/host separation; do not define Linux merely to pass checks.
   Require real C++23 atomics, mutex/condition-variable, TLS and thread teardown
   compile/link/guest tests, including exhaustion. The immediate next experiment
   is a pinned `JSCOnly` configure that gets past OS recognition and reports its
   next actual dependency. ICU must be built for SrvrOS, not accidentally found
   in Homebrew.
2. **No-JIT static JavaScriptCore.** Port the minimal ICU/WTF/runtime dependencies;
   use the generic event loop and system allocator where upstream permits.
   Explicitly validate reservation, guard-page and discard behavior, failure
   paths and thread budgets. Run deterministic arithmetic/string/GC fixtures in
   a disposable matched guest, record heap/peak memory and time, and terminate
   cleanly. Disable JIT, WebAssembly, remote inspection and networking. A passing
   shell still does not demonstrate WebCore or a browser.
3. **Offline HTML/CSS/text to software pixels.** Bring in WebCore plus the pinned
   font/shaping/image subset. Define a supported software-buffer platform path;
   the current WPE DMA-BUF default cannot be assumed usable. Render local fixed
   fixtures at a fixed viewport: block/inline layout, CSS colors, line wrapping,
   combining marks, bidi and font fallback. Check images/text geometry and
   memory limits. Record unsupported features explicitly. If the platform API
   cannot transport these buffers without large divergence, revisit the engine
   choice at this gate.
4. **SrvrApps browser shell and lifecycle.** Use SrvrUI controls, logical input
   coordinates and physical-density backing with SrvrOS application/surface
   lifetimes. Validate scroll, focus, keyboard/pointer routing, resize, 1x/2x,
   cancellation, close/reopen and bounded resource retirement using local files.
   No new competing widget, event-loop or kernel process implementation.
5. **Persistence and service processes.** Consume the filesystem/profile contract
   and reviewed PR252 semantics; determine SQLite multi-connection/WAL needs
   rather than assuming the pending CLI VFS is sufficient. Test crash/error
   recovery, storage quotas, IPC data/rights association and worker exit. Keep
   runtime caches separate from portable settings and exclude secrets from test
   fixtures and source control.
6. **Network and hostile-content readiness.** Only after TLS/trust/entropy/DNS
   and real isolation contracts exist, test controlled HTTPS endpoints, bad
   certificates, redirects, cancellation and parser/resource attacks. Define a
   security-update/release owner and maintenance policy. Arbitrary browsing and
   JIT remain separate acceptance decisions, not consequences of a screenshot.

There are no cross-chat messages or competing shared implementations in this
checkpoint. Ownership conflicts should be recorded with exact files and PRs;
unblocked probe work can continue. No new repository, submodule or hosted CI
was created for the feasibility slice.

## Historical first-checkpoint reproduction

From this isolated checkout on the documented macOS host (CMake/Ninja and
Rosetta are also required for the configure and x86_64 host fixture):

```sh
git submodule update --init ports userspace/widgets userspace/apps
sh tools/macos/build.sh -j2 srvros-sysroot
python3 tools/browser_probe.py --fetch
# Later runs reuse the verified archive without network access:
python3 tools/browser_probe.py
```

`--fetch` downloads only the pinned WPE release. The tool checks size and SHA256
before extraction, requires 2 GiB free space, caps extracted regular data at
1 GiB, excludes links/special files from materialization and uses a temporary
source directory removed after the run. It preserves commands, diagnostics,
input/binary hashes and a JSON receipt in unique ignored
`build/browser-probe/run-*` directories. It builds no full engine or disk image,
starts no VM and does not modify upstream or SDK sources. Compiler/configure
subprocess groups have a 60-second deadline. The initial manual source
inspection occupied about 583 MiB; the SDK/toolchain occupied about 401 MiB.
The temporary extraction adds about 457 MiB while the probe is running.

Exit zero means the positive controls and **expected gap signatures** matched.
It does not mean engine configuration succeeded. A changed diagnostic or failed
control exits nonzero for investigation; do not weaken it merely to regain zero.
CMake uses a real `SrvrOS` system name, the freestanding target and SDK-only
library/header searches. Its compiler checks use static archives, so they do not
prove a JavaScriptCore executable link. On macOS, upstream's early Xcode SDK
lookup still appears in the log; it is not the target SDK. No Linux platform
macro, Node shim, fake mutex or success-returning runtime stub is added.

The host memory fixture compiles the actual libc mapping adapter with private
symbol names and an always-failing modeled kernel boundary. It establishes
adapter rejection/flag forwarding and unchanged local bytes after advice;
it does not simulate working kernel mappings or prove guest memory protection.

## Historical first-checkpoint acceptance and limits

SDK build: `sh tools/macos/build.sh -j2 srvros-sysroot` passed on base
`a936baeb4a20ff28304a13252bf20345dee13bf1`, Zig 0.15.2. The source SDK
and pinned first-party submodules were built in this isolated checkout.
Final first-download run `run-n3i5r6nq` took 18.514 seconds, including download,
archive checks, extraction, configure and probes. Extracted regular data was
478,809,532 bytes. The unrelated Skia fontations Cargo.toml symlink was not
materialized and is named in the receipt. These are this host's observations,
not engine build-time or memory benchmarks.

| Probe | Observed result | What it establishes |
| --- | --- | --- |
| Minimal C program | Compile and static x86_64 ELF link pass | SDK startup/archive/linker script can form an executable; it was not run in a guest. |
| mmap/socketpair/pthread mutex calls | Compile and static ELF link pass | Real symbols resolve from the matched SDK, not proof of complete POSIX behavior. |
| C++23 builtin atomic | Compile and static ELF link pass; 32-bit atomic asserted lock-free | Language/basic target-code support, not contended atomics or a C++ runtime qualification. |
| `std::atomic<int>` | Compile passes | This header/operation is usable with the probe configuration; no full STL or thread claim. |
| Upstream `wtf/Platform.h` | Compiles; explicit `OS(UNIX)` assertion fails | The headers are reachable, but freestanding SrvrOS is not recognized as a Unix target. |
| `std::mutex` | Compiler reports `no type named 'mutex'` | Current libc++ configuration lacks the needed threaded surface. No Node mutex shims were included. |
| Upstream `wtf/Threading.h`, system allocator selected | Missing `unicode/utypes.h`; also an `mbstate_t` platform diagnostic | ICU and libc++ platform adaptation are real next prerequisites. Correct header ordering removes the initial experimental include-order error. |
| Static no-JIT `JSCOnly` configure | Fails with `Unknown OS 'SrvrOS'` | Upstream lacks the target definition. ICU discovery and engine compilation were not reached. |
| x86_64 macOS host libc fixture | Passes all three observed-gap checks | MAP_SHARED rejects before any syscall; MADV_DONTNEED preserves bytes; MAP_NORESERVE is stripped before the modeled failing syscall. |

Final probe-script SHA256:
`03b6f0acdcecd4a04c034f3e9523c5ecdb6a7e6165300c43be428f7a9e81c39b`.
The receipt records the uncommitted source state, script/fixture/SDK/header hashes,
full command arrays and diagnostics; publication does not retroactively claim
that a different Git commit was built. ELF hashes are per-run receipts and may
vary with debug paths. Raw evidence and the SDK build log remain in ignored
`build/browser-probe/` storage. The corrected explicit-download path and a
separate cached rerun (`run-us2tia5n`, 15.214 seconds) are verified;
the earlier failed download receipt is kept. A corrupt-archive negative control
exited 2 before any compiler invocation.

`python3 -m py_compile tools/browser_probe.py` and focused browser-document links
pass. `make check-docs` was run via the macOS wrapper and reports nine missing
existing third-party license/source links because nested upstream submodules
(zlib, Lua, cJSON, inih, linenoise, libuv and Node) were deliberately not fetched.
No missing link points into this new document; the global docs gate is **not a
pass** in this bounded checkout. It should be rerun in a fully initialized
integration checkout during review. No full OS build, VM boot, engine link,
HTML/CSS rendering, guest network browsing or physical-device acceptance is claimed.

The first checkpoint is complete for draft review: a conditional engine choice,
repeatable positive controls and exact early failures, tested libc semantic gaps,
source/resource bounds and owned next milestones. Modern browsing remains
unimplemented. Existing review VMs/disks and the integration checkout are
unchanged; no new VM is left running.
