# MacBookAir7,2 current status

Updated: 2026-11-17 UTC. Reference A1466; partial desktop/device acceptance.
The [dated unit journal](macbookair7-2.md) retains exact-source evidence.

The inherited-framebuffer desktop is usable. Built-in keyboard and trackpad,
secondary click, window management/navigation, saved Home documents, brightness,
sensors, battery indication and earlier USB Ethernet/audio rounds have independent
unit evidence. Native tear-free scanout is still pending.

The **private CPU command arena now completes through the original full-GT
reset and memory lifetime on hardware**. Frozen source `37b54934` (runtime fix
`4b4ca01a`), release `a38270ffedf462fa`, run
`air-gt-arena-isolation-20261117`, session `session-000660767ae66ad9-00`.
All42 frozen validation slots and both exact-stage VM paths pass. The matching
native run is collected, strictly reparsed, externally banked and `DIAGNOSED`.
Main `4016abf0` was fetched unchanged; PR301 owns this work. Independent PR19
(Pro11,2 Wi-Fi) and PR379 (WebCore audit) remain untouched.

| Clock | Observed | Existing practical limit |
| --- | ---: | ---: |
| Preparation including guarded return | 219.986 ms | 250 ms |
| Largest original GT maintenance phase | 77.573 ms | 100 ms |
| Original GT total | 379.954 ms | 500 ms |
| HDA total | 399.672 ms | 750 ms |
| Outer total | 451.561 ms | 1000 ms |
| CPU arena setup / cleanup | 65 / 69 us | 100 ms each |

One actual kind2 owner advances epoch0→1, completes both reset pulses/five
cancellations and retains all32 fences and display/backing. Its original memory
child completes remapper DR/DW in37.001ms and System Agent GGTT invalidation
in2.342ms (83.129ms child). The four-page arena registers against that same
original owner/borrow/epoch/root. Held checks take384us; requester and logical
return require the child to return with storage pinned. Cleanup waits for actual
GT/HDA/outer retirement, verifies2048 zero QWords, removes all4 CPU aliases and
releases4 pages exactly once, with DMA61→61. Both temporary UC aliases and all15
original GPU aliases also retire; original parent clocks remain monotonic.

The startup collector initially refused the73,050-byte exit log because its
size selector omitted wire7 and applied64KiB. An explicit reviewed migration
adds only wire7+wake37 to the existing96KiB allowance. Frozen source, raw logs,
strict receipt/checkpoint parsers and recovery/media guards remain unchanged.
The original error, migration and untouched raw hashes are banked. A permanent
collector correction and regression cover wire1–7, unknown/malformed versions,
missing wake marker, exact96KiB boundary and unchanged manual acceptance.

All1,587 prior USB files preserve apart from expected metadata; three exact logs
yield1,590files/186sessions. Boot/preflight/exit are10,756/39,806/73,050bytes,
within96KiB. Offline fsck/allocation/writable/headroom, unchanged boot region,
installed/banked recovery, Ubuntu default and consumed one-shot pass. All715 run files verify; full run
and reviewed raw evidence are sealed outside `build` and survive distclean.
Currenta382/recovery9aa8/acceptedf0a8 and protected banks remain; USB is unmounted,
no further reboot is armed. Host volume is1%; this guest profile is silent.

