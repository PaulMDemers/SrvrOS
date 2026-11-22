# Air6,1 screenshot code audit

Audit date: 2026-11-10. Source `486b6d2a`, ports `39ad9d7`, after PR453
collected the native `eb3a60e0fbc0b436` baseline. Root and three subagents audited
transport/readiness, storage/durability and lifecycle/coverage independently.
This audit changes no production code and arms no reboot. PR301 scanout,
PR19 networking and PR379 WebCore remain separate. PR301's unpublished
`process.c` changes concern lifetime-owner loss, not these content/readiness
functions; inspect its current diff again before a shared implementation.

The [native result](machines/macbookair6-1.md#native-capture-transfer-baseline-2026-11-09)
passes responsiveness and integrity checks, while screenshot completion remains
slow. This table joins each capture's own records; nested intervals must not be
added together or mixed with unrelated maxima.

| Measured interval | Capture1 | Capture2 | Capture3 |
| --- | ---: | ---: | ---: |
| Complete capture | 18.841s | 25.987s | 17.580s |
| Worker allocation window | 84ms | 902ms | 162ms |
| Reception after allocation | 5.420s | 5.828s | 5.643s |
| Sender write-attempt span | 5.503s | 6.747s | 5.770s |
| Inside sender write syscalls | 8.552ms | 9.123ms | 8.707ms |
| Gaps between sender attempts | 5.494s | 6.738s | 5.762s |
| Worker WRITE phase | 9.265s | 15.893s | 8.249s |
| Raw file-write admission gate | 8.853s | 15.488s | 7.962s |
| Gate share of raw file-write calls | 96.23% | 97.93% | 96.63% |
| Final response production to UI observation | 511ms | 537ms | 840ms |

Allocation windows include progress/READY emission and scheduling, not only mmap.
Sender gaps include UI cadence, rendering, scheduling and backpressure. Gate
READY time can include deliberate exclusion while another process owns I/O.
These observations establish neither pure CPU time nor exact hardware latency.

## Confirmed hotspots and dependencies

### 1. UI-paced pixel transfer

[desk_capture_job.c](../userspace/lib/src/desk_capture_job.c), lines85–169,
admits at most four4096-byte writes and one32-byte response read per invocation.
[deskd.c](../userspace/deskd/deskd.c), lines3137 and3315–3325, calls it after
rendering and selects a one-tick sleep while capture is busy (line1984).
The Air input requires1026 successful writes and at least257 sending turns.
Every native capture exhausted256 budgets and encountered225/322/252 EAGAINs.

This is a concrete pacing constraint and the strongest isolated transport target.
It is not evidence that all five to seven seconds are sleep. Keep admission
bounds, pipe capacity and immutable pixels; decouple admissions from UI turns
using an exclusive owner and a proved readiness contract.

### 2. Poll readiness is still gated behind storage

[shotworker.c](../userspace/shotworker/shotworker.c), lines14–20, waits with
ordinary `srv_poll` after EAGAIN. In
[syscall.c](../kernel/src/arch/x86_64/syscall.c), lines2098–2102, every nonconsole
poll enters `scheduler_io_wait()` before checking readiness, including simple
pipes. Bounded nonblocking pipe read/write paths already have narrower safe
admission at lines486–503 and761–779.

A new thread using ordinary poll would retain this storage dependency. Existing
GUI_WAIT_IO is client-oriented, not a compositor wait; the file-watch helpers in
[process.c](../kernel/src/process.c), lines6606–6627, deliberately defer during
owned I/O. Do not bypass the general gate or grant arbitrary eligibility.
A narrow metadata-only pipe wait needs immutable copied requests, endpoint and
descriptor-generation validation, revalidation after wake, atomic readiness/
enrollment, safe HUP/EOF/error handling and the original gated fallback.
Its contribution to the observed native gaps is not separately measured.

### 3. Global sync repeatedly commits a growing screenshot

[io_stress.c](../userspace/playwav/io_stress.c), line45, calls global sync in
every256KiB burst. [process.c](../kernel/src/process.c), lines5883–5897, visits
each dirty shared content once, including a growing screenshot when present.
There is no duplicate-descriptor walk to eliminate. `content_flush`, lines4795–
4805, passes its full current buffer to the real exFAT writer.

[exfat.c](../kernel/src/fs/exfat.c), lines1980–2053, proves the old media prefix,
writes the appended suffix and publishes allocation/length with barriers.
Sidecar persistence at lines1637–1707 serializes mount metadata. Repeated global
flushes can therefore amplify prefix scans, append transactions and metadata
work during a single save. This mechanism is confirmed by code; selected inode0
records alone cannot identify the screenshot path or writer.

Latest screenshot raw file-write handlers take only20.8–21.3ms total per capture.
Most write-call time is admission waiting, not BMP copying or buffered insertion.
The slowest capture's worst gate is3.368264s; its longest matched owner is the
disk worker's SYS_SYNC for3.303471s. Its3.303981s READY interval and1119 exclusions
must not be called an unexplained runnable-thread scheduler stall.
Earlier same-command native evidence also found long validated CSW waits before
software event observation. Do not remove metadata, barriers, prefix proofs or
BOT status checks to mask the cost.

The existing streaming API is not a drop-in replacement: the captured USB has
128KiB clusters, while [exfat_stream.inc](../kernel/src/fs/exfat_stream.inc),
lines98/115, rejects clusters above its64KiB chunk. Its append path also performs
metadata/barrier work each chunk. Do not widen that gate or assume streaming
would reduce transactions.

### 4. Completion and retirement depend on unrelated work

Every progress frame returns immediately from `desk_capture_job_poll_transfer`.
The207 native progress frames plus READY/final are about6.7KiB, below the64KiB
pipe capacity. Ordinary progress-pipe fullness cannot explain the8–16-second
WRITE phase. Queued frames and UI scheduling can delay final observation;
current records do not reveal queue depth or isolate their respective shares.

[lifecycle_worker.h](../userspace/deskd/lifecycle_worker.h), lines184–245,
processes blocking app waits before capture preparation/cleanup, then allocations
and execs. An unrelated operation can delay capture work or its next retirement
retry. Moving transport onto this shared worker would inherit that coupling.
The release/acquire resource handoffs at lines113–156 appear exclusive; the audit
found no demonstrated double-free or concurrent mutation there.

Session exit at `deskd.c` lines2841–2848 correctly waits for capture preparation,
saving and retirement. A stalled owner can keep apps/exit pending while input
and menus continue. This is a credible architectural failure mode; no new
reproduction of the historical interactive failure was established by this audit.
Keep the retirement predicate and remove the coupling rather than exiting early.

## Additional correctness and robustness concerns

### Fresh-media verification can be masked by retained shared content

This is the highest-priority correctness finding, separate from latency.
[save.c](../userspace/shotworker/save.c), lines9–22, verifies each path by opening
and reading it. Read-only open in [process.c](../kernel/src/process.c),
lines5362–5364, obtains fresh media bytes through VFS. However, `content_acquire`
at lines4778–4782 discards that snapshot if another descriptor retains the same
path's shared content. Subsequent reads return the shared RAM bytes. Rename at
lines5314–5322 preserves that object and updates its path.

An isolated host reproduction using real process/exFAT/VFS code confirms:

1. Write/flush expected bytes, retaining a reader across writer close.
2. Flip an actual payload byte in a disposable128KiB-cluster image.
3. Confirm a fresh `vfs_read_all` sees the corrupted byte.
4. Run the exact read-only-open core (`vfs_read_all` plus `process_file_alloc`):
   `process_file_read` instead returns the original correct RAM bytes.
5. Repeat after actual `process_rename_file`: the renamed final path also masks
   corruption. Without the retained reader, or after its last reference closes,
   the new read detects corruption.

All three host cases reproduce their expected outcomes. This demonstrates the
primitive underlying screenshot verification, not the complete `capture_save`
wrapper, syscall dispatch or native scheduling. It does not establish actual
corruption or a false-success event in any accepted native run. It does show
that two ordinary reopen/read passes are not an unconditional fresh-media proof
when another reader keeps shared content alive. The normal exclusive-temp path
without another opener releases its content before verification.

Preserve ordinary POSIX shared-content semantics. Add an explicit fresh-media
verification contract and a regression running the complete save wrapper with
retained readers and injected corruption at both verification boundaries.
Do not force all readers onto stale media, evict live content or omit the reads.
Keep rename identity, size/type checks, durability and failure preservation.
The current POSIX-host save fixture cannot expose this SrvrOS content-sharing
behavior; the existing real process/exFAT fixture does not inject this fault.

The ignored reproduction directory is
`build/review/screenshot-readback-audit-20261110`, with C sources, replay script,
compiler command, logs and media hashes. Disposable images are deleted after
each replay; sanitized findings and a hash-verified independent backup remain.

### Other findings

- **Scaled capture repeats work on the UI.**
  [display_settings.h](../userspace/deskd/display_settings.h), lines206–219,
  allocates/resamples/frees a full logical image while `capture_pending`, even
  when `capture_tick` only queues preparation and does not consume pixels. It
  repeats when preparation completes, or on failed queue retries. This is
  confirmed redundant work and a blocking-allocation risk. Native Air scale1
  does not exercise it. Defer conversion until a prepared snapshot can consume
  it, with a scale2 delayed-preparation regression.
- **Screenshot child exit status is discarded.** `reap` in
  [desk_capture_job.c](../userspace/lib/src/desk_capture_job.c), lines6–9, clears
  the PID without retaining/checking status. A valid final response currently
  follows full save verification and worker unmaps, so byte proof remains
  separate. The workload's zero worker status is not an independent assertion
  of every screenshot child's exit status. Retain the status and test a valid
  final response followed by abnormal exit.
- **Memory amplification is real but not the main measured delay.**
  A4196406-byte BMP crosses4MiB and doubles the buffered kernel file capacity to
  8MiB (`process.c` lines5233–5244). UI snapshot, worker pixels/BMP and file buffer
  account for roughly20MiB before allocator/readback overhead. Eight growths
  cost about20ms in each latest native capture, so this is a memory-pressure
  concern, not an explanation for15 seconds of WRITE time.
- **Failed temporary cleanup has no separate receipt.** `save.c` lines70–74
  ignores the result of `unlink(temporary)` on a failed save. The save still
  returns failure and uncertain published destinations are preserved, so this
  is a diagnostics gap about possible leftover files, not false success.

## Regression requirements before another native candidate

Current host coverage is split: [test_capture_save.c](../tools/test_capture_save.c)
uses host POSIX file semantics; [test_capture_pacing.c](../tools/test_capture_pacing.c)
models bounded transport and accepted persistence; the desktop fixture mocks
preparation/cleanup. These are valuable, but do not jointly exercise real job
pipes, actual kernel poll admission, unrelated lifecycle blocking and session exit.

1. Exercise a pipe-only metadata wait under foreign and same-process I/O:
   readiness between check/enrollment, descriptor replacement/close, EINTR,
   HUP/EOF/EPIPE, mixed/unsupported descriptors and default gated refusal.
2. Run actual prepare/immutable snapshot/transport/cleanup ownership with an
   unrelated lifecycle operation held pending. Test exit/cancel before pixels,
   midway through send, after accepted work, after buffered final/child exit and
   during retirement. Prove exactly-once FD/mapping/PID release and publication.
3. Retain four4096-byte writes and one response read per admission, pipe capacity,
   ordinary protocol compatibility, all partial-I/O cases and full-size pixels.
   Use a controlled UI delay to show causal transport decoupling, rather than
   declaring a faster cold boot an optimization.
4. Cover scale2 delayed preparation and abnormal screenshot child exit. Keep
   accepted-save completion after reader loss and never report success early.
5. Keep full temporary/final byte checks, checked rename, both global barriers,
   fsync/close errors, corruption/failure retention and offline allocation proof.

Investigate verification correctness before optimizing transport, then handle
storage amplification separately. Keep500ms loop/10s lifecycle, the original
1800s PCM plus60s tail,1% volume, three exact BMPs, disk receipts and every
ownership/watchdog/integrity check. The intermittent battery cause stays open.
No next reboot is armed by this audit.
