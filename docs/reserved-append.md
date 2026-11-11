# Private reserved append and captured writeback prefixes

Screenshot saving can repeatedly wait for another process's global sync to
commit its growing temporary file. `SRV_F_RESERVE_APPEND_RAM` is an optional,
capacity-only `fcntl` hint for a newly created empty write-only exclusive file.
The userspace wrapper is `srv_file_reserve_append_ram(fd, capacity)`. Old kernels
and refused reservations retain ordinary writes. Reservation changes neither
visible length, cursor, dirty state nor media; the existing16MiB buffer bound
still applies.

The raw write entry may admit a copy of at most64KiB into already owned capacity
only when the process/image, nonexhausted FD generation, write description,
content, buffer, capacity and flags still match the lease. Both reference layers
must be private. The scheduler independently proves sole-main process/CR3 and
foreign-I/O eligibility. The actual helper rechecks the stronger existing
mutation proof after classification, with interrupts excluded throughout each
copy. Only the separately audited metadata prelude may park and retry.
Generic filesystem writes and typed signal backends receive no new admission.

Only exact EOF append qualifies. A conservative256KiB suffix window relative to
the active captured prefix, or last committed prefix, bounds the optimization.
A whole request that misses this window may park its metadata-only prelude
behind a foreign owner. Captured-prefix publication and retirement notify the
existing wait queue; each wake rechecks process/CR3/lease/FD/buffer and window
state before copying. Spurious notifications repark. No resolved descriptor,
content or reserved-buffer borrow survives a wait; saved user addresses are
validated again by the actual copy. A still-full window at ownership release, lost lease
or unsupported context retains ordinary admission. This is not a new short-write
policy or a global flush-size/latency guarantee. Full originally
requested user ranges are validated even when FSIZE permits a positive short
write. Zero-fit FSIZE requests retain ordinary signal behavior. A failed copy
cannot publish its hidden tail, cursor, length or dirty generation. Later growth
must overwrite or zero those bytes before exposure.

Writeback borrows an immutable pointer, length and generation. Concurrent
qualified copies can touch only its reserved suffix. Success commits that
captured length and clears dirty state only if the generation still matches;
otherwise the appended suffix remains dirty for a subsequent explicit flush.
No loop drains newly arriving writes indefinitely. Every required pre-entry
dirty prefix remains in global sync. Existing fully unwound return handoffs can
already run an overlapping writer before a sync returns to userspace, so bytes
visible at userspace return are not a newly promised durability boundary.

Borrow retirement, generation/dirty finalization and diagnostic publication
precede restoring the caller's interrupt state. Active borrows refuse moves,
freeing and conflicting direct mutations; BUSY close preserves the descriptor
and references. Last content release retains its reference through backend
service windows, free and slot reset, and restores interrupts only after the
slot is fully retired. A queued new allocation cannot be erased by resumed old
cleanup. Duplication, readers, flags, seeks, metadata/path changes,
overwrite and unsupported growth revoke eligibility. Retryable refusal and
sticky write failure remain distinct. No quarantine, prefix check, checked
rename, full fresh readback, fsync/global/device barrier or runtime limit is
removed.

Version2 capture-write records explicitly partition ordinary admission from
proved copies. Ordinary gate/owner records count only ordinary calls; fast
copies do not manufacture zero-valued scheduler observations. The actual copy
remains atomic; a queued copy's handler wall can include a notified eligibility
wait. Final-only `capture-append-wait` companions retain that wall, wake count
and ready-with-live-borrow coverage explicitly. The `wakes` field counts returned
wait attempts, not hardware interrupts. An eventual ordinary fallback
retains its real gate/handler records; its earlier private prelude remains in
the whole raw-call wall and companion, outside ordinary handler time. Version2 content
records require a bound `sync-content-prefix` companion proving generation,
captured/live/committed lengths, append counts and dirty-after state. Legacy
version1 records retain their existing strict validation. Host fixtures use
actual process/VFS/exFAT code, equal foreign sync/barrier schedules, fresh file
verification and complete allocation checks. Host source-I/O counts do not imply
native latency or physical-media timing.

Run `make test-reserved-append test-capture-write-timing test-sync-timing` for
ownership, copied-user, scheduler, IRQ-return, failure and production-format
regressions. The Air6,1 machine record owns exact-source validation and physical
acceptance; implementing this opt-in does not accept another machine's hardware.

## Queued full-window recheck

The old entry classifier required immediate window space. A writer whose window
was full became an ordinary unsafe waiter, so a later writeback snapshot could
open valid suffix space without making that waiter eligible. The new classifier
requires all stable lease/capacity/FD/process conditions and admits only the
bounded metadata/copy prelude. It never grants a copy beyond the existing window.

Publication and retirement of the real immutable borrow notify the existing I/O
wait queue. The new wait helper independently checks sole-main/CR3/raw-write and
foreign-owner/image separation. It also reserves a normal quiescent turn, so an
unrelated recurring writer cannot starve fallback. Notifications leave unsafe
threads excluded; no live filesystem transaction or DMA resource is released.
After each wake the prelude resolves all metadata anew. Pending cancellation,
revocation and failed content retain ordinary gated cancellation/error handling.

The regression reproduces the old full-window refusal followed by later captured
prefix publication, spurious notifications and four valid copies with an unchanged
prefix. A fifth still refuses. A separate real-scheduler fixture verifies actual
queue blocking/wake, preserved foreign ownership and continued unsafe-thread
exclusion. Copied-user tests check fresh mapping validation and cancellation after
waiting. `make test-append-wait` rejects malformed, missing, conflicting, unbounded
or unbound diagnostic companions. The notification/copy clocks are software wall,
not physical USB completion. Long device status waits remain a distinct cause;
this fixes avoidable exclusion while a new safe suffix is available, with native
latency and actual overlap coverage to be measured in the matched stress test.

Frozen source `b11db747` (runtime implementation `8462addd`, followed only by a
host formatter-test correction) passes matched ordinary/native/VM builds and
all239 core targets plus desktop/docs coverage. The initial full run passed233
targets before the formatter test; its correction strictly parses the new final
companion while retaining exact periodic byte equality. The corrected target,
remaining six core targets and desktop/docs then pass with identical production
sources. Initial failure evidence and explicit source equivalence are retained.

The populated400writeIOPS KVM audio/disk/apps/screenshot trial passes all original
contracts with complete saved lifecycle/app/service receipts. Four notified
retries become ready under a foreign owner and a live immutable borrow. The
139940us loop and1289420/1152100us launch/close maxima retain their original
limits. Exact native stage `bcd7246fbcc828f6` refuses foreign board ownership,
saves/unmounts and returns in its VM rehearsal. This accepts no native speed
gain or change to the device's physical status latency.
