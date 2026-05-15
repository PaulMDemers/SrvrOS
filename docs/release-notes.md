# Release Notes

## Initial Repository Milestone

This milestone captures srvros as a bootable x86_64 research OS with a working
kernel, minimal userspace, filesystem mutation, networking, and a background web
server.

### Highlights

- Boots a higher-half x86_64 kernel through Limine.
- Runs freestanding ring-3 ELF programs from initramfs and `/fat`.
- Adds framebuffer-console parsing for a compact ANSI CSI subset covering
  cursor movement, cursor positioning, clear screen, and clear line while
  preserving raw escape output on serial.
- Adds a minimal console TTY/termios layer with `tcgetattr`/`tcsetattr`,
  canonical/raw input mode toggles, `ICRNL`, `ECHO`, `VMIN`/`VTIME`, erase,
  kill-line, EOF control characters, `ioctl` window-size support, and duplicated
  stdio TTY detection, plus `/fat/bin/ttydemo` smoke coverage.
- Schedules kernel threads and userspace processes with timer preemption.
- Provides foreground/background process control through the monitor and shell.
- Mounts exFAT from initramfs-backed memory or AHCI-backed disks.
- Supports exFAT file create/write/append/delete/rename, directory create,
  empty directory removal, mount/unmount, and consistency checks.
- Drives an Intel e1000 NIC in QEMU with interrupt-backed receive handling.
- Supports ARP, ICMP echo, DHCP, DNS A-record resolution, and enough TCP for a
  userspace HTTP server.
- Ships `/fat/bin/webd`, a poll-driven ring-3 web server serving static files
  from `/fat/www` with nested asset paths, content lengths, MIME/cache headers,
  idle cleanup, and a bounded active-client table.
- Includes a small shell, CLI utilities, service control, redirection,
  multi-stage pipelines, scripts, PATH lookup, and background jobs.
- Adds the first POSIX-compat userspace layer for file, directory, errno,
  malloc, `sbrk`, pipes, time, cwd, IPv4, DNS, and TCP server socket APIs.
- Adds minimal `stdio`, stages zlib and Lua as pinned submodules under
  `ports/upstream`, ships `/fat/bin/zlibdemo`, and adds `/fat/bin/lua` as an
  initial Lua 5.4.8 interpreter.
- Adds pinned cJSON `v1.7.19` and inih `r62` submodules, plus
  `/fat/bin/jsondemo` and `/fat/bin/inidemo` smoke apps for JSON and INI
  parse/roundtrip coverage.
- Adds pinned linenoise `2.0` plus a srvros console adapter used by `srvsh` for
  editable prompt input and `/fat/.srvsh_history`; `/fat/bin/linedemo` verifies
  history save/load behavior.
- Adds SQLite `3.53.1` as a pinned amalgamation snapshot and
  `/fat/bin/sqlitedemo`, which registers a small srvros VFS and verifies
  create/insert/query/reopen behavior against `/fat/sqlitedemo.db`.
- Adds early newlib-style syscall hooks and kernel support for `fstat`,
  `O_RDWR` regular-file fds, relative/end-relative `lseek`, and process heap
  growth.
- Moves userspace `malloc` to `sbrk`-grown heap chunks and adds `dup`/`dup2`
  support for standard streams, pipes, writable regular files, and read-only
  regular files.
- Shares regular-file open descriptions across `dup`/`dup2` and child fd
  inheritance, so read-only descriptors share offsets and writable descriptors
  flush on last close.
- Adds `poll`/`select` support for standard streams, regular files, pipes, and
  TCP listener/connection fds, with pipe readiness and hangup smoke coverage.
- Adds `fcntl(F_GETFL/F_SETFL)` and `O_NONBLOCK` support for the first fd set,
  including pipe, listener, and connection `EAGAIN` behavior.
- Adds `fcntl(F_GETFD/F_SETFD)` and `FD_CLOEXEC` descriptor flags, including
  close-on-exec cleanup during process replacement and socket pseudo-fd flag
  propagation.
- Adds POSIX-style advisory byte-range locks through
  `fcntl(F_GETLK/F_SETLK/F_SETLKW)` for regular files, with `posixdemo`
  plus `/fat/bin/lockprobe` conflict coverage and SQLite VFS locking backed by
  the new kernel lock table.
- Adds real empty-file support, fd flush/truncate hooks, and POSIX-facing
  `access`, `isatty`, `fsync`, `truncate`/`ftruncate`, `chmod`/`fchmod`, and
  `umask` compatibility.
- Persists srvros Unix-like metadata for writable exFAT mounts in
  `/fat/.srvros/meta`, including reboot-tested inode/mode restoration, and
  ships `/fat/bin/chmod` for shell-level permission changes. Sidecar writes
  stage through `/fat/.srvros/meta.tmp`, and mount recovery promotes a complete
  temp file or removes a malformed one before applying metadata.
- Expands libc/POSIX coverage with `pread`/`pwrite`, `getopt`, `uname`,
  `posix_memalign`/`aligned_alloc`, `qsort`, `bsearch`, random numbers,
  integer and floating conversion helpers, process-local environment variables,
  more `stdio` positioning helpers, `atexit`, and newlib `_pread`/`_pwrite`
  hooks.
