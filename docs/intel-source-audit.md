# Source-bound requester, cache and display-reader audit

The separate v35 diagnostic extends the [original source engine hold](intel-source-engines.md).
It observes live PCI requester, CPU0 cache context and inherited display readers
inside that one original, maintained five-engine-held child. It is a prerequisite
observation, never a DMA drain, cache flush, translation invalidation, reader
retirement or GPU interval transfer. All 255 completion obligations remain open;
all assurances, managed GPU pages, PTE writes and hardware ownership stay zero.

## One original lifetime

The exact 15-token `source-audit-v1` recipe and dedicated firmware request/image
enter before user processes. Source kind, post-reset epoch, original provider,
requester registration, private RAM holds, CPU exclusion, root and all 14 exact
UC aliases remain bound to the existing source. Older recipes/images do not gain
the audit. The original 100/300/400/500 ms bounds and ten 10 ms/256-poll readiness
phases remain unchanged. There is no new child budget or deadline refresh.

While PCI Command is 3 and all five readiness requests are acknowledged, capture
two complete nine-DWORD GPU PCI/AF profiles with capability-prefix validation
before each next access. Require the original inhibited stable fields, BME clear
and a fresh AF pending-clear value. The first inhibited parent sample may have
observed pending before its bounded drain; that earlier transient value is not
required to persist after drain completion. Within each sample, independently bracket the existing nine-DWORD
primary/cursor/sprite vector on each of three admitted pages. Each load receives
a fresh same-origin clock and pure live-parent check; the exact page group has
fresh full source/root/map/requester validation on both sides. Compare all words
to the inherited reader vector captured in the original enclosing reset lifetime.

Re-read the complete architectural CPU0 PAT/MTRR context in each sample and
compare it to the original completed cache observation. The read metadata and
bounded times are saved; the complete original cache context stays in the raw
preflight. This establishes observed equality only. It does not prove buffer
coherence, GPU cache retirement, an exhaustive physical-alias inventory or the
absence of later firmware/SMM activity. Display HDA remains independently held
and covered by the enclosing full endpoint preservation/retirement.

The parent restore path also demands the exact original completed audit after
its final clock: original identity/epoch, both complete raw profiles, reader
vectors, cache snapshots and metadata must still agree. This final check is
pure; it performs no new hardware access or deadline refresh.

Failure after the existing engine/requester stores retains the entire parent
chain and private RAM. Do not cancel through uncertain ownership, restore by
replaying an unknown store or grant memory because observations matched.

## Admission and transport

Only v35 removes one redundant full opening engine guard in the post-reset
census dispatcher: no census target access occurs before `completion_snapshot`
performs its own fresh full engine guard. Both actual sample opening guards,
all per-group requester checks, closing guards, root/map/source checks and final
store clocks remain. This avoids paying twice before the same first read; it
adds no target or authority and does not extend a deadline.

All audit storage, including cache-snapshot scratch, is touched before the
excluded native census. Reporting occurs after the bounded operation. The
original version-1 400-byte audit vector carries two exact PCI/plane vectors,
partial counts, cache-read metadata and sample clocks. Version 2 adds two exact
private-buffer range records per sample, making 528 bytes; each record carries
physical base, complete padded page count, hypothetical PAT index, MTRR/PAT/
effective type and the number of evaluated boundary points. Summary and data
versions must agree. The earlier version-1 native evidence remains historical
observation evidence, not full-buffer-policy acceptance. A separate canonical 528-byte
reset receipt transports the unchanged twelve reset-control/ACK records,
including each raw word, poll count and four timestamps. The v35 receiver
reconstructs those records in a checked internal view for existing validators;
saved raw logs remain untouched. Old receivers reject the new recipe.

Qualification must cover both modeled original routes, every clock/source/root/
registration fault, each raw read failure/drift, both cache failures, receipt
mutation/splicing, native object/store/map isolation, conservative saved-log
budget and exact-stage EFI/MBR VM refusal/persistence/unmount/return. VM success
does not accept these native observations. Native acceptance requires the new
exact source/release, same run's prerequisite checkpoint, full receipt chain,
original reader/control preservation and offline workspace/recovery checks.

## Automated trial

