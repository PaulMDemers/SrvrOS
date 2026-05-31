# Node.js Port Manifest

## Upstream

- Project: Node.js
- Version: `v24.16.0` LTS "Krypton"
- Commit: `c7d10158bc31036de6783d66beaaaf551e3167aa`
- Source: `ports/upstream/node`

## Status

This is an exploratory source staging point. Node has not been built for srvros
yet, but the local patch queue now gets upstream configure past OS selection,
generates a Ninja graph, separates V8 host/target generated files, builds
target and host libuv focused archives, and produces V8's host `mksnapshot`
generator plus a reduced static `node` executable under WSL-native probing.

The first configure probe found that upstream Node does not accept
`--dest-os=srvros`. A reduced `--dest-os=linux` cross-configure completes, so
the first real task is to register srvros as a build-system OS and then drive
the failures down into compile/link/runtime compatibility gaps.

`patches/0001-add-srvros-gyp-configure-probe.patch` is the current local probe
patch. Apply it with:

```sh
ports/srvros/node/apply-patches.sh
```

Then run the host-side probe with:

```sh
ports/srvros/node/probe.sh
```

The probe expects MSYS2 packages `python`, `ninja`, `nasm`, and `gcc`. It uses
MSYS GCC intentionally; UCRT/MinGW GCC leaks Windows platform defines into V8
and hides the POSIX-ish surface we actually want to inspect.

For the next serious upstream compile failure, prefer:

```sh
ports/srvros/node/probe-linux.sh
```

from Linux or WSL after applying the patch queue. That path avoids the current
Abseil/Cygwin host rejection.

On this Windows workspace the WSL helper downloads a pinned local Ninja under
`build/wsl-tools` when Ubuntu does not already have `ninja` installed:

```powershell
ports\srvros\node\probe-wsl.ps1
```

For the full V8 compile, prefer the WSL-native helper. It mirrors the workspace
to `~/srvros-node-probe`, skips generated build output, reuses the same patch
queue, and avoids the `/mnt/c` filesystem bridge:

```powershell
ports\srvros\node\probe-wsl-native.ps1
```

The Linux probe defaults to `NINJA_FLAGS=-j2` to keep long V8 compiles from
overwhelming the host. Override it only when the machine has enough spare CPU
and memory for a full Node build.

For narrower iteration, `probe-linux.sh` also accepts `NODE_PROBE_TARGET`.
Useful aliases are `libuv`, `libuv-host`, `v8-base`, `mksnapshot`, and `node`.
The convenience wrappers `probe-libuv-linux.sh` and
`probe-mksnapshot-linux.sh` exercise the two most useful smaller milestones.
The PowerShell WSL helpers expose the same choice as `-ProbeTarget`.

For the first srvros-native link check, run:

```powershell
ports\srvros\node\probe-srvros-toolchain.ps1
```

This exports `build/sysroot/srvros`, then builds a tiny freestanding C/C++
program against `crt0.o`, `app.ld`, and `libsrvros.a`. It does not compile Node
itself yet; it verifies the local ABI package that the Node cross-link will use.

To replay the generated Node final link against that sysroot, run:

```powershell
ports\srvros\node\probe-srvros-link.ps1
```

This expects the WSL-native `node` probe to have already generated and built the
Node object graph. It writes the raw link output and a deduplicated unresolved
symbol list under `build/node-srvros-link-probe`.

To compile real Node translation units with the srvros sysroot, run:

```powershell
ports\srvros\node\probe-srvros-compile.ps1
```

This uses Zig's freestanding C++ frontend, the exported srvros sysroot, and
Zig's bundled libc++ headers. It now enables the minimal
C-locale/filesystem/regex profile needed for libc++ declarations and compiles a
default batch of 147 objects into srvros objects under
`build/node-srvros-compile-probe`. The batch includes the two entry objects,
the first option/error/platform objects, debug/report/process/buffer providers,
environment and startup objects, permissions, filesystem-facing helpers,
stream/TCP/UDP/pipe wrappers, workers, module binding pieces, URL/task/report
helpers, tracing utilities, WASI/SEA/messaging providers, the first V8 API and
cppgc objects, all V8 libplatform objects, V8 heap/object/profiler/builtin
provider slices, merve's CommonJS lexer provider, and dependency replacement
objects from `deps/ada`.

The compile probe accepts `-Objects` for ad hoc comma/space separated object
lists and `-ObjectList` for file-backed batches. Successful replacements are
recorded in `replacements.tsv`.

