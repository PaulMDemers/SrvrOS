# Broadwell cold-takeover prerequisites

The accepted [BCS reset/preservation trial](intel-bcs-reset.md#accepted-native-full-census-retry)
covers one already-disabled engine. Before designing a broader cold graphics
address-space takeover, inspect the other reset controls under the same live
wake owner. This isolated preflight is read-only apart from the existing three
GT wake-request stores. It does not reset an engine or transfer GPU memory.

## Exact read set and lifetime

| Word | Broadwell register | Offset | Existing alias role |
| --- | --- | --- | --- |
| 0 | RCS RESET_CTRL | `0x020d0` | RCS RO/NX/UC |
| 1 | VCS0 RESET_CTRL | `0x120d0` | VCS0 RO/NX/UC |
| 2 | VECS RESET_CTRL | `0x1a0d0` | VECS RO/NX/UC |
| 3 | VCS1 RESET_CTRL | `0x1c0d0` | VCS1 RO/NX/UC; captured GT3 only |
| 4 | BCS RESET_CTRL | `0x220d0` | BCS RO/NX/UC |
| 5 | GDRST | `0x0941c` | Separate RO/NX/UC page |

The original exact Air7,2/1626 firmware/resource/cache/loader gates remain.
All ten aliases are prepared before wake; only the requester page is writable.
The new reset page uses the existing effective-UC/resource predicate without
adding a reset writer. Fresh CR3/physical-translation and requester checks
precede each target load. Each sample has full core guards before and after the callback. The native
provider performs its own entry guard and checks requesters after the last load. Both samples keep all six raw DWORDs in order, including upper
control write-mask observations. No read clears or acknowledges a status bit.

A same-parent/epoch, address-sealed fresh92-word completion census with two
complete guarded sample callbacks precedes entry. The observer registers a
one-use parent borrow and verifies that early parent close is blocked without
stores. Copied/stale censuses, another dependent and foreign parent state refuse.
It records two six-word samples; failed reads retain their completed prefix and
attempt count. Partial results never become a completed observation. Borrow
return requires the same live parent and epoch; failure retains ownership.
The100ms observer,300ms owner and500ms outer limits remain unchanged, including
verified borrow return. No loop retries, cleanup resets or forced returns exist.

## Observation and quiescence are separate

A stable complete capture can observe busy or reserved state and still be a
valid readout. Its `quiescent` bit means only that the sampled reset controls'
active low bits and GDRST are clear, reserved low control bits are absent, all
five sampled rings have exact zero geometry, the prior census has no queued/
saved-wait/TLB-pending masks, and the two parser-idle indications are set.
Upper write-mask halves stay raw observations, not active request bits.

This bit is not proof of drained posted writes, prefetched work, flushed GPU
caches, retired display readers or an available GGTT interval. Every completion
obligation remains open(255), reset/PTE writes and managed pages remain zero,
and all hardware/range/tear-free grant fields remain false. A populated or absent
candidate PTE is still not a transfer. The [managed-memory plan](intel-managed-memory-plan.md)
continues to require a separately justified owner and exposure protocol.

## Image, recipe and receipts

`build/intel-takeover/srvros.elf` has its own firmware request and exact15-token
recipe. `srvros.intel-takeover=preflight` accompanies the existing wake, engine
and completion observation tokens. Reset/ready/shutdown/HDA/audio/AML tokens
cannot enter this profile. Older images and recipes keep their contracts.
Ordinary boot has no new observer request or writer.

Wake version13 has ten maps and up to104 completed observation words:92 census
plus12 reset-control words. The new version1 `intel-takeover-row` receipts contain
sample index, prefix count and raw values; the summary binds attempted/full
samples, counts, clocks, epoch, census callback coverage, registered borrow,
blocked close and verified return. Strict decoders independently derive stability,
control masks, geometry and sampled quiescence, and bind all counts/order to the
wake/census records. Native acceptance additionally needs original firmware,
cache, UC, loader and committed-checkpoint provenance. Host fixtures do not
accept the physical machine.

```sh
make -j2 all test-intel-takeover build/intel-takeover/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-takeover-preflight
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the actual local volume and partition table, then follow the
[Ubuntu physical workflow](ubuntu-device-agent-setup.md). In-guest progress and
saved results require no input. Safe saved/unmounted outcomes return to Ubuntu;
missing persistence or retained uncertainty halts visibly. Volume stays1% for
automated hardware trials. Physical acceptance belongs to the machine record.

## Register evidence

Intel's [Broadwell PRM Volume2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
November2015, SHA256
`09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
defines RCS RESET_CTRL on printed page288, VCS0/VCS1 on1704, VECS on1749, BCS on165
and GDRST on609–610. The GT3 restriction on the second video control is retained.
The pinned Linux v6.12
[reset implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_reset.c)
prepares participating engines before its reset operation. It is a protocol
reference only: this preflight imports no catastrophic-error bypass, forced
non-ready reset, retry or full-reset sequence.
