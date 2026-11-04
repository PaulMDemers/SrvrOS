# Portable profiles: migration and desktop integration

Target: a versioned, machine-independent identity and selected-document/preference
bundle, validated through two fresh installation fixtures, plus a shared bounded
HOME/config/data/cache resolver. This is a host executable migration proof, not
login, guest account switching, cloud roaming or multi-user security isolation.

Current development target: graphical profile transfer, following merged PR326.
The chooser milestone is complete. The new Profiles app will expose existing
guest transfer commands through selected-file export, reviewed import and owned
asynchronous jobs. Main `c90f3522`, Apps `3cf32af2`, UI `9eaaa74e` and Ports
`ca749517` are the inspected baseline. Authentication and permission enforcement
remain separate. Completion requires a matched build, focused host tests and a
rendered two-installation transfer/recovery proof, then scoped draft PRs.

| Transfer state | User decision | Result and correction |
| --- | --- | --- |
| Export selection | Choose individual Documents files and a new bundle path | Keep selections editable; show source and destination |
| Export review | Confirm identity, portable preferences, files/total size and selected-byte consequence | Bind confirmation to previewed bytes; changed input refuses before output creation |
| Import selection | Choose a bundle and destination-local name | Validate the complete bundle and local collisions before review |
| Import review | Confirm profile, files/preferences and local destination | Revalidate exact bytes and local binding before using existing transaction |
| Working | Observe status or request close | Continue rendering and safely wait for owned child completion |
| Completed import | Finish the current session and select the profile at next desktop start | Existing chooser supplies ready/recoverable/blocked status |
| Failure or interruption | Correct inputs, choose a fresh export filename, or resume eligible import recovery | Preserve partial output, existing profiles and ambiguous state |

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

Current checkpoint: first migration proof complete for draft review. Strict bundle
validation, explicit portable settings, collision refusal, staged import/recovery
and two independent fixture round trips are implemented and host-verified. No
guest account activation or security enforcement is claimed.

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

Clean implementation `38c5b8cb34216f027688169ce2c5ecccc442df17` passes the focused
target and a separate 12-invocation CLI proof: initialize/create source, project
real v7-format settings, export/inspect, initialize/import destination with UID
2050/GID 2051 instead of source 1001/1001, compare provider paths, re-export an
identical bundle, refuse duplicate identity/truncation, and reconcile recovery.
The source and destination documents and six portable preferences match; source
desktop settings remain unchanged, destination cache is empty, and credentials
and device settings are absent. Local synthetic fixtures, command receipt and
hashes are retained in `build/review/profiles/cli-qt6a_5yx/receipt.json`; focused
test output is in `build/review/profiles/focused-tests.log`. No skipped tests in
the matched target. Documentation verification checks 832 local links with zero
errors after initializing pinned upstream link targets. Final review found no
actionable defects within the stated offline-fixture assumptions.

First milestone is complete for draft PR review; no merge was requested or
performed. Final documentation-only follow-up leaves the tested implementation
unchanged. No kernel, guest launch or device permission changes are part of this
milestone. Next product checkpoint is a coordinated SrvrApps/compositor consumer
of these path and portable-settings contracts after filesystem/installer bindings
are agreed. Credential and permission enforcement remain separate prerequisites
before real user isolation. The integration checkout and all three named review
VMs remain untouched and running at closeout; this task launched no VM.

## Desktop adoption milestone — verified for draft review

Authorized follow-on: activate one explicitly imported profile in the compositor
and current Files/Text/picker/Terminal/command routes; read and save portable
preferences independently of local device settings; verify actual app interaction,
clean reboot and migration on two independently provisioned disposable installed
VMs. Authentication, kernel credentials and access enforcement remain unchanged.
Branch `device/apple-silicon/profile-desktop` consumes PR309 and fetched main
`3db1a1d3` (merged installed-disk PR311). PR309 is still a review dependency.
The existing three review VMs remain running and untouched. One harness/build at
a time in this isolated checkout; no physical disk or integration checkout writes.

The proof retains `/fat` backing. Explicit profile selection must validate its
home, UUID marker, required directories and portable settings before rendering;
missing/malformed selections refuse to console with a corrective message, never
substitute a shared legacy home. No selection retains existing desktop behavior.
Enforce the actual 159-byte backing path limit when provisioning/using paths;
versioned offline imports remain separate from unsupported guest directory-rename
publication. Two stores/images use different local slots and numeric metadata;
guest credentials remain root and metadata is not authorization.

