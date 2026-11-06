# Intel boot reader and range handoff

The new `--intel-handoff-capture` profile qualifies participating CPU-client
exclusion and exact read-only preservation on Air7,2. **It does not take hardware
ownership or grant a native managed range.** The reference's16 candidate GGTT
entries are populated; matching snapshots cannot make them free.

## CPU exclusion

`display_access` is a boot CPU lease, before desktop clients or APs. Console
flushes and public pixel/blit operations, presenter preparation/transactions,
and brightness provider operations enter a nonwaiting reference count. Acquisition
requires zero in-flight operations. An existing presenter owner must also be
absent; the adapter checks this only after acquisition, when no participating
new transaction can start. This is not a runtime/SMP display takeover protocol.

While held, new operations refuse before framebuffer stores or provider callbacks.
Text still enters the model, boot log and serial log. Deferred text and clear
margins remain damaged and flush after release. Emergency console output does not
bypass the lease. Epoch/owner tokens reject stale release; uncertain retirement
leaves access blocked until reset. No busy operation is forcibly cancelled.

The immutable Intel boot-client policy separately rejects the old Intel
initializer before PCI writes or MMIO mappings. CPU exclusion does not cover
firmware, GT engines, display/audio readers, another CPU or another DMA device.

## Range admission

`scanout_handoff` requires a live CPU exclusion token, an audited provider's
same-boot firmware/engine/DMA/reader/cache assurances, the full inherited fence,
exact endpoint geometry and the original candidate PTE values. Its revalidation
callback must independently maintain those assurances, not infer them from
stable entries. It opens only the supplied transferred interval in
`scanout_space`; all other pages remain unavailable and the inherited fence
stays protected. The current transaction limits a transfer to16 pages, separate
from the arena's256MiB bound and unrelated to desktop window limits.

An active range pins the CPU lease. Closing requires fresh original-state proof
and a fully closed arena; exposed, nonretired or quarantined leases prevent
release. Wrong tokens and missing assurances do not enter a writer. These are
trusted-kernel adapter contracts, not a security boundary against other kernel
code calling the underlying arena directly.

The native provider supplies **zero hardware assurance bits** and refuses any
attempt to assert positive assurances. Native qualification expects missing
mask31, managed_pages0, pte_writes0, aperture_mappings0 and hardware_ownership0.
Host fixtures exercise positive transferred-range admission with explicitly
synthetic assurances; they do not accept any physical ownership.

## Automated qualification

```sh
make -j2 all test-intel-handoff test-intel-window test-console test-backlight test-display-present
python3 tools/hardware_boot.py stage --laptop-probe-volume 37D8-C5D0 --intel-handoff-capture
python3 tools/hardware_boot_smoke.py --probe-check
```

Use the local unit's actual UUID. The distinct version1 handoff recipe preserves
all existing window/cache payload recipes. It runs the existing exact-profile
cache/display/full-backing/window readouts, then pauses CPU clients and attempts
pixel, fill, blit, scaled-blit, brightness, presenter-start and legacy-initializer
operations. It repeats all five fresh window guards and both raw16-entry samples,
including full1356-entry backing/fence comparisons, and matches the original
candidate values. It then exercises range refusal and releases/flushes the CPU
lease. No forcewake, engine control, PTE or aperture-data write is introduced.

A complete summary requires selected/acquired/idle/snapshot/preserved/released1,
blocked127 and missing31. The parser rejects duplicate/missing fields, inconsistent
ordering or unsupported authority claims. The collector additionally requires the
complete native window/cache/backing/display receipts. Native attempts alone do
not prove each provider was initialized: positive causal callback/side-effect
checks are host fixtures, especially for the unavailable native presenter and
brightness provider in this isolated profile.

Q35 without the Intel device must report qualification incomplete, acquire no
lease and save/unmount/return as a handled refusal. It cannot stand in for the
native positive-preservation round. Missing workspace/completion or uncertain
retirement is not a pass. Handled outcomes return to Ubuntu automatically; an
unpersistable report retains the existing visible halt. No human input is needed.

## Remaining hardware ownership work

Before a real native range grant, an audited adapter must establish firmware
handoff and every GT/display/audio/DMA reader's retirement, with documented
register-access conditions and bounded recovery. CPU-client exclusion and
read-only preservation are prerequisites, not substitutes. Only then can the
provider transfer a concrete interval and bind a real writer, exact PTE restore,
translation invalidation and reader retirement to the existing publication core.
The native tear-free desktop and flips remain pending.

## Candidate qualification checkpoint

Air7,2 candidate `8b9e57e43ae025d3` (`7e33d1a5`) passes the complete local
host/core/profile/build/docs gate, sanitizers, compiled negative controls and
exact-stage Q35 saved-refusal/missing-workspace paths. Existing VirtIO recovery,
Intel window/DMA/lifetime and independent Haswell profiles pass. It is installed
with recovery retained and no reboot armed. Its native session
`00065f8fd143d595-00` now qualifies selected/acquired/idle/snapshot/preserved/
released/complete1, blocked127 and missing31, with completed=yes and zero collector
attention. Full inherited backing/fences and original16-entry values survive the
under-exclusion fresh checks. Queued text resumes and the refused legacy initializer
attempt is captured. This accepts bounded participating CPU-client/preservation
behavior; unavailable native presenter/brightness providers retain host causal
tests. Hardware ownership and managed pages remain0. Prior-file preservation,
clean offline filesystem/allocation checks, recovery and return state verify.
The [unit record](machines/macbookair7-2.md) identifies exact payloads, checkpoints
and independent backups. Firmware/engine/DMA/display/audio handoff and actual
range transfer remain pending before a native writer or tear-free flip.

The next [GT access/requester preflight](intel-gt-access.md) audits mandatory
initialization and owned wake retirement without admitting native writes or
borrowing a firmware request as engine-access permission.
