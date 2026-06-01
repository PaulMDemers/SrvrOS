# Node.js Port Notes

srvros now carries Node.js as a pinned upstream source checkout:

```text
ports/upstream/node -> v24.16.0 LTS "Krypton" (c7d10158)
```

The first probe used the current LTS release instead of the newest odd/current
line, because LTS gives the port a less volatile target while libc, POSIX, V8,
and libuv support are still moving.

## Configure Probe

Node's configure script does not currently recognize srvros as a destination
OS. A direct configure attempt fails before generating a build graph:

```text
python configure.py --dest-os=srvros --dest-cpu=x64 --cross-compiling ...
configure.py: error: argument --dest-os: invalid choice: 'srvros'
```

A deliberately reduced Linux stand-in configure does complete:

```text
python configure.py --dest-os=linux --dest-cpu=x64 --cross-compiling \
  --fully-static --without-npm --without-corepack --without-ssl \
  --without-sqlite --without-inspector --without-intl \
  --without-node-snapshot --without-node-code-cache --v8-lite-mode --ninja
```

That means the first porting wall is build-system OS registration, not an
immediate configure-time dependency failure. The stand-in profile is only a
probe; srvros should get its own OS flavor so Linux-only assumptions do not
leak into the runtime.

The first local patch queue lives at:

```text
ports/srvros/node/patches/0001-add-srvros-gyp-configure-probe.patch
```

It adds `srvros` to configure/GYP, maps the first POSIX-ish compiler/linker
branches, fixes MSYS Python/Ninja path handling for the probe, and disables a
few Windows/shared-library assumptions that prevented V8 from reaching normal
source compilation.

After applying that patch, `--dest-os=srvros` configure completes. A
non-cross-compiling host probe reaches V8 compilation and stops when Abseil
rejects the MSYS/Cygwin host environment.

The WSL/Linux probe now gets past configure, Ninja graph generation, V8
inspector/Torque generated-file collisions, libuv's first GNU feature visibility
issue, V8's host `mksnapshot` link, bundled c-ares compilation, and the reduced
static `node` link. The build is slow from the Windows filesystem, but it is no
longer blocked at configure, early graph generation, libuv archive generation,
the first major V8 host generator, or the first static Node executable.

Focused WSL-native probes now confirm that both target and host upstream libuv
archives build with the current srvros patch queue:

```powershell
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget libuv
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget libuv-host
```

The `mksnapshot` milestone now builds with:

```powershell
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget mksnapshot
```

The last blocker there was V8's `v8_libbase` source selection: srvros now maps
onto the POSIX/Linux base sources for `platform-linux.cc` and
`stack_trace_posix.cc`, which provide the host OS and debug helpers needed by
the generator link.

The reduced static Node milestone now builds with:

```powershell
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget node
```

The resulting WSL-host probe binary reports `v24.16.0`, `process.platform` as
`srvros`, and `process.arch` as `x64`. This is not yet a srvros-native
executable, but it proves the patched upstream tree can produce a full static
CLI Node binary shape for the srvros OS flavor. The last two blockers were:

- c-ares needed srvros to select the bundled Linux `ares_config.h`.
- Node metadata needed QUIC version includes and assignments guarded by
  `HAVE_OPENSSL && HAVE_QUIC` for the no-OpenSSL/no-QUIC reduced profile.

The first srvros-native link bridge is now available through:

```powershell
ports\srvros\node\probe-srvros-toolchain.ps1
```

That probe exports `build/sysroot/srvros` with the userspace headers, shared
syscall headers, `crt0.o`, `app.ld`, and `libsrvros.a`, then compiles and links
a tiny C plus C++ executable with the same freestanding Zig/LLD path used by
normal srvros apps. This gives Node and other C/C++ ports a stable local
toolchain contract before the full Node object graph is pointed at the srvros
libc/POSIX surface.

The next bridge probe is:

```powershell
ports\srvros\node\probe-srvros-link.ps1
```

It queries the generated WSL-native Node Ninja graph, translates the final
`node` object/archive inputs to Windows paths, and replays the final link with
srvros `crt0.o`, `app.ld`, and `libsrvros.a`. The expected result today is a
linked static probe ELF at `build/node-srvros-link-probe/node-srvros.elf`; if a
future regression reintroduces unresolved symbols, they are written to
`build/node-srvros-link-probe/unresolved-symbols.txt`.

The first compile-side bridge is now:

```powershell
ports\srvros\node\probe-srvros-compile.ps1
```