Scope ownership: this branch owns the new session/provider and preference
integration; SrvrApps change is restricted to Terminal environment propagation.
PR19's compositor diagnostic additions and PR252 process/libc lifetime scope are
preserved. Existing installation tooling is consumed without editing its contract.
Completion requires fresh matched artifacts, focused host tests, legacy desktop
regression, both installed VM lifetimes/reboots, app and raw-byte readback,
no profile/device setting contamination, missing-profile refusal, and draft PRs
with source/evidence identities. These checks are now complete; acceptance and
review handoff follow below.


### Activated profile contract

An offline imported image can launch its profile explicitly:

```text
run /fat/bin/deskd --profile=/fat/home/local-a --apps
```

This is profile selection, not login. It does not change the root credentials,
libc identity functions, kernel checks, mount permissions or filesystem access
policy. The portable UUID identifies the content; `profile-local.json` records
installation-local alias/UID/GID metadata only. It grants no authority. A profile
can be selected only for an ordinary desktop session, not diagnostic modes.

Before graphics startup, `profile_startup.h` validates a canonical home (at most
95 bytes) under `/fat/home/` or `/work/srvros-work/`, its exFAT mount, required
folders, exact UUID marker and strict portable settings. Missing or malformed
selection returns to the console with a recovery instruction. Apps and command
jobs reject conflicting explicit context instead of substituting a shared home.
No `--profile` retains the previous desktop home and preferences behavior.

The copied session context supplies `HOME`, `SRVROS_DESKTOP_HOME`,
`SRVROS_PROFILE_ID`, `XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_CACHE_HOME` to
queued/direct app launches, command jobs and Terminal shells. Files/Home,
Text/pickers and screenshot destinations resolve from this context. Terminal's
working directory is the selected home; shell history stays per-window/in-memory.
The home-length bound leaves room for the actual guest path buffers; screenshot
name overflow is refused rather than truncated.

Settings combines the six portable values in
`HOME/.config/srvros/portable.json` with device settings in the existing machine
`desktop.conf`. Reads and writes remain asynchronous with independent existence
and CRC guards. Portable changes write only the profile file; local changes
preserve the machine file's other fields. Conflicts/failures stop automatic
writes and retain the existing session-only/conflict indication. Host tests cover
edits during loading or a pending save, and independent local/portable writes.

`tools/profile_image.py` is an offline Python provisioning API for disposable
images, not a mounted-disk installer or guest import command. It validates the
bundle and expanded paths before copying a regular single-link source image,
then uses `profile-image-driver` (the real kernel exFAT/VFS backend) on a private
staging copy. Exclusive publication cannot overwrite an existing destination;
failed stages are removed. Its supported slot is `/fat/home/<ASCII-name>`.
The source must be offline. Import/export remains a host-mediated operation;
a graphical account chooser, in-guest import UI and authentication are outside
this milestone.

### Acceptance on November 3, 2026

Tested OS source: `73c5f2db3fbfce50365602c67cf1e07b42c08705`; implementation
`2e5d2ea2`, plus a harness-only correction to use external `env`/`pwd` commands
because this shell's builtins do not apply redirection. Matched SrvrApps:
`3cf32af2294eebe9d8b9aeb8bc1dcbe46feb2967`, with the pinned SrvrUI. Subsequent
acceptance-note commits do not claim a new guest build.

- Matched `sh tools/macos/build.sh -j2 all` and `check-host` passed.
- `test-desktop test-desktop-io test-lifecycle-wait test-profile-desktop` passed
  with the Darwin x86_64 host compiler adapter (GNU GC-section flag translated to
  Darwin dead stripping). The profile target includes 13 bundle test groups,
  six offline-image groups, session/startup fixtures and dual-store preferences.
- AddressSanitizer/UndefinedBehaviorSanitizer passed for profile session,
  startup and preferences fixtures; the earlier shared-path sanitizer remains
  accepted. No sanitizer finding was suppressed.
- `profile_desktop_smoke.py` passed four independent VM lifetimes: A save, A
  reboot, import into B, B reboot. Each installation is independently prepared
  and applied with a distinct GPT disk GUID, its sole boot disk and fresh EFI
  variable store. The synthetic UUID is unchanged across both local bindings.
- In all four lifetimes Text opens the imported document through its picker,
  Files opens the selected home, and Terminal launches external commands whose
  environment/cwd are read back from disk. A edits the document and changes the
  theme through Settings. Export after A's second boot uses the actual saved
  bytes; B imports that export. The selected binary document is preserved too.
