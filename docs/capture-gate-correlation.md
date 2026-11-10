# Exact screenshot gate-owner observation

A screenshot wait's longest overlapping I/O owner and global sync's longest
content commit can be different operations. Their PID alone does not join them.
The accepted Air6,1 a139 run exhibits that difference; see its
[native result](machines/macbookair6-1.md#native-reserved-append-result-2026-11-10).

When an observed wait selects an owner at outer I/O release, the scheduler
copies fixed live sync metadata under CPU0 interrupt exclusion. A parent joins
only when it is still active, has the owner's PID and began before that entire
owner interval. Active content joins only when its start also precedes the
entire owner interval. The reader never substitutes a completed maximum or a
previous call from the same PID. It performs no allocation, clock read,
transfer, logging, wait or admission change.

| State | Meaning |
| --- | --- |
| 0 | No sync observation was made, including a non-sync owner or disabled observer |
| 1 | Observer enabled, but no matching enclosing live sync |
| 2 | Enclosing live parent, without a content operation enclosing the entire scope |
| 3 | Enclosing live parent and active content, with copied cumulative I/O counters |

States0/1 are unavailable evidence, not proof of zero work. State2 can describe
device flushing or an outer scope that began before the active content. State3
identifies the current content slot/identity/inode/captured size and start time.
Its exclusive-role read/write/flush counters are cumulative from content start
through the sampled owner release; they can include an earlier I/O scope of
that same content. They do not claim this scope's exclusive callback costs,
completed writeback, durable bytes, physical media latency or CPU time.

The chosen owner and its observation are copied together into the wait's retained
maximum. Later owners, global maxima and process retirement cannot overwrite
that value. The original wall/blocked/READY/running partitions and all ordinary
versus proved-copy counts stay unchanged. Optional diagnostics confer no storage,
address-space or hardware authority.

`capture-write-gate-owner` version2 requires one exact snapshot/PID-bound
`capture-write-gate-sync` row and all six `capture-write-gate-sync-io` roles,
including explicit zeros. Missing/conflicting companions, failed/overflowing
counters, invalid clocks or parent/content containment are rejected. Version1
owner records retain their existing parser behavior. The new live sample does
not replace any existing full readback, checked rename, prefix proof, fsync,
global/device barrier, quarantine or final retirement evidence.

The extension is emitted only at the final checkpoint. Periodic snapshots keep
the version1 owner and their original byte format, without repeated correlation
rows competing with app receipts in the unchanged bounded log. The copied
observation remains in RAM through worker retirement. A halt before final can
therefore lack this extension; that is incomplete evidence, not a zero-work
observation. A production-formatter regression requires periodic output to equal
the legacy projection byte for byte.

`make test-io-scheduler test-sync-timing test-capture-write-timing` covers the
real scheduler with the real sync producer, actual producer/production formatter/
strict parser wiring, copied retention and adversarial joins. Controller and
clock backing in these host fixtures remain models. Loaded VM and native runs
must report their actual observation coverage; a non-sync maximum must not be
relabeled to manufacture a content match. Existing workload and timing limits
remain required. Attribution and any eventual latency improvement need fresh
evidence from the applicable machine.