It exports `build/sysroot/srvros`, uses the local patched Node checkout for
headers and sources, and compiles selected Node translation units with Zig C++
for `x86_64-freestanding-none`. The default batch now compiles 147 objects:
the two entry objects, a broad libnode/provider slice, V8 libplatform, the
first V8 API/heap/object/profiler/builtin/provider slices, merve, and ADA with
the srvros C++ profile. The initial seed of that list was:

```text
obj/src/node.node_main.o
obj/src/node.node_snapshot_stub.o
obj/src/libnode.node_options.o
obj/src/libnode.node_errors.o
obj/src/libnode.node_metadata.o
obj/src/libnode.node_config_file.o
obj/src/libnode.node_types.o
obj/src/libnode.node_debug.o
obj/src/libnode.node_task_queue.o
obj/src/libnode.node_platform.o
obj/src/libnode.debug_utils.o
obj/src/libnode.util.o
obj/src/api/libnode.callback.o
obj/src/libnode.node_report.o
obj/src/tracing/libnode.agent.o
obj/src/libnode.node_process_events.o
obj/src/libnode.node_process_methods.o
obj/src/libnode.node_buffer.o
obj/src/api/libnode.hooks.o
obj/src/api/libnode.exceptions.o
obj/src/api/libnode.encoding.o
obj/src/libnode.async_context_frame.o
obj/src/libnode.env.o
obj/src/libnode.node_credentials.o
obj/src/permission/libnode.permission.o
obj/src/libnode.node_dotenv.o
obj/src/libnode.json_utils.o
obj/src/libnode.heap_utils.o
obj/src/libnode.node.o
obj/src/libnode.module_wrap.o
obj/src/libnode.node_worker.o
obj/src/libnode.node_main_instance.o
obj/src/permission/libnode.fs_permission.o
obj/src/libnode.tcp_wrap.o
obj/src/libnode.udp_wrap.o
obj/src/libnode.pipe_wrap.o
obj/src/libnode.stream_base.o
obj/src/libnode.stream_wrap.o
obj/src/libnode.compile_cache.o
obj/src/api/libnode.environment.o
obj/src/libnode.node_binding.o
obj/src/api/libnode.async_resource.o
obj/src/libnode.async_wrap.o
obj/src/libnode.node_file.o
obj/src/libnode.path.o
obj/src/libnode.node_report_utils.o
obj/src/libnode.node_snapshotable.o
obj/src/libnode.node_modules.o
obj/src/libnode.node_contextify.o
obj/src/libnode.node_dir.o
obj/src/libnode.node_api.o
obj/src/libnode.node_process_object.o
obj/src/libnode.node_sea.o
obj/src/libnode.node_task_runner.o
obj/src/tracing/libnode.node_trace_writer.o
obj/src/tracing/libnode.node_trace_buffer.o
obj/src/tracing/libnode.trace_event.o
obj/src/tracing/libnode.traced_value.o
obj/src/libnode.node_url.o
obj/src/libnode.node_perf.o
obj/src/libnode.histogram.o
obj/src/libnode.node_messaging.o
obj/src/dataqueue/libnode.queue.o
obj/src/api/libnode.embed_helpers.o
obj/src/libnode.node_report_module.o
obj/src/libnode.node_util.o
obj/src/libnode.node_builtins.o
obj/src/libnode.node_sea_bin.o
obj/src/libnode.signal_wrap.o
obj/src/libnode.node_wasi.o
obj/deps/ada/ada.ada.o
```

The compile probe writes `build/node-srvros-compile-probe/replacements.tsv` so
the link probe can consume replacement objects without duplicating object-name
knowledge. Use `-Objects` for a comma/space separated list or `-ObjectList` for
a text file when expanding the batch.

The first libc++ implementation bridge is:

```powershell
ports\srvros\node\probe-srvros-libcxx.ps1
```

It compiles a focused subset of Zig's bundled libc++ sources against the srvros
sysroot and writes `build/node-srvros-libcxx-probe/libsrvros-libcxx-probe.a`.
`probe-srvros-link.ps1` automatically includes that archive when present. The
current subset builds 38 implementation objects: string, algorithm, memory,
stdexcept, verbose abort, hash, new/new-handler/new-helpers, ios,
ios-instantiations, ostream, locale, typeinfo, exception, call-once,
system-error, chrono, functional, iostream, fstream, error-category,
random-shuffle, vector, variant, optional, any, memory-resource, print,
strstream, valarray, regex, and the first filesystem path/directory/operations
support. It is still a probe
archive, but it now covers the stream/filebuf, filesystem, regex, allocator,
polymorphic-memory-resource, and common utility pieces pulled in by Node
reports, module/path logic, debug output, and the task runner. The deliberate
no-threads profile still excludes the libc++ thread/mutex/condition-variable
sources, and Zig's packaged libc++ tree is missing the `shared/fp_bits.h`
helper needed by `charconv.cpp`, so those remain explicit runtime frontiers.

