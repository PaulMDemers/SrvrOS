# Portable profiles: first migration proof

Target: a versioned, machine-independent identity and selected-document/preference
bundle, validated through two fresh installation fixtures, plus a shared bounded
HOME/config/data/cache resolver. This is a host executable migration proof, not
login, guest account switching, cloud roaming or multi-user security isolation.

Recovered baseline: `a936baeb4a20ff28304a13252bf20345dee13bf1` from actual
`origin/main`, November 3, 2026. Independent ordinary clone `SrvrOS-profiles`,
branch `device/apple-silicon/portable-profiles`. The integration checkout and
review VMs (6233, 61610, 85869) are preserved. No VM or physical trial is needed
for this fixture milestone. Hosted Actions remain disabled.

Scope: new profile tooling/tests and shared path provider; this topic document.
PR252 owns process/I/O lifetime (`badfeb37`), PR19 Wi-Fi (`993e920a`), PR301
native display readout (`4c9fc184`), PR306 SQLite (`5028871b`, SrvrPorts PR2).
Their fetched diffs were inspected. Filesystem/root and installer owners consume
the profile contracts; no competing mount, credential or kernel changes here.
SrvrApps owns future account UI and preference adapters; SrvrUI owns widgets.

Current checkpoint: implement strict bundle validation, explicit portable settings,
identity collision refusal, staged import/recovery and fresh fixture round trips.
Evidence and final contracts follow below as verification completes.

## Contract v1

The normative validator is [profile_bundle.py](../tools/profile_bundle.py).
A bundle is bounded UTF-8 JSON, with precisely these fields (this empty-document
example is valid):

```json
{
  "format": "org.srvros.portable-profile",
  "schema_version": 1,
  "profile": {
    "id": "0c432e6e-4a3c-4efa-8c91-a23694e88709",
    "display_name": "Example User"
  },
  "preferences": {"text_size": 20, "theme_style": 1},
  "documents": []
}
```

Each document has exactly `path`, `size`, `sha256` and `data_base64`: a canonical
home-relative `Documents/...` name, byte count, lowercase SHA-256 and canonical
base64. Contents are opaque bytes and stay byte-identical. Hashes detect corruption;
they provide neither authenticity nor encryption. The user must recognize the
bundle's source. No archive extraction, permissions, executable bits, links,
UID/GID or absolute source paths are accepted as bundle metadata.

Names are deliberately a limited ASCII subset for the current guest: document
components start alphanumeric, contain only alphanumerics, spaces, `_`, `-`, `.`,
and cannot end in space/dot. No hidden, empty, dot/dotdot, slash-escaped or
backslash components. Components are at most 64 characters and relative paths at
most 160. Full and parent-component case collisions and file/directory collisions
are refused, including on a case-sensitive host. This is an initial interoperable
subset, not a claim of Unicode filesystem compatibility.

Limits: 64 files, 1 MiB per file, 8 MiB aggregate decoded content, 12 MiB encoded
JSON, nesting at most eight levels and integer tokens at most 20 characters.
Unknown keys, duplicate JSON keys, bad types (including bool-as-int), unknown
versions, malformed/truncated encodings, oversized input, inconsistent sizes and
hashes fail before writing any profile state. Unsupported future schemas are
refused; migration must be an explicit version-to-version converter preserving
identity and leaving the original bundle intact. No downgrade or heuristic repair.

`profile.id` is a canonical random UUIDv4, minted at creation and retained through
migration. The display name is portable, descriptive and non-authoritative. It may
collide with another display name. Local account alias and UID/GID are supplied by
the destination provisioner, never imported. The host fixture records them in
`binding.json`; it does **not** call chown or change the invoking host identity.
Duplicate UUID, alias or UID refuses without replacing existing data. Shared local
GIDs are allowed. The proof has no overwrite, merge, automatic identity fork,
account deletion or credential-reset operation.

## Data classes and settings adapter

