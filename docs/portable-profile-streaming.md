# Portable streaming bundle v2 — implementation contract (in progress)

This is the in-progress implementation contract, not an acceptance claim.

**Runtime prerequisite discovered before VM qualification:** the existing guest
ordinary descriptor path snapshots complete files in the kernel and refuses
writes above 16 MiB. A real `process.c` host reproduction reaches 16 MiB in
64 KiB writes, then returns `SRV_ERR_FBIG` on the next byte. Legacy native append
also reads and joins the complete file. The current profile consumer therefore
cannot yet provide the promised large guest export or bounded end-to-end I/O.
A separate opt-in bounded range-I/O path needs runtime/filesystem owner contract
alignment and verification before this milestone can be qualified. Raising the
ordinary cap or relying only on userspace high-water evidence is insufficient.
Authentication and permission enforcement remain separate.

User authorizes larger-document migration with bounded memory, v1 compatibility,
two disposable-VM transfer/recovery proof. Authentication and permission
enforcement remain separate. Base actual main5f670602, integrated main5f4f6fc6, in existing isolated
SrvrOS-profile-326-review. The initial no-kernel/filesystem-change assumption is superseded only by this
concrete bounded-I/O prerequisite; no ordinary device trial or security-policy
change is authorized by the profile format.

Wire: 16 magic bytes `SRVROS-PROFILE2\n` (ASCII), uint32 little-endian manifest
byte length, uint32 reserved zero; exactly that many UTF-8 JSON bytes, then opaque
raw document bytes concatenated in manifest array order, then exact EOF. Header24
bytes. No archive, extraction, compression, user offsets, paths from header, or
padding/trailing data. Unknown framing/version/reserved values refuse.

Manifest exact top keys: format (`org.srvros.portable-profile`), schema_version
(integer2), profile, preferences, documents. Profile/preferences/path/name/depth,
unknown/duplicate key and integer-token validation retain v1 semantics. Each doc
has exactly path,size,sha256; NO data_base64. SHA256 lowercase64 hex, declared
length exact; offsets derived by checked prefix sums. Metadata <=65536 bytes,
<=64 docs, <=16777216 bytes/doc, <=67108864 bytes total; buffer65536 on heap or
owned static storage (not a 64KiB stack frame). File size <=24+65536+67108864.
Manifest whitespace/key order need not be canonical on input; exporter emits
stable compact JSON. Full-container SHA256 binds exact previewed metadata+bytes.
Per-document hash and full hash are verified incrementally through one open,
nofollow, regular single-link source fd; fstat identity/size/timestamps rechecked.
Rehash copied data before rename and full source before publication so same-inode
edits never silently publish. No concurrent-writer or arbitrary power-cut claim.

V1 parser, schema, constants, bytes, default CLI export and creation remain
unchanged. Guest existing import/preview-import/import-reviewed/recover/inspect
recognize v2 by exact magic; v1 path continues existing strict validator.
New guest commands preview-export-auto and export-auto-reviewed select v1 when
all files/aggregate fit v1 limits, otherwise v2 within new caps. Keep old
preview-export/export-reviewed/export behavior. Auto review reports optional
bundle_version (1/2), existing review format srvros-profile-review-v1 and fields
unchanged. Whole hash binds chosen format; mutation revalidates source data,
format and reviewed digest. GUI uses auto commands, default generic .srvrprofile
suffix, and labels large-document compatibility in review; explicit old filenames
are allowed, extensions do not determine format. Existing small GUI exports stay
v1; graphical import supports both. Review JSON <=16384 (prove64maxlegalpaths).

V2 journal uses existing sibling .profile-import.json name with format
srvros-profile-import-v2 and SAME six fields as import-v1: format,home,bundle,
sha256,binding,profile_id. Preserve v1/create-v1 recovery. Source path+container
SHA revalidated on recovery before mutations; source fd stays owned through
copy/publication. Existing marker, binding/UUID/case/collision/tree gates stay.
Only matched-journal reserved root scratch can be removed, never final unknown
data. Stream scratch writes, checked fsync/close, stream readback hash/size,
rename-to-final and final verification; existing final must match exactly.
Recovery can reconcile published journal. New copy-partial diagnostic checkpoint
fires once on first streamed doc after65536 durable bytes in owned scratch,
before rename; recovery safely discards owned scratch and restarts that doc.
Keep journal/directories/document/identity/commit-ready/published checkpoints.
Export O_EXCL existing path policy stays: invalid input creates no output;
late failure/interruption preserves incomplete exclusive output, no success and
no overwrite. No new export-publication reconciliation engine in this milestone.

New tools/profile_stream.py host codec/fixture CLI supports stream v2 and reads
v1 via unchanged profile_bundle helpers. No changes to profile_bundle.py v1
constants/functions, installation_{export,image,inspection}, profile_image native
driver or filesystem. Native installer/offline exporter remain v1-only failclosed;
installation owner's srvros-export-publication-v1 is PRIVATE metadata, unrelated
to this portable schema. V2 host fixture import validates all input before new
stage, streams only into its owned stage, no replacement, existing host recovery.

