# Original-claim GPU requester hold

The separate `intel-gpu-hold` image adds a bounded PCI requester control to the
accepted [joint HDA/GPU lifetime](intel-hda-gpu.md). It is a prerequisite for
native memory transfer. It neither transfers an address range nor implements a
PTE, ring, flush, invalidation or page-flip writer. Ordinary boot and older
images cannot enable this operation by adding a token.

## Exact operation and source

The Air7,2 GPU is 00:02.0, 8086:1626 revision 9, subsystem 106b:011b. Its captured
MSI90→PMd0→AFa4 chain is validated before each successive fixed capability read.
The original [interval consumer](intel-interval-claim.md#registered-interval-consumer)
is registered before callbacks, while the original reset/re-arm child, five
engine holds, GT owner, HDA inhibition and CPU exclusion remain active.

1. Capture the full nine-DWORD PCI/AF profile; require Command 7.
2. Freshly read the exact Command/Status DWORD and compare it with that capture.
   Then take the final original-clock/source check and write only Command's
   halfword at offset 4, value 3. Memory and I/O decoding remain enabled.
3. Capture the full profile again, requiring Command 3 and unchanged identity,
   status, PM and capability fields. If AF TP is set, poll only the Command and
   AF DWORDs until TP clears, with a 32-poll/10 ms cap inside the original budget.
4. Freshly validate the inhibited Command/Status DWORD, take the final admission
   clock and write the exact original Command 7. Capture the full restored
   profile and require TP clear before returning the consumer.
5. The claim still owns its closing original-entry observation. The re-arm,
   reset, preparation, GPU and HDA owners must retire through their original
   preservation/return sequence. No consumer can return or bypass those owners.

The original preparation 100 ms, GT 300 ms, HDA 400 ms and outer 500 ms limits
never restart or increase. Every accepted direct-I/O timestamp advances the
original claim watermark as well as the consumer; return cannot accept an older
clock from a nested path. Full original-source checks bracket the operation;
native per-read/store admission also validates the original root, exact aliases,
requesters, profile and final clock. A pure source predicate works during the
serialized claim callback, without granting I/O or reentry permission. Copied or
rebound consumers, changed original entries, lost aliases and source/clock loss
refuse. Failed or ambiguous stores are never replayed or optimistically restored.
The consumer and parent stay pinned through retained uncertainty.

Only this image permits the intentional observed Command 3 during full backing
comparison, bound to its original registered hold, expected command, known issued
prefix, original joint owner and CPU context. Every other field still has to
match. Restoration returns the endpoint to exact ordinary comparison before
parent retirement. This exception is not a global profile relaxation.

## Completion scope

Broadwell PRM [Volume 2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
printed pages 1171–1173 defines the real PCI Command BME field. Its MMIO mirror
on pages 1075–1076 also describes a GSA implementation in terms of MSI gating.
The [pinned PCI implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/pci/pci.c)
and [AF definitions](https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/pci_regs.h)
separate bus-master configuration from AF pending observation. Do not infer that
this control blocks every internal graphics/display access.

Command readback confirms the observed configuration transition; AF TP clear
covers its pending indication. Neither proves posted writes, internal prefetch,
GPU pipelines, cache/TLB visibility, display readers or firmware/SMM retirement.
All eight completion obligations remain open (255), assurances remain 0 and
managed GPU pages remain 0. The [managed-memory plan](intel-managed-memory-plan.md)
still requires actual interval/private-backing and completion witnesses before
an owned mapping. No populated candidate is made free by this trial.

## Receipts, staging and acceptance

The distinct 22-token recipe adds `srvros.intel-gpu-requester=hold` to the joint
recipe. It uses a separate firmware request and wake v21. Re-arm v3 binds
62 plus pending-poll live checks without widening v1/v2's limits. The canonical
700-byte receipt preserves all three raw nine-word buffers, phase times and all
32 partial poll buffers/times/counts. It retains raw prefixes on failures and
separately records both fresh pre-store command admissions, issued counts,
original clock/epoch/consumer metadata and native PCI-read accounting.
The strict collector joins these bytes to the original claim and joint lifetime;
a missing result, changed source, false completion or unknown authority refuses.

```sh
make -j2 all build/intel-gpu-hold/srvros.elf test-intel-gpu-hold test-intel-gpu-hold-adapter
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-gpu-hold
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use this unit's actual UUID/partition table and the
[Ubuntu workflow](ubuntu-device-agent-setup.md). Exact-stage QEMU must refuse
unsupported hardware before stores, save a matching checkpoint/result, unmount
and return; missing workspace halts. Local tests exercise real ownership cores,
actual native-adapter source and every new read/store/last-admission prefix.
These checks do not accept physical timing, requester behavior or GPU memory.

The physical test is silent and unattended, with on-screen progress and a durable
prerequisite checkpoint. Use volume 1%. Safe saved outcomes return automatically;
retained uncertainty or unsafe persistence halts visibly. Acceptance requires
all original source/clock/preservation/retirement checks, exact command and AF
receipts, a matching saved completion and verified recovery/media on return.

### Air7,2 candidate qualified for a native trial

Source `6fba6e6c`, release `8d88625b30a0cfab`, passes the full local 26-image
gate, three fatal sanitizer suites, all five compiled negative controls, the
278-prefix native adapter matrix, native store/frame/isolation checks and the
unchanged log budget. Exact-stage QEMU verifies unsupported-hardware refusal,
saved checkpoint, unmount and automatic return; missing workspace halts. These
results qualify the test machinery, not native GPU completion or memory transfer.
The Air record owns the installed recovery, volume and USB evidence. This
candidate is banked but uninstalled and unarmed. The next silent physical trial
must verify its command transitions, pending samples and original claim lifetime
before recording acceptance; all eight memory/completion obligations remain open.

The first native attempt of this candidate returned safely with **incomplete**
evidence: the preceding takeover observer found an enabled inherited BCS ring,
so preparation and this requester hold never entered. No GPU Command stores
were attempted. See the Air record for the matched session and original collector
version-binding defect. Fixing that host decoder alone will not accept the hold.
