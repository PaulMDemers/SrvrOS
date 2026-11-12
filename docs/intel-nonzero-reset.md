# Original nonzero-geometry BCS reset

This is the separate opt-in v28 `intel-nonzero-reset` diagnostic for the
MacBookAir7,2 reference unit. It is under local qualification; native acceptance
is pending. It follows the enabled-entry suboperation observed in the v27
trial. The v27 whole entry/reset/requester sequence remains incomplete.

The existing `intel_ring_entry_reset_ready` guard, takeover geometry gate and
legacy preparation/reset APIs are unchanged. Do not clear geometry, copy an
entry/census/owner, fabricate an old ready receipt, or use a saved diagnostic as
live authority. The new path has its own original-object API and wire contract.
It stops before interval claim, re-arm, GPU requester inhibition or memory
transfer. All 255 memory/cache/translation/reader obligations remain open.

## Source and lifetime

The only eligible source is the original completed kind 1 enabled entry under
the same static HDA/GT owner. The actual original post-census must show disabled
BCS with START `0x03eef000` and CTL `0x0000f000`, zero head/tail and the full
captured idle engine/context vector. Original source, post, epoch, return,
reader, provider and timestamp identities remain checked. Kind 2 refuses this
new protocol; its separately accepted zero-geometry route remains available in
its own image.

One registered GT child spans new admission through preparation, reset,
cancellation, preservation and verified return. Admission has its existing
100 ms bound and a 100 ms source-age bound. Preparation starts exactly once,
after complete admission, and has its existing 100 ms budget through final
return. Every final store has a fresh monotonic check against preparation,
its 10 ms phase and the 300 ms GT parent. HDA 400 ms and outer 500 ms limits
remain unchanged. No expired phase is restarted to obtain more time.

## Ordered operations

1. Take two complete six-DWORD reset-control/GDRST snapshots, requiring stable
   clear request/ready/domain bits. Block premature parent close with the child.
2. Request the five existing engine reset controls in order with DWORD
   `0x00010001`; each acknowledgement has at most 256 observations and 10 ms.
   Take two complete held-ready snapshots and the exact original BCS/context
   plus GDRST snapshot.
3. Advance the owner epoch from 1 to 2 before the first possible reset write.
   Issue only BCS domain bit 3 (`8`) at GDRST `0x941c`, twice after successful
   self-clear, with the existing 256-observation/10 ms bounds. Do not retry an
   unissued, failed, uncertain or timed-out pulse. Settle at least 50 us inside
   the same bounded lifetime.
4. Read BCS/context/GDRST again. Require one of the two *whole* classified BCS
   vectors below and unchanged context. Other four controls remain held;
   reset BCS request/ready must be clear. Cancel in reverse order with DWORD
   `0x00010000`, then verify the complete clear control vector.
5. Obtain a fresh full 92-DWORD census at epoch 2 and all 37 original
   display/HDA reader words. Verify the classified geometry and exact remaining
   engine/context/reader values. Finish the final guard and return the original
   child inside the original preparation deadline, then perform the existing
   GT/HDA/alias/CPU-exclusion retirement.

The native adapter uses the same 14 exact UC/NX GPU aliases and original HDA
alias. Writable aliases are restricted to the existing five control pages,
BCS control page and GDRST page. Target stores are fenced DWORDs to those
reviewed control offsets only. There are no software Ring START/HEAD/TAIL,
GGTT/PTE, framebuffer, arbitrary reset-domain or GPU PCI command stores.

## Post-reset classification

Accepted postconditions are either the complete original disabled BCS vector,
or its strict reset-default variant with **only START and CTL zero**. All
remaining six BCS/context words must match the original; both full post-census
samples must agree with the same classification. Mixed START/CTL, re-enabled
submission, changed context, other-engine changes or foreign GDRST bits refuse.

These are narrow observed postconditions, not a guarantee that a domain reset
preserves firmware geometry. Inherited backing stays protected in both cases;
software never clears or restores it to manufacture a pass. Neither result is
fed to the legacy zero-geometry API or grants allocation/cache ownership.

Intel's [Broadwell PRM Volume 2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789)
describes BCS_RESET_CTRL (printed page 165), GDRST (609–610), Ring Control
(1345–1348) and Ring Start (1352). The reviewed local PDF SHA-256 is
`09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`.
The pinned [Linux v6.12 reset implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c)
uses per-engine preparation, selected-domain reset, two successful pulses on
this generation and 50 us settling, and notes reset register volatility. Its
[ring submission implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ring_submission.c)
explicitly programs geometry during ring resume. This diagnostic imports no
forced-ready fallback, retry loop, geometry programming or ownership inference.

## Recipe, evidence and physical trial

Build `build/intel-nonzero-reset/srvros.elf`; stage with
`python3 tools/hardware_boot.py stage --laptop-probe-volume LOCAL_UUID --intel-nonzero-reset`.
The distinct exact 15-token recipe includes
`srvros.intel-ring=nonzero-reset-v1`, its own firmware request and
`nonzero-reset-preinit`; no user services start. Mixed profiles refuse.

Canonical new receipts retain raw control/BCS/ACK/pulse/post/reader values and
source, store, failed-clock, guard and return timestamps. The strict collector
checks coverage/order, original source and epoch, whole geometry classification,
phase clocks, read accounting and enclosing retirement. Old v24–v27 receipts
remain separate. Exact-stage VM identity refusal validates persistence and
recovery only, never native target stores.

The unattended guest shows progress and requires no human input. A proved safe
saved result or pre-operation refusal returns automatically to Ubuntu. Once
eligible, any uncertain source, clock, access, state or return pins the original
root and halts visibly. Do not retry, release aliases or restore unknown hardware
merely to obtain a report or automatic reboot. Missing completion remains a
failure/incomplete trial. See the unit record for the installed candidate,
matching collection, recovery and later physical acceptance.