To build the first focused srvros libc++ implementation archive, run:

```powershell
ports\srvros\node\probe-srvros-libcxx.ps1
```

The archive is written to `build/node-srvros-libcxx-probe` and is consumed by
`probe-srvros-link.ps1` when present. Today it successfully builds 38 libc++
implementation objects: string, algorithm, memory, stdexcept, verbose abort,
hash, new/new-handler/new-helpers, ios, ios-instantiations, ostream, locale,
typeinfo, exception, call-once, system-error, chrono, functional, iostream,
fstream, error-category, random-shuffle, vector, variant, optional, any,
memory-resource, print, strstream, valarray, regex, and the first filesystem
path/directory/operations slice. It is still a probe archive, not a final C++
runtime, but it now covers the stream/filebuf, filesystem, regex, allocator,
polymorphic-memory-resource, and common utility pieces pulled in by Node
reports, module/path logic, debug output, and the task runner. The deliberate
no-threads profile keeps thread/mutex/condition-variable sources out for now,
and `charconv.cpp` is blocked by Zig's packaged libc++ tree missing the
`shared/fp_bits.h` helper.

`probe-srvros-link.ps1` reads that manifest, prefers direct srvros-compiled
entry objects, rebuilds filtered non-thin archives when archive members have
srvros replacements, adds the libc++ probe archive when present, and maps the
freestanding C++ `main(int, char**)` symbol back to the C `crt0.o` entry
contract. The filter now handles `libnode.a`, V8 dependency archives, and
archive members whose paths only match by suffix. The link frontier has moved
on from basic C/POSIX availability to C++ runtime consistency,
libc++/libstdc++ implementation symbols, the remaining host-built V8/Abseil
objects, and the next object-provider batches that must be compiled with the
srvros C++ profile. The current link replay produces
`build/node-srvros-link-probe/node-srvros.elf` with 514 srvros object
replacements, including 512 archive-member replacements across libnode, V8
libplatform, V8 API/heap/object/profiler/builtin/provider slices, generated
Torque objects, merve, ADA, V8 compiler/Maglev/cppgc, and Abseil objects, plus
the 38-object libc++ probe archive. `make node-unresolved-audit` now reports 0
host libstdc++, 0 runtime-shim, 0 libc++, 0 V8-provider, and 0 other unresolved
symbols. The runtime-shim bucket is empty after adding safe srvros definitions
for `std::__throw_*`, `std::thread::hardware_concurrency`, and
`std::_Hash_bytes`; libc++ exposes `std::chrono::steady_clock` through srvros
`clock_gettime(CLOCK_MONOTONIC)`; the compile probe includes no-thread
`std::thread`/condition-variable shims for Abseil probes; Abseil sees srvros as
an mmap-capable POSIX-like platform; and the sysroot now has Linux compatibility
coverage for `dev_t`, `sys/sysmacros.h`, `MREMAP_*`, `MAP_NORESERVE`,
`useconds_t`, and V8 sampler-facing `ucontext.h`/`SA_ONSTACK`. The user linker
script emits `PT_TLS` with `.tdata`/`.tbss`, and the kernel loader now maps a
TLS block for the main thread, allocates per-user-thread TLS copies, restores
the x86_64 `FS.base` MSR during scheduler switches, and chooses a higher user
stack for large static executables that outgrow the original compact stack
window. `make node-runtime-image` now packages the stripped ELF as
`/fat/bin/node` in a small exFAT image, and `tools/node_runtime_smoke.py` boots
QEMU and verifies that `node --version` prints `v24.16.0` and exits with status
0. Process startup is now real enough for Node's C++ constructors, TLS,
stdio metadata, and startup signal/FD probes. The smoke harness can now run
alternate `--program`, `--eval`, script-text, raw argument, and expected-output
probes; script-text probes are injected into a temporary exFAT image before
boot so larger JavaScript tests do not depend on monitor paste length. It also runs
`/fat/bin/cxxprobe`, a standalone C++ user program that verifies global
constructors, heap `new`, and local RAII-style ownership. The image now also
includes `/fat/bin/cxxstlprobe`, which validates 16-byte heap alignment plus
libc++ `std::set`, `std::map`, and `std::vector` operations. Node now runs
small scripts: `node -e "console.log(1+1)"`, pure JS array/string work,
CommonJS `require('node:path')`, script files from `/fat/bin`, synchronous
`node:fs` read/write/stat/readdir, `Dirent` file entries, and requiring a user
module written to `/fat` all complete with status 0. `node -e
"console.log(1+1)"` originally exposed the first upstream Linux libuv loop blockers
after adding pseudo-`epoll_create1`, pseudo-`eventfd`, and best-effort `pipe2`
flag handling. It also gets past the first tracing-agent assertion after a
srvros-only padding guard was added after `node::tracing::Agent::tracing_loop_`;
that guard compensates for the current bridge mixing Linux-built libuv objects
with Node C++ replacements compiled against the smaller srvros/freestanding
`uv_loop_t` view. The current captured boundary has moved into
`V8::Initialize()`: V8 platform workers start, cppgc initializes, and no-access
virtual reservations work, then heap alignment, V8 `RegionAllocator`
construction, and early local logging bridge checks pass. The latest runtime
fixes were pseudo-epoll infinite-wait compatibility, conventional abort status
134, advisory `MAP_NORESERVE` handling, kernel lazy `PROT_NONE` mmap
reservations with `mprotect` commit, libc++ verbose-abort support, a true
16-byte `malloc()` payload alignment contract, bypassing Linux `statx`, and a
srvros-specific synchronous `readdir` path backed by `srv_list()`.
The current captured boundary has moved from V8 startup to event-loop fidelity:
callback-scope draining, loop liveness, and process teardown still use
bring-up shims and need to be hardened before long-lived timers, TCP servers,
and a Node-hosted HTTP demo are reliable. The smoke harness now limits
expected-output matching to the runtime section after `run: entering`; with
regenerated builtin JavaScript, strict QEMU probes now pass for
`process.nextTick()`, Promise microtasks, and `queueMicrotask()`. Preserve that
generated-source step with:

