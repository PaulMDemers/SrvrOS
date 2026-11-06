# Broadwell GT access and requester preflight

The accepted Air7,2 CPU-client qualification does not provide GT register-access
or firmware ownership. The read-only qualification and firmware census below are accepted. A new
explicit cooperative-EFI contract adds a separately compiled, opt-in bounded
native wake trial. Native wake is not yet physically accepted. Engine control,
reader retirement, GPU intervals and physical tear-free flips remain separate.

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

The original transaction was exercised against synthetic host I/O only. The
new opt-in cooperative policy below supplies a distinct native adapter; ordinary
startup still cannot enter it.

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
floating PCI/MMIO values and changed raw observations. The matched native read-only round is accepted below; hardware ownership
remains unaccepted. Initial source/readiness and independent
recovery stay in the [unit record](machines/macbookair7-2.md).

## Matched native read-only result

Release `df5ca792434d9145` (`251a912b`) passes full local host/core/profile/build/
docs checks, fatal sanitizers, four detected compiled negative controls and
exact-stage refusal/save/return and missing-workspace VMs. Existing VirtIO,
Intel and Haswell profiles pass. The staged native ELF's SHA matches its manifest;
only the preflight is linked, with no wake-protocol entry points or writer.
It remains installed with recovery retained. Native session
`00065f90dd5e19b5-00` completes with both matching raw observations:
request `0x00010000`, ACK zero, ASLS `0x8ad03190`, SWSCI `0x00008000`.
The passes take4,117us and4,113us; all four aliases retire, all five guards and
four fresh UC checks pass, and before/after preservation and exclusion release
complete. The strict decoder binds the full native prerequisites. The report has
completed=yes and no collector attention.

This accepts only the bounded read-only requester observation. No firmware,
engine or other hardware reader is retired; no request initialization, wake,
engine inspection or GPU address transfer is authorized by these raw values.
Recovery, media integrity and independent evidence are tracked in the unit record.

## Firmware handoff census and native-wake admission

The accepted raw request `0x00010000` has zero active low16 bits and mask1
in the upper16 bits. Those halves have different meanings in PRM2c; the wake
core now compares only low16 requester state while preserving the raw value.
Its host adapter models readable mask bits and tests each upper bit separately.
This correction does not supply missing ownership grants.

The next explicit recipe, `--intel-gt-firmware-capture`, adds a fixed-field
OpRegion census after the accepted GT observer. It follows ASLS only when the
complete8KiB extent and containing header page are covered by exactly one boot
ACPI-NVS entry, with no overlapping entry or catalog gap. This first version
supports DWORD-aligned pointers whose fixed fields fit one page; page alignment
of ASLS itself is not required. A fresh matching CPU cache snapshot must derive
WB MTRR/PAT/effective types before each RO/NX/WB mapping. It does not modify the
old MMIO/GGTT/cache allowlists. Both aliases must retire, with five guards and
before/after original-window/full-backing revalidation under CPU exclusion.

The observer reads the16-byte signature, size, version and mailbox mask first.
Only an8KiB v1/v2 header with the expected signature admits reads of advertised
mailbox fields: ACPI DRDY/CSTS/CEVT, SWSCI SCIC, and ASLE ARDY/ASLC. No private
firmware payload, VBT, EDID, mailbox arguments or pointer chains are copied.
Every successful pass requires positive monotonic progress within100ms and
stable endpoint/header/status values. Changed, unknown or inaccessible data
refuses; uncertain mapping retirement retains CPU exclusion. The raw resource,
fields and failure status remain diagnostic evidence, including when a valid
firmware region cannot be established. This is a new physical acceptance target.

The layout comes from the pinned upstream OpRegion structs linked above. The
[UEFI runtime-services contract](https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html)
separates boot-services exit from runtime services. We therefore do not turn a
Limine handoff, a quiet mailbox, low16 request zeros or absent ACKs into a claim
that runtime firmware/SMM is retired. No OpRegion command, SCI/AML call, driver
readiness write, forcewake write or GT engine access is enabled by this census.
The staged native ELF must continue to omit all wake-protocol entry points.

