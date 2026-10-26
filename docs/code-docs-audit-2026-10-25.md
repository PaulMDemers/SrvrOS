# Code and documentation audit — 2026-10-25

## Scope and baseline

First pass against fetched `origin/main` at
`eda8bfbcbd3eb6bb9862564352709ab05833f148`, on branch
`device/apple-silicon/macos-build-docs`. This review checks build/VM reproducibility
and current-guide claims against source contracts and the latest machine records.
It is not a complete kernel security, concurrency or POSIX conformance audit.
Dated experiment journals remain history; their old acceptance does not silently
become current acceptance. No kernel, driver or application behavior was changed.

The only open PR at scope inspection was Pro11,2 PR19 (bounded USB Wi-Fi work),
which does not overlap this host-tool/documentation scope. No sibling machine
was booted, installed or marked accepted by this work.

## Corrected claims

| Area | Evidence inspected | Correction |
| --- | --- | --- |
| macOS build | [bootstrap](../tools/bootstrap_zig.py), Makefile, host tool checks and actual build | Native ARM64 Zig 0.15.2, GNU Make/GNU tar and x86_64 TCG are now a documented repo workflow; no ARM64 SrvrOS support is implied. |
| Desktop transport/runtime | [protocol](../shared/include/srvros/desktop_protocol.h), GUI wait ABI and desktop runtime | Current protocol v15, parented dialogs, combined GUI/FD wait, app Quit and open-document routing are implemented; current guides no longer describe these as wholly future work. |
| Capacity | Desktop capacity harnesses, registry/resource budgets and [API plan](desktop-api-evolution.md) | A sixteen-window forced-pressure fixture is not the normal window ceiling. The surface pixel budget is 256 MiB, not 384 MiB; dynamic growth retains other resource limits. |
| Native acceptance | [Air7,2](machines/macbookair7-2.md), [Air6,1](machines/macbookair6-1.md), [Pro11,2](machines/macbookpro11-2.md) | Current summaries include later bounded native 24-window and modal/unsaved trials, partial sibling acceptance and unresolved latency. Large VM workloads do not establish physical limits. |
| POSIX | [file adapter](../userspace/lib/src/posix_file.c), pthread adapter and [supported profile](posix-supported-profile.md) | Positioned I/O uses native syscalls; selected correctness work is merged. GUI wait remains separate from POSIX descriptors, and unsupported semantics remain explicit. |
| Ports | Upstream pins and individual port manifests | Metadata/mapped-memory work is no longer listed as wholly absent; the local libuv adapter is distinct from an upstream backend. Node's unrecovered port history is visible in the port overview. |
| ACPI and CI | Native runtime/machine contracts and repository CI policy | Port/index summaries distinguish isolated probes, Q35 fixtures and separately gated native runtime. Checked-in Actions workflows are not reported as active CI. |
| UTC command | [date.c](../userspace/date/date.c) and libc realtime clock | The command inventory now describes hardware-seeded UTC and supported formatting, replacing its obsolete uptime-only claim. |
| Shutdown/use | Current desktop lifecycle behavior | Normal Return to console permits unsaved-work decisions; emergency exit can discard work. Returning to the console does not terminate QEMU. |

The [release checkpoint](release-notes.md), [status](status.md), [roadmap](roadmap.md),
[development](development.md), [testing](testing.md), architecture, desktop API,
widget integration and port overviews now point to these current boundaries.

## Host-helper review and fixes

The implementation now lives in `tools/macos/`; old workspace entry points only
forward to the repository. The shared Zig bootstrap selects the checksum-pinned
ARM64 macOS release while preserving x86_64 Linux/Windows selection. Homebrew
firmware discovery is added to host checks.

The launcher verifies image hashes against the successful build manifest before
labelling a session with its source. It refuses simulated/touch-only build
manifest generation. Dirty source is identified but not archived. First-use disk
copies are atomic; existing guest writes are preserved, and changed image hashes
select another disk. A lock serializes starts; a live or uninspectable PID causes
refusal. Short private socket paths avoid macOS AF_UNIX path limits even when the
checkout/state path is long. Raw logs and disks are ignored by Git.

Existing workspace VM disks were moved under `.local/macos-vm/`, with matching
before/after SHA-256 hashes recorded locally. This location survives `distclean`.
Historical manifests retain their original paths/source identity; they are not
rewritten as new acceptance. Changed builds do not migrate guest files between
disks automatically.

## Validation

- A fresh download through `tools/bootstrap_zig.py` into an isolated test
  directory passed checksum, extraction and compiler-version validation.
- Host regression tests cover compiler selection, stale-image refusal, disk
  reuse, failed-copy cleanup, active/invalid PID refusal and simulated-build
  refusal. These exercise failure behavior, not just the successful launch.
- `sh tools/macos/build.sh check-host` and `sh tools/macos/build.sh -j2 all`
  passed on MacBookPro18,2, with QEMU 11.1.2, GNU Make 4.4.1 and Zig 0.15.2.
