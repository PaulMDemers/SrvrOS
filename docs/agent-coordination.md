# Working across device agents

Each Ubuntu Mac has a local agent, checkout, recovery installation and exFAT
workspace. `main` is the shared integration baseline. This guide describes how
to contribute to one OS without turning device branches into separate OS forks.
Read it with [AGENTS.md](../AGENTS.md), the [Ubuntu setup guide](ubuntu-device-agent-setup.md)
and the [machine records](machines/README.md).

## Start a work block

1. Read the actual model/unit record and its local checkpoint. Check Git status,
   branch, source commit, installed boot state and any uncollected run. Preserve
   existing work; collect an outstanding physical run before staging another.
2. Fetch `origin` and inspect `origin/main` and relevant open pull requests.
   Fetching does not update the checkout. Record the base actually used; never
   assume a remote fix is present because a chat or document mentions it.
3. Put the bounded objective, subsystem/files, dependencies and next checkpoint
   in your own machine record before shared implementation. Publish that record
   with the work branch; use a draft PR when a shared-code change is underway.
   Check other records/PRs for overlapping work first.
4. Build and test one candidate at a time in a checkout. Hardware/VM scripts use
   shared paths and `latest` pointers. Independent hardware can work concurrently;
   simultaneous builds, staging or VM harnesses in one checkout cannot.

Use `device/<model>/<topic>` branches, with a unit suffix when needed. Each
agent edits its own unit record. A record is a coordination notice, not a lock:
if two unpublished changes overlap, Git cannot prevent duplicate work. Resolve
the overlap before integrating. Do not overwrite another agent's ownership or
claim another physical machine's validation.

## Ownership and overlapping changes

The local device agent owns inventory, profile evidence, recovery, trials and
acceptance on that unit. The integration role owns review of shared contracts
and the order in which changes land; this can be the reference Air agent or a
user-designated reviewer. That role does not grant control of another machine.

For overlapping work, designate one implementation branch for the shared fix.
Other agents can produce sanitized descriptors, a reproducer, host fixtures or
results against that candidate. Keep unrelated device work moving. If no owner
has been agreed, prepare the reproduction and a small proposed change, mark the
conflict in its PR/record and defer conflicting integration. Do not silently
merge two competing implementations or force-push another agent's branch.

At this checkpoint the Air7,2 agent is taking the read-only power baseline and
native battery/AC runtime groundwork. New agents should start with their own
inventory and basic console/display/input/workspace acceptance. Shared ACPI,
EC, scheduler, input, storage and widget changes should consult that active work
before duplicating it. This is an initial allocation, not permanent ownership.
Refresh it in the machine records as work changes.

Git branches, machine records and PRs are the durable coordination surface.
Reading another task is not authorization to message it or schedule agents.
Use cross-task messaging only with direct user authorization; otherwise publish
reviewable repository work and surface an unresolved ownership conflict locally.
No background agent scheduler or automatic reboot permission is implied here.

## Share mechanisms; isolate hardware evidence

- Separate protocol/controller mechanics from model matching, report layouts,
  codec routing and quirks. Put shared controls in the widget library and shared
  window/input policy in the compositor, rather than copying them into apps.
- Add a device profile only from its captured IDs, descriptors and resources.
  A common enclosure or CPU family is not evidence of matching firmware, GPIO,
  audio routes, storage or EC registers. Unsupported hardware remains unavailable.
- Keep generic fixes and device enablement in separate commits where practical.
  A generic fix needs regression coverage for existing behavior; a new profile
  needs positive matching and negative/refusal cases. Do not make every sibling
  pass by widening a gate around one machine's constants.
- Keep blocking I/O out of input/render loops. Hardware operations need bounded
  waits, explicit worker/IRQ/queue ownership, cancellation and safe teardown.
  A timeout must not free memory or reuse DMA state still owned by hardware.
- Evolve shared structs, syscalls and wire formats deliberately: update all
  callers and tests together, document compatibility, and boot a matched kernel
  and userspace image. Do not copy a single binary from another release into an
  accepted payload and keep calling it the same release.
- Keep battery/AC readout, CPU policy, fan control, suspend/resume and GPU switching
  separate milestones. The Q35 full-uACPI fixtures do not authorize native EC/SCI
  activation. Ordinary desktop startup continues to omit optional AML probing.

## Integrate and consume changes

Publish coherent commits and a PR against `main`. Include the problem and
reproduction, affected profiles/contracts, exact tested source, host/VM results,
physical release/session if applicable, remaining unknowns and dependencies on
other PRs. Do not attach raw firmware, machine serials, credentials or private
USB files. Summarize sanitized evidence and retain the full local artifacts.

The integration owner reviews the diff and relevant CI before merging. Resolve
conflicts with the affected implementation owner; rerun tests when the merge
changes behavior. Keep `main` bootable. A passing CI job does not promote all
machines' capability ledgers to accepted. If a regression lands, revert or fix
the shared change with its evidence rather than hide it behind an undocumented
per-machine patch.

After a dependency lands, fetch and merge the actual `origin/main` into a clean
device branch, preserving local commits. Avoid rewriting published history or
reapplying a change already merged upstream. Record the new base, rebuild matched
artifacts and rerun affected host/VM checks before staging. Schedule the relevant
physical checks on that machine under its existing authorization. Acceptance of
an older release stays historical evidence, not a blanket pass for the new tree.

## October 20 battery baseline handoff

