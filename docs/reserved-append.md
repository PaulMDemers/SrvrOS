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
mutation proof after classification, with interrupts excluded throughout.
Generic filesystem writes and typed signal backends receive no new admission.

Only exact EOF append qualifies. A conservative256KiB suffix window relative to
the active captured prefix, or last committed prefix, bounds the optimization.
A whole request that misses this window uses ordinary admission; this is not a
new short-write policy or a global flush-size/latency guarantee. Full originally
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
and references. Duplication, readers, flags, seeks, metadata/path changes,
overwrite and unsupported growth revoke eligibility. Retryable refusal and
sticky write failure remain distinct. No quarantine, prefix check, checked
rename, full fresh readback, fsync/global/device barrier or runtime limit is
removed.

Version2 capture-write records explicitly partition ordinary admission from
proved copies. Ordinary gate/owner records count only ordinary calls; fast
copies do not manufacture zero-valued scheduler observations. Version2 content
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