`probe-srvros-link.ps1` now reads that manifest. Direct final-link objects are
replaced in place; archive members are handled by rebuilding filtered copies of
the affected archives with replaced members removed, then adding the srvros
objects ahead of the archives. This now covers `libnode.a` and dependency
archives such as `deps/ada/libada.a`. Because freestanding C++ mangles
`main(int, char**)`, the link probe also maps `main` to `_Z4mainiPPc` for this
bridge.

The link frontier has moved past the entry-object proof, archive substitution
mechanics, the first libc++ archive slice, and most of the easy C/POSIX/Linux
compatibility aliases exposed by host-built objects. srvros libc now carries
the Node-probe-facing pieces for `*64` file aliases, C23/fortified glibc
aliases, scheduler and pthread naming/affinity shims, root uid/gid helpers,
service/name lookup, terminal raw-mode helpers, wide-character whitespace and
conversion helpers, extra math functions, mmap/resource aliases, conservative
epoll/eventfd/inotify/fork/ifaddrs stubs, and `dladdr`.

The May 23, 2026 replay now links a static srvros-native Node probe ELF:
`build/node-srvros-link-probe/node-srvros.elf`. The link uses 514 srvros object
replacements, including 512 archive-member replacements across libnode, V8
libplatform, V8 API/heap/object/profiler/builtin/provider slices, generated
Torque objects, merve, ADA, V8 compiler/Maglev/cppgc, and Abseil objects, plus
the 38-object libc++ probe archive. The unresolved-symbol frontier is currently
empty: `make node-unresolved-audit` reports 0 host libstdc++, 0 runtime-shim,
0 libc++, 0 V8-provider, and 0 other unresolved symbols. The user linker script
now emits a `PT_TLS` program header with `.tdata`/`.tbss` sections so the ELF
can represent Node/V8 thread-local storage. The kernel loader now consumes that
header, maps a TLS template for the initial thread, gives spawned user threads
their own TLS copies, and preserves `FS.base` across preemptive scheduler
switches. `/tlsprobe` and `/fat/bin/tlsprobe` provide the small local runtime
check for this path.
`tools/node_unresolved_audit.py` now buckets that frontier, ranks referrer
objects, writes priority-preserving object lists for the next compile batch,
and can infer ambiguous V8 source paths and generated providers such as
`flags.cc`, `types.cc`, `json-parser.cc`, and Torque
`objects-printer.o`, and it now treats a successful link with no
`unresolved-symbols.txt` as a zero-unresolved result. After adding the safe
runtime shims for `std::__throw_*`, `std::thread::hardware_concurrency`, and
`std::_Hash_bytes`, enabling libc++ monotonic-clock support on top of srvros
`clock_gettime`, advertising mmap to Abseil through the srvros probe platform
shim, and adding Linux compatibility headers for `dev_t`, `sys/sysmacros.h`,
`MREMAP_*`, `MAP_NORESERVE`, `useconds_t`, `ucontext.h`, and `SA_ONSTACK`, the
remaining work has moved from symbol availability to executable semantics.
The first runtime boundary is now crossed: `make node-runtime-image` strips the
linked ELF into a small exFAT image as `/fat/bin/node`, and
`tools/node_runtime_smoke.py` boots QEMU, runs `node --version`, observes
`v24.16.0`, and verifies that the process exits cleanly. Getting there required
runtime ELF TLS, C++ global constructor/finalizer dispatch from `crt0`,
stdio-compatible `fstat`/`fcntl`, and tolerant default/ignored signal
registration for the signal setup Node performs during startup.
`tools/node_runtime_smoke.py` now also has `--program`, `--eval`,
`--script-text`, `--node-arg`, and `--expect` modes for probing the next
frontier. Script-text probes are injected into a temporary exFAT image before
boot so longer JavaScript tests do not depend on typing large commands through
the monitor. The first `node -e "console.log(1+1)"` run exposed and retired the
initial libuv loop startup blockers by adding a conservative
pseudo-`epoll_create1` surface, pseudo-`eventfd`, and best-effort `pipe2` flag
handling. It then exposed a C++ object-layout mismatch: the bridge still links
upstream Linux-built libuv objects, whose `uv_loop_init()` clears the larger
Linux `uv_loop_t` layout, while `src/tracing/agent.cc` is compiled with the
srvros/freestanding `uv.h` view. A srvros-only padding guard after
`Agent::tracing_loop_` now keeps libuv from zeroing the following C++ fields and
retires the null `TracingController` assertion.