Memory: all v2 payload I/O/hash/copy/readback incremental65536, bounded manifest
and file count, no payload-sized allocations. Measure peak ALL engine allocations
(incl cJSON) in native actual-source tests, plus guest srv_usage(SELF)
peak_mapped_bytes at completion, separately labeled; do not call mapped high-water
exact heap or kernel memory. Capture small/16MiB/64MiB workloads and refuse
metadata abuse without unbounded allocation. Native actual-engine allocation ceiling is 1 MiB for v2 boundary workloads.
Guest acceptance ceilings, frozen before VM qualification: peak mapped <=32 MiB
for each workload and growth from the small v2 workload <=4 MiB. These ceilings
are requirements, not measurements. Production guest stats require explicit
SRVROS_PROFILE_TRACE_MEMORY=1 and report at exit or a selected interruption
checkpoint. Numeric stats stderr only, no review/payload pollution.

Acceptance: host-v2/guest-v2 cross interoperability, v1 unchanged tests plus actual
v1 guest import/export, >1MiB image+16MiB binary+selected Text, aggregate64MiB
boundary/native host test, invalid/truncated/corrupt/extra/version/path/case/count/
size/changed-source/existing-output/lowspace refusals, IO/fsync faults, all import
checkpoints incl copy-partial and published recovery. Two independent installed
A/B plus transport; actual GUI review/confirm, selected bytes/settings/identity
survive different local bindings, chooser/remembered/reboot readback and640 flow.
Exact source/machine/local/excluded sentinels, binaries/GPT/protected/allocation,
owned memory evidence and independent rendered/storage review. Preserve all old
proofs, original baselines and reviewVMs; heavy build/VM only after resource window.


## Bounded runtime I/O dependency: chosen interface, implementation pending

This task owns the isolated prerequisite implementation and local independent
review. Frozen offline GUID selection and v1 publication work remain separate.
No active exFAT/VFS implementation overlap was found in the published PRs.
Signal continuation owns syscall numbers 164–167; this dependency reserves
**168, FILE_STREAM**. It remains excluded from `syscall_io_safe` and joins the
existing namespace syscall service gate through the complete request, I/O and
copyout. The existing scheduler excludes same-process siblings during that
window; do not add a new mapping/preemption bypass.

Use process-local opaque stream handles, not ordinary file descriptors. This
avoids changing general fd read/write/pread/fstat/dup semantics or their 16 MiB
buffer cap. A handle cannot be inherited by fork or passed to another process;
successful exec and all process retirement paths close it, while failed exec
retains it. Tokens never repeat during a boot; exhaustion refuses rather than
wrapping. Reserve a free global slot and one of at most 65 process slots before
exclusive output creation; at most 256 handles exist globally. Kernel resources
are tied to a stable exFAT inode/mount/file slot and retain no payload snapshot.

The version-1 fixed request is 80 bytes: uint32 version, size, operation, flags;
uint64 handle, path pointer, buffer pointer, length, offset, mode, reserved[2].
Flags and reserved fields are zero. Operations are OPEN_READ, CREATE_APPEND,
PREAD, APPEND, STAT, SYNC and CLOSE. Open-read admits only regular, single-link
exFAT data files, and create-append admits only an exclusively new regular file
with the caller's existing creation mode/umask. Internal filesystem metadata
paths are unsupported. Reads use an explicit offset; writes append only. Each
transfer is at most 65536 bytes, including bounded kernel copy and backend data
bounce buffers. STAT uses the existing fixed-width srv_stat shape without
reading payload. Other unused request fields must be zero. Unknown operations,
versions, sizes, flags and pointer/length combinations refuse before mutation.

The manager and syscall bridge own process lifetime, token/quota validation,
namespace/user copies and existing file-size resource-limit semantics. The
exFAT backend owns bounded range reads, chunk-only append with unaligned tails,
stable object identity, checked metadata/length publication, flush, low-space
clean refusal, sticky uncertain failure and final close. Ordinary opens and
file/ancestor rename, replacement, unlink and unmount must refuse conflicts in
both directions while a stream is live. Generic descriptors and unrelated paths
retain their existing behavior. No credential or permission-policy work is
included.

All large-capable profile paths must use the opaque abstraction: magic sniff,
manifest/header reads, source metadata/stat, selected document reads, scratch
creation/readback, export creation/write/sync/close and final rehash. Automatic
format choice uses metadata inspection without opening a payload snapshot.
Native host tests retain POSIX adapters; v1 operations keep their existing path
after bounded format detection. Unsupported non-exFAT v1 sources retain their
bounded legacy fallback; unsupported v2 backends refuse.

Before qualification, real backend and syscall/manager tests must prove the
whole 64 MiB container, unaligned/fragmented I/O, quotas/tokens/lifetimes,
coherence/namespace and user-copy gates, resource limits, low-space and injected
I/O/flush failures. Measure kernel payload allocations separately from volume
metadata/cache and from user allocations/mapped high-water. The existing profile
native tests are consumer evidence, not a substitute for those backend checks.
