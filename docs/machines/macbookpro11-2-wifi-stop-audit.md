# Pro11,2 HTTP/native USB Stop audit

November 10, 2026. Software audit of the failed runtime21387b54 /
releaseb865c96e670e52df, based on merged main0d83d045. The branch fetched and
inspected actual main9a218889 (PR452, userspace capture telemetry), but deliberately
kept the failed source baseline fixed for causal reproduction. A later fetch
inspected maina2a6d692; its additional PR453/454 changes are screenshot audit
documentation only. This is not a
native acceptance or authority to install, arm or boot another payload.

## Failure and evidence boundary

The latest run is closed failed/incomplete. Only its21836-byte startup log was
saved; the immutable2062-file primary index is
`34a88821c013fc3a1627252abef893c3c0e2aee88eed32f7f28bcf0529aec085`.
The separate photograph is also immutable. Its legible fields identify
site2, DRAIN8/OWNER3 and guard_failed1. The user independently read
stack_failed0, stream_failed0, stream_phase3, fault_phase0 and fence_round1.
These values supersede earlier guesses from compressed pixels. The exact failed
raw event and final endpoint context were not saved. The earlier unchanged-image
FINISH9/DRAIN_FAILED9 halt is a separate failure; its raw RX bits remain uncertain.

A concrete legal controller ordering now reproduces the reported native flags and DRAIN/OWNER signature
through the production native event handler, DMA ledger, encrypted owner,
provider/registry and real HTTP `current()` ownership check. The old Stop handler
requires final Stopped context/dequeue/DCS at Stop Transfer Event arrival. A
controller can publish that event while its Output Endpoint Context still shows
Running and the old dequeue, then finalize context before the matching Stop
Command Completion. The premature check faults the fence/DMA owner; HTTP then
latches OWNER while stack/stream failure flags remain clear and round stays1.

