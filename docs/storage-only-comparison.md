# Explicit storage-only write/flush comparison

`hardware_boot.py stage --laptop-probe-volume <local-UUID>
--storage-only-capture` selects a separate opt-in control. It uses the ordinary
matched kernel/initramfs, requires automatic return and refuses other diagnostic,
desktop, battery, AP and polling-policy profiles. Two brief1% speaker cues bracket
the measurements; there is no audio, desktop or battery workload in the measured
interval. Existing combined30-minute stress acceptance remains independent.

The guest shows progress through six balanced pairs, twelve samples total,
within180seconds including preparation, proofs and retirement. Every sample
writes32768sequence-dependent bytes, flushes and verifies every byte by fresh
media reads. The filesystem leg retains normal exFAT replacement/metadata and
workspace barriers. The direct leg re-proves the fresh file's exact contiguous,
exclusive range, parks the filesystem, revokes all public write aliases, claims
the whole device and performs two existing16KiB writes, flush and two16KiB reads.
It releases the claim, remounts and verifies again before any filesystem leg.
The old4096-byte battery scratch API and all transport deadlines,4KiB DMA,
polling, integrity checks and ordinary startup behavior remain unchanged.

The fixture has a unique fresh path; existing paths refuse. No public raw
length/offset API is exposed. Uncertain write, flush, readback or clock evidence
terminally invalidates the device identity and poisons filesystem reentry for
that boot. Claims and live transport state remain retained on uncertainty.
No timeout resets/retries hardware, frees pending DMA or reuses ownership.

Periodic logging is quiesced before measurement. A durable preparation checkpoint
retains fixture identity before parking. Fixed records emit only after the
comparison: sample wall phases, write-stage BOT aggregates, exact slowest CSW's
validated command and software event/service/resume clocks. Flush and readback
have independent aggregate validity. Each line stays below1024bytes and the
complete receipt below32768bytes, within the unchanged64KiB tail. No logging
writes occur inside measurement. The final normal logger saves exit.log before
clean unmount and Ubuntu return. A poisoned device cannot save a final report:
missing final evidence is failure/incomplete, never a pass.

`storage_only_result.capture_result` requires all twelve ordered receipts,
six exact leases, full byte CRCs, consistent wall/command/service coverage,
clean retirement and two quiet boundary cues. Offline collection must additionally
check exact final fixture bytes, every prior file hash, exfatprogs and independent
allocation traversal. Preserve matching failed/preparation evidence before another
trial. VM results validate mechanics only; native acceptance requires this unit's
actual return. Timings are descriptive controls: order, media state and filesystem
metadata differ, and software event observation is not hardware completion or
flash geometry. One trial does not prove a causal performance improvement.

`make test-storage-compare` exercises the real workspace/exFAT/VFS/BOT/block/claim
producer on disposable regular media at512/4096-byte sectors and4/128KiB clusters.
It asserts exact raw-byte containment, fresh readback, clean retirement,
rerun/existing-path/held/busy/clock refusal, partial write/flush/readback/deadline
quarantine, and strict receipt negative controls. Hardware service clocks are
modeled in this fixture; the actual USB command framing and completion validation
are production code.

Each512-byte payload sector and each16KiB command chunk has distinct data, in
addition to its sample sequence. A transport that aliases both reads and writes
of the second half is explicitly fault-tested and quarantined. Controller ring
checks partition exactly into event and empty observations. Empty checks between
clock samples need not produce a last-empty timestamp; a present timestamp must
be bounded by submission/observation and retain full sampled-service coverage.
These checks add no ring/clock reads and change no controller policy or limits.
