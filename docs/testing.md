# Testing srvros

srvros tests are QEMU boot smoke tests. Each harness starts a fresh QEMU
instance, connects to the serial console, drives monitor or shell commands, and
fails if expected markers are missing or a fatal kernel exception appears.
The QEMU-based harnesses run with bounded guest memory, user-mode networking,
and disposable copies of the generated exFAT image. They should not touch host
disks outside the repository build outputs and temporary test directories.

## Build

From MSYS2 UCRT64:

```sh
cd /c/Users/Paul/Desktop/srvros
make -j4
```

## Release Verification

Use the UCRT64 QEMU path if it is not already first in `PATH`:

```sh
python3 tools/cli_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/configure_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/shell_edit_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dir_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/process_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/process_pressure.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/thread_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dhcp_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dns_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/httpget_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/udp_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/netabi_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/sysabi_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/uefi_usb_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
make libc-audit
python3 tools/libc_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
make node-unresolved-audit
python3 tools/node_runtime_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/node_http_demo_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64 --skip-build
python3 tools/node_express_demo_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64 --skip-build
python3 tools/ports_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/uv_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/lua_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/posixutils_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/service_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/web_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/service_soak.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 4
python3 tools/net_soak.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 3
python3 tools/tcp_pressure.py --qemu /ucrt64/bin/qemu-system-x86_64 --connections 44
python3 tools/fs_stress.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 1
python3 tools/fsck_corrupt.py --qemu /ucrt64/bin/qemu-system-x86_64
```

Optional GUI smoke:

```sh
python3 tools/gui_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
```

## What The Harnesses Cover

- `cli_smoke.py`: shell startup, PATH lookup, env/export/which, `$VAR` and `$?`
  expansion, POSIX-style parameter defaults/assignment/alternates/length/trims,
  `env NAME=value command`, child envp inheritance, unquoted `*`/`?` globbing, `test`/`[`,
  `&&`/`||`, core CLI tools, stdin/stdout/stderr redirection, multi-stage
  pipeline fd wiring through `cat | grep | tap`, pipeline output
  redirection/append, per-segment pipeline redirection including left-side
  stderr capture, `2>&1` into a pipe, and stdin override on a later segment,
  zero-byte redirect creation, stdin-aware text tools, source scripts plus
  non-interactive `sh script` and `sh -c`,
  command substitution, `$((expr))` arithmetic expansion, positional parameters,
  `$!`, shell functions/`return`,
  unmatched quote/block diagnostics, `set -e`, `read`, `alias`,
  `type`, `unset`, bare assignments, quoted assignment command substitution,
  `cd -` and directory validation,
  canonical relative paths using `.`/`..` against inherited `PWD`,
  `if`/`then`/`else`/`fi`, `sleep`/`date`/`touch`/`sync`/
  `basename`/`dirname`, `tail`, `tee`, `uname`, `hostname`, `uptime`,
  `for` loops, `/fat/etc/profile`, `PS1`, `find`, `du`, `df`, `sort`, `uniq`,
  `cut`, `xargs`, `seq`, `realpath`, `id`, `whoami`, `readlink -f`, `cmp`,
  `yes | head`, `install -D`, `diff -q/-u`, `tar -c/-t/-x` plus
  `tar --create/--list/--extract --file`,
  `gzip`/`gunzip` file and tarball round trips, simple unified `patch`,
  a small `make` install flow with `--file`, `--dry-run`, and `--always-make`,
  `sed`, `dd if=/dev/zero` generated files, default `TMPDIR`, `mktemp`, `mkdir -p`, recursive
  `cp`/`rm`, recursive copy destination creation, larger binary `cp` with
  `cmp -s` verification, copy/remove, clustered `rm -fRr`, multi-source
  `cp`/`mv` into directories, native file rename and directory destinations through `mv`,
  `/fat/share/help` topic files, `help -l`, `help <topic>`, `man <topic>`,
  `apropos <word>`, generated `/fat/share/examples`, login
  `/fat/etc/profile.d` snippets, `more --plain`, `-h`/`--help`
  usage output across the core CLI tools, `--` option termination for common
  file/text utilities,
  `grep -i/-n/-v/-c/-q` plus `--regexp`/`--ignore-case`/`--quiet`,
  regex matching, `--fixed-strings`, only-match output, and file listing modes,
  `wc -l/-c`, `head -1`/`tail -1`,
  `find -type`/`-print`, `ls -a/-la/-d/-1` and long option aliases,
  `sed -n`/`-e`/`p`/`d` plus `--quiet`/`--expression`, regex addresses,
  and regex substitutions with `&`/capture replacement expansion
  with simple addresses, `expr` arithmetic/string expressions,
  `printf`, `tr`, `while` loops, `case` pattern matching, compound-command
  tail chaining, command-local environment assignments, comments, script line
  continuations, simple here-docs, brace grouping, quoted empty and escaped-space
  arguments, `shift`, `break`/`continue`,
  `command -v`/`command -V` and alias bypass, `fg`/`bg`,
  `test -a`/`-o`/`!`/`-nt`/`-ef`, `test -s/-r/-w/-x`,
  `xargs -n`/`-r`/`--max-args`, long-option `cp`/`rm`/`mv`/`mkdir`/`install`/
  `tee`/`head`/`tail`/`wc`/`ln`, Ctrl-D/EOF shell exit,
  `tap` file splitting,
  foreground/background `fpdemo` userspace SSE checks, and the `posixdemo`
  compatibility-layer smoke app, including pathname `AF_UNIX` stream socket
  bind/listen/connect/accept transfer coverage and port-readiness checks for
  `limits.h`, `lstat`, `realpath`, `scandir`, and `alphasort`. The CLI harness
  also runs monitor `fsck /fat` after the shell exits to catch filesystem
  consistency regressions from the mutation path.
