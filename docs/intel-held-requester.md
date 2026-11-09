# PCI requester capture during maintained engine preparation

This is a read-only software composition of the
[preparation action](intel-engine-prepare.md#maintained-read-only-action-boundary)
and the existing [PCI AF profile](intel-pci-requester.md). It has no native
adapter, boot recipe, configuration writer or GPU memory grant. Acceptance of
the standalone requester capture does not accept this combined lifetime.

## One preparation borrow

`intel_pci_held_observe` consumes an original zero-initialized result during the
preparation callback. It binds the original preparation, parent, epoch and sole
active registered borrow. The preparation's transient action-active state must
remain set; copied, cancelled, completed or stale preparations refuse. The core
preparation validator owns the exact takeover/census and held-state checks.
The sampler never registers, returns or substitutes a borrow. Success leaves
the preparation responsible for its second held snapshot, reverse cancellation,
full preservation and verified return.

The sampler captures the preparation's original deadline origin. Its own start
time is diagnostic only: reads and all surrounding validation must fit within
the preparation's existing100ms budget, including the work already performed.
No fresh100ms allowance is created for the action. The provider must use the
same monotonic timebase as preparation. Provider callbacks must be
bounded; software clocks cannot interrupt a hung hardware access.

## Exact read sequence and failures

Two samples read DWORD offsets0,4,8,2c,34,90,d0,a4,a8, using the existing
Broadwell GPU/MSI/PM/AF profile. The captured prefix is checked before each next
fixed access. Malformed identities, links, AF layout or reserved status bits
prevent subsequent reads; no arbitrary capability pointer is followed. Provider
guards and live-owner/clock checks surround individual loads. A successful load
is counted before checking its value or exit validation, so a malformed word or
lost exit retains the completed raw prefix. Failed reads are attempted accesses,
not fabricated completed words.

The result retains raw rows, counts, full-profile masks, BME/TP/FLR observations,
raw stability, completion and first failed clock diagnostics. Completion is
historical capture evidence, never reusable authority for a live hold. A valid TP change
can complete capture. TP clear, BME clear or raw stability is not a requirement
for a valid observation and is not comprehensive DMA completion.

The caller must propagate sampler failure from the read-only action. Earlier
preparation request stores already established uncertainty; failed callback or
lost action/parent/guard/clock state does not authorize forced cancellation,
returning the pin or freeing aliases. This is a trusted kernel callback contract,
not isolation against arbitrary writes to the preparation object.

## Native integration remains separate

There is no native caller in this change. A future provider must validate the
actual device/firmware/resources, exact current mappings and requester state;
it must not promote host callbacks into positive hardware assurances. It also
needs its own exact recipe, progress, raw receipts and strict collector before
any combined physical trial. Existing installed/recovery payloads are unchanged.

Standalone preparation and requester timings cannot simply be added or treated
as a pass. Their previous93,246us and27,036us measurements show why the entire
combined lifetime must be measured and qualified within the unchanged bound.
Any optimization must preserve the independent admission and exit checks.
Another observation-only hardware trial is justified only by a specific
composition question and a qualified bounded provider.

No reset, BME/FLR/PTE write, maintained firmware/DMA/cache/translation/reader
ownership or GGTT interval transfer follows. All eight completion obligations
remain open. A reset-capable composition needs a separate epoch/control-state
protocol; this read-only action cannot supply it. The
[tear-free roadmap](intel-tearfree-roadmap.md) orders the remaining work.

## Local qualification

The actual preparation/action/sampler cores pass158 directed cases under fatal
ASan/UBSan. These include every attempted read boundary, malformed/floating
prefixes, copied/stale preparations/results, reentry, final-clock lifetime loss,
expired shared budgets and combined read/profile failure with provider exit
loss. Five compiled controls detect missing original-action validation, a fresh
sampler budget, skipped exit validation, missing final-live checks and missing
loaded-prefix validation. A weak initial deadline-test assertion was strengthened
after its control survived; the corrected test requires stopping on the first
expired read even though the sampler's own elapsed time is below100ms.

Existing action/preparation/requester suites, host checks, ordinary build and
isolated no-action preparation image build pass. Symbol checks show the held
sampler absent from both linked images; its compiled core imports only the
preparation validator and existing PCI prefix/offset definitions, with no borrow
management or writer imports. Documentation checks pass. Evidence remains local
and independently backed up outside build. There is no native/VM acceptance of
this sampler and no staged or installed combined candidate.
