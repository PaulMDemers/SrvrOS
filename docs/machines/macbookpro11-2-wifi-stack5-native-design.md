# Next native production-stack Wi-Fi gate

November 9, 2026. Unit-local design against the stack bridge under development
on device/macbookpro11-2/bringup after a12e2835. This document proposes work; it
does not enable a profile or record native production-stack acceptance.

The accepted v136 run uses runtime38f2d76a/release8529308170250844 and proves
the private encrypted diagnostic across real RX/TX boundaries. The subsequent
software fixture combines the actual encrypted owner, unrenamed production
net.c, registry and provider with an independent OpenSSL AP. Its DHCP/DNS/ARP,
ICMP,52 UDP echoes and3072-byte HTTP response are modeled USB/AP evidence. Its
process_current and scheduling/timer environment remain host fixtures. Native
ordinary networking, TCP/HTTP, connection lifecycle and daily-use reliability
remain open. This Edimax7392:a711 is5GHz-only; Demers2.4 requires other hardware.

## Recommended first native workload

Use the original authentication/four-way/CCMP owner and the new bounded provider
bridge. Register/attach that provider before net_init_bounded_probe selects and
seals the registry. Verify the selected provider and secured epoch explicitly.
Keep the captured Demers5,US161/20MHz gates and all parent/key/retirement proofs.

The smallest coherent bootstrap callback is:

1. Production DHCP DORA with the explicit one-Discover retry. Retry requires the
   exact first ciphertext's TX completion, an empty pending transmit owner and
   the existing current-epoch guards. Queue admission alone never permits retry.
2. Production DNS resolution of a fixed public name, including real route/ARP
   resolution. Accept only a valid production-parser answer; do not substitute a
   cached address, static neighbor or synthetic reply when the network fails.
3.64 sequential production ICMP requests/replies to the DHCP gateway, with unique
   sequences and bounded exact payload validation as described below. This
   replaces the software fixture's52 UDP/9000 echoes: no such native LAN service
   has been established. Drain each request through actual provider completion.
4. Require traffic after real RX and TX63-position fences, exact provider
   accounting, all loans released, no queued/pending TX, no active command/fence,
   original-key erasure/leave and complete endpoint/input/MAC/power/RF/parent
   restoration. Then verify detach/registry retirement. Retain backend tombstones
   until independently verified controller halt as before.

Typical TX use is4 handshake frames,2/3 DHCP frames,1/2 ARP requests,1 DNS query,
64 ICMP requests and leave. DNS retries, unsolicited ARP replies and other valid
traffic consume actual positions too. Before starting64 pings, require at least
64 data publications plus the reserved leave capacity; with the current bridge,
setup data sends must be at most11. Do not discard valid traffic, reset counters
or increase80TX/252RX/four-fence limits to fit a run. Insufficient remaining
capacity is a distinct bounded failure. Active RX still reserves8 cleanup slots.

This gate proves production DHCP/DNS/ARP/ICMP and ordinary authenticated receive
under the real owner. It does not prove TCP/HTTP or full connection lifecycle.
The adapter's generic complete flag is not a replacement for workload receipts.
There is no new native profile in the current software qualification block.

## Blocking and frozen-clock hazards

The bridge's15second/150000-service bound does not automatically unwind a caller
blocked inside net.c. Current DNS calls resolve_ipv4_mac, which can wait100ticks;
DNS then has three100tick attempts. net_ping also resolves ARP and waits ticks.
These loops call net_wait_tick. After provider fault they can continue polling
zero packets; a stopped tick clock can prevent timeout, and a scheduler wait
inside net_wait_tick cannot be rescued by an outer C-loop counter.

net_connect(...,true) is not fully nonblocking: route ARP resolution occurs
before its nonblock branch. net_respond_checked(...,true) also yields after a
successful chunk. net_read_checked(...,true) performs a bounded provider poll
before returning AGAIN, but its caller still needs ownership/clock checks.
Ordinary API behavior must remain unchanged when the explicit probe is absent.

## Explicit bounded API contract

Add a small opt-in guard shared by the new probe APIs, rather than a global
ambient switch affecting unrelated callers. Proposed names below are kernel
interfaces, not new syscalls or SDK ABI. The guard freezes a context/current
callback, selected provider identity/MAC, secured epoch and the absolute native
window deadline. Each API copies immutable callback identity locally and owns
start/last tick and monotonic samples plus a finite poll count. Nested ARP uses
the same call budget/deadline; it cannot restart either.

Use guarded variants of production DNS and ICMP with a common internal guarded
route resolver. Preserve existing public entry points through their historical
unguarded paths. Check guard identity, current owner, selected link and clocks:

- Before cache hits, publication, polling and success/output publication.
- After every external current/link/provider callback, including the final one.
- After a poll and before accepting a result that arrived during that poll.
- Before every transition/retry and before the call returns success.