The next runtime pass moved through V8 platform worker startup, cppgc
initialization, and the first segmented-table virtual reservations. That
required making the pseudo `epoll_wait()` compatible with libuv's
`timeout == -1` expectation, treating `abort()` as exit 134, accepting and
stripping advisory `MAP_NORESERVE`, and changing kernel `PROT_NONE` mmap from
eager physical page allocation to virtual reservation with lazy `mprotect`
commit. A follow-up pass fixed the userspace heap alignment contract so normal
`malloc()`/`operator new` payloads are 16-byte aligned, added the libc++
`__libcpp_verbose_abort` hook, and introduced `/fat/bin/cxxstlprobe` to exercise
`std::set`, `std::map`, `std::vector`, and heap alignment under the same libc++
header profile used by the Node bridge. A focused srvros replacement for V8's
`region-allocator.cc` is currently compiled at `-O0` to avoid an aligned SSE
store emitted for a member that V8 places at only 8-byte alignment.

The current runtime milestone is past V8 initialization. `node --version`,
`node -e "console.log(1+1)"`, pure JavaScript array/string work, CommonJS
`require('node:path')`, script files from `/fat/bin`, synchronous `node:fs`
read/write/stat/readdir, `Dirent` file entries, and requiring a user module
written to `/fat` now complete with status 0 in QEMU. The srvros Node bridge
currently bypasses Linux `statx` and uses a srvros-specific synchronous
`readdir` path backed by `srv_list()` while the general libc `DIR`/libuv scandir
surface matures.

The next hard boundary is event-loop fidelity rather than basic execution:
the port still carries pragmatic srvros shims around callback-scope draining,
loop liveness, and process teardown. The smoke harness now searches expected
markers only after `run: entering`, so queued-work probes cannot pass by
matching text echoed while the monitor typed the command. Regenerating Node's
builtin JavaScript after the srvros startup hooks moved top-level
`process.nextTick()`, Promise microtasks, and `queueMicrotask()` to passing
strict QEMU smoke tests. The reproducible path for that generated source is:

```powershell
ports\srvros\node\regenerate-node-builtins.ps1
```

Timers now use the real Node/libuv timer path by default. The earlier srvros
JavaScript fallback is still present for diagnostics, but it is opt-in through
`SRVROS_NODE_TIMER_FALLBACK=1` so long delays no longer collapse into
`process.nextTick()`. The durable fix was to let the srvros checkpoint call
Node's stored timer callback directly after a libuv timer marks itself due, and
to enter that callback under `AllowJavascriptExecutionScope`, matching the V8
guard already needed by TCP accept/connect callbacks. Clean headless QEMU
smokes now pass for `process.nextTick()`, Promise microtasks,
`queueMicrotask()`, `setTimeout()`, `setInterval()`/`clearInterval()`, and
`setImmediate()`.

