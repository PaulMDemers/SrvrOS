# DNS response and socket call audit — 2026-10-28

This continues [draft PR182](https://github.com/PaulMDemers/SrvrOS/pull/182) from
`4cef0949`, on main `46b60b24` through PR246 (merge `39210341`). It covers response
identity/compression in the native and libc DNS clients, per-call nonblocking
flags, and TCP connect/accept continuations. It is not a complete DNS, socket,
security or SMP audit. Earlier ownership evidence remains in the
[datagram report](datagram-audit-2026-10-28.md).

## Confirmed defects and corrections

| Trigger | Original behavior | Correction |
| --- | --- | --- |
| DNS reply has the right ID but a different question, unrelated owner, malformed compression or incomplete trailing section | Parsers could publish its first A record; libc also omitted QR/RCODE checks | One shared bounded parser validates the question, response flags, all section extents and answer ownership before publishing |
| Rejected libc DNS reply followed by retry | Receive overwrote the outgoing query; the next send reused reply bytes | Separate immutable query and response buffers |
| DNS reply from another server, or larger than libc's receive buffer | Libc did not check the source or detect datagram truncation | Connect to the configured UDP peer, use common receive filtering, and reject output MSG_TRUNC |
| Native resolver busy after libc fallback | Temporary refusal became permanent EAI_NONAME | Return EAI_AGAIN, including a public constant and gai_strerror text |
| MSG_DONTWAIT on a live stream/UDP receive shared with another call | Temporary F_SETFL altered shared status and could restore stale flags; UDP sends ignored the flag | New per-call native operation passes nonblocking behavior directly to TCP/UDP/pipe helpers without changing flags |
| Pseudo-socket closes/reuses while connect or accept blocks, or in a restart handler | Late connect could publish into the replacement; accept could retry its replacement listener | Capture generation/native FD, revalidate after returns and handlers, and close a late result that has lost its owner |
| Raw native listener is reused between accept retries | Retry and option inheritance used the reused FD number | Hold a CLOEXEC duplicate across accept/restarts and snapshot options before waiting |

Response validation follows the question and compression format in
[RFC1035](https://www.rfc-editor.org/rfc/rfc1035.html) and response matching in
[RFC5452](https://www.rfc-editor.org/rfc/rfc5452.html). The parser accepts one A/IN
question, checks ID/QR/opcode/TC/reserved Z/RCODE, compares labels case-insensitively,
and follows only relevant answer-section CNAMEs (at most eight hops). It rejects
conflicting CNAMEs or an A and CNAME at the same owner. It validates the complete
packet before output mutation. Unrelated A records cannot supply the result.

Expanded names are at most 255 bytes and packets at most 1,472 bytes. Compression
pointers must go backward to a label boundary decoded by this parser; header,
label-interior, forward, self and out-of-range pointers are refused. RR headers
and RDATA extents in answer/authority/additional sections must fit exactly; A/IN
lengths and CNAME/IN names are validated. Other RDATA remains opaque. This is a
conservative A/CNAME profile: references into names in unsupported RR RDATA are
not decoded. It does not implement every RR type, DNSSEC, EDNS negotiation,
TCP fallback, caching or randomized query IDs/ports. Native DNS still uses one
fixed source port and incrementing ID; libc still uses its fixed ID per request.
Source/question matching is not cryptographic authentication.

## ABI and lifetime contract

`SYS_SOCKET_TRY_IO` (155), exposed as `srv_socket_try_io`, accepts a native FD,
operation, buffer/length and optional UDP peer. Operations are stream receive,
send and TCP peek, UDP receive and UDP send. `srv_socket_peer` contains native
host-order IPv4/port plus a reserved zero field. User buffers are checked in the
correct direction; UDP output is copied only after a successful receive. Unknown
operations and nonzero reserved input fail. This entry retains normal syscall I/O
serialization; it is not added to the GUI-safe admission list.

`MSG_DONTWAIT` now affects only its call, including UDP sends. Stream read/write signal checkpoints remain in place,
including delivery of queued SIGPIPE while preserving the EPIPE result.
An uncached route
emits an ARP request and returns EAGAIN; the caller may retry after resolution.
Local delivery and cached routes remain immediate. Empty/full pipes return
EAGAIN without altering their blocking status, including shared descriptions.
This follows the distinction between per-call MSG_DONTWAIT and shared O_NONBLOCK
in [recv(2)](https://man7.org/linux/man-pages/man2/recv.2.html).
Old syscall interfaces remain available. New libc requires the matched kernel;
rebuild the complete image rather than replacing one binary in an old payload.

Pseudo-slot checks cover the cooperative syscall/restart boundaries tested here,
not arbitrary instruction-level preemption or native syscalls that bypass libc's
ownership records. A pending connect is marked in progress before entering the
blocking call. Accept options come from the captured listener. Raw native accept
needs one extra descriptor while waiting and returns EMFILE if that duplicate
cannot be allocated. A missing FD and allocation failure preserve EBADF and
ENOMEM respectively. Closing the original raw listener does not cancel the held
accept; it continues on the original description, never a replacement number.
These changes do not add pseudo-socket inheritance or general shared libc state.

## Reproduction and validation

The tested implementation is `e40cd7824887c286d7a7356d643bcebfa6b3754a`.
Retained source hashes match that commit; the image build manifest preserves
its original `39210341` dirty-tree provenance.

`make test-dns-response test-socket-calls test-socket-try-io` is part of test-core:

- 23 parser modes run against both production callers (46 cases), covering case,
  question/type/class, header flags, complete section bounds, malformed pointers,
  CNAME chain/cycle/length/conflict and unrelated answers preceding a valid one.
  Exact-length truncated allocations exercise refusal without output mutation.
- 17 libc modes cover connect/accept close/reuse and restart, ordinary controls,
  five per-call flag paths, raw accept pin/options/descriptor exhaustion, resolver
  retry bytes, truncation, foreign source and busy translation. Stream modes
  also check success/EPIPE signal checkpoints and errno preservation.
- The native dispatcher fixture checks the operation/file-type matrix, user
  access direction, bad peer pointers/reserved fields, output copy and unchanged
  flags. Existing pipe tests add per-call pressure/empty checks on blocking FDs;
  network datagram tests add uncached/cached/local nonblocking send controls.

Against source `4cef0949`, 15 kernel-parser and 17 libc-parser modes fail, with
ordinary valid/CNAME/bounds controls passing. Fifteen socket modes differ from
the old source; raw-full is a new descriptor-pressure contract, not an additional
original defect. These counts are cases, not independent bugs. All candidate
modes pass on x86_64/Rosetta. Parser/socket/datagram/query matrices pass native
ARM64 ASan/UBSan; the architecture-specific syscall fixture passes x86_64
ASan/UBSan because it includes x86 inline assembly. The prior 46 TCP modes also
pass normally and with native ARM64 sanitizers.

TCP accept, network worker/device, process output/lifetime, syscall admission,
lifecycle wait, package/platform diagnostics, clock/application, workspace log
and PCI gates pass. The matched kernel/userspace build passes. Six generator
groups and recursive image validation are recorded with the VM results below.
The first matched build caught missing guest netdb/error names hidden by host
headers; the guest now defines EAI_AGAIN and uses existing EOPNOTSUPP.

Eight sequential disposable VMs pass: tcp-ready 10.88 s, tcp-stress 47.74 s, netabi 10.32 s, libuv 10.21 s, threads 40.5 s, lifetime 23.54 s, web-segmented 18.84 s, outbound-local 10.51 s.
Netabi confirms signal delivery, per-call flags/alias/pressure/recovery and both
earlier UDP groups. Local outbound HTTP verifies `/dns-socket-check` and the
exact `srvros-local-outbound-dns-socket` body. Resolver malformed-packet and
close/reuse schedules remain deterministic host evidence, not VM concurrency
claims.

Raw logs, original-source failures, tested source/artifact hashes and the
candidate diff are retained under `build/review/dns-socket/`. The guest netabi
adds `netabi: per-call flags/alias/pressure/recovery ok`: empty receive, successful
transfer, full-pipe refusal, duplicate status, drain/retry and invalid operation.
`netabi: per-call signal checkpoint ok` checks a real closed-peer send and SIGPIPE
handler delivery before returning EPIPE.
PR19 `5556641f` retains separate Wi-Fi scope. No Actions dispatch, native install,
hardware qualification or merge is part of this checkpoint.

## Continued audit

Tracing beyond these fixes reaches resolver retry/deadline policy and query
entropy, duplicated pseudo-socket peer/options state, and remaining native-FD
read/write/shutdown continuations. These are the next directed coverage targets;
this checkpoint does not certify them. In particular, existing native handles
retain generation checks, while libc caches peer/options per pseudo slot or raw
FD number. A surviving duplicate and later option/peer change need separate
cross-alias tests. Signal suppression and restart semantics across send/recvmsg
also need their own bounded pass. Unsupported UDP MSG_PEEK/input MSG_TRUNC and
broader network conformance remain explicit limits.
