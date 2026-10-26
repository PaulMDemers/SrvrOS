# Apple Silicon macOS build host

Updated: 2026-10-25. This is a development host running an emulated x86_64
guest, not a native SrvrOS hardware target. Observed host model: MacBookPro18,2.

## Active scope

Branch `device/apple-silicon/macos-build-docs`, based on freshly fetched
`origin/main` at `eda8bfbc`. Move the working workspace macOS helpers into
`tools/macos/`, make installation and VM state paths portable, and audit the
current documentation against implementation and named acceptance records.
Touched areas: host tool bootstrap/checks, macOS VM tooling, current guides and
documentation audit notes. No kernel, driver, firmware or application behavior
changes are planned. The only open PR at scope inspection was Pro11,2 PR19,
which retains USB Wi-Fi ownership and does not overlap this host-tool work.

## Initial evidence

At source `46f6212f`, the complete ordinary image built with native ARM64 Zig
0.15.2, GNU Make 4.4.1, GNU tar and xorriso. QEMU 11.1.2 TCG ran one x86_64 CPU,
1 GiB RAM, Q35, AHCI, USB input, e1000 and HDA/CoreAudio at 1440×900. The desktop
ready marker appeared 6.61 seconds after launch. Rendering and injected typing
were observed; the user subsequently reported opening, running and testing it
successfully. That report is qualitative and does not establish a performance,
filesystem durability or audio-endurance bound. The automated Save As check was
not completed; it is not a save/readback pass.

This host has no native SrvrOS installation, physical USB acceptance or recovery
payload. No reboot is needed for this scope. Prior guest disks and evidence must
be retained during migration and excluded from Git; required VM state must
survive `make distclean`. Local logs identify their original source and paths.

## Completed host validation

The repo now owns the build/VM helpers and the [reproduction guide](../macos-development.md).
Previous workspace entry points forward to them. Existing VM disks moved to
`.local/macos-vm/` with before/after SHA-256 equality; historical manifests were
preserved. This state survives `distclean` and is ignored by Git.

Validation on baseline `eda8bfbc` with local host/docs changes:

- Fresh checksum-pinned ARM64 Zig bootstrap into an isolated test directory: pass.
- `sh tools/macos/build.sh check-host`: pass.
- `sh tools/macos/build.sh -j2 all`: pass; matched manifest written.
- `python3 tools/test_macos_host.py -v`: five tests pass, including provenance,
  disk preservation/failure, PID refusal and simulated-build refusal.
- `sh tools/macos/build.sh check-docs` and `git diff --check`: pass.
- Separate headless Q35/TCG VM with a long, spaced state path: console at 4.88 s,
  desktop/first frame at 5.13 s, QMP running, 1440×900 Text/desktop screenshot
  visually inspected. That disposable VM was stopped via its own QMP socket.

Local bootstrap/build logs, VM manifest and screenshot are under
`build/review/macos-host/` and `build/macos-build.log`. The session manifest
contains artifact hashes; runtime source and later documentation changes are
not conflated with another machine's native acceptance. The prior interactive
VM was already stopped before migration. No VM is intentionally left running
by this validation, and no native boot/install was performed.

## Audit checkpoint and next scope

The [first-pass audit](../code-docs-audit-2026-10-25.md) records current-guide
corrections and source references. Unresolved work includes condition-variable
clock attributes, Air6,1 native lifecycle latency, Node source-history recovery,
macOS test-suite portability and deeper subsystem review. The Linux host/core,
full desktop/VM suites, physical save durability, UEFI boot and audio endurance
were not accepted by this Mac build/boot check. Keep those gates separate.

## Continued audit scope — 2026-10-25 evening

PR175 publishes the first pass. The continued audit consumes `origin/main`
`aa7b7392` (PR173 adds three failed/incomplete Air6,1 timing attempts; no runtime
change). PR174 owns process startup/worker retirement under disk contention;
PR172 owns shared desktop application dispatch; PR19 retains Wi-Fi diagnostics.
This pass audits libc synchronization/stack contracts, creates bounded host
reproducers, and corrects current documentation. It does not implement changes
in those active runtime scopes or claim new native acceptance.