- `configure_smoke.py`: a compact configure/build-script probe harness covering
  long-option aliases for directory creation, install modes/directories, grep,
  sed, head/tail/wc, xargs batching, tee append, find, recursive copy/remove,
  tar create/list/extract, make dry-run/always-make, the unsupported symbolic
  link path, shell `set -e` same-line abort behavior with configure-friendly
  `&&`/`||`/`if`/`while` exemptions, forbidden-output checks, and a final
  monitor `fsck /fat`.
- `shell_edit_smoke.py`: interactive raw-mode shell editing over serial,
  including TAB completion, longest-common-prefix completion fill,
  Ctrl-A/Ctrl-E cursor movement, Ctrl-U/Ctrl-W kill operations, Ctrl-Y yank,
  mid-line insert, escape-sequence arrow navigation, and preserving an
  in-progress draft while browsing history. It also covers the `history`
  builtin, `HISTFILE`/`HISTSIZE`, explicit history save, script-path/line
  diagnostics for shell errors, and a final monitor `fsck /fat`.
- `dir_smoke.py`: nested directory creation, nested file write/read, file
  rename, non-empty `rmdir` rejection, empty directory removal, directory rename,
  and `fsck`.
- `process_smoke.py`: background process launch, process listing, exit status,
  `wait`, `jobs -l`, `%+` job references, foreground/background pipeline job
  control, `kill -0` probes, `%job` process-group kill, and Ctrl-C interruption
  of CPU-bound processes and pipelines with `status 130`.
- `process_pressure.py`: repeated foreground execs, multi-stage pipelines, many
  concurrent background sleeps, `jobs -l`, `wait`, and a final pipeline pass. It
  fails on process-table, scheduler-thread, pipe, or pipeline-spawn exhaustion
  markers, and it verifies that bare `wait` drains the test-created background
  sleeps from the process table.
- `thread_smoke.py`: launches `/fat/bin/threadstress`, which stresses explicit
  user-thread yields, pthread mutexes, condition variables, `pthread_once`,
  TLS keys, stack and mutex/condition attributes, timed condition waits,
  signal wakeups during futex-backed timed condition waits, repeated create/join
  slot reuse, detached-thread reaping, recursive/error-checking mutexes, shared
  heap allocation, shared regular-file fd writes, and recursive stdio stream
  locks under QEMU preemption.
- `dhcp_smoke.py`: e1000 path, DHCP address acquisition, starting `webd`, host
  HTTP request, and file update served by the web server.
- `dns_smoke.py`: DHCP DNS configuration, `net` status, DNS A-record
  resolution, `/fat/bin/host`, and clean resolver failure for a non-resolving
  name.
- `httpget_smoke.py`: DHCP, DNS-backed `getaddrinfo`, outbound TCP
  `connect`, HTTP request/response flow through `/fat/bin/httpget`, and clean
  process exit.
