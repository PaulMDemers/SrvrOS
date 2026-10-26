# Eventfd and epoll audit — 2026-10-26

## Scope and disposition

PR178 merged as `8bd43adcb5d603082bd9181abb3413711cc26159`. Its bounded AHCI
repeat campaign is complete; historical spontaneous failures remain unattributed
and shelved pending new evidence. This next audit starts at that merged source
on `device/apple-silicon/eventfd-epoll-audit`, addressing the experimental event
adapters deferred by the [initial audit](code-docs-audit-2026-10-25.md).

The initial checkpoint at `526b6353` added executable reproductions without
runtime changes: seven directed probes failed. The subsequent correction below
fixes the descriptor-lifetime group. The next correction fixes immediate waits,
eventfd readiness rechecks, deadline accounting and empty/infinite waits. Nonblocking
counter bounds, writable readiness, semaphore reads and creation-flag validation
are now corrected too, along with stable ready-set rotation and native HUP/ERR
reporting for empty interest masks, one-shot disabling/rearming and priority-bit
conversion. EPOLLET now fails explicitly with EOPNOTSUPP on ADD/MOD, preserving
existing watches; actual edge delivery is unsupported. Closeout adds mask waits,
blocking eventfd I/O, public poll integration, normal-data aliases and ownership
refusals; see the closeout below for verification. See the current
[audit coverage and backlog](audit-progress-2026-10-26.md). The supported profile
limits these experimental APIs; the evidence makes their failure modes concrete.
It does not establish Node support or general Linux API conformance.

## P2: epoll state outlives descriptor ownership — corrected

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

**Initial correction plan (implemented below):** define instance/watch ownership across close,
FD-number reuse and duplication/replacement. Review `dup`, `dup2`, `dup3` and
`F_DUPFD*` together with close-error retirement. Simply clearing one array slot
on every close is not sufficient to establish alias semantics or safe concurrent
wait/control operations. Add directed alias and concurrent lifetime tests before
promoting any expanded contract.

## P2: epoll wait can sleep despite an immediate result — corrected

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

## P2: eventfd accepts reserved and overflowing counter writes — corrected

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

# Run the current complete host set (46 positive modes; no active failure probes).
python3 tools/test_event_adapters.py --audit all \
  --output build/review/event-adapters-audit
# Historical failing edge probe is retained in the pre-fix evidence directory.

# Sanitized positive controls on this Mac.
CC='cc -fsanitize=address,undefined -fno-omit-frame-pointer' \
  python3 tools/test_event_adapters.py \
  --output build/review/event-adapters-sanitized
```

`tools/test_event_adapters.py` copies the adapter prefix through `epoll_pwait`
and the public close/duplication/fcntl implementations verbatim from current
source, retaining `#line` locations; it also compiles the real public poll source.
Symbols are prefixed to avoid replacing host libc calls. The fixture models native
descriptor allocation/retirement, monotonic time, kernel poll and signal services; it uses the
repository's headers and real eventfd/epoll bookkeeping. Every mode starts in a
fresh process; the host mutex model asserts ownership and release around waits. Results retain source hashes, compiler command, platform, exit
status and output. Generated sources/binaries/results remain ignored under the
chosen output directory; use separate directories to retain previous runs.

Positive controls pass for counter accumulation/drain, empty nonblocking reads,
short-write rejection, eventfd close, level readiness, MOD mask/data replacement,
DEL and descriptor close. These narrow MOD results do not accept concurrent mask
replacement, edge/one-shot semantics or arbitrary descriptor lifetimes. ASan/UBSan
controls pass without diagnostics. At the initial checkpoint each directed finding exited 1 at its intended
contract assertion, with the observed result printed first. The current default
target includes positive controls and the corrected lifetime/wait/counter/mode cases; remaining
known failures are neither hidden as passes nor added to the acceptance target.

No kernel/libc runtime changed in the initial `526b6353` host-model audit. Native storage, full Mac test-core, Node recovery, general SMP,
networking and filesystem audit coverage are unchanged. The lifetime correction and further wait findings follow below.


## Descriptor lifetime correction

