# Broadwell BCS-only reset protocol

The [accepted request/ready/cancel handshake](intel-reset-readiness.md) now has a
separate host-tested successor in `kernel/diagnostics/intel_bcs_reset.c`. It is
not linked into any native image. No native reset adapter, ELF, firmware request
or staging option exists. Ordinary startup and the installed payload cannot
execute this protocol. The purpose is to establish the destructive-operation
lifetime and failure contract before adding a hardware entry point.

## Register and ordering evidence

Intel's [Broadwell PRM Volume 2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
printed pages 609–610, defines GDRST at `0x941c`. Bit 3 initiates only the blitter
domain soft reset. Hardware clears that bit on completion; a software clear
cannot cancel it. This is a non-posted register. The core only permits the value
8; it neither combines domains nor imports the full-reset bit. Any non-BCS bit
observed in GDRST refuses the operation. Initially set bit 3 also refuses it.

Page 165 defines `BCS_RESET_CTRL` at `0x220d0`. The request and ready bits clear
on engine reset. Consequently a successful reset must not be required to retain
the pre-reset request/ready value 3. The final explicit masked request cancellation
is still checked, with fresh clear low-half control and GDRST endpoints.
Upper write-mask bits remain observations, not active request state.

The pinned [Linux v6.12 reset implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c),
`gen8_reset_engines` and `gen6_hw_domain_reset`, prepares the selected engine,
issues the selected hardware domain twice, waits for each self-clear and then
waits 50 microseconds before cancellation. Its comments explain that register
state can remain volatile after acknowledgement. The core models these two
bounded pulses and measures at least 50 microseconds of settling. It does not
import retry-on-error, forced non-ready resets, all-engine resets or arbitrary
register restoration. The second pulse is the fixed successful sequence, never
a retry after a failed or ambiguous first pulse.

Neither the register definition nor the upstream sequence establishes an
outstanding system-memory write, prefetch or GPU-cache drain witness. A reset
acknowledgement therefore changes no completion obligation. All eight remain
open (255). CPU fences and the settling interval are not substitute witnesses.
There is no translation invalidation, command submission, PTE update, framebuffer
change, private backing transfer or native tear-free acceptance.

## Lifetime and stale observations

Entry needs the complete same-parent address-sealed 92-read census, unchanged
engine epoch, no enabled/pending/saved-wait/TLB-pending masks, parser-idle mask 3,
an awake pinned GT parent and no other live dependent. A copied result or stale
parent/epoch refuses entry. Fresh BCS raw words must match the observed empty,
disabled entry: 0/0/0/0/1/0/512/0. Reset and final post-state are deliberately
narrower than the hardware's general capabilities.

A registered one-use borrow pins the parent; blocked early close must issue no
store. The sequence is: fresh BCS/control/domain, request and ready, two BCS-only
reset/self-clear phases, measured settling, masked cancellation, unchanged BCS
and clear control/domain, parent check and verified pin return. Read, store,
guard and clock failures stop forward progress. There is no cleanup store after
an uncertain result, no forced pin return and no controller-reset fallback.

Immediately before the first possible GDRST store the owner engine epoch advances
once, including a definitely-unissued callback. It is never rolled back, and
overflow is refused before request. A new completion census records that epoch;
one observed across an epoch change cannot complete. Readiness and subsequent
reset entry reject older censuses. A reset cannot start with another live borrow.
This prevents a pre-reset empty-ring observation from silently authorizing work
after destructive engine state changes. The epoch is internal software state;
it is not a hardware ownership grant or a new field in existing version 4/5
receipts. A future native reset receipt must bind it explicitly.

The limits stay 10 ms/256 polls per acknowledgement phase and 100 ms for the
whole dependent, including verified borrow return, within the existing 300 ms GT
owner and 500 ms outer lifetime. Settling is also capped by 10 ms/256 iterations.
Late but verified clean return invalidates timing acceptance without inventing a
hardware pin. Any possibly issued request followed by an unresolved reset,
foreign state, lost guard, failed cancellation or changed post-state retains
ownership and exclusion until platform reset.

## Native admission still required

Before enabling a physical reset, add and validate all of the following together:

- A distinct image/request/exact recipe; ordinary/readiness images must remain
  unable to issue GDRST. Bind this Air's identity, cooperative firmware submission
  policy, fresh cache/resource/root guards and unchanged protected backing.
- A separate reserved, exact `0x9000` MMIO page with independent effective-UC/NX
  validation, prepared before wake. Existing mapping allowlists stay unchanged.
  The actual native adapter must independently admit only `0x941c`/value 8 and
  the exact BCS masked request/cancel stores.
- Same-boot pre/post other-engine and display-reader preservation, explicit
  reset-side-effect limits and protected inherited allocations. HDA/reader
  evidence from earlier separate boots cannot substitute for joint ownership.
- Raw reset/control/post-state/epoch receipts, unchanged time/count bounds, a
  strict native collector, saved prerequisite checkpoint and sufficient saved
  tail capacity. Actual compiled native MMIO/mapping negative controls are
  required; host callbacks alone cannot validate those addresses.
- Exact-stage VM no-store refusal and failed-persistence/retained-state tests,
  verified installed recovery and USB media, automatic safe return and visible
  halt on uncertainty. Volume 1%, no interactive input required.

The separate milestone of outstanding-memory completion and maintained CPU/GPU
cache/translation ownership remains necessary before changing GGTT/backing or
page-flipping. Do not treat this host model as hardware readiness.
