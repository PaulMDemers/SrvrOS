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
python3 tools/dir_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/process_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dhcp_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dns_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/ports_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/lua_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/web_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
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
  redirection/append, `2>&1`, zero-byte redirect creation, stdin-aware text tools, scripts,
  copy/remove, native file rename through `mv`, `tap` file splitting,
  foreground/background `fpdemo` userspace SSE checks, and the `posixdemo`
  compatibility-layer smoke app.
- `dir_smoke.py`: nested directory creation, nested file write/read, file
  rename, non-empty `rmdir` rejection, empty directory removal, directory rename,
  and `fsck`.
- `process_smoke.py`: background process launch, process listing, exit status,
  and `wait`.
- `dhcp_smoke.py`: e1000 path, DHCP address acquisition, starting `webd`, host
  HTTP request, and file update served by the web server.
- `dns_smoke.py`: DHCP DNS configuration, `net` status, DNS A-record resolution,
  and clean resolver failure for a non-resolving name.
- `ports_smoke.py`: shell launch of `/fat/bin/zlibdemo`, `/fat/bin/jsondemo`,
  `/fat/bin/inidemo`, `/fat/bin/linedemo`, `/fat/bin/sqlitedemo`,
  `/fat/bin/ttydemo`, and `/fat/bin/posixdemo`; zlib
  compress/decompress, cJSON parse/print/roundtrip, inih string/file parsing,
  linenoise history save/load coverage, SQLite create/insert/query/reopen on
  exFAT through the srvros VFS, termios raw-mode/restore/`ENOTTY` checks, and
  libc/POSIX file checks including `fstat`,
  `dup`, writable-fd dup ownership, advisory `fcntl` byte-range locks with a
  spawned `/fat/bin/lockprobe` conflict check, `pipe`,
  nonblocking `fcntl`/`O_NONBLOCK`, `F_GETFD`/`F_SETFD` `FD_CLOEXEC`,
  `access`, `isatty`, `fsync`, `truncate`/`ftruncate`, `poll`/`select`
  readiness and hangup behavior,
  `O_RDWR`, seek, malloc-on-`sbrk`, raw `sbrk`, `qsort`, `bsearch`,
  integer and floating conversion helpers, random numbers, process-local
  environment variables, `pread`/`pwrite`, `uname`, `getopt`,
  process-replacing `execve`, inherited fd and close-on-exec checks, exFAT
  binary file write/read/unlink, and
  post-run `fsck`.
- `lua_smoke.py`: shell launch of `/fat/bin/lua`, script loading from exFAT,
  integer arithmetic, formatted output through the Lua base library, pure-Lua
  `require`, Lua file IO, and post-run `fsck`.
- `web_smoke.py`: login shell init script, background `webd`, host HTTP fetch
  through QEMU user networking, nested CSS asset fetch, `Content-Length`,
  bodyless `HEAD`, and a slow partial client while another request completes
  through the poll loop.
- `fs_stress.py`: repeated file create/read/copy/rename/remove plus fsck before
  and after.
- `gui_smoke.py`: desktop/UI launch sanity and fatal exception detection.

## DNS Test Domains

`tools/dns_smoke.py` currently expects these domains to resolve:

```text
p2.dev
pauldemers.com
montjoyplaces.com
```

It also checks that this exact spelling fails cleanly:

```text
linguitiyworld.app
```

At the time of this milestone, host DNS also reports no A record for that exact
spelling, while `linguicityworld.app` resolves.

## Manual Web Server Check

Start QEMU:

```sh
make run-ahci-net
```

In srvros:

```text
srv> run /fat/bin/sh --login
/ $ service webd status
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