The Node runtime smoke harness can now opt into the same explicit QEMU e1000
device used by the networking smokes with `--net` and `--hostfwd`. Without
that flag QEMU exposes an unsupported default NIC, so `net.createServer()` sees
the srvros stack as idle and `listen()` reports a misleading `EADDRINUSE` from
the current libc fallback. With `--net`, `require('net')` loads, TCP
`server.listen()` reaches its JavaScript callback, referenced TCP handles now
keep the srvros Node embed loop alive instead of falling through to process
exit, and host-forwarded TCP connects reach JavaScript `connection` handlers.
The smoke harness can actively probe the forwarded host port and verify bytes
returned by the Node process; the current minimal server test accepts a client
and replies with `node-ok`. This required a real poll-backed epoll shim for
libuv readiness and an srvros `AllowJavascriptExecutionScope` around the later
TCP accept callback path so V8 actually enters JavaScript during libuv phases.
The harness also has a one-shot host TCP server mode for outbound-client
testing. That probe now keeps the Node process alive while a connect request is
pending, and a numeric IPv4 outbound client now reaches the JavaScript
`connect` callback against `172.66.147.243:80`. DNS-backed hostnames now cross
the first Node boundary too: `dns.lookup('example.com')` returns an IPv4
address in JavaScript, and `net.createConnection({ host: 'example.com',
port: 80 })` reaches the JavaScript `connect` callback. Stream callbacks now
enter JavaScript with the same srvros V8 execution guard used by timers and TCP
connect/accept, so an explicit `socket.write()` HTTP request receives response
data from `example.com`, and the host-forwarded `net.createServer()` smoke can
reply with `socket.end('node-ok')`. The srvros bridge currently runs
`uv_getaddrinfo()` synchronously and allows immediate libuv request callbacks by
counting the request before dispatch; `uv_getnameinfo()` now has the same
srvros direct-completion bridge, which lets `dns.lookupService('127.0.0.1',
80)` return through JavaScript. A first-class async threadpool/libuv backend
remains the durability follow-up. A probe that performed DNS work synchronously
but queued completion through libuv's `wq_async` work-done path linked cleanly
but never delivered the JavaScript callback under Node, so the next resolver
hardening step is making async/eventfd work-done dispatch observable before
moving DNS off the direct completion path.
Node's built-in `http` module now reaches the first hosted-site milestone.
`/fat/bin/node-http-demo.js` is packaged into the generated exFAT image and
serves the existing static `/fat/www` tree with `http.createServer()`,
`fs.readFileSync()`, status codes, `Content-Type`, and `Content-Length`.
The host-forwarded smoke now keeps one QEMU boot and one Node server process
alive while it repeats `/`, `/hello.html`, and `/status.txt`; a five-round run
verified 15 responses with no retries when using the default 15-second
per-request response window.
Getting `http.ServerResponse` through response assembly exposed a V8 bring-up
edge where `NewStringFromUtf8()` could receive a null byte pointer; the srvros
V8 profile now guards null/low pointers and suspiciously large external UTF-8
vectors, treating them as empty strings instead of faulting while the producer
side is still under investigation. The demo smoke launches Node with
`--jitless` by default so the hosted-site milestone does not depend on V8
compiler tiers that are not yet part of the srvros runtime contract.
The repeatable route smoke is:

```powershell
python tools\node_http_demo_smoke.py --skip-build
```

The next Node application smoke target lives at
`ports/node/express-jwt-sqlite-demo`. It installs real `express` and
`jsonwebtoken` dependencies on the host side, bundles a srvros-friendly runtime
server into `/fat/bin/express-demo.js`, and exercises an async API with:
`GET /health`, `POST /token`, `POST /users`, `GET /users`, and `GET /secure`
with a bearer token. This app now runs through Node's real `http.createServer()`
and repeated JSON `http.ServerResponse` writes after the srvros V8 UTF-8 safety
guard was fixed to inspect `Vector::size()` instead of asserting through
`Vector::length()` while handling a suspicious vector. The runtime bundle still
uses a small dependency-light router instead of Express proper. It now bundles
and exercises `jsonwebtoken` with HS256 through the srvros `crypto` shim, while
full OpenSSL, npm/native-addon loading, and `node:sqlite` remain future work.
The database adapter is Promise-based and persists JSON rows to
`/fat/express-demo.sqlite`; it is shaped so the storage layer can switch to
`node:sqlite` once that builtin is enabled.
`require('crypto')` now loads a deliberately narrow srvros provider when Node is
built without OpenSSL. The provider covers SHA-256 hashing, HS256 HMAC,
`randomBytes`, `randomFill`, `timingSafeEqual`, and a minimal symmetric
`KeyObject`/`createSecretKey` shape. This is enough for bundled
`jsonwebtoken` HS256 issue/verify paths. Unsupported asymmetric crypto,
ciphers, TLS, and WebCrypto APIs still throw explicit shim errors until the
full OpenSSL-backed provider is brought over.
The repeatable app smoke is:

```powershell
npm --prefix ports\node\express-jwt-sqlite-demo run build
python tools\node_express_demo_smoke.py --skip-build --skip-app-build
```

