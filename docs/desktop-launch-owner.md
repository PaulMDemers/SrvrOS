# Desktop launch ownership under storage load

The desktop's pixel buffers can legitimately put its mapping count above the
existing 4096-mapping private spawn bound. Keep that bound. A lifecycle worker
thread shares those mappings, so moving the syscall to another thread does not
remove the measured refusal.

The desktop now starts a small `deskclient --launch-owner` process before its
lifecycle threads. It has no canvas, pixel cache or GUI connection. Two private,
nonblocking pipes become fd3/fd4; its ordinary standard descriptors stay intact.
Applications inherit only the original standard-descriptor contract, never the
command channels. Requests contain copied program/role/path/profile values, not
pointers into the desktop. The server constructs the same argument vector and
profile environment as the former lifecycle worker.

The launcher is the actual parent. It records successful PIDs before replying,
waits only for recorded children and retains them until a successful final wait.
The desktop worker serializes commands and retains a matching PID ledger. The
UI observes that ledger with acquire loads; it does not perform synchronous IPC
while the worker runs. Shutdown joins the worker before the UI takes over its
channel. Ordinary unrelated children keep their existing parent/wait path.

Each request/reply has a fixed versioned magic and monotonic sequence. Short I/O
and EINTR are retried, EAGAIN uses strict pipe readiness waits, and EOF or a
crossed reply fails the channel. An ambiguous launch is never retried. Closing
the request channel makes the server kill and wait for every child it recorded,
including a child whose reply could not reach the caller. Normal shutdown
requires an empty child ledger, a successful quit reply and the server's final
zero-status wait. The extra process consumes the existing global process-table
capacity; no capacity, mapping, watchdog or workload limit increases.

The native empty-argument helper syscall has a separate validated prelude. It
can use the existing private image builder only after the original parent,
4096-mapping, scheduler ownership, path/namespace, immutable-source, ELF and
image-size proofs. Explicit stdin/stdout must be live simple pipe endpoints,
with no queued rights and checked reference headroom. The complete proof,
reference duplication, image construction and publication/rollback run under
IRQ exclusion. The private rollback closes only proved simple endpoints and
standard descriptors; it cannot invoke global rights collection or a generic
file/backend callback. Nonempty arguments and every refused proof return to
ordinary admission and the unchanged legacy constructor. Kernel-origin lifetime
pipe handoff in PR19 is a separate constructor and remains untouched.

Only scalar SETFL with zero or NONBLOCK on a revalidated simple read/write pipe
has an atomic metadata attempt. Other flags, descriptor families, queued rights
and commands retain the ordinary gate and fresh handler lookup. No foreign I/O
owner is borrowed, released or reclassified. General process exit and resource
retirement retain their existing strict checks. File, image and screenshot
helpers explicitly close their protocol stdin/stdout through the existing close
syscall before exit. Simple endpoints can retire atomically; every non-simple
close retains ordinary admission. Close failure remains a nonzero exit result.

Host regressions exercise actual protocol framing, exact profile/vector
construction, foreign-PID and malformed-request refusal, readiness/short/
interrupted I/O, lost-reply cleanup and final wait. Real process fixtures cover
pipe refs, queued-rights refusal and failed scheduler-publication rollback.
Syscall fixtures retain copied-input validation and gated fresh-lookup fallback.
Matched loaded VM and native evidence remain separate acceptance steps. The
original 1800-second audio/disk/app/three-screenshot workload, 500ms desktop loop,
ten-second whole-app cycle and all durability/allocation checks still apply.