```powershell
ports\srvros\node\regenerate-node-builtins.ps1
```

Timers now use the real Node/libuv path by default. The earlier srvros
JavaScript fallback remains available only when explicitly enabled with
`SRVROS_NODE_TIMER_FALLBACK=1`, which keeps long timeouts from collapsing into
`process.nextTick()` during networking tests. The srvros checkpoint now calls
Node's stored timer callback after libuv marks the timer due, and enters that
callback under `AllowJavascriptExecutionScope`, matching the guard already
needed by TCP callbacks. Strict QEMU smokes now pass for `process.nextTick()`,
Promise microtasks, `queueMicrotask()`, `setTimeout()`,
`setInterval()`/`clearInterval()`, and `setImmediate()`.

The runtime smoke runner now has networking mode for Node probes:
`tools/node_runtime_smoke.py --net` adds the supported QEMU e1000 device, and
`--hostfwd` can forward a host port into the guest while keeping the QEMU
window hidden. With that path, `require('net')` loads, TCP `server.listen()`
binds successfully, and the srvros embed loop now treats referenced non-timer
libuv handles as live work, so a listening Node server no longer exits
immediately. The host-forwarded TCP probe now connects through QEMU, enters the
JavaScript `connection` handler, and verifies response bytes from Node. The key
fixes were a poll-backed epoll shim that preserves libuv's watched fd/data
state and an srvros `AllowJavascriptExecutionScope` around later TCP accept
callbacks so V8 does not silently skip JavaScript during libuv phases.
`tools/node_runtime_smoke.py` also has `--serve-tcp-port` for outbound-client
tests against a one-shot host server. That path now confirms connect requests
keep the srvros Node process alive, and a numeric IPv4 client now reaches the
JavaScript `connect` callback against `172.66.147.243:80`. Hostname clients now
also cross the first line: `dns.lookup('example.com')` returns an IPv4 result
inside JavaScript, and `net.createConnection({ host: 'example.com', port: 80 })`
reaches the JavaScript `connect` callback. Stream read/write callbacks now use
the srvros V8 execution guard as well, so an explicit `socket.write()` outbound
HTTP request receives response bytes from `example.com`, and a host-forwarded
`net.createServer()` can accept a request and reply with `socket.end('node-ok')`.
The current srvros resolver bridge uses synchronous `uv_getaddrinfo()` and
`uv_getnameinfo()` completions plus a request-counter ordering shim for
immediate callbacks; `dns.lookup()` and numeric `dns.lookupService()` now return
through JavaScript. Replacing that with a real async libuv/threadpool path is
still the durability target. A scoped experiment that performed the DNS lookup
synchronously but queued completion through libuv's `wq_async` work-done path
linked and booted, but the JavaScript callback did not fire under Node;
resolver hardening should therefore start by making libuv async/eventfd
work-done dispatch reliable.

