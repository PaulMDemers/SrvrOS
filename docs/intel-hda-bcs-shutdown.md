# Original-owner HDA/BCS shutdown diagnostic

This separate Air7,2 diagnostic qualifies BCS shutdown inside the original
cooperative HDA/GT lifetime. Native acceptance is pending. It does not grant
reset, DMA retirement, managed GPU memory or tear-free display support.
Read it with the [shutdown contract](intel-shutdown.md),
[HDA/GPU lifetime](intel-hda-gpu.md) and
[local machine evidence](machines/macbookair7-2.md).

## Why a separate image

The standalone shutdown image observed an already-disabled BCS with zero
geometry; the requester image observed enabled inherited geometry and refused
before preparation. Those boots also differed in startup phase: standalone
shutdown followed `/init`, while the requester capture ran before `/init`.
These observations do not establish that HDA inhibition caused the difference.
The new image runs before `/init` and records both actual awake entry providers
under one original HDA/GT owner. It does not read engine MMIO before wake.

`--intel-hda-shutdown` selects `build/intel-hda-shutdown/srvros.elf`, a distinct
firmware request and wake version22. Its exact16-token recipe adds
`srvros.intel-hda-bcs=joint-owner-v1` to the standalone15-token shutdown recipe.
Only this exact recipe defers `/init`; missing, duplicate and foreign tokens
retain ordinary init readiness and refuse the diagnostic writer. The old
shutdown, joint-claim, requester and ordinary desktop images retain their own
recipes and authority boundaries.

## Lifetime and bounded operation

The original static HDA companion owns its root mapping and CPU exclusion. It
captures the quiet endpoint, performs the true PCI Command halfword transition
6→2, and captures the inhibited endpoint before the original GT owner opens.
The existing guarded wake, five-engine census, reader observation and idle BCS
shutdown run inside that original owner. Both eight-DWORD BCS entry groups must
agree with the corresponding six-DWORD BCS census groups before any disable.

The enabled branch permits only the existing single `0xf000` DWORD store to
BCS control at offset`0x2203c`; it preserves captured ring start and length.
Already-disabled entry uses the zero-store observation branch. No Stop Ring,
reset-control, requester, PTE, ring-start or address-transfer writer is present.
The native store checks the original adapter/result/owner and current time
immediately before I/O. Original bounds remain100ms BCS,150ms shutdown/readers,
300ms GT,400ms HDA and500ms outer lifetime. No clock origin is refreshed to gain
admission time.

Full post-engine, plane and HDA config preservation precedes borrow return,
GT close and GPU alias retirement. Only then may HDA restore2→6, verify its
quiet original endpoint and retire the root mapping. Ambiguous writes, failed
postconditions, lost mappings or uncertain restoration retain the original
objects and exclusion. A safe saved refusal returns automatically; retained
uncertainty halts visibly. Guest progress explains this without human input.

## Evidence and collection

`intel-hda-bcs-hda` and `intel-hda-bcs-summary` retain the existing lossless HDA
phase schema under distinct prefixes. Their `complete`, `gpu_complete` and
`attached` fields remain zero: no interval claim was created. The enclosing
version22 wake result is complete only when the shutdown and full HDA/GT
retirement succeed. The separate collector derives this combined result.

Two `intel-hda-bcs-entry` rows record each initial eight-DWORD BCS group,
including raw observations which fail census agreement. `intel-hda-bcs-io`
records attempted groups, agreement bits, final-store check count, observed
store time, previous BCS time, original shutdown/GT origins and clock refusal.
These receipts supplement the existing pre/post census, blitter, reader,
shutdown epoch and GT clock records. Packed backing PTE version3 is explicitly
bound to this image; it changes encoding only and grants no memory authority.

The strict decoder requires exact image, schemas, coverage, phase order,
original clocks, entry agreement, inhibited reader endpoint and clean resource
retirement. Native completion additionally requires independent firmware,
cache, backing, handoff and checkpoint evidence. Host fixtures and unsupported
VM results cannot supply physical acceptance. The preflight and final log must
carry identical16-token boot options and the new pre-init marker, with no
`init: started` event. Earlier image receipts cannot substitute for this run.

## Local validation and physical trial

```sh
make -j2 test-intel-hda-shutdown build/intel-hda-shutdown/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-hda-shutdown
python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
```

The host suite exercises both branches, all HDA read/endpoint/cache/alias
prefixes, source mismatch, final-store clock refusal and post-store retention.
Before hardware installation, qualify native instructions and image isolation,
remaining stack frames and static object lifetimes, saved-tail capacity, exact
staged VM save/unmount/return and missing-workspace refusal. Follow the
[Ubuntu staging/collection workflow](ubuntu-device-agent-setup.md); retain the
installed recovery and save the exact local run record before reboot.