The first public `node:sqlite` path is now usable on srvros through a
transitional JavaScript shim. The stable runtime still keeps `HAVE_SQLITE=0`,
but srvros allows `node:sqlite` at pre-execution time and `lib/sqlite.js` routes
to `internal/srvros_sqlite` instead of `internalBinding('sqlite')`. The shim
implements the narrow synchronous `DatabaseSync`/`StatementSync` surface needed
by the demo: create table, delete, insert, update, select, persistence, and
close. The shim now also covers simple `WHERE column = ?` or named-parameter
filters, `ORDER BY`, `LIMIT`, `COUNT(*) AS alias`, positional and named
bindings, filtered deletes, and `lastInsertRowid`.
`tools/node_sqlite_shim_smoke.py` boots one exFAT image, runs a writer Node
process, then runs a second reader process against the same disk to verify
persistence across process restart. `tools/node_app_suite_smoke.py` is the
broader application smoke: it runs one script across stable core modules
(`fs` synchronous file I/O, timers, `path`, `url`, `querystring`, `events`, and
the srvros `crypto` HMAC shim), one script across the srvros `fs/promises`
shim, one script across file streams and directory iteration, one script across
polling-backed watch APIs, one bundled package-compatibility script, and a
second script across SQLite `UPDATE`/`LIMIT`/named and positional binding
behavior.
The `fs/promises` path is transitional but useful: on srvros,
`lib/fs/promises.js` routes to a synchronous-`fs`-backed Promise shim covering
basic path operations, read/write, mkdir/readdir/stat, rm/rmdir, rename/unlink,
and a small `FileHandle` for common package I/O shapes. `fs.createReadStream()`,
`fs.createWriteStream()`, and `FileHandle` read/write streams also route
through a srvros file-stream shim layered under Node's generic stream classes.
The app suite now verifies file stream reads, writes, `Readable.from()`, and
`pipeline()` file copy behavior without entering the native async FS request
path. `fs.opendir()`, `fs.opendirSync()`, `fs.promises.opendir()`, and
`require('fs/promises').opendir()` route through a stat-backed srvros directory
shim with callback reads, sync reads, async iteration, and `Dirent` file/type
checks. The same shim also fills `readdirSync({ withFileTypes: true })` when the
native srvros binding returns plain names. Recursive `mkdir()` and `mkdirSync()`
now use a srvros JS parent-creation path when the native binding cannot create
multiple missing levels. `fs.watchFile()`, `fs.unwatchFile()`, `fs.watch()`, and
`fs.promises.watch()` route through `internal/srvros_fs_watch`, a polling bridge
that gives packages EventEmitter-style and async-iterator watch behavior without
entering libuv's native FS watcher path. The package smoke bundles real
`accepts`, `cookie`, `mime-types`, and `qs` modules with esbuild and verifies
them on srvros.

Current intentional boundaries are documented by the smoke suite. General
`SELECT column AS alias` projection is rejected by the SQLite shim for now; the
count-specific `COUNT(*) AS alias` form remains supported. The native
FSReqPromise/libuv request path remains under investigation; srvros uses the JS
filesystem shims above for package-facing coverage in the meantime.
The srvros compile probe can also resolve and build Node's native
`src/node_sqlite.cc`, `src/node_webstorage.cc`, and bundled
`deps/sqlite/sqlite3.c` with `HAVE_SQLITE=1`, and the full static link can
succeed when those objects are included. Native runtime enablement remains
blocked: rebuilding Node's builtin/metadata/option dispatch objects with
`HAVE_SQLITE=1` causes an early V8 page fault while defining the sqlite module
object (`v8::Object::DefineOwnProperty` through `MemoryChunk::Metadata`).

`/fat/bin/tcpprobe` is now the native companion for that boundary. It exercises
nonblocking `connect()`, `poll(POLLOUT)`, `SO_ERROR`, send, and receive while
printing srvros TCP state. The probe confirms that outbound TCP to
`example.com:80` can reach `ESTABLISHED` and receive an HTTP response, so the
remaining Node outbound work is hardening shutdown/end ordering and async
resolver durability rather than the kernel TCP state machine. The kernel `poll`
syscall now wakes internally in short chunks for infinite waits so network
progress is still driven without returning spurious timeouts to epoll/libuv
callers.
The libc compatibility layer also has runtime diagnostics for this boundary:
`SRVROS_EPOLL_TRACE=1`, `SRVROS_POLL_TRACE=1`, and `SRVROS_SOCKET_TRACE=1`
trace epoll watches/results, POSIX-to-kernel fd mapping, and socket
connect/SO_ERROR state without rebuilding the Node image.

The timer bridge is still diagnosable without rebuilding Node:
`process.env.SRVROS_NODE_TIMER_TRACE = '1'` traces JS timer queue insertion,
and `SRVROS_NODE_TIMER_FALLBACK=1` opts into the old next-tick fallback when a
minimal bootstrap comparison is useful. The important lesson from the trace
work was that timer storage was correct; the missing piece was V8 callback
entry from a later libuv turn.

A focused upstream-libuv replacement experiment can now compile the Linux
backend's timer/core/loop objects under the srvros sysroot after adding the
small missing Linux/POSIX header declarations. That experiment is intentionally
opt-in through `probe-srvros-compile.ps1 -LinuxLibuvLayout` and should stay in
separate build directories. Replacing only libuv objects makes libuv's
`uv_loop_t` layout disagree with Node objects that allocate `uv_loop_t`,
causing page faults inside the Linux epoll/io_uring path. Replacing libuv plus
the uv-facing Node wrappers now links and boots after merging those overrides
with the known-good replacement manifest, but the observable runtime boundary
is unchanged: `process.nextTick()` passes, `setTimeout()` exits before firing,
and `setImmediate()` stays alive without dispatching the callback. The next
pass should focus on Node's timer binding state and libuv phase dispatch, or
introduce a first-class srvros libuv backend with a stable public layout.