- `udp_smoke.py`: DHCP, userspace UDP socket open/send/receive, `poll`
  readiness, DNS-over-UDP response parsing through `/fat/bin/udpdns`, and
  bound local UDP echo through `/fat/bin/udpecho`.
- `netabi_smoke.py`: launches `/fat/bin/netabi`, which calls the raw network
  list/status/ARP syscalls with smaller versioned structs and verifies the
  kernel does not copy past the caller-declared size.
- `tcpstress_smoke.py`: launches `/fat/bin/tcpstress` behind QEMU host
  forwarding and drives three host clients through listener `dup`/close,
  accepted-connection `dup`/close, socket `fstat` metadata, `accept4`,
  name queries, reply writes, `shutdown`, and EOF handling.
- `tcp_ready_smoke.py`: launches `/fat/bin/tcpstress ready` and verifies
  listener readiness does not fire before an inbound client, repeated
  readiness stays visible until `accept4`, accepted streams report writable
  readiness, and readable readiness repeats once payload arrives.
- `sysabi_smoke.py`: launches `/fat/bin/sysabi`, which calls raw core
  structured syscalls (`stat`, `statfs`, process list, console/gfx info,
  graphics blit, GUI receive, and GUI surface create/blit/copy/destroy) with
  smaller versioned structs and canary checks.
- `uefi_usb_smoke.py`: boots `build/srvros-usb.img` through OVMF from a
  temporary copy, confirms the GPT/FAT32 Limine path reaches the kernel,
  verifies ACPI MCFG/PCI ECAM discovery, exercises QEMU xHCI command
  completion with No-op and Enable Slot, verifies USB HID connected root-port
  reset, confirms the USB keyboard and mouse are addressed/configured as HID
  boot devices, checks `dmesg`, and confirms that the GPT exFAT data partition
  mounts as `/fat`. The optional `--usb-type-text` plus `--usb-type-expect`
  hook sends QMP keyboard events through QEMU's USB keyboard path and can assert
  that the typed command reached the monitor while HID interrupt report counters
  advanced. The optional `--usb-mouse-move dx,dy` hook sends a QEMU mouse move
  through the USB mouse path and checks that mouse report counters advanced.
  `--usb-pointer tablet` swaps the pointer device to QEMU's absolute
  `usb-tablet` and sends QMP absolute pointer events instead of HMP relative
  moves.
  The optional `--usb-hub` topology puts both HID devices behind a QEMU USB hub
  and verifies hub detection, routed downstream HID enumeration, typed keyboard
  input, and pointer reports. `--diag-command hwdiag` runs the real-hardware
  diagnostic bundle instead of the shorter `bootinfo` command.
- `libc_smoke.py`: launches `/fat/bin/libcprobe` and `/fat/bin/nodeprobe` to
  keep the focused libc/POSIX readiness slice fast and visible. It covers
  string helpers (`mempcpy`, `stpncpy`, `strndup`, `strerror_r`), line-oriented
  stdio (`getline`, `getdelim`, unlocked stdio wrappers), formatted output,
  `sscanf`, numeric conversion, C-locale helpers, temp files, process-local
  environment variables, UTC time formatting, and wide-character
  classification/formatting. `make libc-audit` complements it by comparing
  libc header declarations against `libsrvros.a` exports.
- `node_unresolved_audit.py`: buckets the current Node srvros link replay from
  `build/node-srvros-link-probe/unresolved-symbols.txt` into host libstdc++,
  small runtime shims, libc++, V8 provider, and other symbols. It also ranks
  referrer objects, writes priority-preserving compile lists, and infers common
  V8 provider objects including generated Torque sources. It is a report tool
  rather than a QEMU harness.
