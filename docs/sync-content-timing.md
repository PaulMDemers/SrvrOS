# Correlated sync content and capture completion evidence

This explicit timing-profile observer implements the first stage of the
[Air6,1 screenshot plan](macbookair6-1-screenshot-latency-plan.md).
It changes no persistence, admission, transport or device policy.

`sync_timing` retains per-PID worst global sync calls as before. Each call now
counts dirty shared content visits, attempted logical bytes and failures, and
retains its slowest content subcall with slot, never-reused boot identity,
optional inode, size, clocks and outcome. Content identities are internal
observation tokens, not handles or permissions. Ordinary no-profile acquisition
still assigns identities; only the explicit active sync records timing. Exhausted
identities invalidate diagnostics without refusing ordinary file operations.

The final workspace snapshot emits one `sync-content-summary` per retained PID,
then a `sync-content-selection` binding the longest file phase among those
retained calls with content visits. Ties follow record order. The selected call's
`sync-content-worst` and six `sync-content-io` rows describe its slowest content
subcall. This is not an independent global worst-content maximum. An inode or
slot alone cannot join different lifetimes. No paths, pointers or user data are
persisted. Failures stay failures; dirty/sticky-error/quarantine behavior is
unchanged. Missing or malformed joins cannot establish acceptance.

The six exclusive block-callback roles are:

| Role | Meaning |
| --- | --- |
| 0 | Root directory/flags/other metadata |
| 1 | Allocation/free, including nested sidecar allocation |
| 2 | Root payload |
| 3 | Root old-prefix payload proof |
| 4 | Nested sidecar work outside its equality proof and allocation/free |
| 5 | Nested sidecar equality proof |

Counters reuse existing exFAT cost clocks for attempted uncached reads, writes,
owner flushes and failures. Bytes are logical requests to block callbacks, not
physical USB bytes or savings. Cached reads do not count as media reads.
Pre-mutation reservation/scanning and other unobserved work remain in residual
content wall. The sum of observed callback wall must not exceed its content
span; no CPU-time or hardware-completion claim follows. The observer makes no
extra controller access or flush and logs only final bounded aggregates.

The desktop also retains its first three terminal captures in immutable operation
reports. `desktop-capture-history` and `desktop-capture-completion` include worker
identity, prepare/finish/retirement clocks and all12 worker phases. Ordinary
additional captures continue; drop/overflow diagnostics make a three-capture
comparison unavailable. The parser requires three distinct complete successful
lifetimes for native stress, or one for the short positive VM. Old current/slowest
records remain intact. These three complete histories provide a fresh baseline;
old maxima cannot reconstruct an unavailable median.

`tools/sync_content.py` joins to the complete final legacy sync snapshot;
`tools/capture_history.py` requires a complete final history. Wrong bindings,
conflicts, overflow, missing roles/completions, failed operations and invalid
clocks fail acceptance. Final rows must fit existing whole-record writes and
survive the unchanged64KiB tail alongside every previous receipt.

`make test-sync-content` exercises real buffered process writes into real exFAT
with a foreign sync between each64KiB write, unrelated dirty content and shared
writer/reader references. Observer-on/off deterministic controller callback
fingerprints and complete media bytes must agree. An injected write failure
retains the previous durable prefix, dirty contents, reader-only survivors and
quarantine. Host clocks are synthetic costs, not native timing acceptance.
Actual C capture-report output is parsed and deliberately corrupted; focused
observer/parser tests cover worst-call replacement and invalid evidence.
