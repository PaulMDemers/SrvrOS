# Repeatable device trials

Use `tools/device_workflow.py` for the reference Air's v35 source-audit and
opt-in v36 VT-d drain and separately selected combined GGTT invalidation trials.
These profiles are silent and automatic;
it does not enable another model or turn a future GPU operation into an
accepted provider. Other trial profiles still use their documented harnesses.

The runner replaces copying a directory of Python scripts for each candidate.
A manifest freezes the source, branch, submodules, code contents, local unit
settings, hardware question, expected outcome and host jobs. State, raw logs,
VM results and acceptance proofs live under
`~/.local/share/srvros/device-workflow/`, outside `build/` and `make distclean`.
`current.md` and `active.json` there are the short local handoff. Git receives
sanitized summaries, never the unit configuration, USB contents or credentials.

## Configure once on this unit

Follow the [Ubuntu guide](ubuntu-device-agent-setup.md) first: initialize the
submodules, build the host tools, prepare the exFAT workspace, install a known
good payload and preserve Ubuntu as the default. Verify the actual model,
local USB UUID and installed recovery. The helper argument below points to
this machine's already authorized privilege mechanism; the workflow neither
reads nor stores its credentials. Placeholders must be replaced with local data.

```sh
python3 tools/device_workflow.py configure \
  --uuid LOCAL-UUID --recovery RECOVERY_RELEASE \
  --protected-release KNOWN_GOOD_GUI_RELEASE \
  --privilege-helper /absolute/path/to/local/priv.py
python3 tools/device_workflow.py install-service
```

Configuration is private `unit.json` in the durable store. Existing settings
are never silently overwritten. Keep every installed recovery release in the
verified payload bank. The collector requires a uniquely matching USB exFAT
partition. It normally unmounts a matching Ubuntu automount and rechecks identity
before offline access. A busy filesystem is refused: no force/lazy unmount or
process killing. After closing host files, retry with
`systemctl --user start srvros-return-collect`.

## One candidate, one hardware question

Commit the implementation and start with a clean checkout. Review the full
protocol before the boot: original owner and source/epoch; requester/DMA;
translation/cache; every reader; publication; and restoration or retained
failure. Record the capability-sized question and expected observable result.
Do not use extra boots merely to inspect another field in the same packet.

```sh
python3 tools/device_workflow.py new trial-name \
  --question 'Does this original held lifetime finish and retire completely?' \
  --expected 'Matching raw/checkpoint chain, exact release, clean USB and recovery'
python3 tools/device_workflow.py advance --run trial-name
```

`advance` stops at **QUALIFIED** by default. It runs a short development gate,
stages once, boots the **exact release** in disposable QEMU save/return and
missing-workspace fixtures, then qualifies the full frozen candidate. Both VM
receipt trees are verified again before qualification completes. Harness errors
therefore fail before the long host matrices; every host gate remains required.
The VM harness
accepts `--release` and an empty `--output-dir`; it no longer needs temporary
edits to `build/hardware-boot/latest`. VM evidence never accepts native ownership.
Qualification is full for this diagnostic's host matrix and recipe; new kernel
work must add its own required sanitizer, weakening controls, preservation and
image-isolation jobs before it can be treated as qualified. The initial profile
is not a replacement for all project-wide release checks.

Add required checks with `new ... --plan /absolute/path/extra-jobs.json`.
That JSON has `development` and/or `qualification` arrays of jobs with unique
`name`, string-array `argv`, checkout-relative `inputs` globs, `outputs` globs,
and bounded `timeout` seconds; optional `env` holds explicit overrides.
Additional jobs cannot remove the profile's default gates. Shell interpolation
is not used. The per-command agent transport ID is stripped from both the child
environment and cache identity; other environment values are hashed privately.

When the candidate's required checks and existing user reboot authorization
permit a physical trial:

```sh
python3 tools/device_workflow.py advance --run trial-name --through boot_requested
```

