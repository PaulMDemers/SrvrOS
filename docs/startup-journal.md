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