| Class | v1 behavior |
| --- | --- |
| Documents | Only individual explicitly selected `Documents/...` regular files; no recursive/home-wide export |
| Portable preferences | Typed allowlist: `text_size` (16 or 20), `clock_flags` (0–3), `wallpaper`, `theme_accent`, `glass_level`, `theme_style` (each 0–2) |
| Application data | Provider reserves `.local/share`; arbitrary databases/extensions/binaries are not portable in v1 without a versioned application adapter |
| Machine settings | Display size/scale, pointer/device behavior, timezone and driver bindings excluded |
| Credentials | Password hashes, keys, tokens, cookies and credential databases excluded; no credentials in the manifest or local binding |
| Runtime state | Caches, sessions, locks, history and current-window/document state excluded; destination creates an empty cache |

The portable settings correspond to actual current desktop/Settings fields in
[desktop_preferences.h](../shared/include/srvros/desktop_preferences.h) and
[desk_preferences.c](../userspace/lib/src/desk_preferences.c). The explicit
`--legacy-desktop` adapter validates `home/desktop.conf` v7, then projects only
those six fields. The mixed source file is never copied or modified. Import writes
`.config/srvros/portable.json`; current guest apps do not yet read this file.
Applying it to a running desktop requires a SrvrApps/compositor adapter which
merges portable fields with destination-local device settings. Earlier desktop
versions must first use the existing version migration path; this host adapter
refuses them rather than duplicating that migration.

The ordinary exporter reads the same typed projection file, so destination
re-export retains settings without the original desktop file. Unknown preference
keys, including device or credential keys, fail closed. These keys are not an
extension bag for arbitrary application settings.

An explicitly selected document can itself contain secrets or absolute paths.
The tool preserves document bytes and cannot classify their meaning. Selection
is the user's decision to transfer those bytes. The exclusion policy prevents
implicit home/config sweeps; it does not redact documents or promise detection
of sensitive text. Production export UI must display selected files and this
consequence before export. No real user data or secrets belong in the repository.

## Installation and path-provider interface

The offline host fixture is deliberately distinct from a physical installation:

```text
FIXTURE/.profile-store-v1       explicit fixture marker
FIXTURE/.lock                   host advisory lock, held during each operation
FIXTURE/staging/import-*        unpublished imports only
FIXTURE/profiles/UUID/
    identity.json               portable ID and display name
    binding.json                local alias, UID and GID
    home/Documents/...
    home/.config/srvros/portable.json
    home/.local/share/
    home/.cache/
```

Each independent fixture derives its own absolute HOME from its own root. Nothing
in the bundle prescribes `/fat`, `/work`, `/home` or a host directory. This layout
is a test-store implementation, not a competing root/mount decision. The filesystem
owner supplies the eventual writable namespace and metadata contract. Installer
provisioning must allocate the destination binding and supply the actual HOME;
it can consume v1 only after its storage publication contract is qualified.

[profile_paths.h](../userspace/lib/include/srvros/profile_paths.h) exposes
`srv_profile_home_valid(home)` and
`srv_profile_path(out, capacity, home, kind)` for HOME, config, data and cache.
It is allocation-free, performs no filesystem I/O and preserves output on failure.
Canonical ASCII absolute paths obey `SRV_PATH_MAX` and `SRV_NAME_MAX`; dot/dotdot,
repeated/trailing separators, backslashes, controls and non-ASCII bytes refuse.
Root is canonical; derived paths must still fit. Overlapping input/output works.
A valid path is not evidence of existence, ownership or access.

