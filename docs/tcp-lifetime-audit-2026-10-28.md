# TCP close and process-exit audit — October 28, 2026

This bounded continuation of [draft PR182](https://github.com/PaulMDemers/SrvrOS/pull/182)
starts at `98fb56ff` on main `5a94d755` (through PR230). PR19 at `b8d63682`
contains separate Wi-Fi bring-up work, without `net.c` or process-lifecycle
changes. This checkpoint covers native TCP reference ownership and close state;
it does not extend libc pseudo-socket inheritance or claim complete TCP conformance.

## Confirmed defects and correction

- Creator exit discarded zero-reference connections immediately after final
  descriptor close, clearing retransmission history and a sent or deferred FIN.
  The last close now clears the creator pointer and transfers remaining work to
  the existing protocol timer/ACK paths. Both creator-first and inheritor-first
  close orders retain data and FIN until completion, error or bounded expiry.
- Cleanup discarded unaccepted connections owned by the creator even when a
  surviving process retained its listener. Completed pending requests and
  half-open connections now survive with the listener; stale creator pointers
  are cleared. Last listener close still retires unaccepted connections, and
  accepted connections retain their independent references.
- A FIN deferred behind a full byte window or transmit-record table had no normal
  close deadline. Deferral now starts the existing 200-tick close deadline, and
  timer expiry includes deferred FIN state. It does not depend on eventual peer
  ACKs or the much longer idle cleanup.
- An ACK that freed transmit space could trigger a failed FIN send whose return
  value was ignored, leaving pending close after the data records had gone.
  This failure now enters the existing reset/error path, wakes waiters and
  retires on its existing error deadline.
- Receipt of peer FIN could move FIN-WAIT-1 or LAST-ACK to TIME-WAIT without an
  acknowledgment of the local FIN, shortening retransmission lifetime. The
  local FIN acknowledgment now controls that transition. FIN-WAIT-1 plus
  `peer_closed` represents simultaneous close until the local FIN is ACKed;
  no new public state or ABI is introduced.

`tools/test_net_lifetime.c` includes production `net.c` with a captured Ethernet
sink, deterministic clock and process-identity tokens. It uses actual send,
reference, cleanup, pending-queue, accept, ACK/FIN and timer helpers. Eight modes
cover immediate/deferred FIN, both inherited exit orders, inherited completed and
half-open backlog, abort controls, deferred timeout, failed FIN and simultaneous
close. Original source `98fb56ff` fails seven modes; the abort controls pass.
This is a state/helper regression, not a packet-loss emulator or full process
scheduler test. Existing packet-level accept tests and guest network gates
complement it.

The first fixture rejected the normal post-send scheduler yield and assumed a
full-window send could not return short. Those harness assumptions were corrected
before collecting the original-source failures. Preliminary diagnostics are kept
separate from defect evidence under `build/review/tcp-exit/`.

## Verification

All eight new host modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan.
Original-source verification against `98fb56ff` fails seven modes, with abort
controls passing. Existing TCP accept, e1000 receive, network worker/device,
process output/lifetime and syscall I/O host gates pass. The packet accept
fixture's preexisting `kmalloc(uint64_t)` stub conflicted with the production
`size_t` declaration on macOS; matching the declaration fixes the harness build.
The failed build log is retained separately from final results.

The matched kernel/userspace build, six generator groups and 372-entry generated
image check pass. Six sequential disposable VMs pass: tcp-ready 11.17 s, tcp-stress 47.79 s, netabi 10.05 s, libuv 10.68 s, threads 40.00 s, lifetime 23.59 s.
TCP stress uses 12 clients. The lifetime gate repeats 256 sequential apps and
32 competing-waiter pairs over two iterations, followed by PTY checks. Current
source/artifact hashes, original failures, final host/sanitizer results and guest
receipts are retained under `build/review/tcp-exit/`. Earlier storage matrices
remain dated evidence; they were not rerun for this network-only production fix.
Documentation/link and whitespace checks pass. No native hardware acceptance,
Actions dispatch or PR merge occurred.

## Limits and next audit at the close checkpoint

Close releases the descriptor immediately. Retaining protocol state is bounded
best effort, not a receipt that the peer application consumed data. In-flight
connect aborts, reset connections and immediate FIN-send failure still retire;
there is no new linger option, retry syscall or native hardware qualification.

Follow-on inspection reaches ACK range validation and sequence wrap: record
retirement uses a signed sequence-distance comparison, while passive handshake
validation has its own stricter check. Directed coverage of established-state
ACKs beyond transmitted data, wraparound and stale/closing handles is next.
These cases have not been qualified by this checkpoint. Arbitrary packet
reordering, full TCP conformance and concurrent fork/exec/kill remain open.


## ACK admission and stale-handle checkpoint

Continue from `29df1192` on main `5a94d755`; PR19 remains at `b8d63682`
without network-core/process overlap. Seven added modes bring `test-net-lifetime`
to fifteen. Original source fails five new modes; sequence-wrap and slot-reuse/
reference-saturation controls pass without a runtime fix.

Three related contract defects are corrected:

- Established/closing-state ACKs beyond the next transmitted sequence could
  retire data or FIN records and admit payload/FIN from the same packet. These
  packets now receive an ACK and are dropped before peer-window, receive-buffer
  or close-state changes. Internal ACK helpers apply the same upper bound.
- Outbound SYN-ACK handling previously accepted arbitrary acknowledgment numbers.
  The stack sends no SYN payload, so completion now requires the exact next
  local sequence. Invalid SYN-ACKs receive a reset without changing handshake
  state, peer window or receive sequence. Tests include SYN sequence wrap to zero.
- A zero-reference TCP slot retained for retransmission remained accessible via
  its old application handle. It could be revived with `net_hold` or read using
  `net_read`. Application lookups, I/O (including zero length), metadata/error
  queries, shutdown and polling now require a live reference. Final close wakes
  read/write waiters, and their predicates recognize reference retirement.
  Packet/timer lookups still retain the closing slot until protocol completion.

The ACK admission rules follow the relevant
[RFC 9293 segment-processing rules](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.10.7):
a future ACK does not retire unsent data, and an active-open response must
acknowledge the SYN. This is not full RFC conformance. Sequence ordering retains
the existing signed modular comparison with a small bounded transmit window;
old, duplicate and partial ACKs do not prematurely retire whole records. The
wrap control checks distinct records straddling `UINT32_MAX`, followed by FIN.
Partial acknowledgment still retains the whole retransmission record.

New packet fixtures check invalid ACK plus payload/FIN, unchanged advertised
window and close state, subsequent valid completion, and invalid/valid SYN-ACKs.
Handle fixtures check stale hold/read/peek/write/shutdown/query/poll, waiter wake,
continued protocol retransmission, slot reuse with a distinct ID and saturation
of a live reference count. A direct network handle is a kernel backend token;
these checks do not expand libc pseudo-socket inheritance or claim a syscall
path that bypasses the process descriptor table.

Follow-on inspection reaches tuple-matched RST handling, receive-sequence
admission and retained descriptors after terminal timer expiry. Those paths need
directed invalid-RST, duplicate/overlapping-segment and delayed-error tests next.
Full identifier-space exhaustion, malicious traffic conformance and concurrent
fork/exec/kill remain unqualified.

### ACK/handle verification

All fifteen modes pass under x86_64/Rosetta and native ARM64 ASan/UBSan. The
seven new modes reproduce five failures on original source `29df1192`; wrap and
slot-reuse/saturation controls pass. Existing TCP accept, e1000 receive, network
worker/device, process output/lifetime and syscall I/O host checks pass. The
matched kernel/userspace build, six generator groups and recursive 372-entry
image check pass.

Seven sequential disposable VMs pass: tcp-ready 11.08 s, tcp-stress 47.38 s, netabi 10.29 s, libuv 10.21 s, threads 40.17 s, lifetime 23.51 s, outbound-local 10.58 s.
TCP stress uses twelve clients. The outbound gate uses a temporary localhost
HTTP server, verifying `/ack-check` and the exact response body; no public web
service is required. The helper's new `--port` default is 80, with invalid bounds
refused before VM launch. Documentation/link and whitespace checks pass.

Current logs, original failures, sanitizer results and tested source/artifact
hashes are retained under `build/review/tcp-ack/`. Earlier storage matrices are
dated evidence and were not rerun for this network-core change. No native
hardware qualification, Actions dispatch or PR merge occurred.


## Reset, receive admission and terminal-state checkpoint

Continue from `8aa5999a` on main `5a94d755`; separate Wi-Fi PR19 is `a0f4f951`
without net/process-core overlap. Eight additional modes bring `test-net-lifetime`
to 23. Seven fail on the original source; the duplicate/overlap receive control
passes without a runtime correction.

Confirmed corrections:

- RST no longer resets any connection matching only its endpoint tuple.
  During active open it must carry the ACK of the sent SYN. In synchronized
  states an exact next receive sequence resets, an in-window nonexact sequence
  receives a challenge ACK, and an out-of-window sequence is silently dropped.
  TIME-WAIT ignores RST. A connect refusal is reported before INPROGRESS.
- Receive sequence/ACK presence and available capacity are checked before ACK
  retirement, window changes, payload or FIN processing. Pure ACKs outside the
  receive window, payload without ACK, gaps and excess data no longer change
  state. The receiver remains in-order only: duplicates and overlap are ACKed
  and dropped, not trimmed or queued for reassembly. A FIN without payload at
  the next expected sequence can still close a zero-window receiver.
- Retransmitted passive SYN resends the existing SYN-ACK sequence without
  consuming a new sequence number or transmit-history entry. If the initial
  transport send failed, it retries the first send and records it normally.
  Unexpected SYN on
  an established/closing tuple receives an ACK without resetting receive state.
- Terminal timers previously erased live descriptor references, errors and
  buffered EOF data. Expiry now retires protocol ownership while preserving
  referenced identity and the first error, or successful EOF plus unread bytes.
  Timeouts report TIMEDOUT; existing reset/refusal errors remain intact. Failed
  connections release receive storage; orderly completion retains unread data.
  Final close frees the slot. TIME-WAIT pressure cannot reclaim a referenced
  connection. Readable EOF data also survives the TIME-WAIT interval itself.

The retained terminal state is `CLOSED` (numeric state 9 in `srv_net_info`), with
zero close deadline and no wire matching, retransmission or idle expiry. The
information structure is unchanged; netstat uses the supplied state name.
Descriptors can still query endpoint/error state and duplicate their remaining
reference, but cannot resume protocol activity. These retained slots consume the
existing bounded connection table until close; there is no unbounded graveyard.

Reset and sequence-admission behavior is grounded in
[RFC 9293 segment processing](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.10.7.4).
This adds no cryptographic authentication, ACK-throttling policy, receive
reassembly or complete RFC conformance. Packet tests use checksummed segments
through the production parser, including sequence wrap. Timer tests retain
multiple references, advance beyond the original deadline and verify errors,
EOF/read data, pressure refusal and final-close release.

The two earlier deferred-FIN modes now expect a retained application error after
expiry and explicitly close the descriptor. Their old expectation of immediate
slot deletion was the ownership defect corrected here; protocol-only close
modes still require timed reclamation.

Follow-on tracing reaches receive-buffer allocation failures, partial-ACK
retransmission and overlapping segments. Those paths need directed failure and
recovery coverage next. Arbitrary reordering, full identifier exhaustion and
concurrent fork/exec/kill remain unqualified.


A final candidate probe exposed a missing initial-send-refusal case in the SYN
correction: there is no original retransmission record when the first transport
send fails. The final implementation distinguishes that case from retransmission.
`candidate-syn-retry.log` retains the failing candidate; the first passing VM set
is kept in `vms-before-syn-retry/` and is not the final source qualification.

### Receive/terminal verification

All 23 modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan. Against
original source `8aa5999a`, seven of eight added modes fail and the duplicate/
overlap control passes. The candidate initial-SYN-send-refusal probe fails before
its correction and passes in the final repeat-SYN mode. Existing TCP accept,
e1000 receive, network worker/device, process output/lifetime and syscall I/O
host gates pass. The matched build, six generator groups and recursive
372-entry image validation pass.

Seven sequential disposable VMs pass against the final source: tcp-ready 10.92 s, tcp-stress 47.35 s, netabi 10.29 s, libuv 10.13 s, threads 39.87 s, lifetime 23.86 s, outbound-local 10.57 s.
TCP stress uses twelve clients; the outbound gate checks `/receive-check` and
its exact response from a temporary localhost HTTP server. The first candidate
VM set remains separate under `vms-before-syn-retry/`. Current final logs,
original/candidate failures and tested hashes are under `build/review/tcp-receive/`.
Documentation/link and whitespace checks pass. Earlier storage matrices remain
dated evidence; no native hardware acceptance, Actions dispatch or PR merge.
