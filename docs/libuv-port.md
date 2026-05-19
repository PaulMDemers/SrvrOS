# libuv Port Notes

srvros now carries upstream libuv as a pinned submodule:

```text
ports/upstream/libuv -> v1.52.1 (1cfa32f)
```

The runnable code path is still the srvros adapter in `ports/srvros/uv.c` and
`ports/srvros/uv.h`. That adapter deliberately exposes a small libuv-shaped API
while the OS grows the kernel and libc pieces required by the real upstream
backend.

## Current Adapter Surface

- `uv_version()` and `uv_version_string()` now compile directly from upstream
  `src/version.c`, so consumers see the pinned libuv release identity from the
  real source tree.
- The public error surface now uses libuv's upstream errno definitions for
  `UV_E*` values and exposes `uv_translate_sys_error`, `uv_err_name`,
  `uv_err_name_r`, `uv_strerror`, and `uv_strerror_r`.
- Core API parity now includes loop close/data helpers, backend timeout/fd
  stubs, handle type/name/size/data/active/closing helpers, request
  type/name/size/data helpers, handle ref/unref state, `uv_walk`,
  `uv_fileno`, and `uv_timer_get_due_in`. `uv_loop_close` now rejects loops
  that still have unclosed handles, even if those handles are inactive.
- Event loop init, run, stop, alive, monotonic time refresh, and the first
  prepare/check/idle phase handles.
- Platform helpers for cwd/chdir, executable path, pid/ppid, environment
  get/set/unset, high-resolution time, memory totals from the kernel meminfo
  ABI, and sync/queued `uv_random` buffers.
- Timers with one-shot and repeating dispatch, repeat setters/getters,
  `uv_timer_again`, and due-in reporting.
- Filesystem request helpers for open, close, read, write, mkdir, rmdir,
  rename, unlink, stat, lstat, fstat, access, realpath, scandir, fsync,
  fdatasync, ftruncate, sendfile, utime, and futime. Callback forms now run
  through the shared worker pool and complete through the event loop queue
  instead of inline callbacks, and `uv_fs_req_cleanup` releases path, realpath,
  buffer-array, and scandir allocations.
- TCP listener, accept, connect, read, write, shutdown, and close entry points
  over the srvros socket fd layer. Listener/accept/read/write/shutdown now use
  `uv_stream_t` signatures with TCP as the first concrete backend. Connect
  completion, queued writes, and queued shutdown are driven from writable
  readiness instead of synchronous callbacks, and the stream helpers report
  readability, writability, and pending write-queue bytes.
- `uv_pipe` and `uv_pipe_t` over srvros pipe fds, with stream reads/writes,
  nonblocking creation flags, EOF/HUP reads, and close cleanup.
- `uv_tty_t` over the srvros console fd layer, including `uv_guess_handle`,
  terminal window-size queries, normal/raw mode switching, stream writes, and
  vterm state probes. Closing a libuv TTY wrapper leaves the inherited stdio fd
  open.
- `uv_signal_t` delivery for SIGINT/SIGTERM watchers, including start,
  one-shot start, stop, handle sizing, handle naming, and callback dispatch
  through `uv_run`. The adapter uses srvros signal catch/poll syscalls so a
  watched signal wakes blocking poll waits instead of terminating the process.
- `uv_process_t`, `uv_spawn`, `uv_process_get_pid`, `uv_process_kill`, and `uv_kill` over
  `posix_spawnp`, `waitpid(WNOHANG)`, and srvros process kill. The stdio bridge
  can create one-way pipes for child stdin/stdout/stderr, create a duplex pipe
  endpoint for `UV_READABLE_PIPE | UV_WRITABLE_PIPE`, honor `cwd`, and map
  inherited fds or streams into compact child fd tables.
- UDP bind, recv, and send entry points over the srvros datagram fd layer.
- `uv_poll_t` readiness over POSIX `poll`.
- `uv_async_t` pending notification dispatch inside the loop.
- `uv_queue_work` backed by a small reusable srvros pthread worker pool, with
  completion pumped by `uv_run`. `uv_cancel` can cancel queued work before a
  worker begins running it and delivers `UV_ECANCELED` to the after-work
  callback.
- Callback-based filesystem requests can also be canceled while still queued in
  the worker pool, completing through their normal callback with
  `UV_ECANCELED`.
- Per-loop wake pipes so `uv_async_send`, worker completions, and async
  filesystem completions can interrupt a blocked poll wait. `uv_backend_fd`
  exposes the wake-read fd for compatibility probes.
- Thread and synchronization wrappers over the srvros pthread layer:
  `uv_thread_create`, `uv_thread_create_ex`, `uv_thread_detach`,
  `uv_thread_join`, `uv_thread_self`, `uv_thread_equal`, mutexes including
  recursive mutex initialization, reader/writer locks, semaphores, condition
  variables including relative timed waits, `uv_once`, TLS keys, and barriers.
- `uv_getaddrinfo` and `uv_freeaddrinfo` over the srvros POSIX resolver, with
  callbacks queued through the loop instead of invoked synchronously.

`/fat/bin/uvdemo` remains the broad behavioral coverage app, including UDP,
multi-client host-forwarded TCP, and guest-outbound TCP connect/write/shutdown/
read against a host service. `/fat/bin/libuvdemo` is the upstream staging harness and
now covers the core object helpers, loop phases, timers, filesystem metadata
and directory requests, fsync/truncate/sendfile/time requests, platform
helpers, sync and queued random fills, async/work callbacks, poll callbacks,
resolver callbacks, public pipe creation, pipe streams, child stdin/stdout pipe
wiring, `uv_spawn` cwd handling, a duplex stdio child pipe, plus TTY/signal
delivery and the current thread/synchronization wrappers that need to stay
stable while incrementally replacing adapter internals with upstream code.

## Replacement Strategy

1. Keep `ports/upstream/libuv` unmodified and place srvros-specific glue under
   `ports/srvros`.
2. Compile one upstream subsystem at a time behind the existing public adapter
   tests. The first landed subsystem is upstream `src/version.c`; current
   adapter work is filling the core handle/request API expected by portable
   consumers before replacing loop/time/timer code and then fd polling.
3. Keep hardening the local worker-pool path with cancellation and stronger
   shutdown semantics before swapping in upstream thread-pool internals.
4. Replace TCP/UDP glue after socket errno behavior, shutdown states, and
   nonblocking connect/read/write match libuv expectations under pressure.
5. Add upstream test snippets to `tools/libuv_smoke.py` whenever a subsystem is
   swapped so regressions are caught from inside srvros, not just by host-side
   compilation.

## Known Gaps Before Full Upstream libuv

- No `fork`, full process signals, or Unix domain sockets. `uv_spawn` is still a
  compact first pass and returns `UV_ENOSYS` for uid/gid changes and detached
  children until the srvros process ABI grows those semantics.
- Compact TTY model and no PTY support.
- `epoll`/`kqueue` are absent; srvros will need a poll-provider backend.
- Thread cancellation, async signal interruption, and full DNS/thread-pool
  behavior are not yet upstream-compatible. The current thread APIs and worker
  pool map onto srvros pthreads and are useful for portable consumers. Queued
  work/fs cancellation is present, but in-flight worker interruption and the
  upstream libuv threadpool backend have not been swapped in yet.
- Dynamic library loading is not available, so the initial target should be a
  statically linked libuv consumer.
