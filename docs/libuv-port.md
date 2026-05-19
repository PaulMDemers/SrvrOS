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
  `uv_fileno`, and `uv_timer_get_due_in`.
- Event loop init, run, stop, alive, monotonic time refresh, and the first
  prepare/check/idle phase handles.
- Timers with one-shot and repeating dispatch, repeat setters/getters,
  `uv_timer_again`, and due-in reporting.
- Synchronous filesystem request helpers for open, close, read, write, mkdir,
  rmdir, rename, unlink, and stat.
- TCP listener, accept, connect, read, write, shutdown, and close entry points
  over the srvros socket fd layer. Listener/accept/read/write/shutdown now use
  `uv_stream_t` signatures with TCP as the first concrete backend. Connect
  completion, queued writes, and queued shutdown are driven from writable
  readiness instead of synchronous callbacks, and the stream helpers report
  readability, writability, and pending write-queue bytes.
- `uv_pipe` and `uv_pipe_t` over srvros pipe fds, with stream reads/writes,
  nonblocking creation flags, EOF/HUP reads, and close cleanup.
- `uv_process_t`, `uv_spawn`, `uv_process_get_pid`, `uv_process_kill`, and `uv_kill` over
  `posix_spawnp`, `waitpid(WNOHANG)`, and srvros process kill. The first
  supported stdio bridge can create one-way pipes for child stdin/stdout/stderr
  and can map inherited fds or streams into compact child fd tables.
- UDP bind, recv, and send entry points over the srvros datagram fd layer.
- `uv_poll_t` readiness over POSIX `poll`.
- `uv_async_t` pending notification dispatch inside the loop.
- `uv_queue_work` backed by srvros pthreads, with completion pumped by
  `uv_run`.
- `uv_getaddrinfo` and `uv_freeaddrinfo` over the srvros POSIX resolver, with
  callbacks queued through the loop instead of invoked synchronously.

`/fat/bin/uvdemo` remains the broad behavioral coverage app, including UDP,
multi-client host-forwarded TCP, and guest-outbound TCP connect/write/shutdown/
read against a host service. `/fat/bin/libuvdemo` is the upstream staging harness and
now covers the core object helpers, loop phases, timers, filesystem requests,
async/work callbacks, poll callbacks, resolver callbacks, public pipe creation,
pipe streams, and a child process stdout pipe that need to stay stable while
incrementally replacing adapter internals with upstream code.

## Replacement Strategy

1. Keep `ports/upstream/libuv` unmodified and place srvros-specific glue under
   `ports/srvros`.
2. Compile one upstream subsystem at a time behind the existing public adapter
   tests. The first landed subsystem is upstream `src/version.c`; current
   adapter work is filling the core handle/request API expected by portable
   consumers before replacing loop/time/timer code and then fd polling.
3. Replace the local thread-pool path only after pthread cancellation, stronger
   condition-variable timing, and process signal semantics are further along.
4. Replace TCP/UDP glue after socket errno behavior, shutdown states, and
   nonblocking connect/read/write match libuv expectations under pressure.
5. Add upstream test snippets to `tools/libuv_smoke.py` whenever a subsystem is
   swapped so regressions are caught from inside srvros, not just by host-side
   compilation.

## Known Gaps Before Full Upstream libuv

- No `fork`, full process signals, or Unix domain sockets. `uv_spawn` is a
  compact first pass and returns `UV_ENOSYS` for cwd changes, uid/gid,
  detached children, and duplex `UV_CREATE_PIPE` requests until the srvros
  process ABI grows those semantics.
- Compact TTY model and no PTY support.
- `epoll`/`kqueue` are absent; srvros will need a poll-provider backend.
- Thread cancellation, async signal interruption, and full DNS/thread-pool
  behavior are not yet upstream-compatible.
- Dynamic library loading is not available, so the initial target should be a
  statically linked libuv consumer.