Require nondecreasing ticks and monotonic time, remaining absolute budget and
finite iteration headroom. Cached replies do not bypass final ownership checks.
The native window owns the shared15second/150000-service ceiling; each nested
transaction also has a finite local cap. Clock rollback, frozen-clock poll
exhaustion, provider/link loss, callback replacement and process cancellation
must fail promptly without a subsequent frame publication or false success.

The guarded path must not call net_wait_tick, scheduler_wait, timer_delay_us or
another hidden blocking helper. Progress is a bounded net_device_poll(32,false)
followed at most by a CPU relax instruction; all loops have explicit finite
caps. A pure relax instruction does not wait for a timer. Keep these busy bounded
diagnostics separate from eventual ordinary worker-driven networking policy.
If a guarded API returns failure, the supervisor still closes publication and
applies the existing uncertain-DMA retention rules; API return is not retirement.

The existing net_dhcp_request_discover_retry already has current/retry guards,
two clocks and a finite loop, but still calls net_wait_tick. Qualify an explicit
bounded-wait form using the same production DHCP transaction/strict parser and
retry rules; do not quietly weaken the existing guard or change ordinary DHCP.

For ICMP, reserve one probe transaction before nested ARP and clear its waiting
state on every return. Existing ordinary ping verifies checksum, source, ID and
sequence, but does not compare the returned payload. Therefore either record
only that weaker matching claim, or preferably add a scoped guarded expectation
that also checks exact ICMP length and all expected payload bytes. The existing
40-byte ICMP message suffices; a bounded public256-byte pattern is also
possible. No packet bytes, addresses or payload fingerprints belong in receipts.
Do not describe ordinary net_ping success alone as256-byte exact echo evidence.

Directed tests must freeze both clocks inside nested ARP/DNS/ICMP, lose ownership
on the last callback, fault after poll, deliver a late valid result at deadline,
replace the callback, mutate payload with a valid checksum, exhaust provider
capacity and return from the exercise with queued/pending TX. Compare legacy
normal APIs/old v135/v136 outputs independently; new positive tests are not their
compatibility proof. Test every guarded failure for no later publication and
correct outer key erasure, retention or complete retirement as applicable.

## TCP/HTTP prerequisites and later bounded workload

The public endpoint was checked by the root agent before this design:
HTTP/1.1 httpbin.org:80, /base64/ followed by the Base64 encoding of3072 repeating
A-through-Z bytes. The path has4104 characters and the observed direct response
has Content-Length3072. This is endpoint preparation, not native acceptance
or a guarantee about future availability. A refusal, redirect, timeout or changed
framing must remain a failed run; do not replace it with a simulated response.

Native TCP cannot simply reuse the fixture's bootstrap callback. Production
alloc_client_connection requires a real process_current owner. The software
fixture supplies a nonnull stub; scheduler bootstrap has no user process. The
repository has process_lifetime_create/spawn/revoke/drain/release capabilities,
but no general process_kernel_scope binding API. Do not fabricate a process,
set scheduler user context by hand or permit ownerless TCP to make this pass.

Prefer a small real scoped workload process using normal network ownership,
with the kernel native owner supervising the exact lifetime. Reuse established
process-lifetime creation, quiet ELF spawn, revoke, bounded drain/status and
release. Exiting or timing out the process does not retire TCP or native DMA.
Preserve the same process/descriptor identity through nonblocking connect,
partial writes, reads and close; prove descendants and callbacks are drained.
If a different kernel network-owner capability is proposed, it needs its own
review and directed ownership/cleanup tests before use.

TCP timer progress is another independent prerequisite: net_timer_tick currently
returns unless worker_started, while net_init_bounded_probe deliberately starts
no worker. Merely calling it from the fixture or observing PIT ticks does not
prove native retransmission, zero-window, FIN or TIME_WAIT timers. Choose and
qualify exactly one mechanism before HTTP staging:

- Reuse the ordinary boot-scoped network worker, retaining its context through
  shutdown and proving that provider retirement drains references and prevents
  later callbacks/publication. Do not claim the worker stopped. If the trial
  instead needs worker destruction before return, add explicit stop, wake and
  acknowledgement ownership; the current worker has no bounded stop API.
- Or extract common TCP timer work behind a separate explicitly scoped probe
  timer owner, bracketed by verified begin/end identity and mutually exclusive
  with the ordinary worker. Normal worker_started behavior stays unchanged.
  The real IRQ timer must exercise this path; no duplicate manual timer owner.

Prefer the existing boot-scoped worker when its retained lifetime and registry
retirement contract suffice. A new scoped timer owner needs directed real-IRQ
VM evidence, duplicate/stale owner refusals and no publication after revocation.
Disable/revoke such a scoped timer before releasing provider/key/descriptor state.
Prove terminal TCP ownership separately from application net_close success.

