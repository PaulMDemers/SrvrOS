# Broadwell GT access and requester preflight

The accepted Air7,2 CPU-client qualification does not provide GT register-access
or firmware ownership. This slice adds the missing wake transaction contract and
a separate **read-only** native preflight. It does not yet wire a native writer,
read engine controls, retire engines or grant a GPU interval.

## Audited hardware obligations

Intel's [Broadwell PRM index](https://www.intel.com/content/www/us/en/docs/graphics-for-linux/developer-reference/1-0/broadwell.html)
links [Volume2c, November2015](https://cdrdv2.intel.com/v1/dl/getcontent/690789).
The local reference has SHA256
`09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`.
Relevant printed pages:

- 493–495: `0xa188` has16 masked thread requests. A new driver must initialize
  all requests using `0xffff0000`; an owned thread then requests its bit, waits
  for the corresponding `0x130044` acknowledgement, does its work, clears its
  own request and observes acknowledgement retirement before reuse.
- 703: the acknowledgement register is an always-on status view.
- 1344: Stop Rings must not be used for ordinary stall/resume of a running batch
  on Broadwell; the documented warning permits its use before engine reset.
  This implementation does neither stop nor reset.

The [pinned upstream uncore implementation](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/intel_uncore.c)
selects `FORCEWAKE_MT/ACK_HSW` and the thread-C0 workaround for Broadwell.
Its raw initial `FORCEWAKE_MT` read checks MMIO accessibility without taking wake;
the masked upper half cannot read as all ones. Its Gen8 GT range covers
`0..0x3ffff`, so an already-set firmware request is not a lease to inspect engine
registers. The [register definitions](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/gt/intel_gt_regs.h)
identify thread state `0x13805c`, low mask7. That status is used only by the
future owned wake transaction, not the native preflight.

The Broadwell PCI register catalog lists SWSCI `0xe8` and ASLS `0xfc`.
The [pinned OpRegion implementation](https://github.com/torvalds/linux/blob/028ef9c96e96197026887c0f092424679298aae8/drivers/gpu/drm/i915/display/intel_opregion.c)
uses ASLS as a physical pointer and has separate readiness/notification lifetimes.
The Intel index's current OpRegion download is a **Skylake** specification;
it is context only, not authority for Broadwell mailbox writes. This preflight
reads the two PCI values without following ASLS, reading a mailbox or invoking
SCI/AML. A zero pointer does not prove firmware exclusion.

## Wake transaction

`intel_gt_access` is boot-CPU serialized and requires three independently audited
grants: initialization authority, retirement of all other requesters and exclusive
use of the kernel request bit. A live CPU exclusion token is also required.
Missing grants refuse before any I/O. Even with all grants, nonzero initial
requests/acknowledgements are conservatively refused: this first implementation
cannot reclaim stale firmware requests. Zeros alone do not supply grants.

The owner pins CPU exclusion before the first possibly issued write. Its only
write destination is `0xa188`, in order `0xffff0000`, `0x00010001`, then
`0x00010000` on close. It requires acknowledgement of each transition and C0
before admitting awake state. Every callback is guarded; foreign bits, changed
state or uncertain completion stops forward work. No fallback-bit toggling,
blind retry or global reset is available. Faulted owners keep their pin until
reset. Only a definitely unissued first write may drop the pin immediately.

Phase limits are100ms, total lifetime300ms, with4096 polling attempts per phase
and a bounded service/delay callback. Backward, frozen, zero or expired clocks
refuse; success requires positive clock progress. Closing requires a fresh
owned request/ack/C0 check and acknowledgement retirement before unpinning.
These are trusted-adapter contracts, not a boundary against malicious kernel
code. Owners bind their object address and CPU exclusion epoch; accidental structure
copies cannot operate or retire the original owner. An awake clock lease would still not retire engines, transfer addresses
or establish firmware/display/audio/DMA ownership.

The transaction is exercised against synthetic host I/O only. No native caller
supplies its grants or writer callback. Ordinary startup cannot enter it.

## Native preflight

`--intel-gt-preflight-capture` creates a distinct version1 recipe. It includes
the accepted display/backing/cache/window and CPU-handoff qualification, then
acquires a fresh CPU exclusion lease for the new observations. It brackets the
work with exact original-window/full-backing revalidation. Five additional
profile/backing guards surround two passes of the same raw request and ACK reads.
Each pass uses two separate RO/NX/PCD temporary mappings, with a fresh matching
CPU cache context, UC MTRR and effective UC check per page. All four aliases must retire.

Only offsets `0xa188` and `0x130044` are admitted. The accepted backing/window
mapping allowlist and original candidate-page cache helper remain unchanged;
the new two-page cache query is separate and supplies no mapping authority.
ASLS/SWSCI are checked for endpoint stability around each pass. Partial mappings,
failed retirement and changed context remain failure/incomplete evidence.

Receipts retain raw DWORDs, timings, map/guard/cache counts, exclusion and
preservation outcomes, including guard-retained uncertainty. The strict decoder
binds successful native qualification to the complete existing native receipts
and independently derives both new pages' UC MTRR/effective types from the raw
CPU context. PAT-only UC does not satisfy this stricter new-page prerequisite.
It always reports forcewake_permission, engine_retirement, hardware_ownership,
managed_range and physical_tearfree false. A stable or zero request/ACK pair
cannot initialize forcewake or supply a hardware handoff grant.

The unattended guest shows progress, saves/unmounts and returns to Ubuntu on
handled outcomes. Missing persistence remains a halt. No typing is needed;
volume1% is retained and guest audio is disabled. Use the local unit's UUID:

```sh
make -j2 all test-intel-gt-access test-intel-gt-preflight
python3 tools/hardware_boot.py stage --laptop-probe-volume 37D8-C5D0 --intel-gt-preflight-capture
python3 tools/hardware_boot_smoke.py --probe-check
```

Host tests cover protocol/C0, grants and tokens, foreign requests, unissued and
uncertain writes, all I/O failure boundaries, frozen/backward clocks and retained
pins. Actual observation-adapter fixtures cover cache/map/retirement failures,
floating PCI/MMIO values and changed raw observations. Native acceptance is
pending a matched physical round. Initial source/readiness and independent
recovery stay in the [unit record](machines/macbookair7-2.md).

## Prepared candidate

Release `df5ca792434d9145` (`251a912b`) passes full local host/core/profile/build/
docs checks, fatal sanitizers, four detected compiled negative controls and
exact-stage refusal/save/return and missing-workspace VMs. Existing VirtIO,
Intel and Haswell profiles pass. The staged native ELF's SHA matches its manifest;
only the preflight is linked, with no wake-protocol entry points or writer.
It is installed with recovery retained and no reboot armed. Fresh media and
prior-file integrity checks pass; source-qualified artifacts/checkpoints and
independent copies are identified in the unit record. Native readout and every
hardware ownership grant remain pending the next physical round.