[PR #6](https://github.com/PaulMDemers/SrvrOS/pull/6) supplies the shared cached
battery service, explicit worker I/O eligibility, retired scheduler callback
handling, lifecycle/SCI diagnostics and automatic capture/collection mechanisms.
Consume this baseline before building another battery service. Its native backend
is gated to captured Air7,2 firmware/resources; shared mechanisms do not authorize
copying those gates or enabling a sibling's EC/SCI ownership. Ordinary desktop
battery startup remains disabled.

Air hardware accepts bounded cached-service lifecycle and USB **read** overlap.
The first v36 scratch write trial failed allocation preflight before native
runtime entry; ordinary offline fsck reported clean. Later independent traversal
confirmed eight orphan allocations; rehearsed recovery preserved all existing and
orphan data. Use both checks in the Ubuntu guide before future trials. The subsequent retry
accepts bounded scratch transport write/flush overlap. Ordinary filesystem
metadata updates, sustained desktop/input coexistence, gauge calibration and
the unclaimed SCI cause remain open. See the
[service contract](battery-service.md) and [Air record](machines/macbookair7-2.md).

Pro11,2 [PR #5](https://github.com/PaulMDemers/SrvrOS/pull/5) was inspected at
`7c9372b`; its battery work is deferred at the user's request. Preserve its v35
CPU-load capture and unit evidence when incorporating this baseline. Air uses
v36 for scratch writes. Reconcile `kernel/src/main.c`, `tools/hardware_boot.py`
and `tools/hardware_boot_smoke.py` by behavior, preserving both profiles and
automatic-return policy; do not resolve overlapping files wholesale with one
branch's version. Fetch the actual merged `origin/main`, rebuild matched artifacts,
and rerun affected staging/collector and VM checks before a Pro hardware trial.
Each machine retains its own UUID, recovery payload and acceptance ledger.
On the later inspection, Pro PR #5 at `3b3b66a` already uses `d72d2af` as its
base, consuming the merged PR #6 baseline; its local validation remains its own.

Air PR #8 adds the isolated v38 filesystem-image lease and sustained native
workload, based on merged PR #7 (`76508bf`). Air7,2 accepted 25 cycles over a
minute in release `c473b63c44d32cd0`; the outer workspace stays unmounted during
runtime. This does not enable ordinary desktop battery startup or concurrent
writes to a user's filesystem. Preserve Pro's separate v37 desktop network
profile when integrating the shared staging/main changes. Next Air scope is
bounding filesystem work for desktop/input coexistence and investigating the
remaining unclaimed SCI deliveries; no sibling EC gate is authorized.

## CI cadence

The user disabled repository Actions on October 20 to avoid further hosted
minute consumption. Validate PRs locally and record exact source, commands and
results before merging; do not re-enable or dispatch Actions. The workflow below
remains a reference for local coverage if the repository setting stays disabled.


The full build/VM baseline runs for pull requests targeting `main`, pushes to
`main`, and manual dispatch. Ordinary device-branch pushes do not also run a
second copy. Documentation-only diffs (`docs/**` and Markdown files) skip the
full baseline; run `make check-docs` locally. Pull-request path filtering considers
the full PR diff: a docs-only follow-up to an open code PR can still start a new
baseline. Batch acceptance notes before validation, or publish them as a separate
docs-only change after the validated code lands. A newer revision cancels obsolete
runs for the same PR or branch. Keep a PR draft while its scope is being formed,
but note that updating an open draft still triggers relevant PR validation.
Batch coherent changes rather than pushing every intermediate edit. CI evidence
must name the tested code revision; a docs-only follow-up does not claim a new
full CI run. If required status checks are added later, reconsider path filtering
so a skipped workflow cannot leave a required check waiting indefinitely.

## Evidence and checkpoints

Use distinct states: planned, inventory captured, implemented, host-tested,
VM-tested, physically accepted for named checks, failed, and not tested. Give
source/release identities and limits. A general positive user report accepts
what was actually exercised; it does not silently close unmentioned gestures,
audio routes, power readings or failure paths.

Before every physical boot, save an ignored local run record containing:

- model/unit, branch/source and dirty state; exact stage manifest and hashes;
- workspace UUID/mount, current/previous releases and independent recovery copy;
- exact-stage VM receipts, known failures, intended checks and acceptance criteria;
- prior session/screenshot names, evidence destination and reboot authorization;
- reboot state: prepared, installed, armed, returned, collected or failed.

Before ending a work block, add its current objective, commits/PR, test results,
known issues, running processes, the next concrete command and what must not be
repeated. A checkpoint belongs in ignored local storage for volatile state, plus
a concise portable summary in the unit record. Accepted recovery and required
evidence must have copies outside `build` before `make distclean`.

On return, verify the actual host/boot state and collect the matching new session
before another install. Copy screenshots separately and verify hashes. Missing
completion markers and halts are incomplete/failed evidence. Keep failed attempts;
do not overwrite them with a passing retry. Serial writers can interleave, so
inspect process/result evidence before diagnosing a timeout as a kernel hang.
Do not weaken a completion predicate merely to get a green run.

## First handoff for a new machine

Start the local agent with the [setup guide's handoff prompt](ubuntu-device-agent-setup.md#9-agent-handoff-and-integration-contract)
and a copy of the [unit record template](machines/TEMPLATE.md). It should first
publish its actual identity/inventory, source/base and bounded scope. Each host
uses its own exFAT UUID and sudo/reboot authorization. Do not start by replaying
Air7,2 commands with its UUID, accepted release or firmware gates substituted
into a new machine's record.
