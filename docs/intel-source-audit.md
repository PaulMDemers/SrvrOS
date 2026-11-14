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
before each next access. Require the original inhibited profile, BME clear and AF
pending clear. Within each sample, independently bracket the existing nine-DWORD
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
canonical 400-byte audit vector carries two exact PCI/plane vectors, partial
counts, cache-read metadata and sample clocks. A separate canonical 528-byte
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
