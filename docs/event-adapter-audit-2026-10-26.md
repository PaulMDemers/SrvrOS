# Eventfd and epoll audit — 2026-10-26

## Scope and disposition

PR178 merged as `8bd43adcb5d603082bd9181abb3413711cc26159`. Its bounded AHCI
repeat campaign is complete; historical spontaneous failures remain unattributed
and shelved pending new evidence. This next audit starts at that merged source
on `device/apple-silicon/eventfd-epoll-audit`, addressing the experimental event
adapters deferred by the [initial audit](code-docs-audit-2026-10-25.md).

This checkpoint adds executable reproductions and documentation, without runtime
changes. Seven directed contract probes fail; ordinary-operation controls pass.
The three finding groups below remain **unfixed**. The supported profile already
excludes these experimental APIs; the new evidence makes their failure modes
concrete. It does not establish Node support or general Linux API conformance.

## P2: epoll state outlives descriptor ownership

`epoll_create1` reserves one of eight libc slots and uses a native pipe read end
as its descriptor. Public `close` releases the native descriptor and notifies
socket bookkeeping, but never releases the epoll slot. `epoll_slot` subsequently
looks up an instance by descriptor number alone. Eventfd close likewise clears
its counter slot without retiring watches in epoll instances.

The probes compile the real public `close` body alongside the real adapters:

- `close-capacity`: eight successful create/close pairs leave no live modeled
  native descriptors, but the ninth create returns `EMFILE` (24).
- `instance-reuse`: close an epoll instance with a registration, then create a
  fresh instance using the recycled native number. A wait on the new instance
  returns the old registration's tag (123), although it has no registrations.
- `watched-reuse`: close a watched eventfd, then create an unrelated eventfd in
  that slot. The old epoll registration delivers the old tag for the new counter.

Stale `epolls[].used` also keeps the fork-readiness scan false after close; that
consequence is established by source inspection, not a guest fork test here.
The [Linux epoll lifetime contract](https://man7.org/linux/man-pages/man7/epoll.7.html)
associates registrations with open file descriptions and their descriptor keys.
These reproductions use no duplicate aliases, so alias lifetime cannot explain
why registrations survive their final close.

**First correction to pursue:** define instance/watch ownership across close,
FD-number reuse and duplication/replacement. Review `dup`, `dup2`, `dup3` and
`F_DUPFD*` together with close-error retirement. Simply clearing one array slot
on every close is not sufficient to establish alias semantics or safe concurrent
wait/control operations. Add directed alias and concurrent lifetime tests before
promoting any expanded contract.

## P2: epoll wait can sleep despite an immediate result

For native descriptors, `epoll_wait(..., 0)` substitutes a 10 ms poll timeout.
`zero-timeout` confirms the actual call to the modeled native poll receives 10,
not zero. When an eventfd is already ready alongside a non-ready native pipe,
`ready-wait` observes a native poll timeout of -1 despite the buffered event.
A real blocking poll can therefore wait indefinitely instead of returning that
ready event. The model returns immediately so the probe is deterministic; it
proves the submitted timeout, not an observed host or guest hang.

The [epoll wait contract](https://man7.org/linux/man-pages/man2/epoll_wait.2.html)
requires a zero-timeout call to return immediately and a ready event to end the
wait. Preserve zero and use a nonblocking native readiness check when events
are already available. Separately audit waiting for a previously empty eventfd:
its initial scan is followed by sleep/native poll without a counter recheck.
That path, signal-mask handling, deadline arithmetic and wake notification need
additional directed tests; the two probes here do not cover them.

## P2: eventfd accepts reserved and overflowing counter writes

`__posix_eventfd_write` adds without checking the reserved value or available
counter capacity. `counter-overflow` starts at one and writes `UINT64_MAX-1`;
it succeeds and a read returns the forbidden user-produced value `UINT64_MAX`.
`reserved-value` starts at one and writes `UINT64_MAX`; the write succeeds and
wraps to zero, so a subsequent nonblocking read reports `EAGAIN` and loses the
pending count. Both should reject the write and preserve the original one.

The [eventfd contract](https://man7.org/linux/man-pages/man2/eventfd.2.html)
reserves `UINT64_MAX`; nonblocking writes exceeding `UINT64_MAX-1` capacity
return `EAGAIN`, and writing the reserved value returns `EINVAL`. Correct bounds
must also agree with writable readiness. Blocking read/write, semaphore mode,
flag validation and concurrent counter updates remain separate unaccepted
semantics, not covered by these two single-threaded counter probes.

## Reproduction and validation

```sh
# Passing ordinary-operation controls; also included in test-core.
sh tools/macos/build.sh test-event-adapters
# Linux/other supported host workflow: make test-event-adapters

# Explicit known failures: exits 1 while all seven findings remain unfixed.
python3 tools/test_event_adapters.py --audit all \
  --output build/review/event-adapters-audit
# Or select one, for example --audit instance-reuse.

# Sanitized positive controls on this Mac.
CC='cc -fsanitize=address,undefined -fno-omit-frame-pointer' \
  python3 tools/test_event_adapters.py \
  --output build/review/event-adapters-sanitized
```

`tools/test_event_adapters.py` copies the adapter prefix through `epoll_pwait`
and the public `close` body verbatim from current source, retaining `#line`
locations. Symbols are prefixed to avoid replacing host libc calls. The fixture
models native descriptor allocation/retirement, poll and sleep; it uses the
repository's headers and real eventfd/epoll bookkeeping. Every mode starts in a
fresh process. Results retain source hashes, compiler command, platform, exit
status and output. Generated sources/binaries/results remain ignored under the
chosen output directory; use separate directories to retain previous runs.

Positive controls pass for counter accumulation/drain, empty nonblocking reads,
short-write rejection, eventfd close, level readiness, MOD mask/data replacement,
DEL and descriptor close. These narrow MOD results do not accept concurrent mask
replacement, edge/one-shot semantics or arbitrary descriptor lifetimes. ASan/UBSan
controls pass without diagnostics. Each directed finding exits 1 at its intended
contract assertion, with the observed result printed first. The default target
runs only positive controls; known failures are neither hidden as passes nor
added to the normal acceptance target.

No kernel/libc runtime changed and no new guest build or VM was needed for this
host-model audit. Native storage, full Mac test-core, Node recovery, general SMP,
networking and filesystem audit coverage are unchanged. The next bounded work
item is the epoll lifetime correction, followed by wait and counter contracts.
