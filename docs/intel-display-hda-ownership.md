# Display-HDA cooperative requester policy

The accepted Air7,2 read-only census observes an idle display-audio controller
with PCI bus mastering enabled. It cannot distinguish quiet firmware from an
owner that may submit later. This new isolated trial makes that assumption
explicit and exercises a bounded requester inhibit/restore. It grants no GPU
memory, outstanding-DMA completion or all-reader retirement assurance.

## Platform policy and access contract

The opt-in `cooperative-efi-v1` policy assumes trusted firmware yields submission
to display HDA00:03.0 after EFI boot-services exit and does not alter its command,
power, BAR, DMA controls or buffers during the trial. Runtime firmware/SMM is
not proved absent. A violation faults the trial; the policy is unsuitable for
uncooperative firmware and cannot be inferred from matching snapshots.

The pinned [Limine entry contract](https://github.com/limine-bootloader/limine-protocol/blob/3a0526b700e356f0eac1b71a77697b3fd1c707a3/PROTOCOL.md#x86-64-1)
and [UEFI boot-services handoff](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html)
provide the trusted boot-chain context, with the same limits as the cooperative
[GT wake policy](intel-gt-access.md#opt-in-cooperative-efi-wake-contract).
EFI64/Limine11.4.1, exact13-token recipe, CPU0/IRQ serialization, participating
display-client exclusion, native firmware/cache/backing/window prerequisites,
exact Air7,2 endpoint and fresh UC qualification are all mandatory.

Intel's [330835-003 processor datasheet, volume2](https://cdrdv2-public.intel.com/330835/5th-gen-core-family-datasheet-vol-2.pdf),
section3.3.3, printed page89, defines the16-bit PCI command at offset4 for
00:03.0. Bit2 controls bus-master memory/I/O operations and MSI generation;
bit1 preserves MMIO decode. This is the processor's display-HDA function, not
the PCH analog controller. The source PDF SHA256 is
`22368ca371590a6eb5d85c02256936d6eb92badc27d2345e5d6ce8bee9c128b3`.
The [HDA census contract](intel-display-hda-census.md) defines the independent
MMIO allowlist and naturally aligned read widths.

The only native mutations are actual16-bit PCI command stores0006→0002→0006.
No DWORD read/modify/write touches adjacent PCI Status; no HDA MMIO store,
reset, stream/ring stop, codec command, acknowledgement, power change, GT wake
or GPU PTE write is available. Ordinary/read-only/AP/AML images omit this
requester writer and firmware request. Other devices receive no authority.

## Bounded transaction

Before any command store, persist the full prerequisite checkpoint. Then:

1. Acquire CPU exclusion and revalidate inherited firmware/backing/window state.
2. Capture two complete controller/stream passes. Require exact native command6,
   GCAP3001 (three output streams), GCTL1, INTCTL0, zero CORB/RIRB/position
   controls and addresses, and stream control040000 with zero CBL/LVI/BDL.
   Active, changing or differently configured hardware refuses before a write.
3. Recheck exact resources, D0, CPU/cache context and exclusion; mark a write
   attempt uncertain before dispatch; store command2 once and read it back.
4. Capture two more full passes under command2, enforcing the same idle state.
   This verifies inhibition during those observations; it is not a drain proof.
5. Only after successful fresh guards, restore the original command6 once,
   verify readback and capture two final full passes. Revalidate the original
   firmware/backing/window state, retire every CPU alias and release exclusion.

The core allows400ms total inside a500ms full adapter bracket. Each underlying
census retains its200ms limit and one-use object, mapping and clock checks.
There is no retry or guessed sleep-to-drain. Failed/ambiguous command dispatch,
readback, changed state or uncertain alias retirement retains the lease and
prevents restoration/retry; the guest saves diagnostics where possible and
halts instead of rebooting past retained state. Pre-write handled refusals and
clean completed transitions save/unmount and return automatically. A closed but
over-budget outer bracket remains incomplete evidence.

The transaction restores BME only while every observed DMA source remains
unchanged and disabled; it never transfers or frees referenced memory. A future
maintained inhibited-owner lifetime needs its own retirement/teardown contract.
The controller has no completion proof in these receipts for outstanding reads,
writes, prefetch or internal buffers. PCI BME clear/readback cannot substitute
for that proof or for VT-d, GPU readers and translation/cache ownership.

## Test and collection

```sh
make -j2 all test-intel-hda-owner build/intel-hda-owner/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-hda-owner-trial
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the actual local partition table and the [Ubuntu workflow](ubuntu-device-agent-setup.md).
No human input or audio playback is needed. The guest shows progress before
entry; complete results contain three ordered two-pass phases, exact command
readbacks, two attempts/stores and safe closeout. Strict collection derives idle
state from all raw words and binds the result to the saved native prerequisites,
independently decoded UC context and unchanged endpoint. Q35 must refuse before
any command store and persist/return; it cannot accept native ownership.

Host fixtures cover every policy fact, active or changed controllers, power/BAR/
cache/CR3 changes, failed/ambiguous stores, lost readbacks, failed restoration,
deadlines and retained aliases. Separate build/symbol and negative controls must
verify the writer stays absent from other images. Physical acceptance is pending;
the unit record will identify the tested source/release and matched session.
