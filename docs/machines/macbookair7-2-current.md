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
and dated journal. Root free space is about1.5GiB and EFI61MiB; review capacity
before another payload, preserving dirty reproduction work and accepted banks.