`env` emits JSON containing `SRVROS_PROFILE_ID`, `HOME`, `SRVROS_DESKTOP_HOME`,
`XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_CACHE_HOME`. It is data, not a shell
script to evaluate. Config/data/cache suffixes follow the
[XDG Base Directory defaults](https://specifications.freedesktop.org/basedir/latest/).
This proof does not implement XDG search paths, overrides or a secure runtime
directory. The C provider is cross-checked against both fixture environments.
It is available to future consumers; existing `desk_home()` and desktop launch
behavior are deliberately not switched by this patch.

## Publication and recovery

Operate only on new/empty, explicitly initialized, offline fixtures owned by the
invoking host user, with no concurrent external modifications. Store operations
hold an exclusive nonblocking host `flock`; a busy store refuses. Source reads
reject symlink ancestors/leaf files, hard links, special files and observed changes
during a read. This is not protection against a hostile same-user process swapping
parent directories. Fixture roots and profile directories are private host
directories; this does not establish guest permission isolation.

Import validates the complete bundle and destination binding before creating a
stage. It writes private files, checks write/flush errors, then publishes the
whole profile with one same-filesystem directory rename. Python documents the
successful rename's [atomic namespace behavior](https://docs.python.org/3/library/os.html#os.rename).
A failed write leaves unpublished staging and blocks another import. `recover`
under the same lock discards recognized unpublished stages, lists published IDs,
and leaves published profiles intact. Unknown staging entries require manual
inspection. Retry after recovery starts from the original bundle.

An interruption after publication may leave a complete profile even when the
caller did not observe success. Reconcile its ID with `recover`/`env` and export
readback before retrying; duplicate identity refusal prevents silent replacement.
Export writes a temporary file and publishes with an exclusive hard link, refusing
existing output. A late error can leave a complete output; inspect it before retry.
Temporary export remnants after abrupt termination may require manual removal.

These are host process/interrupted-write tests, not power-cut guarantees. Parent
directory durability, controller flush behavior, exFAT journaling/transactions and
physical-media recovery are unqualified. The guest exFAT implementation must not
inherit atomic/durable assumptions from this host proof. Installation initialization
itself is not transactional: if it fails, inspect and recreate the disposable empty
fixture; never point it at an existing user home.

## Account, import and recovery journey

The actor is a person moving selected documents and settings to a new SrvrOS
installation. Completion means the same profile ID and selected bytes/preferences
are present under a new local binding; it does not mean the person authenticated.
This sequence uses optimize-ux source inspection and an expert walkthrough, with
functional host checks. No representative-user or rendered UI testing is claimed.

| State | Decision/action | Result and recovery |
| --- | --- | --- |
| Create on source | Choose display name and installation-local alias; provisioner allocates UID/GID | New UUID plus empty profile; invalid/occupied alias or UID refuses |
| Choose export | Select exact documents; review six allowed settings and exclusions | No directory sweep; cancel before export changes no profile data |
| Save bundle | Choose new output filename | Existing output refuses; inspect completed output after uncertain failure |
| Inspect on destination | Read profile name/ID, file list, total bytes and settings; verify source | Corrupt/unknown schema refuses before provisioning; retain original bundle |
| Bind locally | Choose available alias; provisioner assigns local UID/GID | Duplicate ID means existing identity; use existing profile or cancel, never auto-merge |
| Import | Validate, stage, publish | Success provides new HOME; write failure leaves visible recovery instruction |
| Recover | Discard unpublished stage, then inspect published IDs | Before-publish interruption can retry; after-publish interruption keeps the complete profile |
| Activate later | Authenticate locally and establish session | Not implemented: requires the contracts below; portable bundle is not a login credential |

Future account UI belongs in SrvrApps using SrvrUI controls. It should retain the
chosen bundle/alias on correction, label identity collision separately from name
collision, offer cancellation before publication, and clearly distinguish a saved
profile from an active authenticated session. Credential enrollment is a separate,
local operation. Sign-out must drain applications and file workers before another
profile becomes active. Deactivation/deletion must not remove a live profile;
revocation, deletion and merge policy need a later explicit contract.

## Recovered source gaps before guest activation

- [posix_identity.c](../userspace/lib/src/posix_identity.c) exposes root-only
  passwd/group and constant UID/GID zero; nonzero setuid/setgid is refused. There
  is no authenticated account service behind those compatibility functions.
- [desk_paths.h](../userspace/lib/include/srvros/desk_paths.h) accepts only the
  workspace literal and otherwise returns `/fat/home`. Files, Text and the picker
  use this helper. Terminal supplies HOME/PWD but not SRVROS_DESKTOP_HOME/XDG in
  its explicit child environment; nested callers need consistent propagation.
- [deskd.c](../userspace/deskd/deskd.c) and
  [lifecycle_worker.h](../userspace/deskd/lifecycle_worker.h) construct partial
  per-child environments, with 192-byte temporary buffers. The session and
  preference paths still choose between two fixed locations.
  [desk_command_job.c](../userspace/lib/src/desk_command_job.c) forces
  `HOME=/fat/home`. A general profile path cannot be enabled without auditing
  truncation and all launch routes together.
- Legacy notes/textedit/paint/displayd code contains independent `/fat/home`
  paths. Current desktop preferences mix portable and device fields. App migration
  needs adapters and safe legacy read/copy behavior, preserving originals.
- [vfs.c](../kernel/src/vfs.c) defaults uid/gid to zero; chmod changes metadata.
  [posix_file.c](../userspace/lib/src/posix_file.c) `access()` checks owner bits in
  libc; chown rejects nonzero IDs. These are not per-actor kernel authorization.

Before real users/login: define authoritative process real/effective/saved IDs and
supplementary groups, privileged transitions and exec inheritance (coordinate
PR252); local credential enrollment/authentication/revocation; session creation,
launch environment and teardown; kernel enforcement for lookup/traverse/open,
create/unlink/rename, mode/owner changes, mounts, devices and process control;
filesystem ownership persistence; private runtime/credential storage. Validate
allow/deny behavior across two real users, including inherited descriptors, before
claiming isolation. Profile names, UUIDs, environment variables and host fixture
permissions cannot supply that authority.

## Verification and use

Run from the isolated checkout, with no guest or physical disk involved:

```sh
sh tools/macos/build.sh HOST_CC='cc -arch x86_64' test-portable-profiles
cc -arch x86_64 -std=c11 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-sanitize-recover=all \
  -idirafter userspace/lib/include -I shared/include \
  tools/test_profile_paths.c -o build/tests/profile-paths-sanitized
build/tests/profile-paths-sanitized
sh tools/macos/build.sh check-docs
```

The test target needs no initialized submodules; documentation checking requires
pinned first-party and upstream link targets. On Linux omit `-arch x86_64` and use
the normal GNU Make command. Python requires host POSIX `flock`, no-follow opens
and same-filesystem rename/link. Python is not being ported into SrvrOS here.

CLI commands: `init ROOT`; `create ROOT DISPLAY_NAME --alias NAME --uid N --gid N`;
`export ROOT UUID OUTPUT --document Documents/FILE [--legacy-desktop]`;
`inspect BUNDLE`; `import ROOT BUNDLE --alias NAME --uid N --gid N`;
`env ROOT UUID`; `recover ROOT`. Run `python3 tools/profile_bundle.py --help`.
Every fixture root must be new/empty at initialization, with canonical physical
host paths (no symlink ancestors). Start under ignored `build/review/profiles`.
Do not use real user homes. Documents/settings generated by the tests are synthetic.

Acceptance: x86_64 path tests and ASan/UBSan pass. Thirteen Python groups cover
round trip between independent stores with different HOME/UID/GID/alias; matching
shared C paths; invalid/truncated/duplicate-key/version/resource-limit input;
traversal and case/file-directory collisions; duplicate identity/local bindings;
symlink/hardlink/FIFO refusal; explicit omissions and legacy Settings projection;
exclusive store ownership; interrupted stages, injected write failure and recovery
before/after publication; existing-output refusal and CLI error reporting. The
original excessive-nesting failure was fixed with an explicit parser bound.

Current delivery checkpoint: implementation and focused verification complete;
perform the clean-source CLI proof, final documentation check and draft PR
publication. No kernel, guest launch or device permission changes are part of this
milestone. Next product checkpoint is a coordinated SrvrApps/compositor consumer
of the path and portable-settings contracts after filesystem/installer bindings
are agreed; security enforcement remains a separately verified prerequisite.
