# Explicit fresh-media file verification

`SYS_FILE_VERIFY` (170), exposed as `srv_file_verify`, compares an expected byte
sequence with a fresh VFS snapshot of one regular file. Screenshot saving uses
it for both temporary and final byte proofs. Ordinary open/read/dup/fork and
shared-file contents retain their existing semantics.

An ordinary reopen is insufficient when another descriptor retains the same
shared content: read-only open obtains fresh VFS bytes but may discard them and
reuse the shared RAM buffer. A corrupted backing byte can then be hidden from
a comparison. The complete-save regression reproduces historical false success
for a reader acquired on the published file after checked rename. A reader held
on the temporary path also masks that ordinary readback, but checked rename
still refuses the open prefix; those are separate conditions.

## ABI and admission

The fixed64-byte `srv_file_verify_request` contains ABI version/struct size,
pathname, expected-data pointer, length, expected inode/mtime and zero reserved
word. Only regular files with exactly the expected length and identity/date
are accepted. Zero-length files are supported. The operation has its own8MiB
proof bound and4096-byte comparison-copy bound. Existing256KiB syscall copy,
read/write sizes, screenshot8MiB limit, stream limits and storage budgets do not
change.

The normal namespace syscall gate spans request/path capture, namespace
translation, metadata checks, VFS media read, full comparison and snapshot
release. This call is not I/O-safe and grants no scheduling eligibility.
Active stream ownership still refuses the operation. There is no process-file
content acquisition, cache eviction, descriptor allocation, write, implicit
sync or durability shortcut.

The scalar request and pathname are copied once. The caller must keep the
expected region mapped and immutable until return; expected bytes are sampled
through independently checked4096-byte copies, not an atomic whole-region
snapshot. The wrapper rejects invalid/overflowing regions before I/O and checks
each chunk again after backend continuations. Screenshot expected bytes are
owned by its single-threaded save worker. An expected-data fault fails the
proof and releases the media snapshot.

The backend checks type/inode/length/mtime before reading and checks identity
again after the read and comparison. It compares every byte and exact returned
length, releasing the VFS snapshot on success, mismatch, read error, changed
metadata or user-copy failure. Return0 means complete equality; negative results
indicate invalid request, missing/changed file, busy stream, I/O/mismatch or
user-memory failure. Callers must not translate any error into successful proof.

Fresh means bypassing shared process content through the VFS read backend. It
does not promise bypass of a device's internal caches, physical atomicity or
new persistence guarantees. All existing fsync/close/global barriers remain.

## Screenshot integration and coverage

Both verification phases retain regular-file/length checks and guarded inode/
mtime. A phase reports its start and full verified length after successful
comparison. The old per64KiB read-progress frames are replaced by one completed
proof frame; phase identity, ordered protocol validation, full pixel bytes and
both complete comparisons remain. This intentional diagnostic change can alter
final-response backlog and must not be described as an isolated transport speed
improvement. No success notification precedes final proof and retirement.

The temporary file remains exclusively created; write/fsync/close, both global
syncs, checked rename and uncertain-publication preservation remain in order.
Failure still cleans only the owned temporary name and retains a possibly
published destination for inspection.

`make test-file-verify` exercises the actual syscall wrapper and backend,
including empty/full8MiB proof, malformed ABI/reserved/ranges, stale metadata,
corruption, stream refusal, request/path mutation and post-I/O user-copy faults.
`make test-capture-media-integration` runs actual save/process/exFAT/VFS cores
on disposable128KiB-cluster media, including retained readers, temporary/final
corruption, ordinary sharing, checked-rename refusal, allocation and fsck.
The optional historical-save input demonstrates the old complete-wrapper false
success. Host adapters cover admission seams explicitly; they do not establish
native scheduling, controller or device acceptance.
