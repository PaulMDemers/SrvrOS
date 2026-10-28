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

## Continued audit and limits

Follow-on tracing reaches shared ARP/DNS request state across blocking waits and
libc connected-UDP peer filtering and close/reuse. `resolve_ipv4_mac` and DNS
queries use global pending state; connected `recv` filters the peer separately
from the recvfrom/recvmsg path. These need directed overlap/filter tests next;
this checkpoint does not qualify them. UDP identifier exhaustion, arbitrary SMP
interleavings, ICMP error delivery/PMTU, full socket flag/errno parity and native
network adapters also remain outside this evidence. Existing fragmentation,
TCP reassembly and physical-device restrictions remain in the supported profile.