The synchronization requirement is described in the primary
[Intel xHCI specification](https://cdrdv2-public.intel.com/625472/625472_xHCI_Rev1_2b.pdf),
section4.6.9. Final context validation belongs after matching command completion.
The Stop Transfer Event must still precede that completion. AppendixG.2 makes
this event mandatory for xHCI1.0; only the older0.96 behavior was optional. The
captured Intel8086:8c31 host reports1.0. Missing/reversed Stop events remain faults.

This establishes a source defect and a matching legal failure mechanism. Its
occurrence in this particular physical run remains inferred because the raw
failed event sequence was not persisted. It does not explain every historical
halt or prove that all subsequent native workloads will succeed.

## Audit scope and corrections

The parent and independent reviewers traced the HTTP supervisor/child, bounded
DHCP/DNS/TCP and prepared worker; immutable descriptor/current/deadline guards;
native provider selection, RX cache/loans and terminal receive handoff; raw USB
Stop/command/shared-event routing; ledger fence/rebase; final endpoint/key/MAC/
input/power/RF/parent retirement; and the fixture/collector assumptions between
those layers. Per-file/function inventories and before/after reproductions are
retained locally with the qualification capsule. No other machine's acceptance
or credentials were reused.

Corrections preserve hardware ownership on uncertainty:

- Native fence, stream, NoOp and calibration cleanup handlers retain exact raw
  Stop identity first, then validate final state/dequeue/DCS after the matching
  successful command completion. No event alone releases DMA or grants retirement.
- A completed last RX slot triggers a fence; merely posting that still-pending
  receive no longer blocks the TX request needed to elicit its reply. A quiet
  terminal RX remains held for the existing guarded outer Stop handoff.
- A nonempty shared event frontier before publication is bounded backpressure.
  Callers dispatch/retry within the original iteration/deadline limits. Invalid
  geometry, command ownership and held-page failures still fail immediately.
- Unknown command completions cannot be discarded during those retries. Only
  the actual synchronous waiter, with its exact published pointer, may delegate
  a non-fence command; shared consumers have no such authority. Fence tombstones
  continue to reject duplicate/stale completions.
- After registry selection, HTTP currentness binds the exact provider, rather
  than accepting a foreign adapter with matching MAC/link. Pre-init admission
  remains possible before a provider has been selected.
- The generic supervisor retains independent descriptor/source/window admission
  anchors. Coherent mutation of both live copies cannot replace callbacks, epoch
  or deadline; a changed source pointer refuses before dereference. Anchors are
  scrubbed only after quiescence. This is a separately reproduced contract bug,
  not evidence that a native callback mutated the photographed run's descriptor.

The test fixture also previously renamed `net_device_mac` onto an aborting stub,
which concealed real registry binding. It now links the actual production
registry/MAC selection. The initial regression failure from that fixture defect
is preserved alongside the corrected results.

## Qualification and remaining limits

Normal qualification passes the full xHCI regression, affected native fence/
NoOp/stream/DHCP/calibration/stack tests, HTTP supervisor/bridge/RX-handoff,
prepared worker, HTTP child, TCP frontend and bounded bootstrap, plus kernel/ISO/
USB-image builds. The saved-log suite executes9 cases successfully and skips its
optional external old-reference composition case. Fatal ASan/UBSan builds pass
all six selected native stack/fence/stream/NoOp/calibration and generic HTTP
fixtures, with unchanged source inputs. Leak detection is disabled for these
retained HOST models; this is not a leak-acceptance claim. The clean committed
source1756cb26 then passes build/image/documentation checks and both actual HTTP
guests (numeric and production-DNS bootstrap). Both execute the real client ELF,
exact4159-byte request,3072-byte body, genuine EOF and software retirement. The
production-DNS guest also passes an independent42-frame capture check for DHCP
DORA, DNS, route ARP, exact TCP data, checksums and bidirectional FIN/ACK. These
are isolated e1000 guests, not native Wi-Fi or hardware Stop acceptance.

The native callback seam uses the actual event handlers and HTTP ownership
function. Its locally constructed HTTP state is not a successful
`http_network_probe_run` admission, actual ELF, scheduler/worker lifetime or
physical USB controller. Those whole-process properties have separate existing
VM/native evidence; they must not be attributed to this seam test.

The fixes are committed as `eaec878b` (HTTP admission anchors) and `b4c194bb`
(native Stop/admission/registry corrections and regressions). All tracked runtime
and test inputs match the qualified source snapshot.

The expanded native stack fixture covers41 cases, including two legal delayed
context orders and ten malformed Stop/context/order refusals through the real
HTTP OWNER latch; quiet RX63 with solicited encrypted DHCP; real shared HID
completion backpressure; foreign selected provider refusal; unexplained command
debt; and the prior encrypted production networking/retirement cases. The generic
HTTP fixture covers72 cases, including coherent descriptor/window mutations.
Outer stream/NoOp/calibration tests retain final-context negative cases, and the
fence test exercises real synchronous waiter delegation and shared-consumer
refusal. Qualification results and exact inputs are recorded separately; no test
result should be inferred from this inventory alone.

The current design is still a bounded diagnostic. TX63 with an unrelated quiet
pending RX can exhaust the unchanged750ms boundary window and pin ownership.
Supporting arbitrary persistent traffic needs an explicit pending-RX cancellation
and fence ownership design; no synthetic completion or raised limit is introduced
here. This case has round0 and stack/stream failure flags and does not match the
reported round1/flags-zero failure. Persistent service, reliable reconnect/UI and
unlimited ring/command reuse remain open. This adapter is5GHz-only; Demers2.4
requires different hardware.

No new image has been installed or one-shot boot armed during this audit. Ubuntu
and all retained accepted/factory recovery payloads remain in place. The failed
run is already collected; do not recollect or reseal it. Keep the boot block until
a separately prepared hardware validation replaces this completed software-audit
checkpoint. The durable local audit capsule is named
`wifi-http5-stop-audit-b4c194bb`; all earlier accepted/failed capsules remain
separate and immutable.