- All six portable values are independently compared after every boot, and
  installed `deskd`, `desktools` and `deskterm` match their built runtime binaries.
  Both complete machine preference files remain byte-for-byte equal to their
  own seeds. Guest and offline allocation checks report zero errors/leaks;
  bytes outside the data partition are unchanged.
- Each profile boot refuses missing-home, malformed-marker and traversal
  selections before the valid launch. Legacy `installed_disk_smoke.py` also
  passes two desktop lifetimes/save/reopen and damaged-system-volume refusal.
- Screenshots were inspected for Text's opened document/path, Files/Home and
  Terminal. This is an automated functional walkthrough, not participant research.

| Installation | Home | Local alias | UID/GID metadata | Clock offset | Pointer speed |
| --- | --- | --- | --- | --- | --- |
| A | `/fat/home/local-a` | `local_a` | 1001/1001 | -300 minutes | 4 |
| B | `/fat/home/local-b` | `local_b` | 2050/2051 | +60 minutes | 2 |

For macOS reproduction, create the ignored host adapter used by the tests:

```python
# build/tooling/darwin-host-cc.py
import os, sys
os.execvp('cc', ['cc', '-arch', 'x86_64'] +
    [arg.replace('-Wl,--gc-sections', '-Wl,-dead_strip') for arg in sys.argv[1:]])
```

Run the proof after the matched build and host-driver targets:

```sh
# Use the Darwin host adapter described above for the GNU-linker test targets.
sh tools/macos/build.sh \
  HOST_CC='python3 build/tooling/darwin-host-cc.py -D_DARWIN_C_SOURCE -Wno-array-bounds' \
  test-profile-desktop build/tests/profile-image-driver build/tests/exfat-driver
OVMF_CODE=/opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  python3 tools/profile_desktop_smoke.py \
  --ovmf-vars /opt/homebrew/share/qemu/edk2-i386-vars.fd
```

Retained local evidence is under `build/review/profile-desktop/proof-gf_at45k/`
(receipt, separate installation images, all four data readbacks, export, screenshots,
serial logs and `independent-readback.json`). Receipt SHA256:
`0c6d13dcecc93b24e1557a7a9d721fcde57abb8538506a7c48124988a3609036`.
Legacy evidence: `build/review/installed-disk/proof-x36gz7yw/`.
Host logs: `build/profile-desktop-host-r4.log` and matched-build logs. The first
harness failure is retained separately; it is not acceptance evidence.

All task-owned VMs stopped normally. The three pre-existing review VMs and primary
checkout were left untouched. No physical disk, power-cut durability, external
`fsck.exfat`, authentication or permission-isolation acceptance is claimed.
The milestone is complete for draft PR review; merge/integration remains the
review owner's action. No further implementation is required for this target.

