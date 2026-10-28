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


## Receive allocation, partial ACK and overlap checkpoint (2026-10-28)

Integrated main `7b0cc468` through PR233 in `b36bb072`; its AP diagnostics,
power admission and desktop lifecycle changes have no network-core overlap.
PR19 at `52e47a10` remains separate Wi-Fi work. The original network source is
unchanged from `903030ef` and saved in `build/review/tcp-retry/original-net.c`.

Three defects are reproduced and corrected:

- Receive-buffer allocation failure destroyed live outbound connections. Inbound
  failure additionally injected HTTP 503 bytes and FIN into arbitrary TCP streams.
  Clean refusal now retains descriptors, pending accept ownership and unread data,
  ACKs only the existing receive sequence, and permits later retransmission.
  Compaction may move unread bytes internally, but does not consume them. An
  incoming FIN is admitted only after its payload fits. The redundant data-time
  accept enqueue and application-specific error response are removed; passive
  handshakes already enqueue before data processing, with the existing queue-full
  reset/refusal path retained.
- Partial ACKs kept the full original transmit record, overstating outstanding
  bytes and retransmitting acknowledged prefixes. Records now advance to the ACK
  and shift the remaining payload; SYN sequence is consumed first and FIN remains
  until actually acknowledged. Byte-capacity progress wakes writers and retries
  a deferred FIN, even when the record slot itself remains occupied. Retransmit
  deadlines/retry limits and the transmitted high-water sequence are unchanged.
- An overlapping retransmission containing a duplicate prefix and new contiguous
  suffix was rejected forever if the sender kept retransmitting that segment.
  The receiver now skips the duplicate prefix and applies normal ACK/window
  admission to the suffix, preserving a new FIN after wholly duplicate payload.
  Full duplicates, gaps and oversized suffixes remain rejected. No reassembly
  queue or right-edge clipping is added.

Sequence-space and receive-overlap reasoning follows
[RFC 9293 section 3.4](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.4).
This is bounded recovery coverage, not complete TCP conformance. The fixture adds
seven modes and updates the earlier wrap-ACK and duplicate-receive expectations.
All seven additions and the updated duplicate case fail against original source;
close and future-ACK controls pass. The first test build needed a signed handle
comparison correction; the first candidate run also exposed an invalid fixture
query of an unaccepted handle. Neither fixture correction is runtime defect
evidence; the corrected allocation tests cover both initial allocation and growth
after unread-prefix compaction, for outbound/accepted/pending connections.

Follow-on source tracing reaches peer-window update ordering: synchronized
segments currently call `tcp_set_peer_window` without a last-window-update
sequence/ACK guard. Zero-window probes currently send an empty ACK at the next
transmit sequence and have a bounded timeout policy. Their interaction with
stale advertisements and a lost reopening update needs directed peer/timer
coverage next. No new conformance claim is made for those paths. Broader network
reordering, full identifier exhaustion and concurrent fork/exec/kill remain open.


### Allocation/partial-ACK verification

All 30 lifetime modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan.
Eight directed new/updated cases fail against original network source `903030ef`;
close and future-ACK controls pass. Related TCP accept, e1000 receive, network
worker/device, process output/lifetime and syscall I/O host gates pass. The newly
integrated power, AP receipt/owner and lifecycle-wait gates also pass. The matched
build, six generator groups and recursive 372-entry/16-directory image check pass.

Seven sequential disposable VMs pass: tcp-ready 11.21 s, tcp-stress 47.85 s, netabi 10.21 s, libuv 10.75 s, threads 31.07 s, lifetime 22.95 s, outbound-local 10.82 s.
TCP stress uses twelve clients; outbound HTTP verifies `/retry-check` and the
exact local-server body. Logs, original failures, candidate fixture correction,
source diff and source/artifact hashes are under `build/review/tcp-retry/`.
Documentation/link and whitespace checks pass. Earlier storage fault matrices
remain dated evidence; no physical adapter/native AP acceptance, Actions dispatch
or PR merge is claimed. PR182 remains draft.


## Peer-window ordering and persist checkpoint (2026-10-28)

Integrated main `45b8647a` through PR234 in `82aecaf5`; its two changes are
machine/parity evidence documents. PR19 `52e47a10` remains separate Wi-Fi work;
Air6,1 `c9639988` adds opt-in coordinated-C3 diagnostics beyond main, not included
in this host's qualification. Original network source remains `1d1e106a`, saved
under `build/review/tcp-window/original-net.c`.