`/fat/bin/cxxprobe` verifies that srvros user programs run global constructors,
heap `new`, and local RAII-style ownership before the Node-specific libc++
bridge is involved. `/fat/bin/cxxstlprobe` covers the first STL container and
alignment assumptions needed by the V8 startup path.

Early srvros-link runs captured 200 unique unresolved symbols before the
configured linker error limit. That entire unresolved list has been retired by
rebuilding the relevant Node, V8, cppgc, Torque, Abseil, merve, ADA, libc++, and
runtime-support objects under the srvros profile. True platform decisions still
deferred for a real port include executable memory policy for V8, child process
semantics beyond `posix_spawn`, a first-class srvros libuv backend, and dynamic
loading/native addon policy.

srvros now ships the first minimal no-exception C++ runtime/ABI slice in
`libsrvros.a`: malloc-backed `operator new/delete`, aligned and nothrow
overloads, `std::nothrow`, `__cxa_atexit`, `__cxa_thread_atexit`,
`__cxa_guard_*`, pure/deleted virtual traps, `__stack_chk_fail`, `__assert_fail`,
`__cxa_demangle` as an unsupported stub, `__libc_single_threaded`, and unsigned
128-bit division/mod compiler helpers. After that slice, the intended first
runtime bucket is gone from the Node frontier; the remaining list is led by
host-glibc fortified/C23/`*64` aliases, math and wide-character helpers, and
libuv/Linux backend calls.

The sysroot also picked up the first headers and implementations needed by this
compile probe and later libuv/Node passes: minimal `<wchar.h>`, `<pwd.h>`,
`<grp.h>`, `<semaphore.h>`, `<inttypes.h>`, `<strings.h>`, IPv6 socket structs,
`sockaddr_storage`, `pthread_rwlock_*`, root passwd/group lookup, C-locale
handles, ASCII wide-char conversion helpers, simple unnamed semaphore
operations, locale-aware numeric conversion wrappers, `is*_*_l` wrappers,
`asprintf`/`vasprintf`, `isblank`, `strcasecmp`/`strncasecmp`, BSD string
aliases, integer conversion helpers, and a small `strftime` fallback.
`/fat/bin/libcprobe` and `tools/libc_smoke.py` now keep the fast C/POSIX
readiness loop separate from the larger ports smoke by covering the newly added
string helpers, line-oriented and unlocked stdio, formatting/scanning, numeric
conversion, C-locale helpers, temp files, environment variables, time
formatting, and wide-character classification. `make libc-audit` compares
installed libc header declarations against `libsrvros.a` exports before the
Node probes are run. `make node-unresolved-audit` summarizes the latest
srvros Node link replay by bucket so the next work item is visible without
reading the full linker log.

There are now two probe runners:

- `ports/srvros/node/probe.sh`: MSYS-side discovery probe. This verifies the
  patch still reaches V8 compilation, but it is expected to stop at Abseil's
  Cygwin rejection.
- `ports/srvros/node/probe-linux.sh`: Linux/WSL-oriented probe using the same
  reduced Node profile. This is the better path for the next upstream compile
  failure because it avoids MSYS/Cygwin platform distortion. It defaults to
  `NINJA_FLAGS=-j2` so V8 does not overwhelm the host while probing. Override
  `NODE_PROBE_TARGET` to build a smaller target such as `libuv`, `libuv-host`,
  `v8-base`, `mksnapshot`, or `node`.
- `ports/srvros/node/probe-libuv-linux.sh`: focused Linux/WSL probe for the
  target-side upstream libuv archive.
- `ports/srvros/node/probe-mksnapshot-linux.sh`: focused Linux/WSL probe for
  V8's host `mksnapshot` executable, the first major V8 runtime generator.
- `ports/srvros/node/probe-wsl.ps1`: Windows helper that downloads a pinned
  local Linux Ninja into `build/wsl-tools` if needed, then launches the Linux
  probe inside WSL. Pass `-ProbeTarget libuv`, `-ProbeTarget mksnapshot`, or
  another `NODE_PROBE_TARGET` value for focused runs.
- `ports/srvros/node/probe-wsl-native.ps1`: Windows helper that mirrors the
  workspace into `~/srvros-node-probe` and runs the Linux probe from WSL's
  native filesystem. Use this for long V8 builds. It supports the same
  `-ProbeTarget` parameter.