The continued pass confirms three defect categories in the
[synchronization audit](../synchronization-audit-2026-10-25.md): supplied-stack
bounds (two cases), selected condition clocks and semaphore count overflow.
`tools/audit_sync_contracts.py` compiles real library sources with prefixed
symbols and modeled OS services; no threads or VM are started. Default mode
exits 1 for the known defects; `--expect-known-defects` exits 0 for all four
observations. Separate JSON receipts under `build/review/sync-audit/default`
and `build/review/sync-audit/known` retain source/header hashes and output.
The exact-size stack and ordinary semaphore operations are positive controls.
This is source-level host evidence, not a full thread/concurrency suite. Current
guides exclude the affected contracts and distinguish libc from libuv semaphore
coverage. Documentation links and whitespace validation pass. Runtime corrections
remain follow-up work; no kernel/library implementation was changed.

## Synchronization corrections — active scope

The user authorized fixes for the three confirmed findings and continued audit.
Base is `a90ac149` with `origin/main` still at `aa7b7392`; inspected PR172/174/19
retain desktop dispatch, process admission and Wi-Fi ownership respectively.
This block owns libc supplied-stack bounds, condition clock retention/deadlines,
semaphore count bounds, focused host and guest regressions, and current-guide/PR
updates. Rebuild matched userspace after the condition-variable layout change.
No native hardware trial or expansion of those independent PR scopes is planned.

The three fixes are implemented. The positive host suite passes; all three
isolated reintroduced-defect controls fail as intended. Matched `all` rebuild,
targeted live guest synchronization cases, the complete thread suite and complete
libuv consumer suite pass. Logs and hashes are retained under
`build/review/sync-fixes/`; no physical boot occurred. `pthread_cond_t` is now
8 bytes and all consumers were rebuilt. Supplied stacks preserve exact bounds;
condition clocks/deadlines are honored; semaphore counts use an atomic checked
maximum. Process-sharing/timed semaphore support is not added.

Continued guest audit confirms a separate `pthread_getattr_np(self)` metadata
failure: a 64 KiB supplied stack is reported as a guessed 2 MiB region. The
explicit `stack-inspection-audit` mode returns a failure receipt and is excluded
from the passing `all` suite. See the synchronization report for exact output,
next correction scope and remaining publication/lifetime questions. No claim of
fixing that fourth finding accompanies the three completed corrections.

## Self-stack inspection correction — active scope

The user authorized the fourth confirmed fix, regressions, documentation/PR
updates and continued audit. This block consumes main `1eb60b8f` (merged
PR174/176 scheduler retirement work) into this branch. PR172/19 retain shared
desktop dispatch and Wi-Fi scope. Work is bounded to libc thread metadata,
focused host/guest inspection tests and current documentation. Worker metadata
must exist before the user callback, including when the child runs before
`pthread_create` returns. Initial-process stack reporting retains the kernel's
two fixed layouts; unknown raw-thread stacks must not receive guessed bounds.
Next checkpoint is a matched build, sequential guest thread/libuv checks and a
bounded audit of adjacent thread lifecycle contracts. No physical boot is armed.

The self-stack correction is implemented with private TLS start metadata and
atomic detach-state reads/updates. Host early-child regressions pass; removing
metadata installation in an isolated negative control produces exit 1. Matched
`all` build, targeted inspection, final complete thread suite and full libuv
consumer suite pass on this ARM64 host's x86_64 QEMU. The merged timing parser's
three Python tests also pass. Documentation validation checks 634 local links
with zero errors; whitespace checks pass. The complete Linux/core suite was not
run, and no native acceptance is added. Evidence, exact build manifest and source/
log hashes are under `build/review/stack-inspection/`. Disposable guests exited.

Continued audit's deterministic host model finds early self-detach returning
`EINVAL` before the parent publishes its record. That separate explicit audit
exits 1 and remains outside the passing regression gate; no live race frequency
is inferred. Next scope is record publication/lifecycle ownership, including
reaping. Current status also reconciles PR176's failed Air6,1 native trial; worker
pipe admission is still only a candidate explanation. This block is complete;
no VM or physical boot is left running/armed.
