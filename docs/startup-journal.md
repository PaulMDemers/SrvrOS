# Independent startup journal

The opt-in Pro11,2 startup diagnostic uses profile `wifi-fw-v141`. It retains
the v140 USB enumeration and workspace/logger admission path, then saves and
returns before firmware upload or network capture. Ordinary RAM boots keep
AHCI disabled. The diagnostic initializes the existing AHCI transport before
USB enumeration and retains seven startup checkpoints in a preallocated EFI
file. This extra initialization changes the hardware startup conditions and
requires its own native validation.

The Ubuntu preparation step creates a fresh root 8.3 file on the local EFI
partition, writes 66,048 bytes and flushes them. The read-only FAT walker checks
both FAT copies, the exact file length and a contiguous allocation, then compares
raw file bytes with the mounted file. A 128-byte initramfs descriptor binds disk
sector count, GPT header hash, EFI boot-sector hash, partition bounds, file
extent and a fresh 16-byte nonce. The native writer admits exactly one raw AHCI
disk matching those fields and the file header. A raw block claim excludes
filesystem mounts during the diagnostic. The writer changes only the file's
existing data sectors. Allocation, FAT entries, directory entries and Ubuntu
files are handled by the Ubuntu filesystem during preparation and installation.

The file contains a 512-byte immutable header and eight 8,192-byte slots. Seven
slots record before/after USB enumeration, before/after workspace mount, logger
admission, save/unmount and terminal return. The last slot stays reserved. Each
slot starts empty with its nonce and slot number. The kernel checks the entire
slot before writing, snapshots at most 7,680 console bytes, writes/flushes/reads
back that payload, then writes/flushes/reads back the commit sector. The commit
contains slot, phase, readiness flags, tail length, total console count, payload
SHA-256 and metadata SHA-256. A partial next slot leaves earlier complete slots
readable. A write failure disables further checkpoints. A later boot cannot
reuse populated slots. A host-created empty record means armed with no kernel
checkpoint, rather than completed diagnostics.

The flags record workspace active (1), logger started (2), init ready (4),
timer running (8), log saved (16) and workspace unmounted (32). The last two
phases retain workspace/logger admission flags from phase five and append the
save/unmount outcome. Console snapshots are tails and may omit earlier output.
The descriptor nonce binds them to their host run record and staged image.

`make test-startup-journal` exercises the actual C writer, disk/extent admission,
partial writes/flushes/readback, immutable previous records and independently
decodes emitted C bytes. `tools/startup_journal_smoke.py` boots the exact staged
kernel/initramfs through UEFI/GRUB/Limine with the same descriptor and a sparse
AHCI disk seeded with the pinned header, boot sector and journal. It compares
all allocated disk bytes outside the journal against the seed, and exercises
present/missing workspace, bad marker and exhausted logger capacity. The EFI
file's native FAT allocation is verified separately on Ubuntu. VM AHCI receipts
and software checks do not accept the native controller or diagnose a previous
physical failure.

After return, collect the independent journal before attempting another boot.
Preserve raw bytes before decoding, then collect any matching USB session and
verify prior workspace files, metadata, allocation and recovery payloads. Missing
or invalid final checkpoints leave the trial incomplete. A complete startup
journal can establish a workspace or logger refusal even when USB has no session.
It cannot establish Wi-Fi connectivity or same-boot reconnect acceptance.

If independent admission fails, startup now continues through USB enumeration
and workspace/logger admission. A working USB logger saves the refusal rows in
its complete boot/exit transcript and allows a checked return after save and
unmount. If both destinations refuse admission, the diagnostic holds before
firmware/network work. The host's initial armed file remains available, while
runtime details require an admitted destination. An independent write failure
after admission still holds with earlier committed slots retained.

Admission reasons distinguish repeated open (1), missing/incorrect module (2),
invalid descriptor geometry (3), zero nonce (4), GPT read/hash (5/6), EFI boot
sector read/hash (7/8), journal header read/mismatch (9/10), ambiguous matching
disks (11), raw claim refusal (12) and no matching candidate (13). Candidate
rows show eligibility and capacity; hash refusals include the compared digests.
These rows retain the existing disk/file admission and write bounds.

The UEFI fixture also exercises a GPT hash mismatch with a working workspace,
requiring the actual refusal and digest rows in the USB exit log and zero
journal changes. Its dual-refusal case requires a held guest, an unchanged
journal and absence of firmware/network work. Both use the exact staged image;
the fixture changes only its disposable disk's GPT byte to cause admission
refusal.

## FAT mount-state binding

The native6a39a477 run exposed a host preparation error. Linux sets
`FAT_STATE_DIRTY` (byte65 bit0 on FAT32) while the EFI filesystem is mounted
read/write, then clears it on clean unmount. Preparation had hashed the mounted
sector. The native rejection's digest matches that sector with only this bit
cleared. Controlled clean unmount and remount with direct sector reads reproduced
both exact images. Buffered whole-disk reads can retain a stale cache image
across a partition mount-state update, so that host experiment uses `O_DIRECT`.
See the [Linux FAT implementation](https://github.com/torvalds/linux/blob/master/fs/fat/inode.c)
and [on-disk structure](https://github.com/torvalds/linux/blob/master/include/uapi/linux/msdos_fs.h).

Preparation now hashes a copy of the expected clean sector with only byte65
bit0 cleared. It retains the mounted sector separately in local evidence.
Every other bit stays in the descriptor hash. The native writer still compares
the actual entire512-byte sector with that exact digest. The kernel and AHCI
runtime have no change for this correction. An actual sector that retains the
dirty bit still fails admission. Fixtures cover mounted and clean preparation,
strict refusal of an actual dirty sector, reproduction of the old stale pin,
and preservation of other state/identity bits.


## Native bounded result

MacBookPro11,2 runtime sourcecc1c3775/release7bf30a800c4d68ac saved all seven
checkpoints in session00065ff73d95cc33-00. Separate binary review verifies commit
and payload checksums, the source-bound nonce/phase/flags, the immutable header
and reserved slot, and byte-exact overlap with USB boot/exit logs through the
26,345-byte reconstructed console prefix. Terminal flags record successful USB
save and unmount. One native preallocated-file write/flush/readback lifetime
is accepted. The later raw-claim release has no direct persisted receipt.

Verification of the 449-file baseline passes apart from one bounded marker atime, with
two new logs. Offline fsck/allocation,53 other EFI file hashes and seven protected
recoveries pass. The closed779-file primary and separate acceptance notes are
retained outside build. Ubuntu is unarmed. The consumed file stays populated;
future tests use a fresh file, descriptor and run record. Full firmware/network
capture and repeated lifetimes require their own evidence.
