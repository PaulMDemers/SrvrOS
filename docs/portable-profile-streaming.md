# Portable streaming bundle v2 — implementation contract (in progress)

This is the in-progress implementation contract, not an acceptance claim.

User authorizes larger-document migration with bounded memory, v1 compatibility,
two disposable-VM transfer/recovery proof. Authentication and permission
enforcement remain separate. Base actual main5f670602 in existing isolated
SrvrOS-profile-326-review. No kernel/filesystem changes, no ordinary device trial.

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
metadata abuse without unbounded allocation. Freeze concrete measured bounds
before qualification. Numeric stats stderr only, no review/payload pollution.

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