- `node_runtime_smoke.py`: boots QEMU with the stripped Node runtime exFAT image
  produced by `make node-runtime-image`, runs `/fat/bin/node --version`, and
  treats `v24.16.0` plus a clean process exit as the current executable Node
  milestone. It also supports alternate `--program` probes such as
  `/fat/bin/cxxprobe` and `/fat/bin/cxxstlprobe`, `--eval`, `--script-text`,
  repeated `--node-arg`, repeated `--expect`, and `--allow-boundary`.
  `--script-text` now injects `/fat/bin/node-smoke.js` into a temporary runtime
  image before boot, which keeps larger Node filesystem and module-loading
  probes out of the interactive monitor input path. QEMU defaults to
  `--display none` and uses hidden Windows process startup flags so repeated
  smoke runs do not open visible emulator windows. Expected-output matching is
  scoped to the captured runtime text after `run: entering`, avoiding false
  positives from the monitor echoing a typed command. Current Node runtime
  probes also cover real `setTimeout()` dispatch, numeric IPv4 outbound TCP
  connect completion through `--net`, `dns.lookup('example.com')`, and
  hostname-backed `net.createConnection({ host: 'example.com' })` connect
  completion. Stream smoke coverage now includes explicit outbound
  `socket.write()` receiving HTTP response data from `example.com`, plus a
  host-forwarded `net.createServer()` replying with `socket.end('node-ok')`.
  The generated runtime image also includes `/fat/bin/node-http-demo.js`, a
  Node `http.createServer()` static file demo for `/fat/www`; the QEMU
  host-forward smoke keeps one server alive and repeats `/`, `/hello.html`,
  and `/status.txt`. Run the full route set with
  `python tools/node_http_demo_smoke.py --skip-build` after rebuilding the Node
  runtime image; use `--rounds 4` for the current short repeated-route pass.
  The smoke uses Node `--jitless` by default while srvros V8
  compiler-tier support matures. `node_express_demo_smoke.py` builds and boots
  the `ports/node/express-jwt-sqlite-demo` app, then verifies the async API
  path for health, POST JSON `jsonwebtoken` HS256 token generation, POST JSON local
  DB-backed user creation, user listing, and bearer-token validation over QEMU
  host forwarding. It accepts `--rounds` to repeat that route set against one
  guest server; the milestone closeout used 4 rounds. The app installs real Express and `jsonwebtoken` host
  dependencies, while the srvros runtime bundle uses Node's `http.createServer()`
  with a small dependency-light router and the srvros `crypto` shim for HMAC
  until full OpenSSL and npm/native-addon loading are available. Public
  `node:sqlite` is available through the srvros JavaScript shim; this smoke
  asserts `/health` reports `db: "node:sqlite"`. SQLite-native Node objects can
  compile/link through the srvros bridge, but the default runtime keeps
  `HAVE_SQLITE=0` because the native sqlite-enabled dispatch profile currently
  faults in V8 while defining the module object.
  `node_sqlite_shim_smoke.py` is the focused storage smoke for the transitional
  shim; it runs two Node processes against the same exFAT image and verifies
  named bindings, `WHERE`, `ORDER BY`, `COUNT(*) AS alias`, delete filtering,
  and persistence across process restart. `node_app_suite_smoke.py` is the
  broader application smoke for stable Node app behavior: synchronous `fs`,
  timers, `path`, `url`, `querystring`, `events`, the srvros `crypto` HMAC shim,
  srvros `fs/promises` read/write/mkdir/readdir/stat/FileHandle behavior, and
  file stream read/write plus `pipeline()` copy behavior. It also covers
  `fs.opendir`, `fs.opendirSync`, `fs.promises.opendir`,
  `require('fs/promises').opendir`, async directory iteration, and
  `readdirSync({ withFileTypes: true })` Dirent checks. It also verifies
  recursive `mkdirSync`, polling-backed `fs.watchFile`/`fs.unwatchFile`,
  `fs.watch`, and `fs.promises.watch`, plus a bundled package app using
  `accepts`, `cookie`, `mime-types`, and `qs`. SQLite coverage includes
  `UPDATE`, `LIMIT`, named bindings, positional bindings, counts, and filtered
  deletes. General projected-column aliases remain a known follow-up rather
  than passing coverage.
  The current DNS path is a srvros bring-up bridge; `dns.lookup()` and numeric
  `dns.lookupService()` are covered, while async resolver durability remains a
  libuv/threadpool follow-up.
- `/fat/bin/tlsprobe`: small manual runtime probe for static ELF TLS. It uses
  `__thread` storage, verifies the kernel-loaded `PT_TLS` template is writable
  through the thread pointer, and is useful before trying larger Node/V8-linked
  images.
