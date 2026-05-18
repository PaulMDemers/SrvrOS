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
- Event loop init, run, stop, alive, and monotonic time refresh.
- Timers with one-shot and repeating dispatch.
- Synchronous filesystem request helpers for open, close, read, write, mkdir,
  rmdir, rename, unlink, and stat.
- TCP listener, accept, connect, read, write, and close entry points over the
  srvros socket fd layer.
- UDP bind, recv, and send entry points over the srvros datagram fd layer.
- `uv_poll_t` readiness over POSIX `poll`.
- `uv_async_t` pending notification dispatch inside the loop.
- `uv_queue_work` backed by srvros pthreads, with completion pumped by
  `uv_run`.

`/fat/bin/uvdemo` remains the broad behavioral coverage app, including UDP and
multi-client host-forwarded TCP. `/fat/bin/libuvdemo` is the upstream staging
harness and intentionally focuses on the subset we need to preserve while
incrementally replacing adapter internals with upstream code.

## Replacement Strategy

1. Keep `ports/upstream/libuv` unmodified and place srvros-specific glue under
   `ports/srvros`.
2. Compile one upstream subsystem at a time behind the existing public adapter
   tests. The first landed subsystem is upstream `src/version.c`; next targets
   are loop/time/timer code and then fd polling.
3. Replace the local thread-pool path only after pthread cancellation, stronger
   condition-variable timing, and process signal semantics are further along.
4. Replace TCP/UDP glue after socket errno behavior, shutdown states, and
   nonblocking connect/read/write match libuv expectations under pressure.
5. Add upstream test snippets to `tools/libuv_smoke.py` whenever a subsystem is
   swapped so regressions are caught from inside srvros, not just by host-side
   compilation.

## Known Gaps Before Full Upstream libuv

- No `fork`, full process signals, or Unix domain sockets.
- Compact TTY model and no PTY support.
- `epoll`/`kqueue` are absent; srvros will need a poll-provider backend.
- Thread cancellation, async signal interruption, and full DNS/thread-pool
  behavior are not yet upstream-compatible.
- Dynamic library loading is not available, so the initial target should be a
  statically linked libuv consumer.