The libc adapter now assigns monotonically increasing identities to tracked
native/pseudo descriptors. All public native `dup`, `dup2`, `dup3`, `F_DUPFD`
and `F_DUPFD_CLOEXEC` paths preserve the source identity, including duplicates
made before the first epoll watch. An epoll instance belongs to that identity,
so closing its original number leaves aliases usable. Its final close or
replacement retires the instance and makes its capacity reusable. Failed native
duplication leaves the target binding untouched; `dup2(fd, fd)` is a no-op.
Close retires ownership even when the kernel reports a flush error after removal.
A failed close-on-exec setup tears down the newly created instance.

Registrations use the original descriptor number plus identity. A native watch
may remain after its registered number closes while another alias remains;
readiness uses a surviving alias and preserves the original event data. A new
object reusing the old number gets a distinct identity and may be independently
registered. Its MOD/DEL cannot affect the old object's watch. Final alias close
removes all watches for that identity. Eventfd close likewise removes its watches
before recycling its counter slot. This follows the alias/key distinction in the
[epoll lifetime contract](https://man7.org/linux/man-pages/man7/epoll.7.html).

A shared libc mutex serializes lifecycle syscalls with their bookkeeping and
epoll create/control. Eventfd slot publication uses acquire/release operations
for descriptor dispatch, and counter/flag operations share the lock. The lock is
released before poll, sleep or signal dispatch.
Waits copy registration data and revision numbers, then validate the instance,
watch revision and polled descriptor identity before returning events. Closing
the last instance alias during a wait returns `EBADF`, even if the slot/number
was reused. Results from removed, modified or replaced registrations are
suppressed. Closing only the original epoll handle preserves an in-flight wait
when another alias survives. This protects ownership; it does not fix the
separate wait-notification/deadline policies.

### Acceptance boundary

This is a libc-managed, single-process adapter, not a kernel epoll facility.
The tested lifetime contract covers native descriptors managed through public
libc close/dup APIs and eventfd close/reuse. Direct `srv_*` descriptor mutations,
SCM_RIGHTS-transferred aliases, cross-process/exec preservation and pseudo-socket
state-transition semantics are not accepted. Do not mix raw descriptor mutation
with this tracking. Native and pseudo namespace capacities remain 67, 8 sockets
and 16 eventfds; instance/watch limits remain 8/64.

Eventfd duplication remains unsupported. Pseudo-descriptor replacement through
dup2/dup3 is explicitly refused with `EOPNOTSUPP` (same-number dup2 remains a
validated no-op). Nested epoll is refused rather than polling its placeholder
pipe (`EINVAL` for self/alias-self, `EOPNOTSUPP` for another instance). Fork with
live adapter state remains refused; after final cleanup it becomes eligible
again. Edge/one-shot semantics, general concurrent socket behavior, signal-mask
waits and full Linux compatibility remain outside the profile.

### Regression evidence

The three original lifetime failures now run as positive default regressions.
Additional host modes cover duplicate-before-watch, distinct registrations on
aliases, old-object watches surviving number reuse, MOD/DEL identity isolation,
all five duplication APIs, per-descriptor CLOEXEC, same-number operations,
replacement/failure rollback, capacity exhaustion/recovery and close-error
retirement. Poll callbacks deterministically close/recreate an instance, replace
a watched descriptor, modify a watch or close one surviving instance alias; no
lock is held across those callbacks. Positive host modes pass under ASan/UBSan.

Four compiled source-copy negative controls remove close retirement, break alias
binding, bypass instance validation or bypass watch-revision validation. Each
fails its intended positive assertion. The first alias negative-control compile
failed because its mutation left a local unused; a corrected control compiles
and fails at runtime. Both attempts are retained. Initial fixture integration
also caught a missing modeled fcntl service and a model that incorrectly left
standard descriptors free; these were fixture issues corrected before acceptance.

`threadstress epoll-lifetime` exercises public APIs in a matched QEMU guest:
24 repeated instance/alias lifetimes, instance/eventfd number reuse, native pipe
alias watches, F_DUPFD/CLOEXEC, dup2/dup3 replacement, failure/no-op paths, four
workers each performing 16 lifetimes, and a successful fork after cleanup. It is
also part of `threadstress all`, with a required smoke-harness completion marker.
No kernel or syscall ABI changes are required; rebuild libc and guest programs.

## Historical continued audit: eventfd posted during a wait is missed — corrected

The new explicit `eventfd-wakeup` host mode starts with an empty watched eventfd
and posts one during the modeled sleep of `epoll_wait(..., 20)`. The adapter
returns zero using its original readiness observation. The probe expects the
new event and fails. This confirms the previously suspected missing recheck; it
does not measure actual guest wake latency. A wait correction must coordinate
notifications with native poll, retain deadline/signal behavior and revalidate
identities on every retry. The two prior immediate-wait probes and two counter
probes still fail unchanged. These five failures remain excluded from the
positive acceptance target. Wait handling is the next bounded correction.


Final revision validation: matched build, eight positive host modes and their
ASan/UBSan runs, complete thread guest (including epoll lifetime), libuv guest
and five smoke-reader tests all pass. Four negative controls compile and fail
the intended assertions. The five explicit remaining wait/counter probes fail
as documented. Documentation checks 653 local inline links with zero errors;
whitespace checks pass. Source/build/log hashes and all directed receipts are
retained under `build/review/epoll-lifetime/`. All disposable guests stopped.
No physical trial is armed and Actions remain disabled. Draft PR179 now carries
the lifetime correction; the next bounded item is wait/notification handling.


## Wait/deadline correction

The prior `b51143cb` checkpoint above is preserved as historical evidence. Waits
now rebuild their snapshot after bounded polls rather than returning a pre-sleep
observation. Zero timeouts stay zero. A ready eventfd forces a nonblocking native
readiness check, so an idle pipe cannot hide it. Counter readiness is rechecked
under the lifecycle lock after polling; a concurrent drain cannot produce stale
synthetic readiness. Registration revisions, instance identity and native binding
validation remain enforced on every iteration.

Finite waits use one `CLOCK_MONOTONIC` deadline with rounded-up millisecond
remainders. They do not multiply an `int` timeout into overflowing microseconds
or restart the budget on retries. Empty and negative-timeout waits continue until
readiness, interruption or final instance retirement (`EBADF`); ADD/MOD changes
are discovered on the next snapshot. Poll errors, including `EINTR`, propagate.
No mutex is held across native polling or signal dispatch. These are the bounded
contracts described by [epoll_wait(2)](https://man7.org/linux/man-pages/man2/epoll_wait.2.html),
not acceptance of every Linux epoll behavior.

Synthetic counters and control operations still lack a kernel notification
source. Each blocking native poll is capped at 10 ms; eventfd posts/control changes
are detected on recheck, with kernel clock rounding and scheduler delay in
addition to that interval. Idle waits therefore wake periodically. This avoids
lost persistent readiness without introducing a new syscall or hidden descriptor
allocation, but is not an efficient kernel event queue or a hard latency bound.
Ready-set round-robin fairness, zero-interest HUP/ERR handling, edge/one-shot flags,
`epoll_pwait` signal masks and general socket transitions remain outside acceptance.

### Wait regression coverage

The three former wait failures now run in the default positive gate. Five new
host modes cover finite remainders across a second boundary, `INT_MAX` timeouts
with a simulated long scheduling delay, clock failure, multiple empty poll
returns before an infinite wait is posted, synthetic/native mixtures, ADD into an
empty wait, instance close/reuse, MOD retry, poll errors and concurrent drain.
The existing eight lifetime/ordinary modes remain, for sixteen positive modes.
The modeled clock and poll callbacks make these orderings deterministic; they do
not establish guest latency or SMP behavior.

`threadstress epoll-wait` adds a real empty finite timeout, zero-timeout checks,
already-ready mixed descriptors and 24 worker-driven waits: synthetic posts,
mixed posts, native pipe writes, ADD into an empty infinite wait, close of an
empty waiting instance and targeted signal interruption of `INT_MAX` waits with
`SA_RESTART`. The full thread harness requires its completion marker. See final
validation below for actual results.

## Historical continued audit: full counters incorrectly remain writable — corrected

`counter-writable` creates a nonblocking eventfd, writes the legal maximum
`UINT64_MAX-1`, registers only `EPOLLOUT`, and observes one ready event. Its
assertion expects zero and fails. `eventfd_poll_events` currently reports writable
unconditionally. The [eventfd contract](https://man7.org/linux/man-pages/man2/eventfd.2.html)
requires room for at least one; the future bounds fix must update readiness with
the same capacity rule, and restore it after a drain. This is a third explicit
counter failure, alongside overflow and reserved-value writes. All three remain
outside the passing gate. Blocking read/write, semaphore mode and flag validation
remain separate audit areas. No counter correction is claimed by this wait fix.


Final wait validation: matched `all` build, sixteen positive host modes and
ASan/UBSan, targeted epoll-wait guest, complete thread guest, full libuv consumer
guest and five smoke-reader tests pass. Five compiled negative controls restore
zero-timeout sleeping, ready-event delay, premature infinite-wait return, missing
deadline clamping or stale counter readiness; each fails its intended assertion.
Three explicit counter probes fail as documented, including the new full-counter
writable case. Local manifests, source/log hashes and receipts are retained under
`build/review/epoll-wait/`. All disposable VMs stopped. No kernel/syscall ABI,
physical acceptance or Actions setting changed. PR179 remains draft; next bounded
correction is eventfd counter bounds and matching writable readiness.


## Counter capacity and writable-readiness correction

The `1f83d29f` wait checkpoint above retains its original three failed counter
receipts. Nonblocking writes now reject `UINT64_MAX` with `EINVAL` before checking
capacity. A subtraction-based check rejects additions greater than the remaining
`UINT64_MAX-1` capacity with `EAGAIN`, before any counter mutation. Exact fills
succeed; zero writes succeed even at full capacity. The existing lifecycle mutex
serializes the check/add with reads and readiness, so concurrent writers cannot
spend the same remaining capacity. EPOLLOUT is reported only below the maximum;
read/drain restores it. No kernel or public ABI changes are required.

This counter checkpoint accepted bounded nonblocking behavior, not general eventfd
semantics. At that checkpoint, descriptors without `EFD_NONBLOCK` still return `EAGAIN`/`EWOULDBLOCK` for empty
reads or capacity-blocked writes instead of parking. Eventfd flags/semaphore mode,
duplication, raw mutations, cross-process state and native poll/select integration
remained outside acceptance; the mode/flags correction follows below. The epoll adapter retains its 10 ms polling interval
plus rounding/scheduling delay; counter fixes do not add kernel notifications.

The default host gate now includes all three former counter failures plus two
boundary/readiness modes: near/full/empty transitions, exact fills, zero writes,
reserved values at empty/full, rejection preserving state, short/null buffers,
unaligned eight-byte transfers and oversized buffers with trailing bytes intact.
A poll callback drains a full counter and verifies a waiting EPOLLOUT becomes
ready. There are twenty-one positive modes including the prior lifetime/wait
coverage. Host services remain modeled; no host concurrency claim is inferred.

`threadstress eventfd-counter` exercises public read/write/epoll APIs, boundary
transitions and eight rounds of four guest writers making 128 writes into 31
remaining increments: exactly 31 succeed and 97 return `EAGAIN`, then a read
returns the legal maximum. It also waits for a worker to drain a full counter
and restore EPOLLOUT. The full thread suite requires its completion marker.

## Historical continued audit: semaphore reads and unknown creation flags — corrected

`semaphore-read` creates `eventfd(2, EFD_NONBLOCK | EFD_SEMAPHORE)`. The first read
returns two and drains the counter; the assertion expects one and fails. The
creation path silently discards the semaphore bit. The
[eventfd contract](https://man7.org/linux/man-pages/man2/eventfd.2.html) requires
semaphore reads to return one and decrement by one while retaining remaining
readiness. `invalid-flags` passes unknown bit `0x40000000`; creation succeeds
instead of returning `EINVAL`, and the directed assertion fails. Both are now
explicit known-failure modes outside the positive gate. Next bounded correction
is retaining semaphore mode and rejecting unsupported creation bits without
allocating state. Blocking I/O remains a separate implementation decision.


Final counter validation: matched `all` build, twenty-one positive host modes
and ASan/UBSan, targeted eventfd-counter VM, complete thread suite and full libuv
consumer VM pass. Four compiled negative controls break reserved-value rejection,
capacity enforcement, writable readiness or failed-write preservation; all fail
their intended assertions. Five smoke-reader tests pass. Documentation checks
655 local links with zero errors; whitespace checks pass. Baseline three-counter
failures and two newly confirmed semaphore/flag failures are preserved separately.
Local source/build/log hashes and receipts are under `build/review/eventfd-counter/`.
All disposable guests stopped; no physical trial, ABI or Actions setting changed.
PR179 remains draft. Next bounded scope is eventfd semaphore mode and creation
flag validation; blocking I/O and broader Linux adapter semantics remain separate.


## Semaphore mode and creation flags correction

The `e37213bf` counter checkpoint retains its two original failed mode/flag
receipts. Creation now validates the public eventfd flag mask before allocating
any state, including when all slots are occupied. Unknown bits return `EINVAL`;
valid requests against a full table still return `EMFILE`. All eight combinations
of the three declared flags remain accepted. A private semaphore-mode field is
initialized on allocation and cleared on close, separately from mutable status
and descriptor flags. `F_SETFL` and `F_SETFD` do not change the read mode.

Under the existing lifecycle mutex, a semaphore read returns one and subtracts
one; an ordinary read returns and subtracts the entire count. Buffer validation
happens first, so failed reads cannot consume tokens. A semaphore read from a
full counter restores room for exactly one write and EPOLLOUT, while EPOLLIN
persists until the last token is consumed. This adds no syscall or public ABI.
Blocking I/O, duplication/cross-process state, native poll/select integration and
kernel notification remain outside the bounded adapter contract.

Twenty-five positive host modes now include the two former failures plus flag
combination/slot reuse/fcntl/capacity tests and semaphore buffer/readiness tests.
Unknown-bit coverage includes the sign bit and negative flags. Real adapter and
fcntl code is compiled with modeled native services; this is not a scheduler test.
The `eventfd-modes` guest checks public APIs, all valid flag combinations, full
counter read/refill readiness and eight rounds of four readers making 128 reads
from 31 tokens. Exactly 31 reads return one and 97 return `EAGAIN` in each round.
It is part of `threadstress all`, with a required smoke completion marker.

## Historical continued audit: ready-set starvation and zero-interest hangups — corrected

`epoll-fairness` registers two persistently readable eventfds, then makes four
zero-timeout waits with `maxevents=1`. Every result refers to the first watch;
the second is never visited. The source scans from slot zero and stops once the
output limit is filled. The [epoll wait contract](https://man7.org/linux/man-pages/man2/epoll_wait.2.html)
specifies rotation through a ready set larger than the result buffer. The probe
expects both tags and fails; this confirms a starvation case, not measured
scheduling fairness or a frequency estimate.

`epoll-hup-only` registers a native descriptor with an empty interest mask and
models a native POLLHUP. The adapter skips the watch before calling poll, returning
zero instead of EPOLLHUP. The [epoll control contract](https://man7.org/linux/man-pages/man2/epoll_ctl.2.html)
requires hangup/error reporting without an explicit request bit. The directed
hangup assertion fails; the same source filter also merits error-event coverage.
These two explicit failures stay outside the positive gate. Next bounded scope
is ready-set rotation and unconditional native error/hangup polling, preserving
the existing lifetime/deadline checks. Edge/one-shot and signal-mask semantics
remain separate; no complete Linux compatibility claim is made.


Final mode/flag validation: matched `all` build, twenty-five positive host modes
and ASan/UBSan, targeted eventfd-modes VM, complete thread suite and full libuv
consumer VM pass. Four compiled negative controls break flag rejection, mode
storage, one-token subtraction or fcntl mode independence; each fails its intended
assertion. Five smoke-reader tests pass. Documentation checks 655 local links with
zero errors; whitespace checks pass. The baseline's two semaphore/flag failures
and the two new fairness/hangup failures are retained separately, under
`build/review/eventfd-modes/`, with source/build/log hashes. All disposable guests
stopped. PR179 remains draft; next bounded scope is epoll ready-set rotation and
zero-interest native error/hangup reporting. Blocking I/O, kernel notification,
physical acceptance and Actions settings remain unchanged.


## Ready-set rotation and unconditional native HUP/ERR correction

The `87c48440` checkpoint above retains its two failed fairness/hangup receipts.
Each epoll instance now owns a delivery cursor, shared by duplicate handles.
Every wait snapshots all registrations and polls all native descriptors, even
when an eventfd is already ready (the native timeout then stays zero). Native
poll entries with no requested bits remain eligible for unconditional HUP/ERR.
Unrequested ordinary input/output readiness is still filtered by poll.

After polling, delivery starts at the instance's current cursor under the
lifecycle mutex and advances past each returned event, wrapping at 64 slots.
Reading the cursor after reacquiring the lock preserves progress made by an
overlapping wait. Stale or empty results do not advance it. Instance identity,
watch revision and native binding validation still suppress removed/replaced
snapshots. Slot deletion/reuse does not resurrect old event data. This provides
rotation for stable ready sets, not thread-scheduling fairness under arbitrary
mutation or real-time guarantees. Deadline and 10 ms synthetic polling limits
remain unchanged; no kernel/syscall ABI change is needed.

The positive host gate now has twenty-nine modes. Two former failures become
regressions, with added 64-slot mixed native/eventfd rotation through aliases,
20 batches of seven events crossing the wrap boundary, HUP/ERR-only masks,
unrequested-read filtering and deterministic nested wait/MOD/slot-reuse cases.
The guest `epoll-rotation` uses real pipe aliases and eventfds at full watch
capacity, repeats the same rotation batches, then closes pipe peers to test HUP
and ERR with empty masks and explicit ERR interest. It is included in `all`.

Initial integration retained two obsolete fixture assumptions: skipping native
poll once a synthetic event is ready, and fixed ordering after adding a watch.
Assertions now require nonblocking native polling and the intended rotation.
The first control fixture also incorrectly treated aliases of one object as
independently ready; it now uses distinct pipes. Those failed host receipts are
preserved as `host-first` and `host-second`, not counted as passing evidence.

## Historical continued audit: one-shot rearming and native priority mapping — corrected

`epoll-oneshot` registers a ready eventfd with `EPOLLONESHOT`, receives it once,
then receives it again without MOD/rearming. The assertion expects zero and
fails. The flag is stored but not enforced. `epoll-priority` registers `EPOLLPRI`
and models native POLLPRI readiness; the adapter returns zero. Both conversion
helpers omit priority bits. This proves a mapping defect with modeled readiness,
not that ordinary pipes or any particular native device produce priority events.
The [epoll control contract](https://man7.org/linux/man-pages/man2/epoll_ctl.2.html)
describes one-shot disabling/rearming and priority notification. These two
explicit failures remain outside the passing gate. Next bounded scope is one-shot
state/rearming and priority-bit mapping; edge-triggered delivery, signal-mask
waits, blocking eventfd I/O and a kernel notification mechanism remain separate.


Final rotation/hangup validation: matched `all` build, twenty-nine positive host
modes and ASan/UBSan, targeted epoll-rotation VM, complete thread suite and full
libuv consumer VM pass. Four compiled negative controls break cursor advancement,
full snapshot scanning, empty-mask polling or current-cursor selection; each
fails its intended assertion. Five smoke-reader tests pass. Documentation checks
655 local links with zero errors; whitespace checks pass. Baseline failures,
initial fixture corrections and two new one-shot/priority failures are retained
under `build/review/epoll-rotation/` with source/build/log hashes. All disposable
VMs stopped. PR179 remains draft. Next bounded scope is one-shot disabling/rearm
and native priority-bit conversion; broader event modes, signal masks and blocking
eventfd I/O remain separate. No physical acceptance or Actions setting changed.


## One-shot delivery/rearming and priority conversion correction

The `9a0f51a9` checkpoint retains its original two failed one-shot/priority probes.
Each registration now has private disabled state. A delivered EPOLLONESHOT entry
is disabled while holding the lifecycle mutex; entries excluded by `maxevents`
remain armed. Disabled registrations are omitted from snapshots and rechecked at
delivery, including HUP/ERR, so overlapping waits cannot return the same armed
revision twice and disabled ready sources cannot cause a busy poll loop. Aliases
of the epoll instance share this state; separate registrations of duplicated
watched descriptors remain independently armed.

Successful MOD updates mask/data and revision before rearming, even when supplied
values are unchanged. Failed control calls do not rearm. Old snapshots fail the
revision check and cannot consume a new rearm. Removing EPOLLONESHOT via MOD
restores repeated level delivery. DEL, final close and slot reuse retain the
existing ownership cleanup. Both priority conversion helpers now preserve
EPOLLPRI/POLLPRI; native priority-source availability is not added or accepted.
No public ABI or kernel change is needed.

Thirty-three positive host modes cover the original failures plus unchanged and
changed MOD, mode removal, failed controls, clipped buffers, alias independence,
finite disabled waits, close/reuse, nested delivery, rearm while parked, rearm
against an old active snapshot and HUP/ERR disabling. Priority tests model native
POLLPRI, verify unrequested priority is filtered, and combine IN/PRI/HUP with
one-shot. They do not claim a real pipe or a driver produces priority events.
The guest `epoll-oneshot` exercises public API rearming/mask changes and eight
rounds of four competing waiters on one persistent pipe source via epoll aliases:
one delivery per rearm, with other waits timing out. It also verifies a disabled
HUP stays suppressed until MOD. The full suite requires its completion marker.

## Historical continued audit: EPOLLET silently treated as level-triggered — corrected by refusal

`epoll-edge` registers a readable eventfd with EPOLLIN|EPOLLET. Two consecutive
zero-timeout waits return the unchanged readiness. The second assertion expects
zero and fails: the flag is stored but no transition handling exists. The
[epoll edge-triggered contract](https://man7.org/linux/man-pages/man7/epoll.7.html)
differs from repeatedly reporting a persistent level. This directed failure
remains outside the positive gate. Next bounded scope is defining an honest
edge-mode contract: actual transition support or explicit refusal, rather than
silently accepting the flag. The current 10 ms sampling adapter is not a kernel
notification queue; unseen readiness transitions need consideration before
claiming edge support. Signal-mask waits and blocking eventfd I/O remain separate.


Final one-shot/priority validation: matched `all` build, thirty-three positive
host modes and ASan/UBSan, targeted epoll-oneshot VM, complete thread suite and
full libuv consumer VM pass. Six compiled negative controls break disarming,
MOD rearming, disabled snapshot/delivery checks or either priority conversion;
each fails its intended assertion. Five smoke-reader tests pass. Documentation
checks 655 local links with zero errors; whitespace checks pass. Baseline failures
and the new EPOLLET failure remain separate under `build/review/epoll-oneshot/`,
with source/build/log hashes. All disposable VMs stopped. PR179 remains draft.
Next bounded scope is an honest edge-mode contract; blocking eventfd I/O, signal
masks, kernel notifications and native priority-source support remain separate.
No public ABI, physical acceptance or Actions setting changed.

## Edge-mode correction and continued audit

The baseline at `aba0999a` reproduces silent level delivery for EPOLLET. ADD/MOD
now reject that flag with EOPNOTSUPP before identity allocation or watch mutation.
The existing poll snapshots cannot observe transitions between samples; tracking
only the last returned level would still lose edges. DEL continues to ignore its
event argument. This is explicit unsupported behavior, not edge implementation.

The new host/guest regression checks both a ready native pipe and an eventfd,
seventy rejected additions, successful subsequent level registration, preservation
of old mask/data after rejected MOD, unchanged disabled one-shot state, successful
later rearm and DEL with an edge-bearing event argument. Default coverage is now
34 host modes and seven event-adapter guest groups. The original failing probe
is preserved under `build/review/epoll-edge/baseline/` and in the prior checkpoint.

Continued source review finds epoll_pwait explicitly discards sigmask. Nonnull
masks therefore cannot supply temporary-mask/atomic-wait semantics. This remains
a known gap. The existing ppoll/pselect mask-entry path bypasses handler dispatch
and relies on cooperative delivery and kernel pending checks; that provides a
concrete starting point for the next pass, including immediate-ready epoll paths.
No new runtime signal-mask evidence is claimed here. See the
[current audit inventory](audit-progress-2026-10-26.md) for the prioritized queue
and broader OS coverage limits. The AHCI report's stale draft-PR wording is also
corrected to match merged PR178.

Final edge-contract validation: all 34 host modes pass normally and with
ASan/UBSan. Four compiled negative controls fail the intended refusal or
state-preservation assertions. Matched `all` build, targeted epoll-edge VM,
complete thread suite and full libuv consumer VM pass sequentially. Five
smoke-reader tests pass; docs check 664 local links with zero errors and
whitespace checks pass. Baseline failure, source/build hashes and logs are
retained under `build/review/epoll-edge/`. All disposable guests stopped.
The current audit inventory separates completed bounded work from the event
adapter queue and broader OS reviews. Next scope is epoll_pwait mask semantics;
edge delivery remains explicitly unsupported. PR179 stays draft; no physical
acceptance, public ABI or Actions setting changed.

## Final bounded audit: masks, blocking I/O, polling and ownership

Target: finish the process-local adapter contract and merge PR179 after required
host/sanitizer, matched-build, thread and libuv gates. General Linux parity,
actual edge notifications and cross-process adapter sharing are optional future
capabilities, not this release target. Native hardware and Actions are unchanged.

The pre-fix host receipt at c98dcded fails five directed mask/blocking probes.
The following source changes close the remaining supported-contract defects:

- epoll_pwait installs/restores a temporary kernel mask without sigprocmask's
  early handler checkpoint. Pending signals are accounted for even when an
  eventfd is immediately ready. Restoration preserves errors and applies on every
  return path. Poll supplies cooperative dispatch plus kernel pending checks
  before blocking, as in ppoll/pselect. An asynchronous signal-frame ABI would
  require revisiting this implementation. Reference: [epoll_pwait](https://man7.org/linux/man-pages/man2/epoll_wait.2.html).
- Blocking counter I/O releases the lifecycle lock while waiting, retains a
  generation across retries and snapshots the write value. Reads wait for a
  count/token and writes for full requested capacity. Raw kernel poll lets the
  I/O path honor SA_RESTART rather than losing handler flags inside public poll.
  Other interruption returns EINTR; close/reuse returns EBADF; F_SETFL is rechecked.
  F_GETFL reports O_RDWR. Reference: [eventfd](https://man7.org/linux/man-pages/man2/eventfd.2.html).
- Public poll/ppoll recognizes eventfd readiness before and after native waits,
  clips synthetic polling slices to 10 ms and preserves one monotonic timeout.
  Snapshot generations reject replacement readiness with POLLNVAL. Pure native
  polling retains its previous path. Eventfd numbers exceed FD_SETSIZE; use poll
  or epoll rather than select/pselect. No public fd_set ABI was enlarged.
- Normal-data aliases preserve requested result bits. Unsupported band, half-close,
  message and edge requests fail with EOPNOTSUPP; unknown bits fail with EINVAL.
  Invalid epoll/target descriptors report EBADF; valid non-epoll descriptors report
  EINVAL. Null event buffers retain the profile's EINVAL convention. Existing
  registration, absence and duplicate errors preserve watch state.
- Adapter-handle SCM_RIGHTS export is refused. Exec/spawn inheritance requires
  every adapter handle to be close-on-exec; live adapter fork remains refused.
  Pseudo-socket registration is refused until its transitions share lifecycle
  serialization. Native socketpair data and hangup remain supported. These guards
  do not implement global alias tracking for native descriptors passed via rights.

The final source pass checks 8-instance/64-watch/16-counter capacity, alias cleanup,
revision/one-shot ownership, zero/finite/infinite waits, flags/error preservation,
counter arithmetic and selected process/rights boundaries. Forty-six host modes
compile actual adapter, public close/dup/fcntl and poll code; eight guest groups
cover public runtime behavior. Broader networking, raw syscalls, arbitrary process
inheritance races and Linux conformance remain outside the supported profile.

Validation and delivery disposition are recorded below.

Retained diagnostic evidence: the first guest timed out after entering the new
test. Phase markers narrowed the repeat to blocking signal handling. SYS_POLL
returns zero for pending signals; the first implementation incorrectly expected
SRV_ERR_INTR. The corrected path obtains both dispatch count and SA_RESTART
status after every raw wait. The host model now reproduces the real zero-result
case, including nonrestartable interruption and a restart followed by a post.
The two timeout logs, original candidate source/test and manifest remain under
`build/review/event-adapter-closeout/`; they are not passing results. A subsequent
compile caught a helper inserted before its static implementation; its placement
was corrected before rebuilding. Spawn file actions also refuse duplicating a
CLOEXEC adapter handle into a child, preventing that inheritance escape.

## Closeout validation and delivery

The bounded process-local event-adapter audit is complete. All 46 host modes pass
normally and under ASan/UBSan. Ten compiled negative controls fail their intended
mask entry/restoration, blocking retry/generation, signal dispatch/restart, poll
identity/immediacy, normal-event-bit or export-guard assertions. Native poll/select
host regressions and five smoke-reader tests pass. The matched x86_64 build,
corrected targeted event-adapter-final guest, complete thread suite (all eight
event-adapter groups) and full libuv consumer suite pass sequentially. Docs check
664 local links with zero errors; whitespace checks pass. No unresolved confirmed
defect remains within this selected contract. Failed diagnostic runs remain
separate evidence, not passes.

Source/build/log hashes and the validation receipt are retained under
`build/review/event-adapter-closeout/`. All disposable guests stopped. Delivery is
[PR179](https://github.com/PaulMDemers/SrvrOS/pull/179), whose GitHub record is the
source of the final merge state/SHA. The user authorized merge after these gates;
PR19 remains separate Wi-Fi work. No kernel/syscall ABI, native-hardware acceptance
or Actions setting changed. Broader filesystem/network/process/security reviews
and optional adapter capabilities remain in the current audit inventory.