For the later HTTP callback/process, require cached-route/nonblocking connect
semantics without hidden ARP waits: cache miss returns AGAIN before connection
allocation or SYN, with route resolution performed by the guarded resolver.
Provide a strictly nonblocking write operation that emits at most one available
production TCP chunk and returns partial progress without an internal yield or
wait. Checked nonblocking reads retain normal transfer/descriptor checks. The
supervisor supplies progress and checks the same absolute clock/owner budget
between every call; it does not restart that budget for each chunk.

Send the complete request through production segmentation and count actual
bytes accepted. Parse the response incrementally with a fixed bounded header
buffer, status200, exactly one Content-Length3072, no conflicting transfer
encoding and all3072 body bytes compared to the public expected pattern.
Reject unsupported framing/redirects instead of expanding HTTP scope. Preserve
header/body splits, partial writes, backpressure, retransmission and EOF/FIN
handling. Drain queued acknowledgements and terminal close before claiming
provider/connection retirement; no timeout is a completed close.

The4104-character path alone spans several TCP segments. Combining64 pings with
HTTP may exceed80 TX positions once request chunks, TCP ACK/FIN, retransmissions
and DHCP/DNS are counted. Use a measured smaller ping count, such as52, only
after a worst-case actual fixture projects the whole workload and cleanup
reserve. Preserve real post-boundary data evidence; never claim a ring boundary
from a software counter. Internet latency and packet loss may legitimately fail
the unchanged15second budget. Wider limits are a separate reviewed milestone.

## Receipts, profile and collection

Add a distinct opt-in stack5 recipe and exact token parser route only after the
guarded workload is qualified. Assign its capture version then. Require the
whole existing RF/handshake parent, private PMK mode0600, quiet probe startup and
automatic save/return. Missing parents, mixed historical modes, duplicate or
near-match tokens and wrong dispatch must refuse. Keep ordinary desktop dormant.

Show progress before entering the live MAC window and emit compact result
metadata after it closes. No console, file logging, endpoint names, packet/key
values or credentials inside the live window. Proposed workload receipts include
version/phase, complete/refusal category, DHCP completion/retry count, DNS success,
ARP resolution count, ping requested/matched/exact-body count, elapsed/poll
budgets and first failed step. Later HTTP adds request-byte count, status/framing
checks, exact body-byte count, mismatch count, descriptor/terminal-close proof and
timer/process lifetime retirement. An absent HTTP field cannot imply HTTP pass.

Retain existing raw command/context/dequeue/stop/frontier receipts and continuous
PN/actual TX accounting, selected authenticated RX around each boundary, provider
enqueued/submitted/completed/received/dropped/fault counters, parent restoration,
key erasure and ledger/provider retirement. The strict checker composes the
unchanged full parent with these new workload receipts; missing, duplicated,
reordered or inconsistent receipts refuse. Keep original v135/v136 checkers and
outputs separate. Data after both boundaries and exact workload bytes are
separate conditions from provider packet counts.

Measure actual-C maximum saved prefix against the2MiB logger cap with an explicit
reserve; do not increase or silently truncate capture. Qualify fatal sanitizers,
stack frames and callback structure, whole-prefix mutation refusals, real
filesystem write/read/remount, ordinary desktop, exact staged GPT and missing
workspace VMs. Exercise the actual main dispatch, not only a modeled callback.

Before native testing, capture current source/release/model/UUID0E53-5900,
installed Ubuntu/e47d recovery, complete private workspace hashes and raw metadata,
headroom and offline allocation. Keep known-good accepted payloads and sealed
evidence outside build. Under the user's existing authorization use cold shutdown
while Wi-Fi still requires the10second off-state unplug; workspace stays attached.
Save a durable run record, show guest progress and return automatically. Collect
the matching session first exactly once, then independently verify prior bytes,
metadata, allocation and recovery. Preserve protocol and storage failures
separately; never reseal an older failed/accepted capsule to change its verdict.

## Proposed file ownership and sequence

The root agent owns integration, Makefile/boot/main/staging/profile/checker
selection, exact shared builds/VMs, private credentials, install/USB/power and
independent collection. One protocol agent can own net.h/net.c guarded DNS/ARP/
ICMP and transaction failure tests. The native transport agent owns the bridge,
bounded workload callback and actual encrypted combined fixture. An evidence
agent can own new strict saved-log composition/mutation tests. These are bounded
assignments after agreement, not an automatic scheduler or cross-chat contact.
No concurrent shared builds/staging and no edits during frozen qualification.

First finish and independently qualify the guard/unwind contract, then the
bootstrap DHCP/DNS/64-ping exercise and exact capture/checker/profile. Qualify
real process ownership, timer/worker retirement and strictly nonblocking TCP APIs
in parallel without enabling them in that first native recipe. Stage HTTP only
after those prerequisites and its complete packet/log budget pass. Connection
selection/cancel/disconnect/reconnect, AP/adapter loss/reattach, UI/private
credentials and sustained reliability remain subsequent completion gates.