- TCP socket coverage is split across `httpget_smoke.py` for outbound
  DNS/connect/send/recv, `web_smoke.py` and `dhcp_smoke.py` for inbound
  listener/accept/read/write/close, and `ports_smoke.py` for socket option,
  name-query, shutdown compatibility checks, `tcpstress_smoke.py` for
  descriptor lifetime and close-order pressure, and `tcp_ready_smoke.py` for
  readiness edge semantics. `web_smoke.py` also requests a
  multi-kilobyte static file to exercise segmented TCP writes and ACK-retired
  transmit history under bounded send backpressure, peer receive-window limits,
  dynamic receive-window advertisements, zero-window persist support, and
  guest-side closed-port TCP RST behavior through QEMU host forwarding.
  `httpget_smoke.py` covers outbound DNS/connect/send/recv with the newer
  socket error propagation in place. `/fat/bin/netcheck` also verifies
  `TCP_NODELAY`, nonblocking TCP connect completion through `poll`, and
  `getsockopt(SO_ERROR)` clearing before a short HTTP request, then uses
  TCP `MSG_PEEK` to confirm stream data can be inspected before normal reads.
- `uv_smoke.py`: shell launch of `/fat/bin/uvdemo` for the srvros `uv.h`
  compatibility shim, covering timer/file operations, mkdir/rename/rmdir,
  `uv_async_t`, pthread-backed `uv_queue_work`, pipe-backed `uv_poll_t`
  readability, plus a host-forwarded TCP listener that accepts two clients,
  applies accepted-stream `TCP_NODELAY`/keepalive options, reads requests,
  writes responses, and closes the listener without dropping accepted streams.
  It also runs a guest-outbound TCP client against a host
  service to verify nonblocking connect completion, deferred write callbacks,
  write-queue byte accounting, queued `uv_shutdown`, and response reads.
- `libuv_smoke.py`: shell launch of `/fat/bin/libuvdemo`, the upstream libuv
  staging harness. It validates the currently mapped adapter subset before
  deeper upstream backend replacement: timer dispatch, filesystem requests
  including stat/lstat/fstat/access/realpath/scandir, sync/truncate/sendfile/
  time updates with stat-visible metadata, `uv_fs_poll` change detection, and
  queued fs callbacks, platform
  helpers for cwd/chdir, env/environ, title, home/tmp paths, passwd/group,
  uname, uptime, resource, CPU/interface info, exact kernel-reported exe path,
  pid/ppid, hrtime, memory, and syscall-backed random, prepare/check/idle loop phases,
  async notification, reusable-pool queued work, `uv_cancel` for queued work/fs
  requests, pipe-backed fd polling including multi-handle readiness,
  TCP option helpers, handle ref/unref/walk/fileno helpers,
  thread/synchronization wrappers, queued `uv_getaddrinfo` callbacks, TTY
  handle/window-size/write helpers, and SIGINT/SIGTERM self-signal callback
  delivery through `uv_run`.
  It also verifies `uv_pipe`, `uv_pipe_t`, `uv_socketpair`, and `uv_spawn` by
  writing through a libuv pipe stream, binding/connecting a pathname
  `uv_pipe_t`, sending a pending pipe handle with `uv_write2` and accepting it
  through `uv_accept`, queueing multiple pending pipe handles, repeating handle
  transfer across a spawned child process over a `UV_CREATE_PIPE` IPC channel,
  transferring a TCP listener handle with `uv_write2` and confirming
  `UV_TCP` pending-handle typing after the sender closes its original
  listener fd, checking socketpair polling/handle classification, exercising
  socketpair `sendmsg`/`recvmsg` scatter/gather
  transfers and `SCM_RIGHTS` fd passing, feeding a child `cat` process over stdin, reading child
  stdout through a libuv pipe stream, launching a cwd-scoped child `pwd`,
  exercising inherited-fd stdin, inherited-stream stdin/stdout/stderr, short
  process-only exit loops, and a duplex child stdio pipe. The process section
  also covers
  pre-side-effect spawn validation, failed-spawn loop cleanup, unsupported
  process flags, bad stdio source combinations, missing executables, and bad
  cwd.