This qualifies CPU-owned backing and retirement in one original full-GT
lifetime under the explicit practical policy. It does not accept the historical
stricter preparation/GT limits. All255 whole-device DMA/cache/prefetch/firmware/
display-reader obligations remain open, with zero managed GPU pages, GPU PTE
writes or submissions. Zeroed context storage is not an executable LRC. Enabled
entry on this hardware and repeatability remain pending. No native tear-free
grant follows. See the [dated result](macbookair7-2.md#native-full-gt-private-cpu-arena-completes).

## Next bounded scope

Bind exact private GPU intervals, DMA/cache and display-reader ownership to the
maintained original owner before enabling the existing mapping transaction.
Keep private CPU ring/context/completion storage pinned until GPU completion and
all readers retire; do not treat stable snapshots or this CPU-only result as
hardware authority. Shared subsystems remain the original-owner adapter,
command mapping/memory contracts, isolated image/recipe and strict collector.
Retain current recovery and independently accepted banks. No new trial is armed.

Earlier memory-child acceptance, failed host-only candidates and verified staging/
EFI duplicate cleanup are preserved in the [GPU qualification history](macbookair7-2-gpu-history.md)
and dated journal. Root free space is about1.3GiB and EFI207MiB; review capacity
before another payload, preserving dirty reproduction work and accepted banks.


## Host mapping-policy and display-range prerequisites

Base4016abf0 and independent PR19/379 remain unchanged; PR301 owns this work.
The host-only mapping transaction now uses existing selected aggregate aliases,
including250/500/750/1000ms for the full-GT practical policy. Both legacy policies
retain their original values and all nested100ms transfer/publication bounds.
Three policies pass26,712 composed cases, fatal sanitizers,21 existing compiled
controls plus4 practical clock controls and2KiB native compiler frames.
A220ms-aged original parent is admitted only under the practical policy;
legacy targets retain RAM and issue no stores. No native image links this core.

The new [primary-range classifier](../intel-bdw-primary-range.md) checks two
raw geometry/reader snapshots, tile padding, the documented128-PTE display tail,
capacity and separation from the complete protected fence. It passes18,216
cases, twelve compiled controls, fatal sanitizers and native compiler/image checks.
Sanitized saved-Air replay confirms the primary envelope/tail fit inside34MiB;
it supplies no fresh same-phase/owner proof or additional hardware acceptance.
Unknown layouts/readers refuse. Fixture JSON bytes and both new host targets/
suites are included in future frozen qualifications; all42 job slots remain.

The native provider gap remains outstanding pipeline/cache/prefetch and full
firmware/display-reader retirement. The new classifier cannot certify those
from saved or matching samples. Keep all255 obligations and the existing GPU
write refusal. No new hardware trial is prepared for this host-only change;
currenta382/recovery9aa8 and externally banked native evidence are unchanged.
Next specify the documented completion witness before integrating a native
range transfer/mapping lifetime, preserving the original backing and clocks.


## Private BCS submission/completion software

Base4016abf0 and independent PR19/379 remain unchanged; PR301 owns the
[private execlist submit protocol](../intel-bdw-bcs-submit.md). It writes only
the documented four DWORDs through a trusted provider, records known prefixes
and distinguishes a triggered submission from nonce completion and retirement.
All failures retain source/context/backing; success also keeps registration
until the independent original completion/retirement owner can release it.
It requires already initialized private context/descriptor, execlist mode,
reserved empty queue, mappings and visibility. No native image links it.

Three existing policies pass2,519 port cases each,24 compiled controls, fatal
sanitizers and2KiB native frames. The actual port and flush/nonce cores also
pass35 joined cases per policy, with fatal sanitizers and immediate/256th-poll/
failed completion, partial/uncertain writes, lost post-trigger verification and
held backing. A220ms-aged parent completes only under the selected practical
policy; legacy targets refuse before publication/stores. The existing flush
core now uses selected aggregate aliases too, retaining all original legacy
values,2,326 cases per policy,24 controls and independent10ms waits.

Broadwell INSTPM bit5 is reserved (PRM2c826); the older MMIO sync-flush shortcut
is unavailable. Neither reset ACK nor the narrower remapper operation supplies
the still-missing whole pipeline/cache/prefetch/reader ownership witness. No new
hardware trial/stage/arm is prepared. Currenta382/recovery9aa8 and all original
native evidence remain unchanged, with all255 GPU obligations open. Next is the
real bootstrap ownership mechanism, followed by private LRC/page-directory
initialization and native queue/nonce/context retirement under that same owner.


## Active native ownership/context block

Starting from113e2b31, integration main4016abf0 fetched unchanged. PR301 owns
this bounded work; PR19 Wi-Fi and PR379 WebCore remain independent. Audit the
Broadwell bootstrap DMA/cache/display-reader boundary and implement the next
independently verifiable context-initialization prerequisite. Touched scope:
private BCS context format, original command arena/native adapter, host fixtures
and exact image/recipe/collector only as required by the resulting hardware
question. Do not infer a mapping grant from the port/nonce model or old receipts.
Publish measured evidence and initialize executable context only after its exact
mapping/address-space contract is admitted. Keep currenta382/recovery9aa8 and
all accepted banks; no new physical run is pending. A new bounded qualified trial
may reboot automatically at1% under the user's standing authorization.


## Private CPU context-image candidate

The next bounded [context trial](../intel-bdw-bcs-context.md) uses the same four
private CPU pages and original full-GT/memory/arena owner. It materializes a
complete ring/HWSP/register/completion image before GPU exclusion, verifies all
bytes initially and at both held-source guards, then verifies and erases it
only after actual enclosing retirement. Existing exact alias/zero/once-free
cleanup remains. The image/recipe/wire8 are separate; older gates stay strict.
This tests CPU preparation, with prospective GPU addresses deliberately unmapped;
actual mapping/context execution and DMA/cache/display-reader ownership remain
unaccepted and all255 obligations stay. Qualification and hardware are pending.

The codec passes4,463 decode/word-corruption/input cases; initial actual adapter
coverage includes72 retained-prefix faults on two modeled entry routes, fatal
sanitizers and2KiB native frames. Frozen validation will retain all42 prior slots
and add context image/model slots, then exact-stage VM paths before hardware.
Currenta382/recovery9aa8/acceptedf0a8 and protected banks remain. The old installed
434e duplicate was removed only after all files matched its independently verified
external payload bank; that bank and all native evidence remain. EFI now has207MiB
free for the next candidate. No user input is required for this silent trial;
a qualified candidate may reboot automatically at1% under standing permission.
