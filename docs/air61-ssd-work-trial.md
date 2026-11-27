# Air6,1 internal SSD write trial

This trial uses the dedicated 4 GiB unformatted `srvros-work` partition at
the end of the Air6,1 internal SSD. Its first phase writes one 4 KiB page
between two guard pages and issues one ATA flush. The second phase verifies
the page and both guards after a reboot, with zero ATA writes or flushes.
SrvrOS keeps the EFI and Ubuntu partitions unmounted in both phases.

## Prepare the layout offline

Take and verify a backup of important Ubuntu files on separate media before
editing the partition table. Start the Ubuntu live USB in **Try Ubuntu** mode.
Leave the internal volumes unmounted and mount only the evidence USB. Open
a terminal in the `air61-offline-work` directory on the evidence USB and run:

```sh
sudo python3 ./air61_work_layout.py check ./layout.json
```

The check verifies the model, PCI and ATA identities, original GPT extents,
partition and filesystem UUIDs, offline state and proposed layout.

After the check passes and the backup is verified, apply the offline shrink
from the same directory:

```sh
sudo python3 ./air61_work_layout.py apply ./layout.json --backup-dir "$PWD"
```

The helper saves both original GPT copies and the before/after partition
descriptions to the evidence USB. It checks and shrinks ext4 before altering
the GPT, preserves the EFI and Ubuntu starts and identities, and checks ext4
again afterward. Keep the evidence USB connected and boot installed Ubuntu
after a pass. If any step fails, retain the error and backup files for review
before retrying.

## Run the native trial from installed Ubuntu

With the planned GPT in place, run `tools/air61_ssd_work.py seed` as root with
the layout JSON, a new persistent evidence directory, and the mounted
evidence USB's `srvros-work` directory. The tool verifies the realized layout,
seeds three pages through the exclusive work-partition device, and saves the
exact GPT proof. The Linux seeding write is separate from the SrvrOS test.

Build the kernel and stage with `tools/hardware_boot.py stage
--air61-ssd-probe-volume VOLUME --air61-work-proof FIXTURE`, using the
volume and full proof from the seed result. Rehearse that exact stage in Q35
and require foreign-platform refusal. Follow the device guide to verify and
retain Ubuntu recovery and confirm the evidence USB is clean and writable
offline before each boot. Install by explicit release ID and use a one-shot
boot for the write phase. Ubuntu stays the default.

After the return, validate the first phase using `tools/air61_ssd_work.py
check-write` with the same arguments. If that passes, boot the same release
once more for readback. Use `tools/air61_ssd_work.py collect` in Ubuntu to
compare both receipts, both GPT copies, the guard pages and the payload
against the reference. Save the results with the trial record before
declaring native acceptance.
