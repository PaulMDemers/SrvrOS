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
python3 tools/shell_edit_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dir_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/process_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/process_pressure.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dhcp_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dns_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/httpget_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/udp_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/netabi_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/sysabi_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/ports_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/lua_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/service_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/web_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/service_soak.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 4
python3 tools/net_soak.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 3
python3 tools/tcp_pressure.py --qemu /ucrt64/bin/qemu-system-x86_64 --connections 44
python3 tools/fs_stress.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 1
```

Optional GUI smoke:

```sh
python3 tools/gui_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
```

## What The Harnesses Cover

- `cli_smoke.py`: shell startup, PATH lookup, env/export/which, `$VAR` and `$?`
  expansion, child envp inheritance, unquoted `*`/`?` globbing, `test`/`[`,
  `&&`/`||`, core CLI tools, stdin/stdout/stderr redirection, multi-stage
  pipeline fd wiring through `cat | grep | tap`, pipeline output
  redirection/append, `2>&1`, zero-byte redirect creation, stdin-aware text
  tools, source scripts plus non-interactive `sh script` and `sh -c`,
  command substitution, positional parameters, `$!`, shell functions/`return`,
  unmatched quote/block diagnostics, `set -e`, `read`, `alias`,
  `type`, `unset`, bare assignments, quoted assignment command substitution,
  `cd -` and directory validation,
  canonical relative paths using `.`/`..` against inherited `PWD`,
  `if`/`then`/`else`/`fi`, `sleep`/`date`/`touch`/
  `basename`/`dirname`, `tail`, `tee`, `uname`, `hostname`, `uptime`,
  `for` loops, `/fat/etc/profile`, `PS1`, `find`, `du`, `df`, `sort`, `uniq`,
  `cut`, `xargs`, `sed`, default `TMPDIR`, `mktemp`, `mkdir -p`, recursive
  `cp`/`rm`, copy/remove, clustered `rm -fRr`, multi-source `cp`/`mv` into
  directories, native file rename and directory destinations through `mv`,
  `/fat/share/help` topic files, `help <topic>`, `more --plain`,
  `grep -i/-n/-v/-c/-q`, `wc -l/-c`, `head -1`/`tail -1`,
  `find -type`, `ls -a/-la/-d/-1` and multi-path headers, `sed -n`/`-e`/`p`/`d`
  with simple addresses, `expr` arithmetic/string expressions,
  `printf`, `tr`, `while` loops, `case` pattern matching, compound-command
  tail chaining, command-local environment assignments, comments, script line
  continuations, simple here-docs, brace grouping, quoted empty and escaped-space
  arguments, `shift`, `break`/`continue`,
  `command -v`/`command -V` and alias bypass, `fg`/`bg`,
  `test -s/-r/-w/-x`, Ctrl-D/EOF shell exit,
  `tap` file splitting,
  foreground/background `fpdemo` userspace SSE checks, and the `posixdemo`
  compatibility-layer smoke app.
- `shell_edit_smoke.py`: interactive raw-mode shell editing over serial,
  including Ctrl-A/Ctrl-E cursor movement, Ctrl-U/Ctrl-W kill operations,
  Ctrl-Y yank, mid-line insert, escape-sequence arrow navigation, and preserving
  an in-progress draft while browsing history. It also covers the `history`
  builtin, `HISTFILE`/`HISTSIZE`, explicit history save, and script-path/line
  diagnostics for shell errors.
- `dir_smoke.py`: nested directory creation, nested file write/read, file
  rename, non-empty `rmdir` rejection, empty directory removal, directory rename,
  and `fsck`.
- `process_smoke.py`: background process launch, process listing, exit status,
  `wait`, `jobs -l`, `%+` job references, foreground/background pipeline job
  control, and Ctrl-C interruption of CPU-bound processes and pipelines with
  `status 130`.
- `process_pressure.py`: repeated foreground execs, multi-stage pipelines, many
  concurrent background sleeps, `jobs -l`, `wait`, and a final pipeline pass. It
  fails on process-table, scheduler-thread, pipe, or pipeline-spawn exhaustion
  markers, and it verifies that bare `wait` drains the test-created background
  sleeps from the process table.
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
- `sysabi_smoke.py`: launches `/fat/bin/sysabi`, which calls raw core
  structured syscalls (`stat`, `statfs`, process list, console/gfx info, and
  GUI receive) with smaller versioned structs and canary checks.
- TCP socket coverage is split across `httpget_smoke.py` for outbound
  DNS/connect/send/recv, `web_smoke.py` and `dhcp_smoke.py` for inbound
  listener/accept/read/write/close, and `ports_smoke.py` for socket option,
  name-query, and shutdown compatibility checks. `web_smoke.py` also requests a
  multi-kilobyte static file to exercise segmented TCP writes and ACK-retired
  transmit history under bounded send backpressure, peer receive-window limits,
  dynamic receive-window advertisements, zero-window persist support, and
  guest-side closed-port TCP RST behavior through QEMU host forwarding.
  `httpget_smoke.py` covers outbound DNS/connect/send/recv with the newer
  socket error propagation in place.
- `ports_smoke.py`: shell launch of `/fat/bin/zlibdemo`, `/fat/bin/jsondemo`,
  `/fat/bin/inidemo`, `/fat/bin/linedemo`, `/fat/bin/sqlitedemo`,
  `/fat/bin/ttydemo`, and `/fat/bin/posixdemo`; zlib
  compress/decompress, cJSON parse/print/roundtrip, inih string/file parsing,
  linenoise history save/load coverage, SQLite create/insert/query/reopen on
  exFAT through the srvros VFS, termios raw-mode/restore/window-size/duplicated
  TTY fd/`ENOTTY` checks, and libc/POSIX file checks including `fstat`,
  `dup`, shared regular-file offsets, writable-fd dup ownership, `statvfs`, advisory
  `fcntl` byte-range locks with a
  spawned `/fat/bin/lockprobe` conflict check, `pipe`,
  nonblocking `fcntl`/`O_NONBLOCK`, `F_GETFD`/`F_SETFD` `FD_CLOEXEC`,
  `access`, `isatty`, `fsync`, `truncate`/`ftruncate`, `poll`/`select`
  readiness and hangup behavior,
  `w+`/`r+`/`a+` stdio update streams,
  anonymous and file-backed private `mmap`/`munmap`, `mprotect`/`PROT_NONE`,
  `msync`,
  `O_RDWR`, seek, malloc-on-`sbrk`, raw `sbrk`, `qsort`, `bsearch`,
  integer and floating conversion helpers, random numbers, process-local
  environment variables, `pread`/`pwrite`, `uname`, `getopt`,
  formatted-output width/precision/flag handling, stream buffering/`fflush`,
  EOF/error state, `scanf` scansets and suppressed assignments, `system()` shell execution,
  `popen`/`pclose`, standard-fd and non-stdio spawn `dup2`/`open`/`close`
  actions, `POSIX_SPAWN_SETPGROUP`, process-replacing `execve`,
  inherited fd and close-on-exec checks, exFAT
  binary file write/read/unlink, and
  post-run `fsck`.
- `lua_smoke.py`: shell launch of `/fat/bin/lua`, script loading from exFAT,
  integer arithmetic, formatted output through the Lua base library, pure-Lua
  `require`, Lua file IO, and post-run `fsck`.
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
- `fs_stress.py`: repeated file create/read/copy/rename/remove plus fsck before
  and after.
- `gui_smoke.py`: desktop/UI launch sanity and fatal exception detection.

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