Six new test modes reproduce defects; two timeout controls pass unchanged:

- Window advertisements were assigned unconditionally for synchronized packets.
  ACKs older than the unacknowledged send position, reordered segment sequences,
  and stale overlap packets could change write readiness/persist state. The
  stack now records the original wire sequence and ACK for the last update and
  admits new window values only with an in-range ACK and non-stale ordering.
  Equal sequence/ACK pairs remain admissible for legitimate read-driven window
  changes. Overlap trimming affects received data only, not update ordering.
- Invalid passive-open final ACKs changed window state before validation. Window,
  MAC and activity publication now follow successful handshake validation.
- Empty persist probes used the next send sequence and could be silently accepted
  after a lost reopening ACK, leaving a writable peer unreachable. With retained
  data the probe retransmits just its first unacknowledged byte; without data it
  uses an empty segment at one sequence before the unacknowledged position. No
  probe advances the local send sequence or inserts a transmit record. A probe's
  accepted byte is retired by the ordinary ACK path exactly once.
- A fixed total probe count aborted responsive zero-window peers; ordinary data
  retry exhaustion could abort them even earlier. Persist now owns retransmission
  while the local write side is open and the peer window is zero. Valid ordered
  advertisements clear unanswered attempts without resetting exponential backoff.
  Reopening clears persist and renews the ordinary retry budget. The silent-peer
  cap remains twelve unanswered attempts (expiry at the following deadline), and
  failed transport sends consume that bounded budget. Initial delay is 25 ticks,
  subsequent delays are 50/100/200 ticks, capped at 200. Close/shutdown bounds and
  the ordinary idle policy remain unchanged; responsive traffic refreshes activity.

Ordering and persist behavior are grounded in
[RFC 9293 segment processing](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.10.7.4)
and [zero-window probing](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.8.6.1).
The no-queued-data path is an old-sequence ACK probe; the API retains no new
application bytes on an AGAIN result. This is a bounded implementation, not
complete TCP conformance or a new configurable timeout/socket ABI.

The eight additions bring the fixture to 38 modes. Window tests cover stale ACK,
newer/older segment sequence, sequence wrap, old overlap carrying new data, and
invalid final-handshake ACK/sequence. A captured two-endpoint exchange drops a
window-reopening message and refuses a probe send, then checks exact recovery
with/without queued data and across wrap. Fifty responsive rounds exceed both
the old probe limit and idle interval, including a previously exhausted ordinary
retry budget. Silent-peer/transport-refusal and close controls preserve bounded
TIMEDOUT and final release. The first candidate build placed the persist helper
before its state predicate declaration; that compile failure is retained, and
the helper was moved before collecting passing runtime evidence.

Follow-on tracing moves outward from `handle_tcp` to IPv4 packet admission and
TCP options: the IPv4 handler validates length/checksum before protocol dispatch,
and TCP validates data offset/checksum before flags, but fragment semantics and
option/MSS negotiation need directed malformed-packet tests. No complete parser,
WAN reordering, congestion-control or native-adapter qualification is claimed.


### Peer-window/persist verification

All 38 lifetime modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan.
Six added modes fail against original network source `1d1e106a`; silent-peer and
close-deadline controls pass. Existing TCP accept, e1000 receive, network
worker/device, process output/lifetime and syscall I/O host gates pass. The
matched build, six generator groups and recursive 372-entry/16-directory image
check pass. The integrated PR234 baseline changes only device-evidence docs.

Seven sequential disposable VMs pass: tcp-ready 11.36 s, tcp-stress 47.87 s, netabi 10.5 s, libuv 10.77 s, threads 30.99 s, lifetime 22.75 s, outbound-local 10.84 s.
Stress uses twelve clients; outbound HTTP verifies `/window-check` and the exact
local-server body. Logs, original failures, the candidate compile correction,
source diff and tested source/artifact hashes are under `build/review/tcp-window/`.
Documentation/link and whitespace checks pass. Earlier storage matrices and
native-device acceptance remain dated evidence; no Actions dispatch, physical
install or PR merge occurred. PR182 remains draft.


## IPv4 admission and TCP options/MSS checkpoint (2026-10-28)

