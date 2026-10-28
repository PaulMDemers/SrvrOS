# Bounded secondary-CPU ownership diagnostic

This experimental payload establishes AP ownership before any coordinated deeper
idle experiment. It is not SMP scheduling or a permanent idle policy. Native
acceptance is pending on MacBookAir6,1; no sibling machine is accepted.

## Isolation and lifetime

`make ap-probe-kernel` builds `build/ap-probe/srvros.elf` with separate main and
Limine request objects. The ordinary kernel has no MP request. The diagnostic
requires `srvros.capture=ap-ownership`; without it the kernel halts before storage
initialization. Limine processes the MP request before SrvrOS parses the command
line, so a command-line toggle on the ordinary image would not provide isolation.

Limine initializes AP architectural state, including APIC mode and MTRR
synchronization. The diagnostic itself installs per-AP GDT/TSS, a 16 KiB stack,
a separate 16 KiB emergency IST stack and a private IDT. These are architectural
setup operations; `policy_writes=0` means no CPU **power-policy** MSR writes.
The AP assembly never calls the scheduler, allocator, console or BSP handlers.
Every exception/NMI terminates that AP in a retained fault state; it never resumes
the interrupted diagnostic. Early entry still depends on Limine's valid mapping
and handoff contract, before the private descriptors are installed.

All slots, stacks and descriptors are static. The current PMM allocates only
`LIMINE_MEMMAP_USABLE`, retaining bootloader-reclaimable memory, including the
boot page tables and Limine's unclaimed AP stacks/park code. No timeout, final
acknowledgement or successful receipt authorizes reclamation. Memory stays
reserved until reset. There is no second diagnostic epoch or AP reuse in a boot.

## Protocol and bounds

BSP admission checks the MP response, unique APIC IDs and BSP membership before
publishing any entry address. Native admission requires Intel signature `0x40651`,
four CPUs with IDs 0–3/BSP 0 and the captured 64-byte MONITOR/C1 capability.
CPUID hypervisor guests are identified as `mode=vm`, allow two or four CPUs,
and may use PAUSE when MONITOR is unavailable. VM results do not accept native
MONITOR behavior. This profile is installed only after local Air6,1 inventory
verification; it is not a general Apple model detector.

Each AP independently reports its CPUID signature/APIC ID. A dedicated cache
line holds BSP commands; AP receipts occupy another line. Commands 1–32 are
strictly ordered. APs arm MONITOR, recheck the command and use only C1 MWAIT
(EAX=0, ECX=0, IF clear), with the BSP store as wake source. A one-millisecond
settle interval precedes each round. A counter records attempts to wait, not
proof of physical C1 residency or a guaranteed sleep on every exchange.

The BSP starts measurement before publishing each entry/command. Startup,
each acknowledgement and terminal retirement use the existing 20,000 us limit
and an independent finite spin bound for a stopped reference. IRQ masking keeps
these measurements out of scheduler-preemption time. No threshold changes.
Stale/duplicate acknowledgements keep waiting; future, late or faulted replies
fail. STOP is distinct from sequence numbers and always requests terminal
`cli; hlt` parking. A retirement receipt precedes the final HLT instruction and
is not evidence that storage is reclaimable. Failure still sends STOP to all
launched APs and reports each resulting state. The BSP saves logs, unmounts and
returns through the existing automated recovery path. A halt or absent completion
receipt remains failed/incomplete evidence.

## Validation and physical sequence

Host tests exercise sequence/deadline classification and strict saved-log parsing,
including duplicate/missing/corrupt receipts. Multi-CPU Q35 rehearsals check two
and four CPUs, single/over-capacity refusal, missing-start acknowledgement and a
real `UD2` exception on one AP. Injections are honored only in hypervisor guests;
injected configurations are temporary VM copies, never installed payloads.

Stage with `python3 tools/hardware_boot.py stage --laptop-probe-volume UUID
--ap-ownership-test` after building matched ordinary and diagnostic artifacts.
Use the [Ubuntu guide](ubuntu-device-agent-setup.md) for exact-stage rehearsal,
USB integrity/headroom checks, durable backup, install, one-shot and collection.
The harness defaults to four CPUs for this profile; `--ap-cpus 1|2|4|6` and
`--ap-inject missing|fault` exercise directed VM cases. `hardware_ap_owner.py`
requires a complete successful native receipt separately from a VM result.

No C3 hint, power measurement or energy-saving claim is part of this test.
After native ownership passes, a separately reviewed coordinated C3 experiment
may reuse the existing 32-attempt, 20 ms and BSP HLT-fallback protections.