This performs fresh local model/EFI/AC, recovery, default Ubuntu, payload and
one-shot checks; read-only USB baseline hashing and offline filesystem,
allocation, writable and headroom checks; exact-release installation keeping
recovery; one-shot arming; and a final baseline/check before reboot. Host audio
is verified at **1%**. The v35 guest profile is silent. Intent is flushed before
each side effect and the reboot request is durable before `systemctl` is called.
No human checklist is needed for this profile. Uncertain retained state still
halts visibly in SrvrOS; safe saved/unmounted outcomes return automatically.

## Return, collect, review

The enabled **user** service runs at Ubuntu login. `resume` only collects a
pending run after the Linux boot ID changed. It never runs validation, installs,
arms or reboots. No pending return means no work and no notification. It refuses
changed collector code. After modifying code between boot and return, preserve
the raw logs and review an explicit collector migration instead of silently
claiming the original qualification.

```sh
python3 tools/device_workflow.py resume
python3 tools/device_workflow.py status
journalctl --user -u srvros-return-collect.service
```

Collection copies and verifies new raw session directories to durable local
storage **before parsing**. It then hashes prior file preservation, checks the
exact release/exit/checkpoint/native chain, repeats offline integrity checks and
verifies consumed one-shot/recovery. A missing completion, extra new sessions,
changed prior file or parser rejection is failed/incomplete evidence. Raw logs
remain available. The prepared partition is always unmounted in cleanup.
Dirty or changed boot metadata on return is refused after preserving readable
raw logs; it never becomes a pass and the runner performs no repair.

`COLLECTED_REVIEW_REQUIRED` is intentionally separate from native acceptance.
Review every required original lifetime retirement assertion and the scope of
the result, then attach the full reviewed proof. A positive nested diagnostic
alone cannot close the run:

```sh
python3 tools/device_workflow.py accept --run trial-name --proof /absolute/path/return-proof.json
# Or, after reviewing a completed collection as a failure:
python3 tools/device_workflow.py reject --run trial-name
```

A proof must match the exact runtime source, release and session and affirm
native acceptance and return integrity. This explicit agent review step is not
a request for human input. A new run is blocked until the previous run is closed.
A well-formed retained native failure is collected as incomplete for review and
rejection; an acceptance proof cannot promote that incomplete chain. Malformed
or missing evidence still fails collection after raw preservation.

When Ubuntu protects `/boot/grub/grub.cfg`, the runner reads that exact file
through the configured privilege helper and then performs every original
recovery assertion. Builds, VMs and PipeWire remain in the normal user session.
`import-legacy CHECKPOINT` verifies and replays a previously accepted v35 run's
sealed raw/payload/physical proof and preserves its **original** attribution; it
never upgrades old evidence to the current runtime.

Interrupted host validation resumes using intact receipts. Interrupted staging
requires `reconcile --run NAME --action stage --release EXACT_RELEASE` after
verifying the payload. An uncertain install/arm/reboot is never blindly repeated;
inspect the durable intent and use `reconcile --action disarm` only if the
installed candidate and unchanged host boot prove that action safe. A changed
host boot requires collection first. `cancel` only closes pre-install runs.

## Validation and fixture reuse

Host validation receipts key the argv, executable bytes, environment, timeout,
declared dependency contents/glob membership and outputs. Inputs are hashed again
at completion; failure, timeout or changed inputs never seals a pass. Altered
outputs invalidate reuse. Documentation outside a job's declared inputs does not
invalidate it. Full frozen qualification deliberately executes the complete
suite; completed receipts within that same run make interruption resume safe.
Declare all dependencies when adding a job. The source-audit test jobs also bind
the actual fixture executable bytes, so a new compiler result cannot reuse an old
parser/test pass.