Apps companion: [SrvrApps PR #5](https://github.com/PaulMDemers/SrvrApps/pull/5).
The OS branch pins its reviewed commit; land the shared header and app change
together after the PR309 dependency. Neither PR is merged by this task.


### Integration review — complete

The user authorized review, fixes and dependency-order merge of OS #314 and
SrvrApps #5. Foundation #309 was independently reviewed, updated with main
`8f47f196`, and merged after its 13 bundle groups, C path fixture, ASan/UBSan and
documentation checks passed. This branch now consumes that baseline.

Review found that accepted `--profile=... --apps --trace` arguments left the
legacy `apps` predicate false, disabling app launch and preference loading.
Explicit validated profile selection now always enables ordinary desktop apps;
the four-boot proof exercises both tracing forms. The refreshed proof provisions
identity-bound roots and requires the explicit selected-root boot marker. It
also checks all six portable values and each installed desktop/app binary in
every readback, incorporating the earlier independent readback audit.

No authentication or permission behavior changes. Apps #5's environment storage,
NULL terminator, cwd restoration and failure cleanup were reviewed against the
matched shared header; no additional Apps code change is required. The required implementation and runtime gates below pass. Integration order is
foundation #309, Apps #5, then OS #314; the OS pin keeps the reviewed Apps commit.


The identity-bound rerun found a second integration defect: the offline adapter
mounted at `/vol`, so the filesystem's absolute-path metadata sidecar named
`/vol/...`. Strict guest admission at `/fat` correctly refused that image.
The adapter now uses `/fat`, requires clean admission before every operation,
and reopens the completed stage through the same gate before publication. No
kernel admission or permission check was weakened. A real-driver host regression
checks canonical sidecar paths and injects a bad prefix, proving refusal preserves
the source and publishes no target. All seven image groups pass. The failed
identity-bound attempt is retained at `build/review/profile-desktop/proof-s3kg0g7c/`.


Final tested source is `4c81085b13b98fca94c931be80ff211146c6882a`, with Apps
`3cf32af2294eebe9d8b9aeb8bc1dcbe46feb2967`, consuming merged foundation #309
at main `9d642c759be26624f9cb35e50bd50ef8c292c69a`. Both findings are resolved:
`cac316e1` fixes profile startup flags, and `4c81085b` fixes offline metadata
provisioning. No unresolved review finding remains within the agreed scope.

- Full desktop/I/O/lifecycle/profile and installed-root host regressions pass
  (`build/profile-integration-host.log`). The runtime source is unchanged by
  the later provisioner-only fix. All seven image groups, including the real
  driver refusal regression, pass in `build/profile-provisioner-review.log`.
- The matched build passes (`build/profile-integration-build-r2.log`). Four
  profile lifetimes pass with identity-bound roots, both accepted trace forms,
  document/picker/Files/Terminal interaction, exact document bytes, all six
  portable values, exact machine settings and built runtime binary readback.
  Each boot has zero guest/offline allocation errors and unchanged protected
  regions. Evidence: `build/review/profile-desktop/proof-p2vxb6ic/`.
  Receipt SHA256: `132cbfc0454e755847ade24da00f3dd80bf27d9e76f0acc453ea73f5c9fefe03`.
- The fresh legacy installed-desktop save/reboot/reopen and damaged-volume
  refusal regression passes at `build/review/installed-disk/proof-kjq0tunh/`.
- The earlier shared-path/session/startup/preferences ASan/UBSan checks remain
  applicable: those implementations did not change. Current installed-root
  host admission and malformed-metadata controls also pass without weakening
  any kernel check. Authentication and permission enforcement remain separate.

The final acceptance-note commit changes documentation only. Task-owned VMs
are stopped, and the primary checkout and pre-existing review VMs remain
untouched. Older large disposable images are retained as verified lossless gzip
files, with restoration paths and original hashes in each run's
`compressed-images.json`; current final proof images remain directly readable.
OS hosted Actions are still disabled; Apps has no workflow files. No hosted CI
pass is claimed. Local acceptance above is the authorized merge gate.


## Guest transfer milestone — verified for review

Authorized target: guest export, import and activation using the v1 bundle,
with real migration between two disposable installed VMs and interrupted-import
recovery. Source main is `4c4c37a5`; branch `device/apple-silicon/profile-guest`.
The profile tool owns selected-document transfer and local binding metadata;
the desktop refuses pending imports. Filesystem publication is qualified through
guest operations rather than assuming host directory rename. Authentication and
permission enforcement remain independent. This task owns guest profile command,
its tests/build registration, this document and its disposable VM harness.
No physical disks or existing review VMs are part of this trial. One build or
VM harness runs at a time in this checkout. Completion requires full guest proof,
malformed-input refusal, interrupted import refusal/recovery, reboot persistence,
unchanged machine settings and a reviewable PR.


### Guest commands and recovery

The installed `profile` command uses the same v1 JSON bundle and strict bounds
as the host tooling. Run from the monitor (the transport volume is mounted at
`/work`):

```text
run /fat/bin/profile export /fat/home/local-a /work/transfer.json Documents/migration.txt Documents/binary.dat
run /fat/bin/profile inspect /work/transfer.json
run /fat/bin/profile import /work/transfer.json /fat/home/local-b local_b 2050 2051
run /fat/bin/profile activate /fat/home/local-b
run /fat/bin/profile recover /fat/home/local-b
```

Export includes only the named Documents files and portable preferences; zero
selected files exports identity/settings only. Import requires a new single-slot
`/fat/home/SLOT`, validates the complete bundle and expanded guest paths before
mutation, and refuses existing homes, UUID/alias/UID collisions and reservations
held by pending imports. Destination binding numbers remain metadata. Activation
executes the existing desktop with the validated profile context.

Before creating HOME, import exclusively creates and flushes a sibling
`HOME.profile-import.json` journal. It binds the destination, profile ID and local
binding to the original bundle's absolute path and SHA256. A pending marker inside
HOME blocks both the CLI and direct compositor activation. A reserved root scratch
file is flushed then renamed into each destination file; final files must match
the manifest exactly before recovery proceeds. Publication removes the pending
marker only after all required content, identity and directories are complete and
flushed. A remaining completed journal can be reconciled without rewriting data.

Recover resumes a valid journal using the unchanged original bundle at its
original mounted path. It verifies the existing tree before mutation, retries an
interrupted reserved scratch copy and never recursively deletes a profile.
Unknown files/directories, changed final bytes, changed bundles, malformed
journals and linked/special host fixture files refuse and preserve state.

A torn initial journal created before HOME exists requires manual inspection;
recover does not infer ownership from unreadable metadata. Interrupted export can
leave an incomplete exclusive output: inspect it and choose a fresh filename;
completed outputs are never overwritten. These are process-interruption and
clean-reboot guarantees for cooperative single-writer operation, not power-cut
atomicity or a hostile-writer boundary. Guest symlinks/access control are not
implemented here. Authentication and kernel permission enforcement remain separate.

The merged canonical filesystem namespace is consumed without duplicating its
implementation. This milestone retains the explicit `/fat/home` backing provider,
which is also exposed by the filesystem's `/home` alias.


### Final guest acceptance

Tested integrated source: `d309ae5819b7bfa1e44634e8dea70523e565e520`, consuming main
`c7be6574` (filesystem/installed-path acceptance and process-lifetime integration).
The full build and focused `test-guest-profiles test-profile-desktop check-docs`
pass. Fifteen real-filesystem host groups cover v1 cross-compatibility, binary
roundtrips, SHA/base64 boundaries through 1 MiB, aggregate/document/path limits,
malformed/deep/duplicate JSON, collisions/reservations, all five interruption
checkpoints, partial scratch recovery, unknown/mismatched data preservation,
changed bundles, torn journals and completed-publication reconciliation. The
same CLI implementation passes x86_64 ASan/UBSan (all 15 groups).

`tools/profile_guest_smoke.py` passes 13 VM lifetimes across source A and
destination B, including five independent interrupted-import copies of B. A uses
Text/Settings to edit the source profile and exports directly to a second,
guest-mounted GPT transport disk. That same disk moves to B; the host never
injects the exported profile payload. Each fault copy exits at `journal`,
`directories`, `document`, `identity` or `commit-ready`, reboots, refuses
activation and successfully recovers. Fresh B then imports, reboots and activates
through the guest CLI; Text reopens the edited document. Screenshot inspected.

The stable UUID, exact text/binary bytes and all six portable settings match.
A's 1001/1001 binding becomes B's 2050/2051 binding. Both complete machine settings
files remain unchanged. Installed profile/desktop/app binaries match the build;
guest and offline allocation checks have zero errors/leaks; GPT/boot regions
remain unchanged and the transport bundle is preserved byte-for-byte.

Evidence: `build/review/profile-guest/proof-b3mc85lt/`, receipt SHA256
`fa3c610698160f6e3e0166e780afa660a7748b95e6df4c0714749692f6fefae4`. Logs: `build/profile-guest-integrated-build.log`,
`build/profile-guest-integrated-host.log`, `build/profile-guest-integrated-vm.log`
and `build/profile-guest-sanitized.log`. Earlier proof images are losslessly
compressed with original hashes/restoration paths in `compressed-images.json`.
The first harness trial caught a monitor-line-length issue; the corrected
failpoint commands are bounded below 128 bytes, and that failed run is retained.
All task-owned VMs are stopped. No physical disk or existing review VM was changed.
This milestone is complete for PR review; a graphical chooser remains future work.


### PR320 integration review — complete

The user authorized review, fixes and merge after passing checks. Main
`805519b2` (workspace lifecycle PR319) is merged into this branch; existing
filesystem, process-lifetime and ownership policy is consumed unchanged.

Review found that a journal preceding HOME creation was insufficient evidence
to delete a reserved scratch file in an unmarked home. Recovery now refuses any
scratch without the pending marker, before removing anything. A real-filesystem
regression and a guest VM case prove refusal preserves the bytes. The VM test
then explicitly removes its synthetic unknown file as the operator and resumes
normal recovery. Authentication and permission enforcement are separate; the
change establishes transaction state, not user authority.

The existing torn-initial-journal boundary remains explicit: preserve unreadable
intent for manual inspection, with no HOME created. An interrupted creation of
the pending marker can similarly leave an ambiguous pre-marker scratch; that
state is preserved for manual inspection too. Valid marked transactions resume
partial scratch copies. No automatic ownership inference or recursive cleanup
is added, and no hostile/concurrent-writer or power-cut guarantee is claimed.


Reviewed runtime source `0cafd5806989a361166d1c48df64396643111595` passes 16 host groups,
x86_64 ASan/UBSan, the full matched build, existing profile tests and documentation
checks. The fresh 13-lifetime VM proof passes all five interrupted-import reboots,
the new unknown-scratch preservation test, migration and final desktop activation.
It verifies exact document/binary data, all portable settings, local bindings,
unchanged machine settings/transport, matched installed binaries, clean allocation
and protected regions. Evidence: `build/review/profile-guest/proof-_jun_0yp/`,
receipt SHA256 `41e78554d2d6ef4ffe64e1bcecad646aceef5f30fbe66afc8029facab8b6a319`. Logs: `build/profile-320-review-host.log`,
`build/profile-320-review-focused.log`, `build/profile-320-review-sanitized.log`,
`build/profile-320-review-build.log`, `build/profile-320-review-vm.log`.

No unresolved finding remains within this documented recovery scope. The final
acceptance-note commit changes documentation only. All task-owned VMs are stopped;
existing review VMs and the primary checkout remain untouched. Hosted Actions
remain disabled; the recorded local checks are the authorized merge gate.


## Desktop chooser milestone — complete for review

Authorized target: desktop-start profile selection, installation-local remembered
choice and recovery status, proved on two disposable VMs. Branch
`device/apple-silicon/profile-chooser` starts from merged main `b9ac7435`
(profile installation plans PR321), and integrates main `4f5edb0e` through PR323.
The newer main changes add filesystem tests/documentation without changing the
runtime policy consumed here. This topic owns catalog/selection persistence,
startup chooser, read-only recovery validation, host tests and VM proof. It
consumes the existing profile import and startup contracts. Authentication and
permission enforcement remain separate.

The startup journey preserves ordinary explicit `--profile=HOME` activation and
diagnostic modes. A valid remembered selection opens directly; a missing or
invalid remembered selection returns to the chooser with a reason. An explicit
`--choose-profile` opens the chooser even when a remembered selection is valid.
An ordinary legacy desktop with no profiles and no remembered choice retains its
existing startup behavior. Selection applies only when starting a desktop;
existing applications are not switched to another home during a session.

| State | Available action | Completion or recovery |
| --- | --- | --- |
| Ready profile | Open desktop | Revalidate ID/home and remember the local choice |
| Valid remembered choice | Ordinary desktop start | Open that profile without another prompt |
| Stale/corrupt remembered choice | Choose another profile or return to console | No silent shared-home fallback |
| Valid pending import | Recover | Existing guest command runs off the render loop; rescan afterward |
| Blocked import | Inspect status and return to console | Preserve ambiguous data; reconnect original bundle or inspect malformed state |
| Saving/recovering | Visible working state | Keep rendering/input responsive and show success or failure truthfully |

Recovery status uses `profile check-recover HOME`, a read-only invocation of the
same journal/bundle/tree preflight as actual recovery. Host tests verify that
checks at all five durable interruption checkpoints leave the tree unchanged.
The chooser does not introduce another import validator or guess ownership.
No physical disks or existing review VMs are part of acceptance. One matched
build or VM harness runs at a time in this checkout. Completion requires rendered
interaction, independent remembered choices/reboots, missing/invalid selection
handling, ready/pending/blocked recovery status, preserved machine settings and
reviewable changes with source-matched evidence.

### Selection and recovery contract

The catalog is installation-local under `/fat/home`. It considers completed
profile markers and sibling import journals, including a journal whose HOME has
not been created yet. Entries show their display name, local home and Ready,
Recovery needed or Unavailable status. Invalid identities, missing bundles,
ambiguous UUIDs/home casing, incomplete discovery and overflow refuse selection.
The scan is bounded to 4,096 directory entries and 16 displayed profiles. Names
follow the existing portable format's printable ASCII contract. Keyboard and
pointer navigation work at 640 × 480 logical pixels and larger supported modes.

`/fat/home/.desktop-profile` stores a versioned record of UUID, exact local HOME
and CRC32. It is local convenience/integrity metadata, never authentication or
permission authority. Opening revalidates the profile before writing an exclusive
temporary file, syncing, checking the previous record, renaming and syncing the
parent. A save error offers Continue anyway or Stay here. A late sync failure can
leave the new record visible; the UI truthfully says persistence was not confirmed.
An existing unknown temporary file is preserved for inspection. Explicit
`--profile=HOME` overrides the remembered choice without changing it.

Recovery is an explicit action using the existing guest CLI. Revalidation,
recovery and selection writes run in a worker while the chooser keeps processing
input and rendering. Returning to the console waits visibly for active work to
finish; it never kills or abandons a mutating child. Recovery success rescans and
requires a separate Open desktop action. Unreadable intent and ambiguous
pre-marker scratch remain preserved for manual inspection under PR320's rules.
No concurrent-writer or power-loss recovery guarantee is added. Profile metadata
and local UID/GID bindings retain their existing semantics; authentication and
permission enforcement remain separate work.

### Acceptance and evidence

Clean runtime source `3f9c225a504830580440488b41c267bdf9992d72` passes the matched
full build, 16 guest CLI host groups and the production catalog's real-filesystem
tests. Earlier unchanged provider/CLI code also passes ASan/UBSan, existing
profile/session/preferences tests, the desktop host suite and seven real image
adapter groups. Six installation-profile groups pass after preserving display
names in offline provisioning.

The fresh proof passes 14 VM lifetimes across two disposable installations and
their fault/small-screen copies: guest export and migration, forced chooser,
independent saved choices and reboot restoration, explicit override, missing,
stale and corrupt selections, genuine 640 × 480 pointer/keyboard interaction,
interrupted import recovery, and blocked-state console return. It verifies exact
documents/binary bytes, all six portable preferences, installation-local bindings,
unchanged machine settings/transport, matching installed binaries, protected
GPT/boot regions, and clean guest/offline allocation checks. External
`fsck.exfat` is not part of this proof.

Blocked-state comparison checks all 397 namespace entries and 396 metadata rows:
all payloads and ownership/mode/inode fields remain unchanged except four exact
background `svscan: webd waiting dependency` log lines and their timestamps; six
ordinary access times advance. Raw before/after snapshots and hashes are retained.
This distinguishes normal filesystem observation from profile mutation without
excluding profile, journal, choice or machine-settings bytes.

[Portable proof manifest](assets/screenshots/profile-chooser-manifest.json),
[small-screen chooser](assets/screenshots/profile-chooser-small.png),
[recoverable import](assets/screenshots/profile-chooser-recovery.png), and
[blocked import](assets/screenshots/profile-chooser-blocked.png) are committed for
review. The images were visually inspected, including the complete recovery
status label. Full evidence: `build/review/profile-chooser/proof-3zcf8_ih/`, receipt
SHA256 `3fd65ce67376ab6a9e6ca9929d4abccb735f8820fd61b57b15761552873172bc`.
Logs: `build/profile-chooser-build-final.log`, `build/profile-chooser-host-final.log`
and `build/profile-chooser-vm-final.log`. Byte-verified full evidence and matched
artifacts are also archived outside build cleanup at
`.local/profile-chooser-evidence/proof-3zcf8_ih/`.

The ordinary no-profile legacy installed-desktop regression also passes at
`e32ee17e`, evidence `build/review/installed-disk/proof-v6yp3fyg/`. Subsequent
runtime changes only widen chooser label rectangles. Earlier failed harness runs
are retained with verified lossless image compression: they exposed an incorrect
requested-versus-actual resolution assumption, stale serial matching, and an
overbroad whole-partition equality assertion. The final proof uses actual
framebuffer dimensions, ordered serial cursors and the strict comparison above.

Reproduce with a matched `make all`, `make test-profile-chooser` and
`python3 tools/profile_chooser_smoke.py --ovmf-vars PATH`; configure `OVMF_CODE`
and QEMU for the host. On Apple Silicon use `tools/macos/build.sh` for the full
build and the host compiler setup in the macOS development guide. Run one build
or VM harness at a time. All task-owned VMs are stopped. Existing review VMs,
the primary checkout and physical media remain untouched; Actions stay disabled.
Delivery is a scoped draft PR, with merge left for the requested review step.

### PR325 integration during publication

Main advanced to `4b5ddb7c` while the chooser proof ran. Its merged PR325 adds
host installation publication recovery and documentation, with no guest runtime
change. Both unit-record sections are preserved when resolving their conflict.
Integration `e8795d80` has identical top-level runtime Git objects and all three
recorded build payload hashes compared with the accepted `3f9c225a` proof.
The [integration receipt](assets/screenshots/profile-chooser-integration.json)
records this comparison and the three changed host tools explicitly; it does not
relabel the VM proof as a new source run. Fresh combined checks pass 29
installation preparation/profile/recovery tests, the chooser/catalog/guest host
checks and profile/session/preferences/bundle/image-adapter regressions. Logs
are `build/profile-chooser-pr325-{host,installation,docs}.log`, also preserved
outside build cleanup with the integration receipt. [PR326](https://github.com/PaulMDemers/SrvrOS/pull/326)
is the scoped chooser draft for review.

### PR326 merge review

The user authorized review, fixes and merge after checks. Main `1f177761`
(PR327's userspace salvage helper) is integrated; both independently appended
Makefile test targets are preserved. Review covered discovery bounds and parsing,
choice revalidation and save failures, worker/child ownership and console return,
shared recovery preflight, startup compatibility, and the proof assertions.
No unresolved blocker remains within the documented recovery scope. No guest
runtime correction was required. Authentication and permission enforcement
remain separate; selection metadata does not establish either boundary.

Clean `115899c3832ad67d9cc41fdb52b7697cf03acc40` passes fresh chooser/catalog,
guest CLI, profile/session/preferences, desktop and salvage host checks; 29
installation preparation/profile/recovery tests; ASan/UBSan on all 16 guest CLI
groups and the production catalog; and the complete matched build. The sanitizer
build retains warnings for vendored cJSON's existing deprecated `sprintf` calls
with a vendor-only warning exception; first-party warnings remain errors. The
initial strict SDK-warning failure remains logged. No vendor code was changed.

The rebuilt kernel/bootstrap hashes differ from the earlier proof, so review
uses a fresh complete two-installation, 14-lifetime VM run rather than reusing
that proof. Migration, independent choices and reboot restoration, explicit
override, missing/stale/corrupt choices, real 640 × 480 interaction, interrupted
import recovery and blocked-state preservation all pass. The same document,
preference, local-binding, machine-setting, transport, binary, allocation and
protected-region checks pass. Blocked comparison again covers 397 namespace
entries/396 metadata rows, with six access-time advances and four exact service
log lines as the only observation/background changes. Screenshots were inspected.

[Review receipt](assets/screenshots/profile-chooser-review.json) records exact
source/build hashes and limits. Full evidence:
`build/review/profile-chooser/proof-yrfw5f6o/`, receipt SHA256
`3e524cfd4604c49da75729d49f2aa126ec74609eb4e6560cd2c9b75c99e8c94c`.
Logs: `build/profile-326-review-{host,installation,sanitized,build,vm,docs}.log`.
Byte-verified full proof, logs and matched artifacts are retained outside build
cleanup at `.local/profile-326-review/proof-yrfw5f6o/`; completed proof images may
be losslessly compressed with original hashes/restoration paths recorded.

Review ran in isolated `SrvrOS-profile-326-review`, preserving separate
uncommitted proof-storage work in `SrvrOS-profiles`. All task VMs are stopped;
the three existing review VMs and primary checkout remain untouched. Actions
remain disabled. The final acceptance commit changes only documentation and the
portable review receipt. Ready for the user's authorized merge of PR326.

## Graphical transfer milestone — in progress

The desktop's SrvrOS menu and launcher open Profiles. Export explicitly adds
individual Documents files and a new output filename; Import selects an existing
bundle and a destination-local name. Review shows identity, actual portable
preferences, file names/sizes and destination. The visible export confirmation
also explains that selected bytes may contain sensitive content. A completed
import appears in the existing chooser at the next desktop start; running
applications retain their current home.

The UI consumes four fixed guest command forms, sharing the existing strict
codec/exporter/import transaction:

```text
profile preview-export HOME [Documents/FILE ...]
profile export-reviewed HOME OUTPUT SHA256 [Documents/FILE ...]
profile preview-import BUNDLE HOME ALIAS
profile import-reviewed BUNDLE HOME ALIAS UID GID SHA256
```

Previews return versioned bounded review JSON without creating output, HOME or a
journal. Export fingerprints the exact generated bundle; import fingerprints the
raw bundle bytes. Confirmation reconstructs/rechecks before mutation. The local
provisioner proposes available positive UID/GID metadata starting at 1000, with a
bounded 65,536-value allocation window and 4,096 sibling scan limit. Existing
homes and pending journals reserve values. Normal collision rules remain in
force, including shared GIDs; none of these values create an account or grant
permission. Source changes, existing output, duplicate profiles and ambiguous
metadata refuse with existing bytes preserved.

CLI execution, output draining/reaping and picker initialization have explicit
worker ownership outside the render loop. Close waits for active work. Failure
views retain inputs and explain partial-export/new-filename handling or import
recovery with the original bundle. No recursive cleanup or overwrite is added.

The VM harness uses explicit `deskd --profile-transfer-stop=CHECKPOINT` only in
an ordinary `--apps-trace` session. It passes the strictly allowlisted checkpoint
only to Profiles through the normal owned-child launch route. The app's matching
`--stop-after=CHECKPOINT` forwards the existing CLI test environment value;
ordinary launches do not inherit faultpoints. Desktop admission is unchanged:
unmanaged GUI clients are still refused. `export-partial` leaves a flushed partial
exclusive output; the five existing import checkpoints retain their previous
meaning. These are controlled process-interruption diagnostics, not power-loss
qualification. Authentication and permission enforcement remain separate.
