# Display-HDA DMA reader census

The Air7,2 native BCS/plane trial found display HDA00:03.0,8086:160c,
class040300/rev09, Apple011b, in D0 with PCI command0006. Bus mastering remains
enabled. Stable config, zero GPU ring controls and inactive visible planes do
not establish that HDA has relinquished DMA or firmware memory.

## Current contract

A separate CPU0-only image and exact12-token recipe select this read-only census.
The image requests EFI firmware type, requires EFI64/Limine11.4.1 and omits GT
wake and BCS writers. Ordinary/AP/AML images cannot invoke the native adapter.
Existing Air7,2 firmware/GPU/cache/backing/handoff qualifications must complete;
missing prerequisites refuse before HDA mapping or MMIO. The HDA observer needs
no new cooperative write/ownership assumption and grants no retirement.

The exact local endpoint is8086:160c rev09, subsystem011b:106b, BAR0c1610004 /
BAR1=0 (64-bit memory BAR atc1610000), matching the Linux resource capture's
16KiB extent. No BAR-size probing is performed. D0, valid PM capability,
memory decode and no I/O decode are required. Every access checks the current
endpoint, CPU0 interrupt serialization, held display-client exclusion, CR3 and
private alias identity. The declared extent must not overlap RAM/non-reserved
Limine entries; malformed/overflowing maps refuse. Unlisted PCI MMIO holes are
permitted only with the exact BAR and independently verified UC context.

Only the first page is mapped, RO/NX/PCD, in the private diagnostic address space.
A new narrow cache query admits only that physical page and requires unchanged
native PAT/MTRR context and MTRR/effective UC; older GPU predicates are unchanged.
Cache context is checked before mapping and around each pass. Failed/uncertain
alias installation or retirement retains the alias/exclusion until reset.
No controller state is restored or reset to make cleanup appear successful.

The core is one-use and bounded to200ms, inside a500ms adapter/exclusion bracket.
Zero, backward, frozen or expired clocks refuse. Two passes capture GCAP, GCTL,
INTCTL, CORB/RIRB control/base, DMA-position base and every supported stream's
control, cyclic length, last-valid index and BDL base. GCAP must expose1–8 total
streams; this is an evidence/log budget, not an OS device-support limit. Larger
or malformed topology refuses before stream reads. The access allowlist enforces
register widths and natural alignment; the24-bit stream control is read as a
16-bit low and8-bit high part. Status/acknowledgement, wall-clock/LPIB,
immediate-command and codec interfaces are excluded. Descriptor/ring/data memory
is not followed or mapped.

Active RUN/CORB/RIRB/position-DMA bits and changing programmed addresses are
observations: complete means two bounded full snapshots and safe CPU alias
closeout, not idle hardware. Derived masks union both passes; stability is
reported separately. No PCI/MMIO data writes, bus-master changes, power/wake,
reset, stream/ring stop, interrupt acknowledgement, codec commands or GPU PTE
updates exist. Bus-master enabled or disabled is reported, never used as a
standalone DMA-retirement proof. No firmware allocation becomes reusable.

## Source audit

[Intel HDA specification rev1.0a](https://www.intel.com/content/dam/www/public/us/en/documents/product-specifications/high-definition-audio-specification.pdf),
section3.1.2, specifies naturally aligned byte/word/dword access. Sections3.3.2,
3.3.7,3.3.13,3.3.18–3.3.27,3.3.33–3.3.35 and the stream descriptor base/length
sections define the selected capability, DMA and descriptor registers. CORBCTL
CORBRUN and RIRBCTL RIRBDMAEN are bit1; position-buffer enable is bit0;
SDnCTL RUN is bit1. The generic register layout is used only after the exact
captured display-HDA identity/resource/power checks. PCH analog HDA and other
models are separate devices and receive no acceptance from this diagnostic.
The source PDF/hash, extracted text and Linux BAR capture stay in ignored
`build/air-display-hda-20261106`; no machine firmware or private files are published.

## Trial and collection

```sh
make -j2 all test-intel-hda build/intel-hda/srvros.elf
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-hda-capture
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the local UUID and actual partition table, following the
[Ubuntu guide](ubuntu-device-agent-setup.md). The profile excludes audio playback,
AML, GT wake and BCS control. Before mapping, persist the full native prerequisite
checkpoint to session-a.log. The guest shows progress, saves diagnostics and
returns after handled safe outcomes. Unsafe persistence or retained mappings can
halt visibly. Q35 validates EFI/recipe, refusal-before-MMIO and persistence;
it cannot accept the native endpoint.

Strict collection binds all receipts to the saved prerequisite checkpoint,
native firmware/cache chain, exact endpoint and independent HDA-page UC proof.
It verifies read widths/counts, topology, derived RUN/DMA/stability masks,
budgets and alias/exclusion closeout. Preserve prior files, check offline media,
Ubuntu default/recovery and consumed one-shot before starting another run.
Candidate `8185aa9875c61e9a`, source `7d7dfa56`, is installed and not armed.
The full local host/build/sanitizer/negative-control and exact-stage VM matrix
passes; native acceptance is pending. The accepted BCS/reader baseline `32859f5868aedd46`
and recovery are retained. Worst-case native log projection fits the existing tail.
See the [unit readiness record](machines/macbookair7-2.md#read-only-display-hda-candidate-ready-native-acceptance-pending)
for matching payloads, media and independently SHA-backed evidence.

## Remaining ownership obligations

1. Account for HDA command/response/position/stream DMA, outstanding transactions,
   prefetch and firmware/SMM submission ownership before disabling bus mastering
   or reusing any referenced memory. Read-only snapshots cannot establish this.
2. Audit non-observed GPU readers, pending flips, stereo/left-eye surfaces and
   protected firmware allocations beyond the inherited display fence.
3. Establish exact GGTT/PTE backing ownership, cache/translation invalidation and
   completion ordering with retained/quarantined resources on uncertainty.
4. Only then qualify a managed GPU interval and bounded scanout flip/read-complete
   lifetimes. Existing private RAM/address leases grant no hardware authority.

The separately accepted BCS/plane observations in the
[unit record](machines/macbookair7-2.md) remain narrow evidence. Physical tear-free,
all-reader retirement and managed GPU memory are still unavailable.
