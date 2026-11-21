# Air7,2 PCI Advanced Features requester capture

The accepted [BCS reset](intel-bcs-reset.md#native-provider-boundary-acceptance-2026-11-09)
does not retire outstanding DMA or transfer a GGTT interval. This next isolated
image captures the GPU's PCI command and Advanced Features (AF) status under a
live GT owner. It adds no PCI configuration, BME, FLR, GDRST, engine, translation
or memory writer. Existing ordinary startup and older diagnostics are unchanged.

## Captured device and registers

Ubuntu on the reference Air7,2 identifies GPU00:02.0,1626:8086, revision9,
subsystem011b:106b. Its conventional capability chain is MSI90 → PMd0 → AFa4 → end;
AF length6 advertises Transactions Pending (TP) and Function Level Reset (FLR).
There is no PCIe Device Status capability in this chain. Ubuntu's live i915
snapshot is inventory evidence only, never an SrvrOS result or ownership grant.
The machine-local raw256-byte capture and pinned references are retained in
`build/air-requester-audit-20261109/reference` and its independent backup.

The exact read-only DWORD offsets are0,4,8,2c,34,90,d0,a4,a8. Each of two samples
retains all nine raw values in that order. Device/class/revision/subsystem,
cap-list/memory decode, linked IDs/next pointers, PM version and AF length/caps
must match the captured profile. In the AF control/status DWORD only TP bit8 is
accepted; FLR control and reserved bits must be zero. The already captured prefix is validated before each next fixed capability/
payload read; a malformed pointer or absent AF prevents the payload read.
No arbitrary capability pointer is followed, and no PCIe register is substituted for the AF status.
The aligned AF read includes its two captured zero padding bytes, never writes
or acknowledges them, and retains the raw value rather than discarding it.

Linux v6.12 defines the [AF layout and TP/FLR fields](https://github.com/torvalds/linux/blob/v6.12/include/uapi/linux/pci_regs.h)
and its [PCI implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/pci/pci.c)
uses the AF control/status word for its pending test before FLR. These references
justify register interpretation, not copying FLR, timeout fallback or ownership
policy. SrvrOS implements no AF reset or pending wait in this capture.

## Lifetime and receipts

The dedicated16-token recipe adds`srvros.intel-requester=read-only` to the
read-only takeover prerequisites. Its firmware request/image and version15
wake receipt are distinct. Only the existing wake-requester MMIO alias is
writable; all engine/reset/context aliases remain RO/NX/effectiveUC. The GPU
PCI function is admitted by the existing exact identity/firmware/resource gate.
No sibling device or additional requester is enabled by this image.

Entry requires the original returned takeover result and its complete stable
two-batch92-word census, on the same live parent/epoch with no other dependent.
The capture independently registers one original one-use borrow and verifies
that premature parent close is blocked without a store. Fresh clock/parent
checks bracket full provider backing/endpoint validation; after a guard callback
finishes, clock and parent identity are checked again before sampling. Each
PCI read is preceded by exact current requester-alias translations and fresh
requester/ACK/thread checks. Full provider exit loss poisons native ownership;
raw completed prefixes remain counted and no alias is freed through uncertainty.

The original100ms dependent,300ms owner and500ms outer bounds remain. Failed
clock samples, exact raw prefix counts, completed profiles, derived bus-master/
pending masks, raw stability, borrow return and parent retirement are serialized.
A read-only failure may return through a still-valid original parent; copied,
stale, detached or unknown ownership remains pinned. Verified late return is
incomplete timing evidence, not a fabricated live borrow.

Observation completion does **not** require TP clear, BME clear or raw stability:
a valid TP transition is useful capture evidence. Both raw snapshots and derived
masks remain visible. Only complete, profiled prefixes contribute derived masks.
The strict collector verifies raw coverage/order, profile/masks, clocks, original
preflight/epoch binding,122 total census/takeover/PCI observations, exact recipe,
cache/resource/firmware prerequisites and clean return. Missing or inconsistent
receipts are not passes. Native acceptance needs matching machine evidence; the result below binds
this source to its Air7,2 session.

## Ownership limits and next step

A clear TP sample does not establish that all posted writes reached memory,
retire GPU prefetch/cache/translation/display readers, or maintain exclusion of
firmware/SMM submissions. BME observation does not disable bus mastering. FLR
capability does not authorize resetting this display function. All eight
completion obligations (mask255), zero native assurances and zero managed pages
remain unchanged. A future requester gate must establish the scope/completion
of its exact operation, preserve live display/backing and own a fresh lifetime;
it cannot promote these two samples into a DMA-drain or GGTT-transfer token.

```sh
make -j2 all test-intel-requester build/intel-requester/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-requester
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Follow the [Ubuntu device workflow](ubuntu-device-agent-setup.md) for local model,
UUID/table, immutable stage, recovery, one-shot boot and matching collection.
No human input is needed; display progress and save results. Safe saved/unmounted
outcomes return to Ubuntu; uncertain ownership or failed persistence halts
visibly. Keep volume1%. Host/VM evidence does not accept native capture.

## Local qualification (2026-11-09)

Source`fe6677c5`, release`3ff42f33e6c01391`, passes the full local source gate
(installed-root omitted),19-image isolation,202 fatal sanitizer cases,10 compiled
semantic and10 native object/staged controls. Tests detect missing prefix/exit/
translation checks, wrong AF offsets/header/command gate/read width or mapping
permissions, premature return, copied/foreign lifetimes and clock failure.
Exact immutable MBR EFI Q35 refusal/checkpoint/save/unmount/return and missing-
workspace halt pass. The actual16-token parser matches four saved logs.
Full-width projection62,044/65,536 includes the unchanged1,536-byte allowance.

Installed/durable/immutable, accepted reset and recovery payloads verify;1,425
prior USB files/131 sessions preserve with clean offline integrity checks.
Volume1%, Ubuntu default, no reboot armed; full evidence is independently backed
up outside build. These host/VM checks do not accept native PCI state or clear
a DMA/cache/ownership obligation. Physical capture was pending at this local checkpoint; see the result below.

## Native capture acceptance (2026-11-09)

Source `fe6677c5`, release `3ff42f33e6c01391`, session
`session-00065fdf108c49b4-00`: both ordered nine-DWORD PCI snapshots pass the
exact MSI90 → PMd0 → AFa4 profile. Capture completes in **27,036/100,000us**;
the wake bracket is223,039us. Both samples match, bus-master mask3 indicates
BME enabled in both, TP pending mask0 and FLR control0. All18 raw reads are
retained. The original borrow returns, all10 aliases retire and exclusion
releases; reason, clock fault, uncertainty and retained state are zero.
The existing wake protocol has three MMIO writes; this capture adds zero PCI
configuration, BME/FLR/reset/PTE writes. Limits remain100/300/500ms.

Strict native receipt/checkpoint and collector checks pass without attention.
All1,425 prior files preserve apart from expected metadata; three new logs give
1,428files/132sessions. Offline filesystem/allocation/writable/headroom checks,
installed/durable recovery/default and consumed one-shot pass. Volume1%; no
reboot armed. Full raw evidence and independent analysis survive distclean in
the reviewed-evidence backup. Checkpoint:`build/air-requester-audit-20261109`.

This accepts one read-only native capture, not DMA drain or repeatability.
TP clear does not retire posted writes, prefetch, caches, translations or other
readers, and BME remains enabled. All eight completion obligations (mask255),
firmware/SMM ownership, GGTT transfer, zero managed pages and no physical
tear-free grant remain. A future operation needs fresh same-lifetime authority.

The later [held requester sampler](intel-held-requester.md) is a separate
software composition under the maintained preparation borrow. The standalone
observer above still acquires and returns its own dependency and cannot be
nested unchanged. Its accepted native result does not qualify the held sampler.