- Enables FPU/SSE/SSE2 for kernel and ring-3 code, preserves per-process and
  per-scheduler-thread SIMD state across traps, syscalls, scheduler switches,
  and kernel/user transitions with `fxsave64`/`fxrstor64`, and ships
  `/fat/bin/fpdemo` as a preemption stress test for userspace double math.
- Adds an initial userspace `math.h`/`float.h`, floating `%f`/`%g` formatting,
  and switches `/fat/bin/lua` to its normal floating profile with the stock
  `math` library enabled.
- Links all userspace programs through a shared `crt0.S` startup object, removing
  per-app copied `_start` assembly while keeping static ELF apps self-contained.
- Adds basic `scanf`/`sscanf`/`fscanf` support for integer, string, character,
  and floating conversions.
- Adds shell `env`/`export`/`which` builtins and small `/fat/bin` compatibility
  tools for `which`, `env`, `pwd`, `true`, and `false`.
- Adds first CLI milestone quality-of-life tools: `sleep`, monotonic-uptime
  `date`, `touch`, `basename`, and `dirname`.
- Adds more table-stakes CLI tools: `tail`, `tee`, `uname`, `hostname`, and
  `uptime`.
- Adds shell `for`/`in`/`do`/`done` loops, login profile loading from
  `/fat/etc/profile`, `PS1` prompt customization, and first filesystem traversal
  tools with `/fat/bin/find` and `/fat/bin/du`.
- Adds exFAT-backed filesystem capacity reporting through a small `statfs`
  syscall, POSIX-facing `statvfs`, and `/fat/bin/df`, plus pipe-friendly
  `/fat/bin/sort`, `/fat/bin/uniq`, and `/fat/bin/cut`.
- Adds lightweight shell functions with positional arguments, `return`, one-line
  and multiline script definitions, and `type` reporting for functions.
- Adds `/fat/bin/xargs`, a small literal-substitution `/fat/bin/sed`, and shell
  unmatched-quote diagnostics for friendlier CLI scripting failures.
- Adds `/fat/bin/mktemp`, `mkdir -p`, recursive `cp -r`/`rm -r`, shell
  unterminated-block diagnostics for scripts, and Ctrl-C prompt recovery in the
  linenoise console adapter.
- Adds `/fat/etc/profile` and shell fallback `TMPDIR=/fat/tmp`, makes default
  `mktemp` honor `TMPDIR`, decodes quoted assignment values after command
  substitution, and teaches `/fat/bin/mv` to move files and empty directories
  into existing directory destinations.
- Adds interactive Ctrl-D/EOF shell exit coverage and expands common text-tool
  flags: `grep -i/-n/-v/-c/-q`, `wc -l/-w/-c`, compact `head -1`/`tail -1`
  forms, and `find -type f|d`.
- Expands the next CLI compatibility slice with `ls -a/-l` combined flags and
  multi-path headers, `sed -n`, `sed -e`, literal `p`/`d` commands, simple
  line-number and `/pattern/` addresses, and `test -s/-r/-w/-x`.
- Adds `/fat/bin/expr` for script-friendly integer arithmetic, comparisons,
  string length/substr/index operations, and literal-prefix `:` matching.
- Adds `/fat/bin/printf` and `/fat/bin/tr` for portable script output and
  simple byte translation/deletion pipelines.
- Adds shell `while ... do ... done`, `shift`, and the no-op `:` builtin for
  more realistic script control flow.
- Adds shell `break`/`continue` loop control and the POSIX-style `command`
  builtin for `-v`/`-V` lookup and alias/function bypass execution.
- Adds first-pass shell `case`/`in`/`esac` handling with glob-style patterns,
  `|` alternatives, default arms, and multiline script support.
- Allows completed `if`/`for`/`while`/`case` compound commands to continue with
  `;`, `&&`, and `||` tails.
- Adds another shell compatibility slice: command-local environment assignments,
  current-shell brace grouping, comment handling, script line continuations,
  simple here-docs, and better quoted/escaped argument preservation.
- Adds shell `$!` expansion, `fg`/`bg` builtins for the current background
  process model, `wait` status propagation, and tab completion for builtins,
  aliases, functions, PATH commands, and filesystem paths.
- Adds shared userspace path normalization for shell/libc/syscall wrappers,
  including inherited `PWD`, relative paths, repeated `/`, `.`, and `..`.
- Adds kernel-routed Ctrl-C/SIGINT delivery to the active foreground process,
  SIGTERM-backed `kill`, and shell-visible `128 + signal` termination statuses.
- Extends foreground signal handling to shell pipelines by assigning native
  process groups at exec time, waking blocked pipe readers/writers on signal,
  and preserving interrupted pipeline status as `128 + signal`.
- Adds shell job-table tracking for background pipelines, allowing `cmd | cmd &`
  to be foregrounded with `fg $!` and interrupted as a process group.
