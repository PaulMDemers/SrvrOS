# Validated command context for long private waits

The final-only `capture-owner-command` companion binds full BOT command
validation and block context to the exact status callback selected by the
[private-wait owner observer](commit-wait-isolation.md). It does not substitute
the unrelated global content/command maximum. This diagnostic follows the
Air6,1 native result where status callbacks cover 87.13% of one 2.894-second
private wait and the largest 16 KiB WRITE status lasts 352.020 ms.

## Ownership audit

`process_file_sync_all` visits each dirty live content once. Its writeback borrow
pins the captured data/prefix while independent reserved appends may extend a
bounded suffix. `content_flush_observed` retires that borrow only after the
backend returns. The filesystem holds `fs_operations` and nested I/O ownership
through mutation reservation, dirty marking, writes, durable publication and
clean-marker flush; mutation state, read-cache proofs and sidecar state remain
live inside the operation. Each USB BOT request also owns its bounce buffer and
busy gate through all data segments, status validation and quarantine decisions.

The current code therefore cannot safely drop shared ownership around an
in-flight status callback or between arbitrary transaction writes. Service
windows already preserve owner/address-space exclusion while servicing eligible
work. This audit adds evidence, not a speculative ownership handoff. Filesystem,
scheduler, xHCI, syscalls, commands, deadlines, fairness, flushes and every
fresh-media/CRC/allocation check remain unchanged. Long device completion can
still delay other storage users; no performance fix is claimed.

## Exact context and unavailable phases

The original timed status callback copies its existing start/end clocks into a
small command-local anchor. At the original terminal validation path the driver
forwards the command tag, opcode, LBA, block count, logical sector size and CDB
flags, plus the final validation result. The sink attaches this only while the
same waiter and actual owner are live, with equal tag, opcode and both status
anchors. A later larger status peak clears its previous command context. A
completion after wait return or owner release never backfills retained data.
Bulk success alone cannot pass full command validation.

Inside existing content tracing, the companion copies already-collected CBW,
data, CSW and checkpoint totals. It adds no clock samples or controller/ring
reads. Outside that trace it still records validated command/block context,
with phase availability false, role7 and zero uncollected phase counters. An
unavailable command has an explicit zero payload; it is not inferred from a PID,
nearby timestamp or previously selected maximum. LBA is a logical block address,
not a flash address or hardware arrival timestamp.

One version1 row accompanies the existing version2 wait/owner/status/optional
content rows at their same final timestamp. Each row stays below 1,024 bytes at
full uint64 width; the combined maximum is 3,803 bytes, below the existing 4 KiB
extension budget. Fixed eight-slot storage and one worst snapshot remain within
the existing bounds. Periodic bytes, 10-second cadence and 64 KiB tails are
unchanged. The new parser is separate; every original timing parser and stress
requirement remains independently mandatory.

`make test-commit-wait` runs the actual C producer and production formatter
through both strict parsers. It covers exact/mismatched tags/anchors, late and
replaced peaks, duplicate completion, failed full validation, full phases,
command-only/unavailable evidence and malformed clocks/bytes/counts. Real USB
fixtures retain all original malformed-wrapper/quarantine/ownership cases and
compare callback and clock counts with the new observer on/off. The automated
native trial retains 1,800 seconds at 1% volume, three screenshots and the
original 500 ms loop / 10-second app limits. Native command-context acceptance
remains pending the matching return.