The first Node-hosted static-site demo is now packaged as
`/fat/bin/node-http-demo.js`. It uses Node's built-in `http` and `fs` modules to
serve the existing `/fat/www` tree, including `/`, `/hello.html`, and
`/status.txt`, through QEMU host forwarding. The smoke keeps one server process
alive and repeats those routes; the current five-round run verified 15
responses with no retries when using the default 15-second per-request response
window. The `http.ServerResponse` path required one more V8
bring-up guard: under srvros, `Factory::NewStringFromUtf8` now tolerates
null/low byte pointers and suspiciously large external UTF-8 vectors by
returning the empty string, and `NonAsciiStart` avoids zero-length null vector
scanning. The smoke launches the static demo with Node `--jitless` by default
while srvros compiler-tier support matures.
Use `python tools\node_http_demo_smoke.py --skip-build` after building the
runtime image to rerun the three-route demo.

`/fat/bin/tcpprobe` is the native TCP companion for the outbound boundary. It
uses nonblocking `connect()`, `poll(POLLOUT)`, `getsockopt(SO_ERROR)`, `send`,
and `recv`, and it prints srvros TCP state while it waits. The probe reaches
`ESTABLISHED` and receives HTTP response bytes from `172.66.147.243:80`
(`example.com`), so kernel TCP, e1000 polling, and the libc socket wrappers are
good enough for outbound HTTP at the native POSIX layer. Node numeric outbound
TCP and DNS-backed hostname outbound TCP now receive libuv/Node connect
completion, and explicit outbound `socket.write()` plus inbound server
`socket.end()` are verified. The remaining outbound work is shutdown/end
ordering polish and async resolver hardening.

For that investigation, the srvros libc shims now expose environment-gated
traces: `SRVROS_EPOLL_TRACE=1`, `SRVROS_POLL_TRACE=1`, and
`SRVROS_SOCKET_TRACE=1`. These traces are intentionally runtime-only so the
same linked Node image can be used for normal smokes and for fd/readiness
diagnostics.

`node:sqlite` has a verified transitional srvros path. The default runtime
keeps `HAVE_SQLITE=0`, but `pre_execution.js` allows the builtin on srvros and
`lib/sqlite.js` routes to `internal/srvros_sqlite`, a narrow JSON-backed
`DatabaseSync`/`StatementSync` shim. That public require path now passes the
QEMU smoke for create/insert/update/select/close and the Express/JWT demo smoke
asserts that `/health` reports the `node:sqlite` backend. The shim supports
simple positional and named bindings, `WHERE column = value`, `ORDER BY`,
`LIMIT`, `COUNT(*) AS alias`, delete filtering, persistence, and
`lastInsertRowid`. General projected-column aliases such as
`SELECT id AS ident` are intentionally rejected until the runtime-side alias
fault is understood; the count alias path is the supported exception.
`tools/node_sqlite_shim_smoke.py` verifies that a writer process and a later
reader process see the same `/fat` database on one mounted image.
`tools/node_app_suite_smoke.py` adds a broader app-compatibility check for the
stable core surface: synchronous `fs`, timers, `path`, `url`, `querystring`,
`events`, the srvros `crypto` HMAC shim, and the SQLite update/limit path.
`fs/promises` and wider stream plumbing remain active follow-ups because the
current runtime can still fault when that broader async file/stream surface is
exercised.
The native
binding remains at the compile/link bridge stage:
`probe-srvros-compile.ps1` has manual source mappings for
`obj/src/libnode.node_sqlite.o`,
`obj/src/libnode.node_webstorage.o`, and
`obj/deps/sqlite/sqlite.sqlite3.o`, and includes Node's bundled `deps/sqlite`
headers when compiling selected objects. With `HAVE_SQLITE=1` and the bundled
SQLite feature defines, those objects compile against the srvros sysroot and
the static Node link can complete, but enabling the native dispatch/config
profile still page-faults in V8's `v8::Object::DefineOwnProperty` path during
early module setup.

Timer tracing remains available with
`process.env.SRVROS_NODE_TIMER_TRACE = '1'`, and the old fallback can be
enabled with `SRVROS_NODE_TIMER_FALLBACK=1` when comparing callback paths. The
debug pass confirmed that public `setTimeout()` inserted into the expected
timer queue and that libuv fired the native timer; the missing piece was
explicitly allowing JavaScript execution when entering the stored timer
callback from the srvros loop checkpoint.

