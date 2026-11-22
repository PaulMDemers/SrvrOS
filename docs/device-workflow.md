# Repeatable device trials

The Air7,2 `intel-firmware-import-v1` profile selects `--intel-firmware-import`.
It imports both classified firmware buffers as a permanent GPU-address reservation
and borrows slot 8704 under the existing CPU and display guard. The receipt
records the two PTE samples, the return and three refused competing claims.
A successful run verifies software ownership and original-entry preservation.
Native GPU write and traffic-drain acceptance remain separate.

The Air7,2 `intel-private-range-v1` profile selects `--intel-private-range`.
It uses the range probe's two existing deadlines and the common display and
CPU guards. Its version 13 result records the aperture searched, allocation,
reread and return, including an exhausted search or an active reader that
prevented the search. This is an unattended software reservation trial with
the original primary plane active. [Implementation milestones](intel-bdw-primary-flip.md#driver-owned-second-surface-on-air72)

The [GT flush acknowledgement trial](intel-completion-prerequisites.md#engine-and-l3-acknowledgement-trial)
uses `intel-gt-flush-v59`, `--intel-gt-flush` and wire12. The source-bound
image takes three read-only message snapshots under the original requester
hold. Both exact-stage VM paths are required. The native receipt preserves
engine acknowledgement changes and sampled L3 state under the existing clocks
and 96 KiB log bound. New wire12 images also record the boot CPU SMI counter
at protected store and retirement checkpoints. A change retains exclusion
and halts. Version 3 of the flush receipt also records the raw and decoded
graphics memory arbitration state. It closes only as a diagnostic. This
profile is silent and saves and returns to Ubuntu on a safe outcome.

The opt-in [private CPU context-image](intel-bdw-bcs-context.md) profile
`intel-bcs-context-v46` selects a separate `--intel-bcs-context` image/recipe
and exact wire8. It retains all42 v45 slots and adds context image/model slots.
Initialization, both original held checks, post-retirement verification/erase
and original arena cleanup must agree. The version 3 context receipt also
requires the five submission mappings to close and reopen, six successful
alias walks and two traffic samples bracketing 30 GGTT reads. Version 4
also verifies that the WiDi queue register page is unmapped in the boot CPU
address space at both held boundaries. Both exact-stage VM paths remain
mandatory. It is silent/automatic and closes only as a diagnostic; no GPU
mapping, context execution or DMA/cache/reader ownership acceptance follows.
The raw-copy96KiB bound recognizes wire8 with wake37; unknown wires retain64KiB.
The first [matching native CPU image lifetime](intel-bdw-bcs-context.md#native-result-one-cpu-image-lifetime-complete)
now completes, with strict automatic collection and no raw-size migration.
It closes as DIAGNOSED, preserving the distinction from actual GPU acceptance.

The opt-in `intel-gt-arena-v45` profile composes the
[private CPU command arena](intel-command-memory.md#full-gt-composition-one-native-diagnostic-complete)
with the original full-GT memory child. It selects `--intel-gt-arena`, a separate
image/recipe and exact wire7;42 required host/qualification slots preserve all36
v44 gates, add actual arena faults/controls, legacy-policy regression and the new
image. The explicit remapper/GGTT caller-isolation guards also run immediately
after fixture compilation in development; full qualification reruns both modules.
Both exact-stage VM paths remain mandatory. The ring/save/raw bound stays
96KiB and all practical/individual hardware deadlines stay unchanged. Complete
matching native evidence closes as a diagnostic, with separate CPU setup/held/
cleanup timings and no GPU mapping/tear-free acceptance. Safe saved/unmounted
outcomes return to Ubuntu; uncertain lifetime/retirement retains storage and
halts. Earlier wires reject the new arena packets.

The first Air7,2 wire7 run now [completes and retires on hardware](machines/macbookair7-2.md#native-full-gt-private-cpu-arena-completes).
The generic historical-session size selector must include wire7 together with
wake37 before reading its full96KiB allowance. The first collection refused a
73,050-byte log under64KiB; raw-first copies survived and an explicit reviewed
read-bound-only migration completed every unchanged integrity/strict receipt
guard. A larger read allowance never grants native acceptance. Unknown versions
and ordinary sessions retain64KiB; the absolute cap remains96KiB.

Use `tools/device_workflow.py` for the reference Air's v35 source-audit and
opt-in v36 VT-d drain, combined GGTT invalidation, v37 CPU command-memory and
[v38 joined native admission](intel-command-ownership.md) and separate
[v41 full-GT owner/epoch](intel-bdw-gt-bootstrap.md) trials. Only v38 and the full-GT profiles
select a96KiB ring/save/raw collection bound; older profiles remain64KiB.
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

The separate [existing-mapping census](intel-bdw-stolen-scanout.md) profile
`intel-stolen-v55` selects isolated pre-init wire8/configuration9. It freezes
classifier/actual-adapter, reader/cache transport and the actual VMM whole-root
[CPU backing guard](intel-bdw-cpu-backing.md), plus source/cache/window/return
regressions. Both exact-stage VM paths,64KiB collection and the original
recovery/integrity sequence remain. It also freezes the actual VMM reserved-span
guard/mutators. The native adapter pins original display exclusion, performs
guard begin/check/close with three fresh PMM/whole-root validations, then unpins
only after clean close. New guard and per-phase walk rows reject impossible
prefixes, lost retention, missing scans, changed clocks and fabricated authority.
The current trial captures the PCI firmware endpoint and requester state around
a fresh reader sample inside the held CPU guard. The result identifies the
first unmet ownership condition separately from diagnostic completion.
The CPU phases use the second pass's
remaining2-second budget; it does not restart that clock. Historical profiles
47–54 remain readable but cannot launch the current image.

The [native v55 return](intel-bdw-stolen-scanout.md#native-v55-ownership-gate-result)
completed the held observation and closed as `DIAGNOSED`. The collector
preserved the missing firmware transfer as the first unmet condition and
verified the matching logs, offline USB integrity and consumed one-shot.

Read-only completion closes only as DIAGNOSED. It can report that CPU backing
is unavailable because of a reservation/tag conflict or an existing CPU alias;
it does not grant firmware/DMA/cache/all-reader ownership or tear-free output.
No reset/submission protocol or ordinary desktop startup is selected. Existing
metadata UC3 preflight, source/root/actual-alias checks, both complete vectors
and independent whole-span cache replay remain mandatory. Unknown retirement
retains exclusion and halts; safe saved/unmounted outcomes return to Ubuntu.

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

### Intel recovery without a framebuffer

Use the exact staged Intel capture to check serial logging, USB persistence
and reset with the VM's display adapter removed. The harness requires the
kernel's `framebuffer: unavailable` line before recording a result.

```sh
tools/host-env.sh python3 tools/hardware_boot_smoke.py --release RELEASE \
  --probe-check --probe-no-framebuffer --output-dir build/review/RELEASE-no-fb
tools/host-env.sh python3 tools/hardware_boot_smoke.py --release RELEASE \
  --probe-check --probe-no-framebuffer --probe-missing-workspace \
  --output-dir build/review/RELEASE-no-fb-no-workspace
```

The first case must save and unmount the disposable USB workspace and reset.
The missing-workspace case must report the halt over serial and stay running
until the harness detaches. Both are VM checks for a future display takeover.

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

The isolated `--profile intel-edp-baseline-v1` trial checks Air7,2 inherited
PHY, the complete primary/tail translation range and fresh primary-plane
CRC across reference, alternate and repeated reference frames. Its exact
source-bound image runs both no-framebuffer VM checks.
The final reference window checks an acquired then released driver
display-well request while the firmware request keeps the well powered.
It also verifies an independent translation census and records any observed
blockers to a separate native display disable and restore trial.
On the Air it saves before reaching live CRC control. A safely retired
outcome saves and returns to Ubuntu. An uncertain lifetime keeps the lease
and halts. Review a complete native receipt with `diagnose`. The receipt
qualifies the inherited baseline, with post-reset restoration and ownership
remaining separate. [Hardware trial contract](intel-bdw-edp-recovery.md#inherited-display-hardware-trial).

The explicit `--profile intel-edp-cycle-v1` runs the native display-only
shutdown and restore after the reference checks. It saves its checkpoint
before the screen goes dark. Both restored primary CRCs must match the
original, and the original interrupt mask and well request must be
verified at retirement. The guest is silent at the 1% host volume
setting and requires no input. A safe saved outcome returns to Ubuntu.
If a retained failure leaves the screen dark for 60 seconds, restart
manually. The collector treats a validated cycle as a diagnostic of the
original display path. [Cycle contract](intel-bdw-edp-recovery.md#display-disable-and-restore-rehearsal).

The explicit `--profile intel-edp-flr-preflight-v1` verifies a CPU-owned
framebuffer copy and PCI configuration across that display cycle. It
saves a boot checkpoint, runs silently at 1% host volume, and returns to
Ubuntu on a safely retired outcome. Its completion receipt requires
three matching pixel and configuration samples and release of the CPU
copy. The physical function stays active during this preflight.
[PCI reset preparation](intel-bdw-edp-recovery.md#pci-function-reset-preparation).

The explicit `--profile intel-edp-dark-well-v1` adds a driver-well
release and reclaim while the panel is dark. It requires three matching
firmware-well samples and both driver request readbacks, followed by the
same pixel, PCI and display restore checks. This guest is silent and
returns after saving a safe result. If the panel stays dark for 60 seconds,
restart manually. [Driver well recovery](intel-bdw-edp-recovery.md#pci-function-reset-preparation).

The explicit `--profile intel-edp-pci-hold-v1` closes PCI decode after
the panel is dark and bus mastering is inhibited. It checks the CPU RAM
copy while decode is closed and compares the live backing with memory
decode reopened before restoring bus mastering. It then verifies two
original CRCs and returns after saving a safely retired outcome. The
profile is silent at 1% host volume. If the screen stays dark for 60
seconds, restart manually. [PCI decode hold](intel-bdw-edp-recovery.md#pci-decode-hold-and-reset-default-clock).

The explicit `--profile intel-edp-dark-pwm-v1` tests recovery of the
original backlight PWM with the panel off, then checks the framebuffer and
both display CRCs. It is silent at 1% host volume and returns after saving
a safely retired result. If the panel stays dark for 60 seconds, restart
manually. [Backlight PWM recovery](intel-bdw-edp-recovery.md#backlight-pwm-recovery).

The explicit `--profile intel-edp-dark-pps-v1` waits for panel T12 to
expire before restoring and verifying the original panel timing words.
It keeps the panel dark until the framebuffer and both original CRCs
verify. The guest is silent at 1% host volume and returns to Ubuntu
after saving a safely retired result. If the panel stays dark for
60 seconds, restart manually.
[Panel timing recovery](intel-bdw-edp-recovery.md#panel-power-sequencer-timing-recovery).

The v49 census and historical progress-enabled v48 frozen contract add `stolen-progress.log` to
its three original raw logs. The collector preserves and parses its pending
operation history before refusing a missing exit snapshot. See the
[progress contract](intel-bdw-stolen-scanout.md#pending-operation-progress-journal).
Progress is diagnostic only and cannot complete or accept a run. Historical
manifests keep their original raw-file contract.

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
exact-boundary first record and raw total-byte sequence. Ordinary/older images
retain64KiB; the isolated v38 and full-GT profiles use96KiB. Raw
bootlog byte APIs retain their existing semantics. Missing protocol/checkpoint
records still refuse; a logging change never accepts a native lifetime by itself.
The full suites still repeat their structural checks; no gate is skipped.

### Paired native admission result

The [v38 observer](intel-command-ownership.md) now has one accepted original
kind2 Air7,2 lifetime on2abaf8f9/f0a84ce57c164e9c. All23 frozen checks and both
exact-stage VMs pass. Ubuntu's unchanged return service collects the matching
session; full original retirement, raw/checkpoint validation, prior-file and
offline/recovery review close it as bounded native admission evidence. The
67,351byte raw exit fits96KiB without prefix recovery. This accepts no GPU
mapping, DMA/cache completion or all-reader ownership. The local one-shot
handoff waits for the qualifier to finish, requires every frozen receipt and
then uses the existing guarded install/arm/reboot sequence; it never retries
failed qualification or bypasses fresh media/recovery/1% checks.


## Separate full-GT owner and epoch trial

New v41 runs require version3 owner receipts: the original GT live-check
interval/snapshot receipt, a single-use read-admission receipt and five measured
guard-cost rows. Historical v39/version1 and v40/version2 collection remain
explicitly supported; new v39/v40 creation refuses. Runtime/header/collector
versions must match the frozen manifest. No legacy record is upgraded into a
fresh live admission.

The Air7,2-only `intel-gt-bootstrap-v41` profile selects the separate
`--intel-gt-bootstrap` image and exact `srvros.intel-ring=gt-bootstrap-v1` recipe.
It does not require or reuse the old nonzero/reset/VT-d source graph. Its
version3 owner/version1 reset receipts use wake wire37; these names are separate from
the older v37 CPU-command-memory profile. It accepts no GPU memory or tear-free
capability. Native acceptance is pending until the matching physical run passes.

```sh
python3 tools/device_workflow.py new original-gt-bootstrap \
  --profile intel-gt-bootstrap-v41 \
  --question 'Does this original full-GT owner restore all fences and retire with a fresh new-epoch census?' \
  --expected 'Two full pulses, exact fences/display/backing, fresh epoch, original clocks, complete parent retirement and clean media/recovery'
python3 tools/device_workflow.py advance --run original-gt-bootstrap
```

For the firmware-cost follow-up, capture this host's DSDT before creating a run:

```sh
mkdir -p build/device-fixtures
sudo install -m 0644 /sys/firmware/acpi/tables/DSDT build/device-fixtures/macbookair7-2-dsdt.bin
```

Keep this raw firmware private and ignored. Do not substitute a sibling table.
The Air7,2 matcher must accept the captured positive fixture; exhaustive prefix,
byte/paired mutations, independent original-reference equality, fatal sanitizers
and compiled controls run in both frozen firmware-identity jobs. The table bytes
are hashed job inputs; missing/changed inputs cannot seal qualification. Preserve
the local fixture outside build before cleanup, or recapture it on the same host.
This adds two jobs (22 total), keeps v41/wire3 and all original deadlines, and
supplies no native timing or ownership acceptance.

As with the other profiles, ordinary advance qualifies without arming. Under
existing user reboot authorization, advance through `boot_requested` only after
all frozen development, native fault/control/sanitizer/legacy/image gates and
both exact-stage VM paths pass. The workflow verifies1% volume and records the
model/source/release/workspace/recovery and one-shot state before reboot.
Instructions/progress are visible before aperture exclusion. No input is
required. A safely saved outcome returns automatically; uncertain GPU/fence or
parent state retains the physical owners and halts behind the visible restart
instruction. It cannot rely on drawing through an unverified CPU fence.

The collector preserves raw logs before applying its explicit historical-entry
view. Original32-fence vectors, five controls, two pulses, partial PCI/display
prefixes, source clocks and the actual new epoch remain independently required.
A logical child returned before a failed closing clock does not release the
independent physical parent or become a pass. Collect/review the matching run
before any next candidate. VM refusal/save/return is never native acceptance.

## Multi-second diagnostic series

Air7,2 has an explicit `intel-gt-seconds-v42` profile for the user-requested
three-run timing experiment. It selects a separate image/recipe and owner wire4:
preparation3s, GT phase3s/total4s, HDA5s and outer6s. Individual hardware waits,
poll caps and ownership/preservation checks remain unchanged. The original
full-GT profile retains its original limits. See the
[timing contract](intel-bdw-gt-bootstrap.md#separate-multi-second-diagnostic).

```sh
python3 tools/device_workflow.py new seconds-trial-r1 \
  --profile intel-gt-seconds-v42 \
  --question 'Does the complete original GT lifetime retire under multi-second aggregate limits?' \
  --expected 'Complete diagnostic chain, preserved state and clean media/recovery; ordinary timing acceptance withheld'
python3 tools/device_workflow.py advance --run seconds-trial-r1
# Only after full qualification, with existing local physical reboot authorization:
python3 tools/gt_seconds_series.py start --first-run seconds-trial-r1
```

The first run must be fully qualified. The profile adds wide-policy/series tests
and the separate image to both validation levels, retaining all22 original jobs
(26 total), followed by both exact-stage VM persistence/recovery paths. The
explicit series installs its own user service, ordered after the unchanged
collection-only startup service. The configured user manager must start on
Ubuntu return; verify this host's setup rather than assuming another unit's
login/linger policy. Keep the source commit and unit configuration frozen until
the series ends, including documentation commits. Every later trial goes through
the guarded qualification/install/media/recovery/1%/one-shot sequence again.

After each return it collects and strictly verifies the matching raw session and
offline integrity, runs `diagnose`, independently banks evidence, then advances.
It stops after exactly three complete diagnostics, or on any failure, incomplete
result, source/configuration change or unsafe interrupted operation. It never
promotes diagnostic timing to ordinary acceptance or bypasses a failed gate.
An uncertain GPU/fence/parent state still halts for a manual restart. Safe saved
completion returns to Ubuntu automatically; no human input is required.

```sh
python3 tools/gt_seconds_series.py status
python3 tools/gt_seconds_series.py stop
```

`stop` prevents future trials; an already requested reboot may finish and is
still collected. The private plan/action log and per-run records live under
`~/.local/share/srvros/device-workflow`, and banks under
`~/.local/share/srvros/evidence`. A stopped plan must be reviewed/archived before
another explicit series. A successfully diagnosed run is terminal `DIAGNOSED`,
with original timing acceptance and physical tear-free both false. `diagnose`
rechecks raw hashes, typed chain and installed/media/recovery state before closing.
The summary records actual preparation, original parent lifetimes and separate
closing guard, borrow return and final guard costs for selecting future budgets.

The first Air7,2 series on source `2a61bed1` / release `2281c7cc04bacba7`
completed all three runs, with26 jobs and both exact-stage VMs repeated for each.
The controller reached `COMPLETE`, independently banked53 files per result and
disabled its own service without a fourth boot. All three diagnoses remain
separate from ordinary timing acceptance. The
[unit journal](machines/macbookair7-2.md#three-multi-second-gt-diagnostic-runs-complete)
records actual times, original-limit comparisons and remaining GPU ownership work.

## Practical GT timing candidate

The Air7,2-only `intel-gt-practical-v43` profile selects
`--intel-gt-bootstrap-practical`, its own image/recipe and owner wire5. Limits
are250ms preparation,100ms maintenance phase,500ms GT,750ms HDA and1s outer;
ordinary and seconds policies remain unchanged. Successful completion still
requires diagnostic review through `diagnose`; ordinary `accept` refuses it.
No GPU memory/completion authority is added.

```sh
python3 tools/device_workflow.py new practical-gt-trial \
  --profile intel-gt-practical-v43 \
  --question 'Does the original full-GT lifetime retire within the measured practical limits?' \
  --expected 'Exact wire5 policy, original owners and complete preservation/retirement; clean media/recovery'
python3 tools/device_workflow.py advance --run practical-gt-trial
# After all30 frozen jobs and both exact-stage VMs, under existing reboot authorization:
python3 tools/device_workflow.py advance --run practical-gt-trial --through boot_requested
# On Ubuntu return, the ordinary service collects only; review and close:
python3 tools/device_workflow.py diagnose --run practical-gt-trial
```

The30-job plan retains the original22 gates and four seconds gates, then adds
four practical gates. The private replay fixtures are
`build/device-fixtures/gt-seconds-history/r{1,2,3}/` with `report.json`, `exit.log`
and `session-a.log` from this unit's completed seconds series. Restore only from
its independently verified banks, checking each source hash against `bank.json`
before copying; never copy another unit's acceptance or edit the raw bytes.
Both practical jobs require exactly three original reports and exact reparse
equality/checkpoints; all nine files are hashed job inputs. Missing fixtures
refuse frozen qualification. Portable tests may skip native replay when those
private files are absent, but cannot establish this unit's qualification.

The previous seconds controller remains complete/disabled and is not reused or
generalized for this trial. The single-run workflow verifies model, AC, this
unit's USB/recovery, exact payload, media,1% volume and one-shot state. Guest
instructions precede aperture exclusion; no input is required. Safe saved and
unmounted outcomes return to Ubuntu, while uncertain ownership halts visibly.
The current [unit record](machines/macbookair7-2-current.md) distinguishes host
qualification from the new physical result.

The first practical trial now completes on Air7,2 runtime `b7752e6f` / release
`33d9e5dacde0d12b`, session `session-0006607172203a36-00`. All30 jobs, both
exact-stage VMs, receipt-guarded single handoff, automatic return collection and
`diagnose` pass;58 independently banked files and raw/checkpoint reparse verify.
Preparation153.910ms and outer385.267ms fit the250ms/1s bounds, with complete
parent/15-alias retirement. It remains diagnostic-only with no original-timing
or GPU ownership grant. The single handoff has ended and no further boot is armed.

### Full-GT memory composition candidate

`--profile intel-gt-memory-v44` selects the original post-reset child,
`--intel-gt-memory` stage flag and `gt-bootstrap-memory-v1` recipe. Wire6 requires
its complete same-owner remapper/GGTT/alias/census receipt in addition to the
original full-GT chain. All36 frozen jobs and both exact-stage VM paths must pass
before boot. Current practical clocks and96KiB raw capacity stay unchanged.
The ordinary collector validates the matching raw checkpoint and offline USB
integrity; `diagnose` can close a complete diagnostic. `accept` refuses it.
This cannot accept native tear-free or discharge memory/cache/reader obligations.
See the [composition contract](intel-bdw-gt-bootstrap.md#same-owner-post-reset-memory-preparation-wire6).

Air7,2's first completed wire6 native run is source `c86685d8`, release
`434e31fd8123e5bd`, session `session-00066073af2a582b-00`. All36 slots and both
VM paths pass; strict collection/diagnosis verifies both operations, alias/parent
retirement, raw snapshots and offline preservation. The705-file evidence bank
and separate handoff records are outside build. `DIAGNOSED` records this narrow
operation result; it does not promote all255 remaining GPU obligations or native
tear-free acceptance. See the [unit evidence](machines/macbookair7-2.md#native-full-gt-memory-child-completes).

### Full-GT display translation preservation

`--profile intel-gt-range-v56` selects the isolated `--intel-gt-range` image
and `gt-bootstrap-range-v1` recipe. It freezes the local Air7,2 firmware
fixtures, host checks and exact wire9 image. The release ID also binds the
source commit, including when two commits build byte-identical binaries.
QEMU must refuse hardware
admission before reset, PTE mapping or translation reads, save intact
checkpoints and return with its disposable workspace. The missing-workspace
rehearsal must halt. Both VM receipts bind the same staged manifest.
Historical log collection recognizes bootstrap wires 1–11 paired with wake37
for the existing 96 KiB limit. Unknown wire versions and ordinary sessions
retain the 64 KiB limit.

The unattended native trial uses the original full-GT memory sequence.
It compares all 128 trailing translations of the inherited display plus
one candidate entry across reset and memory maintenance. The matching
session is saved to USB with a 96 KiB log limit. Clean completion returns
to Ubuntu. If the display is unchanged for 60 seconds while exclusion is
held, photograph it and restart manually. The collector closes complete
preservation evidence as `DIAGNOSED`. [Preservation contract](intel-bdw-gt-bootstrap.md#held-display-translation-preservation-wire9).

`--profile intel-gt-primary-v57` adds two geometry and display-reader samples
after the wire9 comparison while the original requester is inhibited. Its
source-bound stage uses `--intel-gt-primary` and a distinct wire10 receipt.
The workflow keeps both exact-image VM paths and accepts a clean native return
only as a diagnostic. [Sample and footprint contract](intel-bdw-primary-range.md#post-maintenance-wire10-capture).

`--profile intel-gt-aux-v58` uses `--intel-gt-aux` for the source-bound
wire11 image. It checks VGA, FBC, PSR and the eDP route around the held primary
samples. The two VM checks and all older host checks still run. A complete
report records the observed activity and alias retirement as a diagnostic.
[Auxiliary display capture](intel-bdw-primary-range.md#auxiliary-display-readers-under-the-same-hold).