- `ports_smoke.py`: shell launch of `/fat/bin/zlibdemo`, `/fat/bin/jsondemo`,
  `/fat/bin/inidemo`, `/fat/bin/linedemo`, `/fat/bin/sqlitedemo`,
  `/fat/bin/nodeprobe`, `/fat/bin/ttydemo`, `/fat/bin/posixdemo`, and
  `/fat/bin/ed`; zlib
  compress/decompress, cJSON parse/print/roundtrip, inih string/file parsing,
  linenoise history save/load coverage, SQLite create/insert/query/reopen on
  exFAT through the srvros VFS, termios raw-mode/restore/window-size/duplicated
  TTY fd/`ENOTTY` checks, process-group/session libc wrappers, foreground TTY
  process-group queries, and libc/POSIX file checks including `fstat`,
  `dup`, shared regular-file offsets, writable-fd dup ownership, high-fd
  `select`, expanded `poll` entry capacity, `statvfs`, advisory
  `fcntl` byte-range locks with a
  spawned `/fat/bin/lockprobe` conflict check, `pipe`,
  expanded file/pipe fd capacity,
  nonblocking `fcntl`/`O_NONBLOCK`, `F_GETFD`/`F_SETFD` `FD_CLOEXEC`,
  `F_DUPFD`/`F_DUPFD_CLOEXEC`, `open(O_CREAT)` mode and `O_EXCL` behavior,
  `mkostemp`,
  `access`, `isatty`, `fsync`, `truncate`/`ftruncate`, `poll`/`select`
  readiness and hangup behavior,
  `w+`/`r+`/`a+` stdio update streams,
  anonymous and file-backed private `mmap`/`munmap`, `mprotect`/`PROT_NONE`,
  `msync`,
  `O_RDWR`, seek, malloc-on-`sbrk`, raw `sbrk`, `qsort`, `bsearch`,
  integer and floating conversion helpers, random numbers, process-local
  environment variables, `mempcpy`, `stpncpy`, `strndup`, `strerror_r`,
  `getline`, `pread`/`pwrite`, `uname`, `getopt`,
  `readv`/`writev`/`preadv`/`pwritev`, `sendmsg`/`recvmsg` with
  local-socketpair `SCM_RIGHTS`,
  libc `regex.h` compile/execute/error behavior including alternation,
  captures, and bounded repeats,
  scriptable `ed` line editing, regex address/substitution, and writeback
  verified with `cmp`,
  pthread create/join plus mutex/condition/once/TLS compatibility, `nanosleep`, `sysconf`,
  formatted-output width/precision/flag handling, stream buffering/`fflush`,
  EOF/error state, `scanf`/`sscanf` width, scanset, suppressed-assignment,
  character, `%n`, and EOF/match-failure behavior, `system()` shell execution,
  `popen`/`pclose`, standard-fd and non-stdio spawn `dup2`/`open`/`close`
  actions, dynamically grown non-stdio spawn action lists,
  `POSIX_SPAWN_SETPGROUP`, reset-id and signal-mask/default attr storage,
  libc `sigaction`, kernel-backed signal mask inheritance, pending-signal,
  targeted `sigwait`, blocked `SIGCHLD` child-exit notification behavior, and
  cooperative caught `SIGCHLD`/`SIGUSR1` handler dispatch, `sigsuspend`,
  `poll`/`waitpid`/sleep/pipe-read `EINTR`, `SA_RESTART` pipe-read retry,
  Unix-domain accept interruption, and burst child exit reaping,
  process-replacing `execve`,
  inherited fd and close-on-exec checks, exFAT
  binary file write/read/unlink, and
  post-run `fsck`.
- `lua_smoke.py`: shell launch of `/fat/bin/lua`, script loading from exFAT,
  integer arithmetic, formatted output through the Lua base library, pure-Lua
  `require`, Lua file IO, and post-run `fsck`.
- `posixutils_smoke.py`: shell launch of the shared POSIX utility applet under
  its installed names: `ln`, `sync`, external `test`/`[`, `cksum`, `sum`,
  `comm`, `paste`, `join`, `split`, `od`, `hexdump`, `strings`, `file`,
  `tty`, `stty`, `time`, `timeout`, `nohup`, and `nice`. It also confirms the
  generated exFAT image still has writable headroom after the applet aliases are
  installed.
- `service_smoke.py`: kernel-started `/init --system`, `/fat/var/log/init.log`,
  boot-owned `svscan` service startup, service dependency/health/max-log
  metadata, `service webd check-config`, `service webd check`,
  `service set`, `service unset`, intentionally broken config validation,
  `service disable`, stopped-service persistence, explicit `service reload`,
  `service enable`, direct `webd` kill followed by supervisor reaping/restart,
  and live `/fat/var/log/svscan.log` output.
