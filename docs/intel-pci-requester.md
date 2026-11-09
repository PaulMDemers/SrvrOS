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
receipts are not passes. Native acceptance still needs matching machine evidence.

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