Integrated main `6cbfedab` through PR237 in `57d8f80a`, including quiescent-I/O
fairness, isolated coordinated-C3 diagnostics and device evidence. PR19 at
`7ac6b0e6` remains independent Wi-Fi work without net.c overlap. Air7,2
`4f1ca2a1` records its separately owned desktop-worker follow-up. Original network
source is `811c4685`, saved in `build/review/tcp-parser/original-net.c`.

Six directed modes reproduce these defects:

- IPv4 fragment offset/MF and reserved bits were ignored. A checksummed fragment
  could be treated as a complete TCP segment, including ACK, payload, FIN or RST.
  The IPv4 boundary now refuses those bits before TCP/UDP/ICMP dispatch, while
  accepting DF-only and ordinary unfragmented packets. This adds no reassembly.
- TCP trusted the header's option area without validating its entries. Malformed
  length fields reached connection mutation. The bounded option walker now handles
  EOL/NOP and skips well-formed unknown options, refusing missing, zero, one-byte
  or overrun lengths before ACK/window/reset/data/FIN changes. MSS requires length
  four; zero and duplicate SYN MSS offers are refused as unusable or ambiguous.
  Non-SYN MSS options cannot change the negotiated limit. Bytes after EOL remain
  ignored header padding.
- TCP ignored peer MSS and sent up to 1,400 bytes even when the offer was smaller
  or absent. Both opens now retain the validated offer, capped at 1,400, with the
  IPv4 default of 536 when absent. Readiness and transmit segmentation use that
  cap; the low-level transmitter also enforces it for retries. Every outgoing
  SYN/SYN-ACK, including retries, advertises local MSS 1,400 in a 24-byte TCP header.
  Options consume no sequence space. Duplicate SYNs retain the original peer limit.

Option and MSS behavior follows
[RFC 9293 options](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.1)
and [maximum segment size](https://www.rfc-editor.org/rfc/rfc9293.html#section-3.7.1).
No window scaling, SACK, timestamps, path-MTU discovery, congestion-control or
complete IP/TCP conformance is claimed. Refused fragments/options are silent
packet drops; no application error or artificial response is synthesized.

Eight additions bring the fixture to 46 modes. Six fail against original source;
existing boundary/checksum and well-formed-option behavior passes. Exact-size
truncated allocations, variable IP headers, invalid TCP data offsets and Ethernet
padding run through the IPv4 parser. Tests span malformed option lengths 0–255,
valid unknown options 2–40 bytes, unaligned MSS and active/passive/default limits
including MSS 1. Wire assertions check payload bounds, SYN option/checksum and
retransmission size. The full-window legacy fixture now handles short writes
under the 536-byte default. The first fixture link needed a console stub because
IPv4 dispatch retains the UDP diagnostic path; it is not runtime defect evidence.

Follow-on tracing reaches UDP/ICMP receive validation and endpoint ownership:
UDP has a four-datagram queue with oldest-entry replacement at capacity, and its
receive API has separate truncation and close behavior. ICMP echo request/reply
validation needs directed checksum/length coverage. These are the next bounded
questions, not completed datagram or ICMP acceptance. Full WAN fragmentation,
loss/reordering and physical adapter behavior remain outside this qualification.


### IPv4/TCP parser verification

All 46 lifetime modes pass on x86_64/Rosetta and under native ARM64 ASan/UBSan.
Six added modes fail on original network source `811c4685`; packet-boundary and
well-formed-option controls pass. TCP accept, e1000 receive, network worker/device,
process output/lifetime and syscall I/O host gates pass. The integrated baseline's
I/O scheduler/fairness, AP receipt/owner/C3 parser and lifecycle-wait gates pass.
The matched build, six generator groups and recursive 372-entry/16-directory
image check pass.

Eight sequential disposable VMs pass: tcp-ready 11.38 s, tcp-stress 47.84 s, netabi 10.43 s, libuv 10.84 s, threads 40.59 s, lifetime 22.72 s, web-segmented 19.34 s, outbound-local 11.1 s.
Stress uses twelve clients; web smoke includes a segmented static response;
outbound HTTP verifies `/parser-check` and the exact localhost-server body.
Logs, original failures, source diff and source/artifact hashes are retained
under `build/review/tcp-parser/`. Documentation/link and whitespace checks pass.
Earlier storage matrices and native-device acceptance remain dated evidence;
no Actions dispatch, physical install or PR merge occurred. PR182 remains draft.