## Initial Minimal Profile

The first runnable target should be a small, static CLI Node:

- `--dest-cpu=x64`
- `--dest-os=srvros`
- `--cross-compiling`
- `--fully-static`
- `--without-npm`
- `--without-corepack`
- `--without-ssl`
- `--without-sqlite`
- `--without-inspector`
- `--without-intl`
- `--without-node-snapshot`
- `--without-node-code-cache`
- `--v8-lite-mode`
- `--ninja`

This avoids npm, corepack, inspector, ICU, SQLite, and snapshot generation until
the base executable, event loop, filesystem, console, timers, and TCP path are
stable.

## Early API Surface Node Uses

The upstream tree makes the expected platform demands for a real Node port:

- V8 executable memory: anonymous and file-backed `mmap`, `mprotect`,
  executable mappings, and `MAP_FIXED`-style replacement behavior.
- Threads and TLS: `pthread_create`, joins, mutexes, condition variables, TLS
  keys, stack inspection, and signal masking around runtime threads.
- libuv: fd readiness, timers, async work, child process plumbing, TCP/UDP,
  pipes, TTY, filesystem requests, signals, and platform info.
- Process/runtime: `getpid`, cwd/chdir, environment, argv, exit status,
  signal handling, and spawn/stdio inheritance.
- Filesystem: `stat`, `lstat`, `fstat`, `realpath`, directory scanning,
  `readlink`/symlink behavior eventually, temp files, rename/unlink/rmdir, and
  robust open flags.
- Networking: nonblocking sockets, `getsockname`/`getpeername`, socket
  options, DNS, TCP connect/listen/accept/read/write/close, and UDP send/recv.
- Dynamic loading and diagnostics: `dlopen`/`dlsym` for native addons and
  `execinfo.h`/`backtrace` for debug paths. These can be stubbed or disabled
  for the first static CLI milestone.

srvros now also ships `/fat/bin/nodeprobe`, a small userspace readiness probe
that exercises the local pieces Node is likely to lean on before V8 itself is
portable: `clock_gettime`, `getrandom`, anonymous/file-backed `mmap`,
`mprotect`, `msync`, `mkostemp`, `fcntl(F_DUPFD_CLOEXEC)`, `writev`,
`realpath`, pthread attrs/TLS/once/condition variables, `pthread_getattr_np`,
`sysconf`, `getrlimit`, `getrusage`, `sysinfo`, `getauxval`, CPU affinity
stubs, `madvise`, `prctl`, basic `syscall` dispatch, `socketpair`, numeric
`getaddrinfo`, libuv version linkage, `execinfo` stubs, and static-first
`dlfcn` stubs.

The WSL probe host is available in the current workspace. Ubuntu did not have
`ninja` installed and `sudo` required an interactive password, so the helper
uses a pinned local Ninja instead of modifying the WSL distro:

```powershell
ports\srvros\node\probe-wsl.ps1
```

For long compile passes, prefer:

```powershell
ports\srvros\node\probe-wsl-native.ps1
```

For example, the fastest upstream libuv check is:

```powershell
ports\srvros\node\probe-wsl-native.ps1 -ProbeTarget libuv
```

If Ninja already exists in WSL, the direct path is still:

```sh
ports/srvros/node/apply-patches.sh
ports/srvros/node/probe-linux.sh
```

## Next Porting Steps

1. Compile the next libnode/V8 provider slice with the srvros C++ profile so
   replaced objects and their dependencies agree on libc++ ABI and namespace.
2. Expand `probe-srvros-libcxx.ps1` into a deliberate libc++/libc++abi archive
   plan, starting with locale C wrappers or a reduced no-localization profile.
3. Decide whether to keep pruning iostream/filesystem/regex-heavy diagnostic
   Node code from the first CLI milestone, or to support those libc++ pieces
   early.
4. Keep C/POSIX additions tied to concrete unresolved symbols from the link
   probe rather than growing Linux compatibility blindly.
5. Replace the temporary libuv Linux-like srvros probe mapping with a real
   srvros backend or a narrower compatibility shim.
6. Map the first `srvros` build profile near the POSIX/Linux/OpenHarmony
   branches while auditing every Linux-specific syscall assumption.
7. Decide whether the first milestone links against the existing srvros libuv
   adapter or starts replacing it with an upstream libuv srvros backend.
8. Add a `nodeprobe` build target that compiles only the platform probe layer
   before attempting the full V8 and Node executable.
9. Keep upstream clean: carry srvros-specific patches or generated build glue
   outside `ports/upstream/node` until a patch queue format is chosen.