The reusable compiler obtains flags and translation units from the canonical
Make recipe, discovering a requested batch in one Make invocation. It fingerprints cc and its selected cc1/assembler/linker helpers,
flags, environment, and compiler-discovered **system and project** headers.
Only changed units rebuild; each mutation can reuse the unchanged support units:

```sh
python3 tools/fixture_compile.py build/tests/intel-source-audit-adapter
python3 tools/fixture_compile.py build/tests/intel-source-audit-adapter \
  --source tools/test_intel_source_audit_adapter.c=/absolute/path/mutant.c \
  --output build/tests/audit-mutant
PYTHONPATH=tools python3 -m unittest test_intel_source_audit
```

Source-audit fault cases use a framed server that forks a pristine parent for
each case. The parent never executes a case. No retained or one-use object is
reset and reused across cases; all existing checks and clock/line fault points
remain. `SRVROS_FIXTURE_BATCH=0` retains ordinary subprocess execution.
When sanitizer options are set, the client defaults to that canonical path unless batching is explicitly selected.
The server has bounded request/output sizes, disabled core dumps and per-case
wall-clock limits. A transport failure fails the case instead of retrying it.

The workflow and fast fixture compiler share a checkout lock. Do not run an
ordinary Make/stage/VM harness alongside them: older tools do not acquire it.
The helper's `--under-workflow-lock` is only for an already locked caller.

## Retention

```sh
python3 tools/device_workflow.py gc                 # proposed deletions only
python3 tools/device_workflow.py gc --apply         # managed disposable caches
```

Retention affects only content-keyed validation/object cache entries, preserving
the newest entries and applying age/size limits oldest first. Runs, raw logs,
installed/banked recovery, accepted evidence and imported pinned banks are never
eligible. Legacy build directories are not guessed disposable. Archive or prune
those only after checking their manifests and durable accepted copies. Keep the
short [Air current status](machines/macbookair7-2-current.md) current; retain dated
journals as history instead of adding the same result to several long ledgers.

Successful VM runs losslessly gzip their own exFAT scratch image, verify the
decompressed SHA-256, save a durable archive receipt, then remove the unpacked
copy. VM receipts also hash all saved artifacts. Failed runs retain their
original evidence. This packing never targets a physical USB device or a
banked boot payload. Use `gzip -dk IMAGE.exfat.gz` when an offline tool needs the
original VM image again.

## Native VT-d drain profile

`new --profile intel-vtd-drain-v36` selects the separate writer image, recipe
and strict collector. It retains the full v35 regression matrix and adds the
actual adapter, core, boot gate and native image build. The same safe install,
one-shot/recovery, raw-first collection and explicit native-review gates apply.
Ordinary/source-audit recipes cannot stand in for this profile.

Prepare `build/device-fixtures/macbookair7-2-dmar.bin` from this unit's previously
accepted raw DMAR capture, never another host's table. The runner requires the
actual Air7,2 model and validates the exact136-byte checksum/hash/topology before
creating the run; the bytes enter both the frozen manifest and validation keys.
Keep an accepted copy outside build for restoration after distclean. Raw firmware
is not committed. See the [native protocol and receipt](intel-vtd-drain.md).

```sh
python3 tools/device_workflow.py new trial-name --profile intel-vtd-drain-v36 \
  --question 'Does the original held lifetime complete one disabled-state graphics memory drain?' \
  --expected 'Exact global DR/DW completion, full original retirement and preserved files; no GPU grant'
python3 tools/device_workflow.py advance --run trial-name --through qualified
```

Qualification cannot accept physical completion. After local/VM qualification,
use the existing install/arm/reboot sequence under the user's authorization, at
volume1%. An unsafe retained failure halts visibly; a missing saved completion
stays failed or incomplete when Ubuntu collection resumes.


