# Pinned non-posted requests and display-reader audit

The accepted [HDA inhibit lease](intel-display-hda-lease.md) prevents participating
software from restoring bus mastering while a dependent is pinned. This separate
Air7,2 diagnostic adds a useful outstanding-request check and expands display
surface coverage. It does not grant DMA retirement or reusable GPU memory.

## Documented completion scope

Intel's [330835-003 processor datasheet, volume2](https://cdrdv2-public.intel.com/330835/5th-gen-core-family-datasheet-vol-2.pdf),
sections3.3.25–29, printed pages98–100, defines the display-HDA PCIe capability at
00:03.0 offset70, version1 integrated endpoint0091, device capabilities10000fc0,
control78 and read-only device status7a. TXP bit5 means non-posted requests have
not completed; zero means their completions have been received. Source SHA256
is `22368ca371590a6eb5d85c02256936d6eb92badc27d2345e5d6ce8bee9c128b3`.

The core reads six config DWORDs at34/50/60/70/74/78, validates the exact linked
PM/MSI/PCIe chain and capability identity, refuses FLR-in-progress/reserved
status bits and requires TXP clear. The78 DWORD read observes both control and
status; neither is written or acknowledged. TXP set refuses immediately: no
retry, arbitrary quiet delay or reset supplies a substitute completion proof.
Two samples with command2 remain conditional on the explicit cooperative EFI
submission policy. They do not account for posted writes, internal prefetch,
firmware/SMM, analog HDA or another requester. Memory stays protected.

## Display coverage and lifetime

Intel's [Broadwell PRM Volume2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
printed pages1221–1223,1251 and1446 define live surface and stereo left-eye
addresses. Existing primary/cursor/sprite controls and programmed bases retain
the [earlier reader contract](intel-blitter-retirement.md#read-only-reader-coverage).
The exact RO/NX/UC pages are70000/71000/72000; each pipe reads13 DWORDs at
180/19c/1ac/1b0/1bc/080/084/0ac/280/29c/2ac/2b0/2bc. PipeA is always-on;
B/C use the same power-qualified inherited-display context as the accepted
three-pipe census. No new power/wake permission is inferred from a register.

The new one-use core runs pass0, a separate complete HDA revalidation, pass1,
and finish while a live sealed borrow holds the HDA owner. Each pass starts with
PCIe observations, then reads all three planes. Fresh endpoint/CPU exclusion and
UC checks surround temporary mappings. Lost identity, map uncertainty, changing
state, failed reads or deadlines keep the parent borrow/inhibition/exclusion
until reset. No forced unpin or restoration follows a failed dependent.

Complete receipts contain12 config reads plus78 plane reads, six retired RO
aliases and12 fresh independent cache checks. The reader bracket is150ms inside
the unchanged400ms owner/500ms full bracket; pins never extend those limits.
The original HDA four-phase232-read/two-command16-store protocol is unchanged.
There are no GT wake, display, ring, HDA MMIO, adjacent PCI Status or GPU PTE
stores. Old lease/owner/read-only images cannot enter the new recipe.

Raw controls, both surface eyes and programmed/live values survive in receipts.
The collector independently derives enabled masks and address-mismatch masks,
checks ordering around the separate HDA revalidation and binds shared words to
the earlier inherited-display capture. Address equality is not an exhaustive
pending-flip proof. Disabled controls do not retire prefetched/outstanding work.
No engine stop, FBC/PSR/cache retirement, translation flush, backing transfer or
`scanout_handoff` assurance is supplied.

## Isolated automated trial

```sh
make -j2 all test-intel-readers build/intel-readers/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-reader-audit
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the actual unit's volume and partition table and the
[Ubuntu workflow](ubuntu-device-agent-setup.md). The dedicated image/request
requires all14 recipe tokens, including `srvros.intel-readers=read-only` and the
cooperative HDA lease. Before command stores, it saves the same strict native
prerequisite checkpoint. On-screen progress says no input is required. Audio is
off, volume1%. Q35 must refuse before writes, persist, unmount and return;
missing workspace halts. VM evidence cannot accept native TXP or display reads.

The next physical run needs native receipt/checkpoint/UC and lifetime validation,
complete saved logs, prior-file preservation, clean offline media/recovery and
consumed one-shot checks. A retained dependent or missing completion is not a
pass. Remaining ownership work includes posted-write completion and prefetch,
all-engine/flip/other-reader handoff, exact backing and translation/cache
completion before a managed interval or physical tear-free presentation.