```sh
tools/host-env.sh make -j2 all usb-image test-intel-source-audit build/intel-source-audit/srvros.elf
python3 tools/hardware_boot.py stage --intel-source-audit --laptop-probe-volume YOUR_UUID
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the unit's actual filesystem/partition table and the
[Ubuntu device workflow](ubuntu-device-agent-setup.md). Bank the immutable exact
payload and evidence before installation; preserve Ubuntu default and known-good
recovery. The guest displays progress, requires no human input, and saves,
unmounts and returns after safe outcomes. Retained uncertainty halts visibly.
Use 1% volume. Collect the matching session before another test.

The subsequent ownership work must still establish documented outstanding DMA/
prefetch completion, cache/translation ordering and every affected display/
firmware reader before a managed interval or first GPU PTE/submission operation.
Matching AF/plane/cache samples cannot satisfy those separate contracts.

## Qualified source and native observations

Frozen source `4387351b` / release `f1a9627dff866ee0` passes local qualification.
The complete producer/core baseline is `f90fba95`; the receiver-only positive
origin/start correction and zero-field mutations then pass four affected tests
with byte-identical C/Makefiles/userspace/shared sources and a 40-image build check.
Nine fatal ASan/UBSan adapter tests, eleven compiled weakness controls, fourteen
historical raw classifications,119 staging cases, seven 2 KiB native frames and
forty-four saved-tail projections pass. Maximum projected tail is 64,905/
65,536 bytes including reserve/header. Exact-stage EFI/MBR no-store refusal,
persistence, clean unmount/return and missing-workspace halt pass. Immutable
payload and qualification evidence are banked outside build. The matching Air7,2 kind 2 native result below supplies raw/checkpoint/full-chain
and offline return evidence for that one observation lifetime.
See the [unit checkpoint](machines/macbookair7-2.md#qualified-held-requester-cache-and-reader-audit).

The receiver requires positive parent/origin/start order for claimed completion.
An earlier parser-only zero-start reproducer remains preserved as failed review
evidence; the native C producer already prohibited that state. No deadline,
register gate, target access or hardware grant changed in the receiver correction.

## Air7,2 original kind 2 native observation accepted

Source `4387351b` / release `f1a9627dff866ee0`, session
`session-000660355cc7ac9d-00` accepts one original already-disabled entry observation
lifetime: two PCI/AF profiles, all54 reader words and two37-read complete CPU0
cache contexts, with the original completed audit checked before PCI restoration.
Audit 1,247 us, engine 2,475 us and requester 3,137 us
fit the original enclosing deadlines. Preparation is 99,552/100,000 us.
The complete original diagnostic source/RAM/root/HDA/CPU chain, exactly-once
2,712-page release, four padded patterns, DMA57→57 and offline workspace/recovery
preservation pass;1,512files/160sessions remain. Raw and complete physical proof
banks survive build cleanup. See the [unit result](machines/macbookair7-2.md#native-held-requester-cache-and-reader-audit-accepted).

Enabled entry on v35, delayed pending/ACK branches and repeatability remain
unaccepted. Equality observations add no DMA/prefetch/cache/translation/firmware/
display-reader retirement assurance or managed-memory authority; all255 obligations
and zero assurances/managed GPU pages remain. Native tear-free is still pending.

## Full private-buffer CPU cache-policy prerequisite

After integrating main `00ab2d90`, the version-2 audit additionally checks both
complete 1,356-page pinned buffers against each fresh CPU cache snapshot. The
pure `x86_cache_range` decoder validates the starting page and every enabled
variable-MTRR start/end within the allocation. These are the only policy-change
boundaries above 1 MiB; equal first/last pages alone are insufficient. Its
conservative result requires uniform MTRR, PAT and effective types throughout,
with at most 33 evaluated points and no output change on refusal. Source-bound
native admission requires WB/WB/WB at hypothetical PAT index 0. Exact physical
bases and padded lengths stay bound to the original RAM holds; matching range
results remain required before PCI restoration. Four successful range queries
are necessary for claimed version-2 completion. Original deadlines stay fixed.

The architectural rules remain those of Intel SDM Volume 3A, sections 12.11–12.12
and table 12-7 ([Intel combined manual](https://cdrdv2-public.intel.com/671200/325462-sdm-vol-1-2abcd-3abcd.pdf)).
C tests compare the bounded decoder against an independent exhaustive per-page
oracle, including an interior UC hole with matching WB endpoints, overlap,
alignment, disabled MTRRs and physical-width/overflow boundaries. The native
receiver independently decodes every page against the complete original cache
receipt and checks exact range identities and boundary counts. Incomplete
receipts retain partial observations without conferring successful range policy.

This is hypothetical CPU cache-policy eligibility. It does not inspect actual
alias PAT bits or prove alias completeness, CPU/GPU coherent visibility, cache
flush/translation completion, outstanding DMA/prefetch retirement, firmware
exclusion, display-reader retirement or GPU interval ownership. No new hardware
access target, cache instruction, PTE, submission or display writer is added;
all 255 obligations and zero assurances/managed GPU pages remain. The matching version-2 kind2 native observation is now recorded below; other
routes and actual hardware ownership remain unaccepted.

## Version-2 local qualification and native observation

Frozen `2f5d0ee3` / `679a03e543968655` integrates main00ab2d90 and passes full host/core/USB/
docs with40 images; fatal actual-adapter9 plus separate hold/cache-oracle
sanitizers;13 compiled weakness controls,15 unchanged historical classifications,
119 staging cases, actual native objects/seven2KiB frames and48 saved-tail
projections (65,088/65,536 bytes). Exact-stage EFI/MBR refusal/save/unmount/
return and missing-workspace halt pass. Immutable payload and exact qualification
proofs are independently banked. The matching version2 kind2 result below accepts one observation lifetime; the
earlier version1 kind2 pass remains historical, without full-buffer acceptance.
See the [unit checkpoint](machines/macbookair7-2.md#qualified-full-buffer-cache-policy-trial).

## Native version-2 kind2 observation accepted

Air7,2 source `2f5d0ee3` / release `679a03e543968655`, session
`session-000660374d3a6447-00`, accepts one original already-disabled kind2 lifetime.
Four original-bound complete1,356-page buffer queries match at hypothetical
PAT0 with MTRR/PAT/effective WB; each has one evaluated boundary point. The strict
native receiver independently decodes every page against the complete original
CPU context. Audit 1,276 us and preparation 99,117/100,000 us
meet unchanged limits; all enclosing diagnostic holds, mappings, RAM patterns,
exact2,712-page release, DMA57→57 and offline file/recovery integrity pass.
Source-specific raw/physical evidence is banked;1,515files/161sessions remain.
See the [unit result](machines/macbookair7-2.md#native-full-buffer-cpu-cache-policy-observation-accepted).

This accepts hypothetical CPU policy observations only. Actual alias attributes/
completeness, ordered CPU/GPU visibility, DMA/prefetch, GPU cache/translation,
firmware/display-reader retirement and exclusive GPU interval transfer remain
open. All255 obligations and zero assurances/managed GPU pages remain; native
tear-free is pending. Enabled version2, delayed branches and repeatability need
separate real evidence.