New VT-d runs created after the authorized2026-11-15 policy review pin wire
version3 and its explicit150ms aggregate preparation/return budget. Historical
version1/2 runs keep100ms, including their failed outcomes; an older receipt
cannot be promoted by changing the current policy. Admission/source age and
individual operation waits remain unchanged, as do GT300ms, HDA400ms and
outer500ms. Include both the legacy and150ms core fault/boundary/sanitizer/
weakening suites when qualifying the selected policy. See the
[policy contract](intel-vtd-drain.md#explicit150ms-aggregate-policy-authorized2026-11-15).


### Combined System Agent invalidation profile

`new --profile intel-ggtt-invalidate-v36` selects the dedicated
`--intel-ggtt-invalidate` payload and pins **wire version4**. It keeps the complete
original CPU/audit/remapper hold, then adds the exact System Agent DWORD write/
posting read and its independent432byte receipt. Legacy `intel-vtd-drain-v36`
continues to pin version3; historical runs retain their immutable versions.
No ordinary image gains the new writer. The source/epoch/root/alias/policy and
complete enclosing retirement checks, original150/300/400/500ms budgets,
raw-first collection and reviewed acceptance remain mandatory. The exact-stage
VM proves refusal/persistence/return only, not native execution.

Qualification includes the original native/core/audit/boot/workflow/replays plus
the new adapter's both-route fault/record/sanitizer/compiled-control suite, both
pure-core policy matrices, native object width/fence checks and image isolation.
Measure actual combined hardware timing before accepting that same lifetime;
there is no new GPU mapping, pipeline/cache/reader assurance or page-flip grant.
See the [native operation contract](intel-ggtt-invalidation.md#combined-original-held-native-candidate).


Combined stages now use configuration revision3, retaining the same15-token
recipe, wire4 record, kernel operation and existing budgets. Revisions1/2 remain
readable; this creates independent immutable release metadata for host-only
qualification corrections. Both graphics profiles run `image-structure` directly
after fixture preparation in development and qualification, before the long
fault suites. It checks the explicit image/fixture allowlists, legacy exclusion,
staging and (for the combined profile) VM classification, the explicit Broadwell
TCG CPU fixture and collector dispatch. Battery/ordinary VMs retain their own
CPU selection. The CLI also exposes the combined profile directly.

### Private command-memory profile

`new --profile intel-command-memory-v37` selects the separate
`--intel-command-memory` image and pins **wire version5**. It retains the full
remapper/GGTT qualification and adds the
[private CPU command-memory owner](intel-command-memory.md),512byte receipt,
allocator/mapping/cache/retirement fault cases, fatal sanitizers and weakening
controls. The structural preflight checks the new route and raw collector;
native object checks verify supervisor writable/NX aliases and submission-core
exclusion. The exact-stage VMs use the explicit Broadwell fixture. Version4 and
older acceptance stays bound to its unchanged original raw logs.

The first native question is four private RAM pages held through the original
GPU diagnostic and fully retired afterward. Neither CPU alias ownership nor
zeroed context storage grants a GPU mapping, executable LRC or submission.
The selected150/300/400/500ms original bounds remain unchanged. Only matching
native collection and independent media/recovery review can accept that lifetime.

Air7,2 now has one [accepted original kind2 CPU arena lifetime](machines/macbookair7-2.md#native-private-command-memory-accepted)
on4864ad3a/838082d16f6be16d, after33 frozen host gates and both exact-stage VMs.
Its original final log clipped a boot-command-line prefix at the64KiB bound.
Acceptance records an explicit reviewed view with exact complete-checkpoint
overlap and snapshot sequence/length checks; raw files and all hardware receipts
remain unchanged. The original collection refusal is retained. This is not a
general permission to discard unknown receipts or replace missing hardware rows.

Future workspace snapshots omit only a partial leading line and preserve an
exact-boundary first record, the64KiB cap and raw total-byte sequence. Raw
bootlog byte APIs retain their existing semantics. Missing protocol/checkpoint
records still refuse; a logging change never accepts a native lifetime by itself.
The full suites still repeat their structural checks; no gate is skipped.
