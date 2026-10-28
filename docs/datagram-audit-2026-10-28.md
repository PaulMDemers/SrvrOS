# UDP/ICMP and datagram ownership audit — 2026-10-28

This bounded continuation of [draft PR182](https://github.com/PaulMDemers/SrvrOS/pull/182)
covers production IPv4 UDP/ICMP admission, queue pressure, libc datagram receive
reporting and native endpoint lifetime. It follows the
[TCP parser checkpoint](tcp-lifetime-audit-2026-10-28.md). Main `2c34a460` through
PR238 is integrated in `300cbcb7`, including the desktop lifecycle-worker fixes.
The pre-fix network and libc sources match checkpoint `950a4ee0`.

## Confirmed defects and corrections

| Trigger | Previous behavior | Corrected behavior |
| --- | --- | --- |
| ICMP echo with a corrupt checksum or nonzero code | Requests could emit replies; a code-nonzero reply could complete a pending ping | Verify the complete checksum before dispatch; echo request/reply require code zero before reply or pending-ping mutation |
| Final UDP close and slot reuse while a send waits for ARP | The resumed send used the replacement socket's source port | Re-find the original handle and bound port after resolution; refuse a retired endpoint before emitting UDP |
| Bind an already bound UDP endpoint | The port could change while queued data or an operation still belonged to the old binding | Refuse rebinding, including same-port and automatic-port requests; retain the original port and queue |
| Failed libc UDP bind | The cached port changed despite native failure | Publish the cached port only after successful native binding |
| Receive a datagram larger than the recvmsg iov capacity | Excess bytes disappeared without a truncation indication | Return the copied prefix and set output MSG_TRUNC |
| recvmsg with zero aggregate capacity | Returned without consuming the pending datagram | Consume one datagram; set MSG_TRUNC only when it was nonempty |
| Receive with a short source-address buffer | The address and full required length were not reported | Copy only the available prefix and report the full sockaddr_in size |

The checksum/code rules follow [RFC792](https://www.rfc-editor.org/rfc/rfc792.html).
UDP validation preserves the optional zero source port/checksum allowed by
[RFC768](https://www.rfc-editor.org/rfc/rfc768.html). Output MSG_TRUNC describes
lost trailing datagram bytes, as in the
[socket receive contract](https://man7.org/linux/man-pages/man2/recvmsg.2.html).
This adds the output flag only; input MSG_TRUNC is still rejected and UDP
MSG_PEEK remains unsupported.

The libc adapter receives into a bounded 1,472-byte temporary buffer before
scattering the copied prefix. The kernel and libc now share
`SRV_NET_UDP_PAYLOAD_MAX`; no syscall number, signature, layout or ABI version
changes. Allocation failure occurs before receive and leaves the datagram
available for retry. Empty datagrams are successful zero-byte receives, not EOF.
The existing four-entry queue drops its oldest entry on a fifth arrival; oversized
payloads are refused before eviction. There is no new loss counter or reliability
promise. Native handle references preserve queued data across creator cleanup;
this does not extend libc pseudo-socket fork/dup support.

## Regression evidence

`test-net-datagram` runs ten modes against the production network source:

- ICMP checksum, request code/minimum length and reply identity/code/checksum.
- UDP exact-size truncation allocations, lengths, checksum, empty datagrams,
  omitted source port/checksum, oversize refusal and IP trailing-byte isolation.
- Fifty queue wrap/pressure rounds, retained source ports/order and oversize
  non-eviction; short/zero receives discard exactly one datagram.
- ARP-wait final close/reuse and surviving-reference control; blocked receive
  final-close wake, stale handle refusal and isolated replacement queue.
- Both cleanup orders with held native references; immutable binding and released
  port reuse.

Five modes fail against original network source `950a4ee0`: `icmp-checksum`,
`icmp-code`, `icmp-reply`, `udp-send-reuse` and `udp-bind`. The other five are
passing controls, not claimed new defects.

`test-posix-datagram` runs six production libc modes: multi-iov truncation,
zero-capacity receive, short source address with canaries, empty datagram,
allocation failure/retry and failed-bind cache preservation. The first three and
bind-cache fail against original libc source `950a4ee0`; empty/OOM controls pass.
Host compatibility stubs, Darwin feature macros and byte-order macro handling
were corrected before recording those original-source results. Harness build
failures are not OS defect evidence.

All ten kernel and six libc modes pass under x86_64 Rosetta and native ARM64
ASan/UBSan. The separate 46-mode TCP lifetime suite and TCP accept, network
worker/device, process output/lifetime and syscall-I/O gates pass. Integrated
lifecycle-wait, capture and desktop-session gates pass. Capture-save needs
`-D_DARWIN_C_SOURCE` on this host; desktop-session additionally needs
`-Wno-array-bounds` for Clang's diagnostic about the unused inline-array arm of
the dynamic-registry macro. These compiler accommodations do not change guest
code or establish general host-suite portability.

The matched kernel/userspace build, six generator groups and recursive image
validation (372 entry sets in 16 directories) pass. Eight sequential disposable
VM gates pass: tcp-ready 10.88 s, tcp-stress 47.35 s, netabi 9.87 s, libuv 10.18 s, threads 30.22 s, lifetime 23.09 s, web-segmented 18.72 s, outbound-local 10.6 s.
TCP stress uses twelve clients. Netabi confirms the new
`netabi: udp truncation/empty/zero/address/queue ok` marker. Web smoke includes a
segmented static response; outbound HTTP verifies `/datagram-check` and the exact
localhost-server body. Documentation links and whitespace checks pass.

Tests used the uncommitted candidate on merge `300cbcb7`; the retained source
hashes are checked against the published commit, alongside matched image hashes.
Raw original failures, final host/sanitizer logs and disposable VM logs are under
`build/review/datagram/`. Earlier storage matrices are dated evidence, not newly
rerun by this network-only checkpoint. No Actions dispatch, native installation,
physical adapter acceptance or PR merge occurred; PR182 remains draft.

## Follow-on scope recorded at the datagram checkpoint

Follow-on tracing reaches shared ARP/DNS request state across blocking waits and
libc connected-UDP peer filtering and close/reuse. `resolve_ipv4_mac` and DNS
queries use global pending state; connected `recv` filters the peer separately
from the recvfrom/recvmsg path. These need directed overlap/filter tests next;
this checkpoint does not qualify them. UDP identifier exhaustion, arbitrary SMP
interleavings, ICMP error delivery/PMTU, full socket flag/errno parity and native
network adapters also remain outside this evidence. Existing fragmentation,
TCP reassembly and physical-device restrictions remain in the supported profile.


## Resolver and connected-UDP ownership checkpoint

Main `4ae360ef` through PR243 is integrated in `9f427171`. The Makefile merge
retains both independently appended test groups; no network source conflicted.
This pass uses `c43a1749` as the original network/libc source. Evidence is under
`build/review/query-ownership/`.

| Trigger | Confirmed behavior before the fix | Correction |
| --- | --- | --- |
| Two ARP lookups overlap | A resumed caller could consume the other target's MAC | Each wait checks the cache for its own captured target, with a local deadline |
| Unknown ARP opcode | Sender mapping entered the cache | Only request/reply opcodes can publish cache entries |
| Native DNS calls overlap, including after a reply is published | Shared result/ID/wait state could be overwritten before the original caller consumed it | One active owner spans ARP, retries, reply publication and return; overlap returns SRV_ERR_AGAIN without changing caller output |
| DNS response arrives from another source IP | Matching port/ID could complete the active request | Require the captured configured server address as well as the existing port/ID checks |
| Process exit during ARP/DNS waits | Calls continued until the normal timeout | Exit checks terminate the wait; DNS releases ownership on the common completion path |
| Connected recvfrom/recvmsg sees foreign datagrams | Foreign IP/port payload could be returned | All three UDP receive adapters discard nonmatching peers through one helper |
| recv on an unconnected UDP socket | Failed with ENOTCONN | Receive a datagram without imposing a peer filter |
| Pseudo-socket closes/reuses during receive or a restart handler | The loop could retry/read or restore flags through a replacement FD | Capture pseudo-slot generation, native FD and peer; reject retirement with EBADF before another retry or flag restoration |

ARP status now reports one cached mapping rather than a global lookup completion;
its public structure/version is unchanged. The cache remains eight entries with
existing eviction behavior; eviction can make a waiter time out, never satisfy
it with another target's MAC. Native DNS remains a single fixed-port transaction,
not a parallel resolver. Busy refusal, timeout, transmit/name-encoding failure,
process exit and success leave the output/ownership contract explicit. This is
not a general DNS authentication implementation: query/question/answer-owner
validation, compression, entropy and DNSSEC remain separate work. The source/ID
matching rationale is described in
[RFC5452](https://www.rfc-editor.org/rfc/rfc5452.html).

UDP peer state is captured for each receive call. Connected `recv`, `recvfrom`
and `recvmsg` now share filtering, including zero-capacity receives and
MSG_DONTWAIT exhaustion. This matches the connected datagram peer contract in
[connect(2)](https://man7.org/linux/man-pages/man2/connect.2.html).
Pseudo-slot generations live separately from cleared/copied socket records;
allocation and duplication advance the destination generation, and an exhausted
counter prevents reuse. A stale continuation fails even when both pseudo and
native descriptor numbers are reused. This adds no public ABI or syscall change.
Native UDP status flags currently belong to the descriptor, so skipping restore
after its close does not leave a surviving duplicate with the temporary flags.
This does not qualify concurrent flag changes on the same live descriptor,
arbitrary user-thread/SMP interleavings, or direct native-FD manipulation behind
the libc adapter.

### Directed verification

`test-net-query` adds 13 modes: ARP different-target overlap, cache/timeout,
exit and invalid opcode; DNS overlap during ARP/response wait/after reply,
source and ID admission, exit, send/encoding failure cleanup and timeout retry.
Eight fail on original source; five are passing controls. Nested DNS responses
use distinct answer values to expose cross-caller result consumption.

`test-posix-udp-owner` adds 12 modes: filtering through recvfrom/recvmsg/recv,
zero-capacity filtering, unconnected recv, same-number close/reuse during filter
and restart, nonblocking flag restoration, close without replacement, duplicate
reuse, ordinary restart and MSG_DONTWAIT exhaustion. Nine fail on original libc;
three controls pass. Host stubs were completed before collecting the final
before/after evidence; missing declarations/symbols are not OS defects. The
nonblocking control checks final flags rather than counting internal toggles.

All 25 modes pass under x86_64/Rosetta and native ARM64 ASan/UBSan. The prior ten
kernel/six libc datagram modes and 46 TCP modes also pass normally and under
sanitizers. The old datagram ARP hook now injects a real ARP reply instead of
writing removed shared completion fields. TCP accept, network worker/device,
process output/lifetime, syscall I/O and lifecycle-wait gates pass. Integrated
package/platform diagnostic, PCI, tool timing and file-pool host gates pass.
The matched build and six generator groups pass, including recursive validation
of 372 entry sets in 16 directories.

Eight sequential disposable VMs pass: tcp-ready 11.28 s, tcp-stress 47.76 s, netabi 9.95 s, libuv 10.63 s, threads 40.41 s, lifetime 23.1 s, web-segmented 19.21 s, outbound-local 10.94 s.
Netabi confirms both the earlier datagram marker and the new
`netabi: udp connected filtering/unconnected receive ok` marker. Outbound HTTP
checks `/query-check` and the exact localhost-server body; no public DNS or web
service is required. Resolver overlap is deterministic host coverage, not a VM
concurrency claim. Documentation links and whitespace checks pass. Tests used
the candidate on `9f427171`; retained source hashes are checked against the
published commit, together with matched image hashes. No native hardware
acceptance, Actions dispatch, physical install or PR merge occurred.

### Next bounded audit

Continued tracing reaches DNS question/answer identity and compressed-name
parsing in both native and libc resolvers. The libc resolver's dedicated UDP
socket and error translation need their own source/response tests. Also queued
are same-live-FD per-call flag isolation and TCP connect/accept publication after
libc close/reuse. The current evidence covers deterministic UDP continuation
boundaries, not every socket operation or complete DNS/network conformance.


The subsequent [DNS/socket checkpoint](dns-socket-audit-2026-10-28.md) completes
that bounded queue: shared response identity/compression validation, libc
source/retry/error handling, per-call flag isolation and connect/accept
continuation checks. Its own report records the newer syscall, tests and limits;
results above remain the earlier ownership checkpoint's evidence.