| Requirement | What exists | Still required before native wake |
| --- | --- | --- |
| CPU clients | Accepted bounded exclusion and preserved inherited state | Fresh same-boot lease through all issued/uncertain writes |
| Request encoding | PRM-correct low16/mask split and bounded host transaction | Native grants independent of sampled zeros |
| Firmware surface | Stable native ASLS/SWSCI; new catalog/header/mailbox census | Explicit supported firmware/requester lifecycle contract; quiet words are not retirement |
| Initial clear | PRM-mandated `0xffff0000`, host-tested only | Initialization authority over all requester bits |
| Kernel bit | Host-owned bit0 with ACK/C0 and exact release | Independently established exclusive bit ownership |
| GPU memory/readers | Populated original slots and preserved fence | Separate firmware/engine/DMA/display/audio reader handoff and actual interval transfer |

A successful census always emits grants0/writes0/wake0/ownership=none. The strict
host decoder checks field coverage, advertised-mailbox read counts, resource
bounds, timings, native prerequisite ordering and independent WB derivation.
It reports unresolved firmware/requester retirement even for all-zero mailboxes.
A native wake writer remains intentionally unbound until those lifecycle facts
can be established; its bounded protocol is ready for integration once they are.

## Firmware-census validation and native result

Release `45815233908f2d02` (source `4ca8f490`, merged main `7b887bbe`) is
installed with recovery retained and no reboot armed. Full local validation,
fatal sanitizers, all260 collector/staging tests, seven detected compiled
negative controls and exact-stage refusal/save/return and missing-workspace
VM behavior pass; existing Intel/Haswell and VirtIO recovery profiles pass.
Its native ELF links the observers and omits every wake-protocol entry point.

Ubuntu's read-only i915 reference and the exact captured DSDT confirm the fixed
OpRegion layout. Both prior native CPU/cache contexts derive WB for the proposed
header page. Those references are independently retained but do not accept the
new native observation or grant runtime firmware/requester retirement. The matched native census result is recorded below; a supported ownership
contract and native wake adapter remain next. See the current unit
record for source, recovery, run/checkpoint and independent evidence locations.

Native session `00065f91ff581bbd-00` completes with both13-word passes equal
(3,909us /3,911us), two retired mappings, five guards, two fresh WB cache checks,
full inherited preservation and clean CPU exclusion release. The8KiB v2.0
OpRegion has mailbox mask`0x1f`; all six selected readiness/command words are
zero. Its complete resource is one ACPI-NVS entry at`0x8ad01000`, length`0x48000`.
The strict decoder accepts all native prerequisites; the saved report has
completed=yes and no collector attention. Offline return-integrity and recovery
checks pass, with the matching evidence independently backed up.

This accepts bounded read-only firmware observation only. Native initialization,
requester retirement and kernel-bit grants remain absent, and the wake writer
remains unlinked. Quiet firmware words are not a runtime firmware/SMM exclusion
contract; this acceptance does not permit forcewake, engine inspection, GPU
range transfer or physical tear-free flips.

## Opt-in cooperative EFI wake contract

The read-only captures above establish observations, not retirement of opaque
firmware. The new version1 **cooperative EFI boot policy** is an explicit driver
contract: conforming firmware yields boot-time GT requester control to the OS.
It assumes firmware/SMM does not independently reuse the OS-owned requester
while this bounded lease is live. It does not assert that SMM is absent, stopped,
or generally excluded. Uncooperative firmware lies outside this policy; visible
foreign activity or changed state faults the lease and retains uncertainty.
This is a stated platform assumption, not a conclusion drawn from quiet bits.

