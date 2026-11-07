# Maintained display-HDA inhibit lease

The accepted [requester round trip](intel-display-hda-ownership.md) proves a
bounded command16 inhibit/restore on Air7,2. A maintained lifetime additionally
needs to prevent restoration while another operation depends on inhibition.
This boot-only lease retains the same exact endpoint, cooperative EFI policy,
CPU0 exclusion, D0/resources/UC checks and400ms core/500ms full bounds. It grants
no outstanding-DMA completion, firmware/SMM retirement or managed GPU memory.

## Owner and dependent lifetimes

`intel_hda_owner_open` captures the I/O descriptor once, consumes a zeroed owner
object and issues an owner/epoch token only after command6→2 and two complete
idle-state observations. The live owner is not a completed transaction.
`pin` creates a separate address-sealed, one-use borrow object for each dependent;
`check` needs that live borrow and repeats the guarded two-pass HDA observation.
`unpin` retires only that borrow. A copied, stale, wrong-owner or already-released
borrow cannot decrement another dependent's pin. `close` refuses without I/O
while any pin remains; after all dependents retire it performs the accepted
command2→6 readback/observation/closeout. The original one-shot API uses open/close
without pins, preserving its three-phase receipt and isolated recipe.

The owner, context and borrow storage must stay alive and unmoved until clean
retirement or reset. Do not zero/reuse a borrow during a live owner lifetime.
These are trusted CPU0 kernel-driver contracts, not protection against arbitrary
kernel writes into their structs. Owner copies and stale epochs fail before I/O;
callers cannot replace the captured callbacks by resupplying a descriptor to
close. Pins never restart or extend the400ms deadline. Revalidation/power/cache/
root/alias/write uncertainty faults the owner and retains inhibition/exclusion;
there is no forced unpin, rollback, retry or expiry-based restoration.

The same [cooperative EFI assumption](intel-display-hda-ownership.md#platform-policy-and-access-contract)
assigns submission to the boot driver during the bounded lease. Pinning governs
participating software dependents; it cannot stop opaque firmware/SMM or prove
absence of prefetched/outstanding transactions. No descriptor/data memory is
followed or freed, and every inherited allocation remains protected.

## Separate qualification image

`build/intel-hda-lease/srvros.elf` has its own EFI firmware request and exact
13-token recipe, replacing the earlier token with `srvros.intel-hda=lease-trial`.
It cannot enter the old recipe; ordinary/read-only/AP/AML/wake images cannot
activate this native writer with a command-line token. The same isolated CPU0
main path persists a prerequisite checkpoint before any command store.

The automatic qualification runs open, pin, attempted close while pinned
(which must refuse without another store), a separate read-only dependent check,
unpin and close. Four full two-pass phases observe command6/2/2/6,232 reads total.
A separate lifetime receipt records the epoch, pin/check/unpin, blocked close and
continued live state. It must end with pins0, checks1, blocked_closes1, exactly
two PCI command halfword attempts/stores, all four HDA aliases retired, inherited
state preserved and exclusion released. Each census retains its200ms bound.
No arbitrary wait is used as a transaction-drain proof.

The receipt prefix is `intel-hda-lease-`. Endpoints/commands retain explicit hex;
raw controller/stream words use bounded unsigned decimal to keep all native
prerequisites and four phases within the unchanged65,536-byte saved tail.
Strict collection validates all row widths/order/coverage, derived idle state,
phase timing, lifetime counters, native firmware/cache/UC context and the saved
checkpoint. Successful phase `reason=0` headers use typed warning classification;
refused/active/retained/malformed phases and inconsistent raw words still fail.

```sh
make -j2 all test-intel-hda-lease build/intel-hda-lease/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-hda-lease-trial
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the actual unit/partition table and the [Ubuntu workflow](ubuntu-device-agent-setup.md).
Q35 must refuse before command stores and save/unmount/return; it cannot accept
native inhibition. The physical trial needs no input, uses volume1% with audio
off, and saves progress/results before automatic safe return. Retained ownership
or unsafe persistence halts. Physical lease acceptance remains pending.

After this lifetime is accepted, remaining work includes outstanding DMA and
firmware/prefetch accounting, other GPU/display readers, exact backing ownership
and translation/cache completion. A successful pinned lease alone grants no
`scanout_handoff` assurance or physical tear-free presentation.
