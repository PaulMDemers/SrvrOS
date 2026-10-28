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

## Limits and next audit

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