The [pinned Limine protocol's x86-64 entry contract](https://github.com/limine-bootloader/limine-protocol/blob/3a0526b700e356f0eac1b71a77697b3fd1c707a3/PROTOCOL.md#x86-64-1)
guarantees exited boot services on EFI entry. The
[UEFI boot-services contract](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html)
assigns resource management to the OS at that handoff; runtime services remain
separate. The diagnostic requires an EFI64 response with revision0 and the
pinned Limine11.4.1 identity. Staging verifies the pinned loader binary alongside
the source-qualified kernel. A loader identity string alone is not attestation
against a malicious loader; these are trusted boot-chain assumptions.

Only `build/intel-wake/srvros.elf` contains the native writer and new firmware-type
request. Ordinary, read-only observer and AML/AP builds keep the writer absent.
`--intel-wake-trial` selects that distinct binary and recipe; an ordinary image
cannot gain a writer by adding a command-line token. The kernel independently
requires all12 expected recipe fields exactly once, valid volume/release hex,
and no other tokens. This excludes AML, battery, audio, AP and interactive modes.
The immutable observer claim still grants no write authority by itself.

All ten independent admission facts are required:

- The dedicated compiled capability and exact explicit recipe.
- The pinned loader identity and EFI64 handoff.
- Exact Air7,2 firmware/GPU/resource profile and CPU0 serialization.
- A live participating-client exclusion lease and full native prerequisites.
- Fresh quiet firmware fields and zero low16 request/ACK state under that lease.

These facts assign initialization authority under the explicit cooperative boot
policy, retire participating boot/OS requesters for this transaction, and assign
kernel bit0 to its only driver owner. They are not firmware/engine/DMA/display/
audio reader grants for a GPU memory interval. Any missing fact refuses before
write-capable mappings or writes. The existing GT protocol still independently
refuses active initial request/ACK bits even if its caller supplies all grants.

The main capture saves and verifies `session-a.log` before entering the trial,
using the existing logger pause path; this also drains/disables periodic logging.
The guest displays recovery instructions before that checkpoint. Missing durable
preflight refuses the wake attempt. CPU0 interrupts stay masked only through the
bounded lease/mapping work and are restored on every handled outcome; optional
firmware/runtime callbacks and other GT drivers cannot be entered on that CPU.
An opaque SMI can still execute, as stated by the cooperative policy above.

Under the new lease, the adapter repeats firmware and request/ACK observations
and original-window/full-backing preservation. It prepares all three pages before
any store: request`0xa000` RW/NX/UC, ACK`0x130000` RO/NX/UC, and thread-C0
`0x138000` RO/NX/UC. Each requires a fresh matching MTRR/effective-UC context and
an absent, separately tracked virtual alias. The old observation/cache allowlists
stay unchanged. The only store destination is request register`0xa188`, using
the three masked values required by the core. Ordered UC stores are followed by
ACK/C0 checks. No engine register, reset/stop, GGTT PTE, private GPU buffer,
OpRegion readiness or command write is available.

Closing verifies own-bit retirement before unpinning, retires every known alias,
then repeats the firmware and inherited-state checks before releasing CPU
exclusion. Uncertain maps, writes, failed ACK/C0 or changed state retain the
appropriate mappings/exclusion until reset. There are no retries, forced lease
release or fallback request bits. Per-phase100ms, core-total300ms and finite
poll limits remain; physical acceptance additionally requires the complete
bracket within500ms. A closed but slow bracket is incomplete evidence, not
hardware uncertainty or a license to repeat it.

Receipts distinguish the cooperative contract, admission facts, fresh initial
request/ACK, issued-write attempts, confirmed writes, protocol state/reason,
map/cache/retirement counts and complete closeout. The strict decoder requires
the native firmware/GT/handoff/window/cache/backing/display chain, loader
provenance, a saved-preflight marker and independent UC derivation for all three
pages. `native_wake_complete` can accept this bounded lease; `smm_retirement`,
`engine_retirement`, `hardware_ownership`, `managed_range` and `physical_tearfree`
remain false. No native wake or broader ownership acceptance exists yet.

## Prepared first native lease trial

`93b497d37af24dae` is installed with recovery preserved and no reboot armed.
Its payload source`d5ff462d` passes the full host/core/profile/build matrix and
sanitizers; host-only decoder follow-up`8a528a0c` corrects padded hex handling
with independent DWORD bounds. The payload bytes are unchanged. All261
collection/staging tests, the decoder regressions, seven detected compiled
negative controls, partial-map preparation cleanup and exact-stage checkpoint/
refusal/return and missing-workspace behavior pass. Existing presentation and
Intel/Haswell profiles pass. The ordinary image matrix has no wake protocol or
new firmware-type request; the dedicated staged image has both.

This is ready for its first physical bounded cooperative wake lease. It is not
physically accepted yet. The unit record identifies recovery, exact source,
assumptions, run/checkpoint, preflight state and independently preserved evidence.