- Adds shell job references (`%+`, `%-`, numeric `%N`), `jobs -l` pid listing,
  and built-in `kill %job` delivery across every process tracked for a job.
- Expands `srvsh` with `$VAR`/`${VAR}` expansion, `$?`, `$$`, and `&&`/`||`
  command chaining.
- Adds shell-side unquoted `*`/`?` globbing plus `test`/`[` builtins for string,
  integer, and file/directory checks.
- Adds non-interactive `srvsh` entry points through `sh -c command` and
  `sh script`, allowing ports and smoke tests to launch scriptable shell work
  without first entering the editable prompt.
- Adds `$(command)` command substitution, including quoted and nested forms,
  by capturing stdout through a short-lived shell temp file.
- Adds `if`/`then`/`else`/`fi` control flow for one-line commands and multiline
  script blocks.
- Adds more CLI table-stakes shell behavior: positional parameters
  (`$0`, `$1`, `$#`, `$@`) for scripts and `sh -c`, `set -e`/`set +e`,
  `read VAR`, `unset`, `alias`, `type`, `export NAME`, bare `NAME=value`
  assignments, and `cd -` with directory validation.
- Adds shell stdin redirection plus stderr `2>`/`2>>`/`2>&1` redirection, and moves
  external shell launches onto an `execve`-shaped native request that carries
  argv, envp, background state, and stdin/stdout/stderr fd overrides.
- Adds a shell `exec` builtin that replaces the current shell process with a
  resolved command while honoring the same redirection path.
- Adds POSIX-facing `waitpid`, `posix_spawn`, `posix_spawnp`, basic spawn file
  action fd remapping for standard streams, and true process-replacing
  `execve`.
- Adds `/fat/bin/execdemo` as smoke coverage for in-place `execve`; it replaces
  itself with `/fat/bin/false`, and the parent observes the replacement image's
  exit status. The companion `/fat/bin/fdprobe` verifies inherited and
  close-on-exec fd behavior.
- Tightens empty-file handling so zero-byte exFAT files are registered,
  truncating a file through an fd creates a real zero-byte file, and later
  writes can grow that file by allocating fresh clusters.
- Expands the generated exFAT image builder to reserve multi-cluster root and
  `/fat/bin` directory tables with explicit overflow checks.
- Moves `/fat/bin/webd` onto the readiness API so a partial client no longer
  blocks another HTTP request from completing.
- Extends the generated `/fat/www` sample site with a nested CSS asset and
  smoke coverage for GET, HEAD, and slow-client behavior.
- Ships `/fat/bin/tap`, a small stream splitter for stdout plus a secondary
  file, and uses it in the pipeline smoke path.
- Includes early GUI/windowing experiments and sample GUI apps.
- Includes QEMU smoke tests for CLI, processes, directories, DHCP, DNS, web
  serving, and filesystem stress.
- Smoke harnesses launch QEMU with bounded guest memory and temporary copies of
  the exFAT disk image so test writes do not mutate the repository image.

### Screenshots

![srvros boot console](assets/screenshots/console-boot-monitor.png)

![srvros desktop with GUI apps](assets/screenshots/desktop-apps.png)

### Verified Commands

The latest release prep pass verified:

```sh
make -j4
python3 tools/dir_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dns_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64 --line-wait 12
python3 tools/cli_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/process_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/dhcp_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/web_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/ports_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/metadata_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/lua_smoke.py --qemu /ucrt64/bin/qemu-system-x86_64
python3 tools/fs_stress.py --qemu /ucrt64/bin/qemu-system-x86_64 --rounds 1 --line-wait 3
```

### Known Limits

- TCP is intentionally small and aimed at the current web-server milestone.
- DNS resolves A records only.
- exFAT mutation has no journaling or crash-safe transaction model.
- Empty directory rename/removal is supported; non-empty directory removal is
  rejected.
- Device support is oriented around QEMU q35, AHCI, e1000, PS/2, serial, and a
  linear framebuffer.
- The GUI stack is a prototype and not yet a general application ABI.
- Permission bits are srvros-managed metadata. Writable exFAT mounts persist
  them in `/fat/.srvros/meta`; the format is not native exFAT metadata and the
  recovery path is limited to sidecar temp-file promotion/cleanup rather than a
  full journal.
- `stdio` is deliberately small: enough for early command-line ports, not a full
  ISO C implementation.
- Lua uses its normal floating-number profile with `math` enabled. The `os`
  library and native dynamic loading remain disabled.
- Process-exit teardown is non-preemptible while freeing the exiting address
  space, so repeated larger interpreter launches do not leave scheduler context
  pointing at freed page tables.

### Next Release Themes

- Harden exFAT writes with sync, rollback, and broader fragmented-chain testing.
- Add richer userspace networking APIs and readiness primitives.
- Add UDP sockets and stronger DNS resolver behavior.
- Add NVMe storage.
- Expand the shell and support library toward a small libc-shaped environment.
- Move GUI windows toward client-owned shared framebuffers.
- Expand floating-point library coverage and switch Lua toward its normal
  floating-number profile.