- A separate headless VM used a long state path containing spaces. Console
  readiness took 4.88 s; desktop readiness and the first app frame took 5.13 s.
  QMP reported running and its 1440×900 screenshot visibly showed Text and the
  desktop. The test instance was stopped through its own QMP socket afterwards.
- The earlier visible Cocoa/CoreAudio-configured VM at `46f6212f` reached the
  desktop in 6.61 s and received the user's qualitative acceptance. These are
  single-run observations, not performance or audio-endurance benchmarks.

The new test used runtime baseline `eda8bfbc` with uncommitted host/docs changes;
its manifest records exact artifact hashes. Evidence remains locally under
`build/review/macos-host/`. A successful boot does not establish save durability,
network conformance, a UEFI boot, native hardware acceptance or full Linux-suite
portability. The [host record](machines/apple-silicon-build-host.md) carries the
final verification checkpoint.

## Remaining audit work

1. **Condition-variable clock semantics:**
   `pthread_condattr_setclock` accepts `CLOCK_MONOTONIC`, but `pthread_cond_init`
   discards attributes and timed waits use realtime. Confirmed in
   [posix_pthread.c](../userspace/lib/src/posix_pthread.c); already excluded by the
   supported profile. A follow-up should preserve/use the selected clock or
   explicitly reject unsupported selection, with clock-specific deadline tests.
2. **Native input-under-I/O latency:** Air6,1's retained failure and subsequent
   lifecycle timing instrumentation need a matched native retry and diagnosis.
   This Mac's TCG boot cannot close that issue. See its machine record.
3. **Node reproducibility:** recover the missing port Git history or establish
   a fresh bounded port before claiming the historical runtime milestones.
   See [recovery notes](development.md#node-port-recovery).
4. **macOS test portability:** qualify individual host/VM harnesses before
   advertising `test-core`, the desktop suite or hardware/GRUB staging as Mac
   workflows. Current Linux assembly/linker/filesystem assumptions remain.
5. **Deeper subsystem audit:** review filesystem failure/ownership paths,
   scheduler/thread lifetime, networking and experimental eventfd/epoll adapters
   with directed reproductions. Those contracts were not exhaustively audited in
   this pass. Expand current-guide checks as each subsystem is reviewed.

## Continued synchronization pass

The [synchronization/stack audit](synchronization-audit-2026-10-25.md) adds source-
level executable evidence for a caller-supplied stack out-of-bounds write, the
condition-clock gap and silent semaphore overflow. The supported profile and
port guides now exclude those contracts accurately. It also reconciles current
fork/spawn/readiness/time descriptions and PR173's newer Air6,1 failure records.
These findings are not fixed by documentation or by reproducing the failures.

## Corrections after the continued pass

The user authorized fixes for all three confirmed defects. PR175 now contains
checked supplied-stack handling, retained condition clocks and checked semaphore
counts, plus positive host/guest regressions. The original failure receipts above
are historical. See the synchronization audit's current disposition and matched-
rebuild requirement rather than treating those three items as still unfixed.

The subsequent self-stack inspection finding is also corrected: workers install
metadata before entering user code, and live tests cover supplied/owned storage,
parent agreement and detach-state updates. Initial-process inspection retains
explicit kernel layouts. Continued host audit now isolates early self-detach
failing before parent record publication. See the synchronization report for
the separate failing reproduction and the limits of this modeled evidence.

The next pass corrects early self-detach and serializes record publication and
cleanup claims, consuming main `aa931093` (shared Text/Preview runtime). Positive
host cases cover child exit before create returns, parent resumption, cleanup
retry and slot reuse; guest cases add concurrent creators/reapers. Continued
source review confirms fork refuses even exited-but-unreaped kernel workers,
so the supported profile now states that limit. A separate host reproduction
shows thread-name setters silently discard their input; these nonportable APIs
are now explicitly documented as stubs. Neither documentation change claims
new kernel fork behavior or working thread-name storage.

The thread-name correction replaces those stubs with protected, bounded libc
storage for the initial thread and worker records. Default host and guest suites
now cover round trips, range/refusal behavior, naming across publication/exit,
record reuse and concurrent readers/writers. Main `fa5a6cc1` supplies Air7,2's
shared-runtime hardware acceptance documentation; current status preserves its
reported scope. Continued guest audit confirms that initial-thread `pthread_exit`
terminates live workers. The explicit failing mode is excluded from `all` and
that contract is now excluded from the supported profile pending a kernel/process
lifetime correction. See the synchronization report for exact receipts.


The initial-thread exit correction now retains process resources and the kernel
teardown continuation while workers run, then performs libc `exit(0)` after the
last worker. Positive host and guest regressions replace the former failing
exit audit, including TLS destructors, `atexit`, descendants, descriptors and
explicit process termination. Kernel and libc must be rebuilt together for the
changed `SYS_THREAD_EXIT` handoff. Continued audit isolates another pre-existing
gap: joining the initial thread returns `EINVAL` and loses its pthread return
value. The separate `main-join-audit` guest mode fails as expected and remains
excluded from `all`; initial-thread join/detach is explicitly outside the current
profile. See the synchronization report for reproduction and evidence.