- `web_smoke.py`: boot-owned `webd` startup, `service list`, `service log`,
  `service tail`, live `/fat/var/log/init.log`, `/fat/var/log/svscan.log`, and
  `/fat/var/log/webd.log` output, `netstat` listener visibility,
  `ifconfig` interface visibility, host HTTP fetch through QEMU user
  networking, nested CSS asset fetch, `Content-Length`, bodyless `HEAD`,
  404/405 responses, and a slow partial client while another request completes
  through the poll loop.
- `webd_soak.py`: repeated sequential host HTTP requests, a bounded concurrent
  client burst, an early client disconnect during a larger response, and a
  post-abort request to verify `webd` keeps serving static files. It also
  checks query-tolerant static paths, malformed request handling, the
  `/__status` endpoint, and `webd: stats` log lines for accepted, completed,
  failed, busy, idle, active-client, high-water, and byte counters.
- `service_soak.py`: repeated host-side HTTP GETs with interleaved
  `service webd check-config`, `service webd check`, `service status --all`,
  `netstat`, log tailing, `service restart --wait`, log rotation, and svscan
  event log inspection.
- `net_soak.py`: repeated host-side HTTP GETs against background `webd`,
  interleaved with guest-side `/fat/bin/netcheck`, `netstat`, `ifconfig`, and
  `arp`. `/fat/bin/netcheck` exercises DHCP/status, kernel DNS, ICMP ping,
  local UDP socket echo, and outbound TCP HTTP through `getaddrinfo`.
- `tcp_pressure.py`: opens more short host-forwarded HTTP connections than the
  fixed TCP table can keep in `TIME_WAIT`, then verifies `ifconfig` pressure
  counters, `netstat`, and guest-side `/fat/bin/netcheck` still behave.
- `udp_smoke.py`: DHCP setup, `ifconfig`, `route`, `ping`, and `arp`
  diagnostics, DNS-over-UDP, local UDP echo, and zero-length UDP datagram
  handling.
- `fs_stress.py`: repeated file create/read/copy/rename/remove, generated
  short-to-long and long-to-short rename entry updates, generated fill/delete
  fragmentation pressure, fragmented large-file copy/compare, plus fsck before
  and after with allocated-cluster leak checks.
- `fsck_corrupt.py`: mutates temporary exFAT image copies to verify `fsck /fat`
  reports leaked bitmap allocations and stale FAT entries on bitmap-free
  clusters.
- `gui_smoke.py`: desktop/UI launch sanity and fatal exception detection.
- `displayd_smoke.py`: hidden-QEMU compositor seed smoke that verifies
  `/fat/bin/displayd --smoke-autostart` registers, draws its root backbuffer,
  launches `/fat/bin/surfacedemo`, `/fat/bin/gui2demo`, and `/fat/bin/notes2`,
  maps all three v2 surface windows, routes configure events to the clients, and
  exits without a fatal exception. Manual GUI checks should also cover the
  compositor-owned v2 frame controls: title-bar dragging, focus raise,
  minimize/restore, and close.

## DNS Test Domains

`tools/dns_smoke.py` currently expects these domains to resolve:

```text
p2.dev
pauldemers.com
montjoyplaces.com
linguicityworld.app
```

It also checks that a reserved invalid name fails cleanly:

```text
no-such-srvros.invalid
```

## Manual Web Server Check

Start QEMU:

```sh
make run-ahci-net
```

In srvros:

```text
srv> run /fat/bin/sh
/ $ service webd status
/ $ service list
/ $ service webd tail 8
/ $ service webd log
```

On the host:

```sh
curl http://127.0.0.1:8080/
curl http://127.0.0.1:8080/hello.html
curl http://127.0.0.1:8080/status.txt
```

## Manual Filesystem Check

```text
srv> run /fat/bin/sh
/ $ mkdir /fat/projects
/ $ write /fat/projects/readme.txt hello
/ $ mv /fat/projects/readme.txt /fat/projects/renamed.txt
/ $ cat /fat/projects/renamed.txt
/ $ rm /fat/projects/renamed.txt
/ $ rmdir /fat/projects
/ $ exit
srv> fsck /fat
```

## Notes

- The smoke tests use temporary copies of the exFAT disk image where mutation is
  expected.
- Network tests require QEMU user networking and the e1000 device.
- A fatal exception line fails the harness unless it is the intentional boot-time
  breakpoint test.