The compile bridge can now opt into Linux-shaped upstream libuv replacement
objects with `probe-srvros-compile.ps1 -LinuxLibuvLayout`, and the srvros libc
headers have enough Linux/POSIX declarations for the first timer/core/loop
backend objects to compile. Keep those experiments in separate build
directories, then link from a manifest that merges the Linux-layout uv-facing
overrides with the known-good replacement manifest. Replacing only libuv is not
safe because libuv and Node disagree on `uv_loop_t`; replacing libuv plus the
uv-facing Node wrappers now links and boots as an experimental image, but the
runtime boundary has moved past first callback dispatch through the srvros
JavaScript fallback. The next pass should focus on true Node timer binding
state and libuv phase dispatch rather than broad C/POSIX symbol availability.
The audit tool still ranks referrer objects and writes
priority-preserving compile lists for future frontier regressions.

The srvros image also includes `/fat/bin/nodeprobe`, a small local readiness
probe for the libc/POSIX/libuv surface Node needs before the full V8 build is
worth iterating on, including resource/accounting calls such as `getrlimit` and
`getrusage`, Linux-ish discovery calls such as `getauxval`, `sysinfo`, `prctl`,
`madvise`, and scheduler affinity stubs. `/fat/bin/tlsprobe` verifies static
ELF TLS initialization through `PT_TLS`. `/fat/bin/libcprobe` and
`tools/libc_smoke.py` are the faster companion path for the narrower C/POSIX
slice: string helpers, `getline`/`getdelim`, unlocked stdio, formatted output,
`sscanf`, numeric conversion, C-locale helpers, temp files, environment
variables, time formatting, and wide-character classification. `make
libc-audit` verifies that declared libc functions are exported by
`libsrvros.a`. `make node-unresolved-audit` summarizes the latest Node link
frontier without opening the full linker log.

The Windows workspace can enter Ubuntu/WSL at `/mnt/c/Users/Paul/Desktop/srvros`.
The mounted-workspace WSL probe reached late V8/Node compilation. It is slow on
the Windows filesystem, so the WSL-native helper is now the preferred path. The
last concrete build blockers fixed were:

- Ninja duplicate outputs from host and target V8 generated files.
- libuv's Linux/GNU feature branch not being selected for the srvros probe.
- Target and host libuv focused probes now build successfully.
- V8 `v8_libbase` omitting POSIX/Linux platform and stack-trace sources for the
  srvros OS flavor; `mksnapshot` now links and answers `--help`.
- c-ares not selecting a generated config header for srvros.
- QUIC metadata version includes not respecting the no-OpenSSL/no-QUIC reduced
  profile.
- The reduced static `node` WSL-host probe now links and reports platform
  `srvros` and arch `x64`.

## First Target

Build a static CLI-only Node executable without npm, corepack, inspector,
ICU, SQLite, snapshots, or the code cache. That keeps the first milestone aimed
at the runtime core: V8 startup, libuv event loop, filesystem, stdio, timers,
and TCP/UDP.

## Known Gaps

- Node/GYP/V8 OS flavor for `srvros`.
- Remaining C++ runtime and exception/unwind expectations once the binary
  starts executing.
- V8 executable memory support through `mmap`/`mprotect`.
- Fuller pthread stack/signal behavior.
- Real upstream libuv backend or a compatibility bridge broad enough for Node.
- Diagnostic stubs for backtrace/debug paths.
- Static-first policy for native addons until `dlopen` exists.
- More targeted srvros libuv backend work instead of the temporary
  Linux-like probe mapping.
- Real srvros cross-linking instead of the current WSL-host static probe.
- Replacing glibc-only static-link dependencies such as passwd/group/service
  lookup and dynamic-loader paths with srvros libc/POSIX behavior.
- Wiring the linked `node-srvros.elf` into a runnable image/startup test.
- The first real srvros-compiled Node entry objects plus the broad
  libnode/V8/cppgc/Torque/Abseil provider batch now build and link against the
  sysroot. The next object-level work is keeping that replacement set
  reproducible while runtime startup exposes the next semantic gaps.
- Abseil/V8 host-probe behavior: MSYS/Cygwin is useful for discovery but
  Abseil rejects it, so the next serious compile probe should use a Linux host
  or the eventual srvros cross compiler rather than treating MSYS as the final
  build environment.
