# Air7,2 aperture and candidate GGTT entry observation

The explicit `--intel-window-capture` profile extends the read-only
[cache audit](x86-cache-audit.md) and inherited display/backing observation. It
reads16 GGTT entries immediately after the **whole34MiB inherited fence**.
This supplies original values for a future one-page mapping/restore experiment;
it grants no owned slot, private GPU mapping, engine exclusion or writer.
An absent, scratch or stable PTE is not evidence that a slot is available.

## Geometry and access

The pure geometry decoder follows the mobile Broadwell
[datasheet330835-003](https://cdrdv2-public.intel.com/330835/5th-gen-core-family-datasheet-vol-2.pdf),
section3.2.24, MSAC byte at PCI offset0x62. It accepts the documented aperture
size encodings, checks the64-bit prefetchable GMADR shape, alignment,39-bit
address reach and extent overflow. Illegal encodings refuse rather than emulate
hardware rounding. There are no BAR-sizing or configuration-data writes.

The reference adapter additionally requires the exact current Air7,2 firmware,
GPU/PCH/backing profile, full32-fence vector,8MiB GGTT and256MiB aperture. MSAC
and endpoint fields must remain identical through the study. The first observed
index is8704;16 entries occupy128 bytes in BAR0+0x811000. No aperture pixel/data
page, physical PTE target or unobserved stolen-memory allocation is dereferenced.
Entries remain opaque64-bit values. `bit0` is a raw bit census, not a writable or
owned-memory claim.

Every guard runs with the temporary window unmapped. It checks live kernel CR3,
absence of an old window, exact profile/address identity, a **fresh** CPU0 cache
snapshot equal to the completed native audit and effectiveUC at PAT index2,
then independently revalidates all inherited1356 PTEs/fences. Two bounded samples
use one temporary read-only/NX/PCD mapping each; high/low/high reads detect torn
entries. All entries must agree between samples. There are48 successful32-bit
reads per sample,100ms phase/gap limits and finite callback counts. Callbacks
must themselves be bounded; the core cannot interrupt a hung callback.

After unmapping, repeat guards and endpoint checks before accepting the sample.
Changed state, clocks or vectors refuse. A failed unmap or unexpected mapping
after a failed map latches retained uncertainty and forbids reuse until reset.
The receipt distinguishes candidate-window mappings from a retained backing
check window. No page-table RAM, inherited backing or physical PTE target is
freed. Only a complete final guard can report final backing preservation.

## Receipts and limits

The summary records raw MSAC/GMADR, decoded extent, full protected fence,
window map/retention counts, five fresh cache/backing checks and explicit zero
PTE writes/aperture mappings. Both vectors are saved in canonical base64 with
prefix-progress counts, little-endian byte hashes, zero and bit0 counts. A
strict independent decoder checks the complete vectors, geometry, bounds,
timing, hashes and native display/backing/cache prerequisites. Refusals and
partial/retained work never become a native observation pass.

The combined profile first logs its cache-only phase, which maps no proposed
pages. The later window phase explicitly maps the named GGTT alias read-only.
It still maps **no aperture data page** and installs no GPU PTE. Neither phase
can accept a managed range or actual scanout publication.

Host coverage includes legal/illegal geometry, refusal with a shortened fence
or foreign table shape, complete and changed/torn vectors, callback failures,
clock rollback/deadlines, cache changes, failed-map ownership and failed unmap.
Q35 exercises real CPU cache reads and native GPU refusal; it cannot accept
native GGTT contents, ownership or a hardware restore.

```sh
make -j2 all test-intel-window test-x86-cache
python3 tools/hardware_boot.py stage --laptop-probe-volume VOLUME_SERIAL \
  --intel-window-capture
python3 tools/hardware_boot_smoke.py --probe-check
python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace
```

Native installation/testing follows the [Ubuntu guide](ubuntu-device-agent-setup.md).
The no-input diagnostic saves/unmounts and returns automatically after handled
outcomes; persistence failure visibly halts. Retain independent recovery and
collect the exact matching session before another run. Native acceptance stays
in the [machine record](machines/macbookair7-2.md).
