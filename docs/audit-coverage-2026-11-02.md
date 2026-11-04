# Audit coverage ledger — 2026-11-02

This ledger describes the audit evidence recorded in the four companion reports: [kernel](audit-kernel-2026-11-02.md), [userland and ports](audit-userland-2026-11-02.md), [tooling](audit-tooling-2026-11-02.md), and [documentation and integration](audit-integration-2026-11-02.md). It is a coverage map, not an assertion that the repository received a complete manual audit or that its unreviewed code is safe.

## Counting and depth

The inventory is the tracked tree at `dc0a3fef`, counted with `git ls-tree -r --full-tree dc0a3fef`: **1,618 entries**, including **1,611 regular files and seven submodule gitlinks**. The checked-out `25cd19e5` is a later audit checkpoint; reports describe a shared working tree that also had concurrent edits. The inventory does not count newly created, untracked tests and reports or expand submodule contents. Counts are entry counts, not measures of risk or proof of line-by-line review. For a reproducible file-level inventory of any cohort below, use `git ls-tree -r --name-only dc0a3fef -- <cohort>`; the generated inventory includes every tracked path in that cohort.

- **Inventory:** the tracked path was counted or its type identified. This alone says nothing about contents.
- **Screening:** the report records a mechanical read, lexical search, parser, compiler, static analyzer, link check, or format check. The exact screen and exclusions differ by domain; matches and clean results are leads, not correctness verdicts.
- **Manual sample:** the report identifies a particular path, call chain, or risk area examined by a person/agent. A sampled domain or file does not imply every line, caller, configuration or concurrency interleaving was reviewed.
- **Focused reproduction:** a report records a specific host fixture, fault injection, sanitizer check, or before/after reproduction. It covers that scenario and the version/configuration used. Private stubs and host builds do not establish guest ABI integration.
- **Integrated runtime:** a built target, full `test-core`, booted guest, physical hardware, or current CI run exercises combined changes. Companion reports explicitly distinguish existing historical evidence from current integration, and their private host runs do not establish this level.

Manual review was selective within the screened cohorts. Where a report names a subsystem rather than every file and line, this ledger does not invent an exhaustive file-by-file review status; all other files in that cohort remain screening-only for purposes of this ledger. The first-party inventory and seven gitlinks were screened/integrated as described in the companion reports; the gitlinks do not have a comprehensive source review. Small dependency helpers were traced where needed to classify callers.

## Exhaustive tracked directory cohorts

These top-level rows are disjoint and sum to 1,618. Subtables partition the larger directories; `(direct)` means files directly in that directory, not files in descendants.

| Cohort | Tracked entries | Recorded coverage and remaining depth |
|---|---:|---|
| Repository root files | 9 | Documentation/integration programmatic read; sampled build, security, notices and setup relationships. Whole Makefile and policy behavior are not exhaustively verified. |
| `.github/` | 4 | Programmatic read and sampled workflow/templates; YAML and shell-block checks recorded. Current hosted run remains pending. |
| `boot/` | 1 | Boot config inventoried, screened and statically traced to image inputs; matched guest boot and suite results are recorded below; physical boot remains unverified. |
| `docs/` | 135 | Programmatic read, bounded link/format checks and selected manual cross-checks; historical facts, screenshots and dynamic prototype behavior not exhaustively validated. |
| `initramfs/` | 3 | Programmatic read and selected archive/overlay wiring review; matched guest suite results are recorded below; broader image and hardware validation remain pending. |
| `kernel/` | 189 | All paths mechanically screened; 92 kernel C units syntax checked and analyzed, seven assembly units assembled privately, with manual risk samples and host fixtures. Hardware, concurrency and all drivers remain incomplete. |
| `shared/` | 31 | All shared headers inventoried and mechanically screened; included portions reached by kernel compiler checks. ABI combinations and every user/kernel contract were not exhaustively reviewed. |
| `ports/` | 191 | First-party and tracked vendored files mechanically screened; C units included in generic analyzer attempt. Samples cover shim, manifests and integration. Seven upstream gitlinks are external dependencies and not expanded as full source-review cohorts. |
| `tools/` | 561 | Every tracked path opened; all 295 Python files parsed/compiled, C/headers searched, three shell files syntax checked, six ASL files screened and compiled with iasl, fixtures classified. Selected scripts and tests manually traced and run; most tools were not executed. |
| `userspace/` | 494 | First-party files screened; C units included in generic analyzer attempt. Specific libc, desktop, utility and shim paths manually sampled; threadstress/PTY manual review was assigned elsewhere. Most utility, GUI, synchronization and ABI flows are not exhaustively reviewed. |
| **Total** | **1,618** | **1,611 regular files + seven gitlinks.** |

### Directory partitions

| Parent | Exhaustive child cohorts (entry counts) |
|---|---|
| `.github/` | direct 1; `ISSUE_TEMPLATE/` 2; `workflows/` 1 |
| `docs/` | direct 87; `assets/` 31; `design/` 4; `history/` 4; `machines/` 9 |
| `initramfs/` | direct 2; `etc/` 1 |
| `kernel/` | direct 1; `diagnostics/` 4; `include/` 80; `src/` 104 |
| `kernel/src/` | direct 36; `arch/` 31; `drivers/` 32; `fs/` 1; `memory/` 4 |
| `kernel/include/`, `shared/` | `kernel/include/srvros/` 80; `shared/include/srvros/` 31 |
| `ports/` | direct 1; `node/` 5; `qemu-a1466/` 5; `srvros/` 58; `upstream/` 122 |
| `ports/srvros/` | direct 5; `ed/` 1; `node/` 15; `uacpi/` 37 |
| `ports/upstream/` | seven direct gitlinks; `byacc/` 42; `sqlite/` 4; `stb/` 2; `uacpi/` 67. The latter 115 are tracked vendored files; generic analysis is not an upstream security audit. |
| `tools/` | direct 531; `fixtures/` 27; `macos/` 3 |
| `tools/fixtures/` | direct 16; `acpi/` 6; `images/` 5 |
| `userspace/` | direct 2; `deskd/` 22; `deskfind/` 2; `deskhelp/` 3; `deskstudy/` 3; `desktools/` 25; `desktop_runtime/` 5; `examples/` 5; `guifail/` 3; `imageworker/` 3; `lib/` 169; `libcprobe/` 2; `netabi/` 2; `playwav/` 9; `shotworker/` 3; `threadstress/` 51; `widgets/` 65; the remaining single-file directories are enumerated below. |
| `userspace/lib/` | direct 1; `assets/` 4; `include/` 101; `src/` 63 |
| `userspace/widgets/` | direct 1; `include/` 31; `src/` 33 |

The remaining 120 `userspace/` cohorts contain exactly one tracked file each: `arp`, `basename`, `calc`, `cat`, `chmod`, `clear`, `clockprobe`, `cmp`, `cp`, `cut`, `cxxprobe`, `cxxstlprobe`, `date`, `dd`, `deskclient`, `deskclock`, `deskfs`, `deskio`, `desknet`, `desksketch`, `deskterm`, `desktop`, `deskview`, `df`, `diff`, `dirname`, `displayd`, `du`, `echo`, `ed`, `env`, `execdemo`, `expr`, `false`, `fdprobe`, `fileman`, `find`, `fpdemo`, `grep`, `gui`, `gui2demo`, `gzip`, `head`, `hello`, `host`, `hostname`, `httpget`, `id`, `ifconfig`, `inidemo`, `init`, `install`, `jsondemo`, `kill`, `libuvdemo`, `lifetimeprobe`, `linedemo`, `lockprobe`, `ls`, `lua`, `make`, `mkdir`, `mktemp`, `more`, `mv`, `netcheck`, `netstat`, `nodeprobe`, `notes`, `paint`, `patch`, `ping`, `posixdemo`, `posixutils`, `printf`, `ps`, `ptyprobe`, `pwd`, `readlink`, `realpath`, `rm`, `route`, `sed`, `seq`, `sh`, `sleep`, `sort`, `spin`, `sqlitedemo`, `stat`, `surfacedemo`, `svscan`, `sysabi`, `tail`, `tap`, `tar`, `tcpprobe`, `tcpstress`, `tee`, `textedit`, `tlsprobe`, `touch`, `tr`, `true`, `ttydemo`, `udpdns`, `udpecho`, `ui`, `uname`, `uniq`, `uptime`, `uvdemo`, `wc`, `webd`, `which`, `whoami`, `write`, `xargs`, `yes`, and `zlibdemo`.

## Named manual and focused-test cohorts

These rows identify known reviewed paths or bounded flows; they are **subsets** of the directory cohorts, not additional entries. A file may appear in more than one row. The reports remain authoritative for exact line-level findings, versions and commands.

| Named paths / flows | Recorded depth | Evidence and gap |
|---|---|---|
| `kernel/src/memory/pmm.c`, `kernel/src/initramfs.c`, `kernel/src/memory/vmm.c`, ELF loading, fault entry and selected process/syscall paths | Manual samples plus targeted host reproductions | High-frame accounting, malformed archive, page permissions, canonical user ranges, ELF validation and fault handling were exercised with private fixtures. Matched guest scheduling and boot checks passed; physical hardware remains unverified. |
| `kernel/src/arch/x86_64/` entry and privilege setup, GUI queue, scheduler and selected exFAT paths | Manual samples plus analyzer triage | Invariants were traced, not exhaustively tested under SMP, real MMIO, adversarial storage, or every execution path. |
| `userspace/lib/src/posix_alloc.c`, `posix_stdio.c`, `posix_time.c`, path and string layers | Manual samples; targeted allocator/integer host reproductions | Individual bounds and analyzer traces were examined; no complete libc conformance, allocation/thread race, or guest ABI proof. |
| `ports/srvros/uv.c`, `userspace/uvdemo/`, `tools/test_uv_bounds.c`, `tools/test_uv_lifetime.c` | Manual lifetime/bounds review and focused actual-source private tests | Tested specific queue, async handoff, wake, copy and callback scenarios; not all async teardown, recursive loop use, simultaneous close/send or upstream libuv compatibility. New tests are outside baseline inventory. |
| `userspace/imageworker/`, `userspace/lib/src/bmp.c`, `userspace/lib/src/gui2.c`, desktop file picker, `userspace/deskfs/`, `deskview/`, `desksketch/` | Manual samples and bounded host sanitizer tests | Decoder limits, image mutation, BMP round trips and one maximum path case tested. Every image input, desktop flow, cancellation, worker failure and path race remains untested. |
| `userspace/cp/`, `diff/`, `expr/`, `patch/`, `tap/`, `init/`, `webd/`, and option parsing in `head/`, `tail/`, `make/`, `xargs/` | Manual warning/path samples and selected actual-source fixtures | Specific error, integer, allocation, syscall-output and argv invariants investigated. Other commands and edge cases remain screening-only. |
| `tools/*_records.py`, `serial_records.py`, `desktop_foundation_smoke.py`, `libc_symbol_audit.py` and selected boot, archive and evidence consumers | Manual trace plus Python subprocess/unit tests | Recorded negative cases and optimized-Python guard checks passed. Parser completeness, dynamic imports, other evidence formats and most fixtures remain pending. |
| Root README, workflow, security and third-party notices; `docs/` current guides; boot config and initramfs inputs | Manual cross-check plus link, format, static parse and shell checks | Matched VM suites passed; no current hosted CI, rendered documentation, remote-link, complete license or physical-device validation. |

## Results and pending integrated evidence

The kernel report records private host passes for PMM, initramfs, VMM, ELF loading and user-fault/user-copy fixtures. Its integrated follow-up records fixes for the reviewed process-exit and user-pointer mapping lifetime concerns and matched guest regression results. Broader concurrency coverage remains incomplete. The userland report records private sanitizer and actual-source tests for allocator/integer bounds, image/BMP inputs, `expr`, `diff`, `patch`, `tap`, GUI path bounds and the libuv bridge. The tooling report records 105 record-parser unit tests, a separate 58-test earlier selection, optimized-interpreter guard tests and symbol-audit subprocess tests. These are distinct bounded results; the subsequent combined results below have their own scope and execution point.

The generic userspace/ports analyzer attempted 367 C units: 351 completed, 15 failed under generic missing-header/configuration conditions and SQLite timed out. Thirty-four units emitted warnings; the userland report classifies their original diagnostics and later follow-ups, including remaining unconfirmed vendored and runtime leads. Its commands do not reproduce every actual target configuration. The kernel analyzer covered 92 C units and reported 18 warnings in 11 units, with source-level triage recorded; hardware mappings and interleavings were not dynamically proved. Tools' AST/compile checks do not execute their contents, and documentation format/link checks do not prove historical accuracy.

Pending integration includes rerunning the combined checks against any changes made after the reported execution points, current CI execution, physical hardware and device-specific validation, and review or reproduction of outstanding analyzer leads. The matched guest suites cover the exercised scheduling-controlled fault/copy/PTY/network cases, not every possible interleaving. Submodule source security, real boot media, the full Python/C fixture inventory, all desktop/app paths, every utility and ABI wrapper, and concurrency/teardown edges remain outside the depth of evidence recorded here. The companion reports were written during concurrent work; repeat checks against a frozen final tree before treating them as release evidence.


## Integrated results reported after the domain audits

The audit coordinator verified the following subsequent runs; they supplement the earlier private evidence and reflect the tree and artifacts at those run points:

- The final Linux `test-core` run completed successfully across 173 targets, including 385 Python cases in 53 groups. The additional 30-case offline namespace suite passed in the same command (415 cases/54 groups combined). The expanded user-copy fixture is included. All six ASL sources compiled.
- Nine matched guest suites passed: `sysabi`, `netabi`, full `threadstress`, libc, local libuv, upstream libuv, process lifetime, CLI and staged ports. The lifetime run covered four iterations, 512 applications and 64 waiter pairs. These results cover the executed configurations and test cases, not all guest workloads or physical hardware.
- In a baseline comparison, the old ISO with the current test disk panicked in `net.c` at `copy_bytes` line 282 during the UDP regression. The matched build passed all three mapping/reuse cases. This comparison supports those specific regressions; it is not evidence of exhaustive network or concurrency coverage.

The original generic userspace/ports analyzer results are stored in `/tmp/srvros_analyze.json` on the review host, with each record containing the path, exit status and diagnostic text. This temporary file is session-local and is not a tracked audit artifact; its configuration and exclusions are described above and in the userland report.

The subsequent tracked-uACPI review reproduced and fixed null-scope name
resolution, swallowed physical-table installation failures and GAS width/mapping
errors. The shell follow-up fixed signed arithmetic and logical short-circuit
parsing. Their four new core fixtures are included in the final 173-target pass
and have independent native ARM64 sanitizer coverage. The rebuilt ordinary AML
resource fixture and Q35 event-enabled runtime guest passed, as did sysabi and
netabi reruns after the vendor changes.

Configured follow-ups completed SQLite and 14 previously missing-header C
units, bringing compiler/analyzer completion to 366 of the 367 attempted
userspace/ports units across the recorded configurations. The remaining custom
QEMU model lacks its separate QEMU build headers; its source received a bounded
manual review. New warnings and their source-invariant triage are recorded in
the userland report. Compilation/analyzer completion is not a source-audit
completion percentage.

Final frozen implementation checkpoint: `e8491549`. The complete 173-target
Linux core pass, optional 30-case namespace pass, 491-command CLI and staged-port reruns
include the later shell changes. Tests run before that checkpoint retain the
execution scope recorded in their reports; the affected ACPI and syscall/network
guests were rerun after the vendor fixes. Documentation-only updates follow
the implementation checkpoint. Source screening of every baseline path is
complete; sampled review and these tests do not close the deeper manual,
external-dependency and native-device gaps listed above.


Subsequent review: the [2026-11-03 kernel isolation follow-up](audit-kernel-isolation-2026-11-03.md)
records a low-level VMM canonical-address fix, active-root retirement regression
and observed BIOS/QEMU CR0.WP state. It does not close the broader manual audit.

The same follow-up subsequently confirms protection/validation service gaps,
failed mmap rollback without turns, and unaligned process-copy checkpoint loss.
It records fixed owner-retaining batches and their baseline/host/guest evidence;
allocator and free-range search latency remain open.

The subsequent free-range/allocator pass fixes skipped virtual gaps and repeated
PMM contiguous-run refusal scans, backed by independent oracles. The kernel
follow-up records unchanged full heap validation cost and remaining initial-scan,
cache-churn, poisoning and driver-lifetime work.


PR317 command-ring continuation: exact TRB completion identity, nested refusal,
wrap/cycle and timeout/late-event saturation regression coverage now accompany
storage/ECM quarantine checks. Command abort/reset, physical USB removal and
hotplug/re-enumeration remain unaccepted; see the kernel-isolation audit report.


PR317 reset/removal continuation covers root disconnect/CSC quarantine, late HID
suppression, failed EP0 ring reuse, failed-enumeration record retention and a QMP
unplug/replug containment guest. Runtime command reset, downstream hub change
handling and successful re-enumeration/provider retirement remain open; the
kernel-isolation report records the specification prerequisites and limits.


PR317 endpoint/provider follow-up closes USB removal cache visibility, HID fatal
completion rearm and network callback admission before root-change observation.
Actual block/cache/claim code now participates in USB retirement tests. Bounded
continuous stale-event drain is modeled across ring cycles. Hardware stop/drain,
provider unregister/reference retirement, reset and re-enumeration remain open;
software quarantine retains all backing rather than authorizing reclamation.


PR317 quiescence/reference follow-up reviews existing reset-before-free and
pre-publication-only cleanup, with new retained VirtIO-net callback/nested
delivery and PCI-disable-failure regressions. No new runtime defect confirmed;
lifecycle header overclaims corrected. Source/model evidence does not implement
xHCI hardware stop/drain, generic provider reference release, hotplug or SMP
reclamation. See the kernel-isolation report for exact scope and prerequisites.


PR317 now adds verified terminal xHCI schedule halt/bounded event drain and
network-registry callback reference release. Stop rejects false halt proofs and
waits for software owners/provider callbacks. Context-release and terminal-stop
host/VM fixtures accompany it. All xHCI backing stays pinned: block/worker/
diagnostic reference release, per-slot teardown, restart and physical acceptance
remain open. See the current kernel-isolation checkpoint for exact limits.


PR317 terminal DMA retirement now adds USB initializer/request/diagnostic callback
drain, poll-worker acknowledgement, diagnostic/poll ownership and PCI bus-master
readback before bounded 16-frame xHCI reclamation. Offline metadata/claims remain;
global writable-media failure visibility is preserved. Exact-once poisoned frees,
combined USB/block/claim tests and QMP storage+HID retirement cover the boundary.
Per-slot reset/restart, live metadata recycling, physical/SMP acceptance and the
separately owned managed-workspace fault/salvage proof remain outside this result.


### November 3: xHCI per-slot and restart boundary checkpoint

PR317 adds explicit child-first slot retirement after callback/owner drain, one
Disable Slot per call, with all DMA/identity tombstones retained until terminal
reclaim. Confirmed stale-Halted startup acknowledgement is fixed; malformed duplicate
live-slot responses refuse before DCBAA overwrite. Continuing hub review fixed route
depth exhaustion aliasing the parent's path. Host fixtures cover these boundaries.
See the [kernel audit](audit-kernel-isolation-2026-11-03.md) for evidence and limits.
Runtime restart/re-enumeration, fresh generation ownership, hub port-change service,
endpoint reset/dequeue recovery and physical/SMP acceptance remain open. This is a
bounded driver checkpoint, not complete codebase coverage.


### November 3: hub changes and EP0 recovery checkpoint

PR317 continues with asynchronous USB2 hub status monitoring and subtree
quarantine, exact control-stage error matching and verified hub-reset acknowledgement.
Explicit one-shot root-attached EP0 protocol-STALL recovery uses Reset Endpoint
and a fresh Set TR Dequeue ring while retaining both allocations on uncertainty.
Published storage, timeouts, TT recovery and identity recycling remain closed.
See the [kernel audit](audit-kernel-isolation-2026-11-03.md) and
[testing guide](testing.md) for bounded host/VM coverage and limitations.
Generic endpoint clear-halt/dequeue cancellation, USB3 hubs, native TT behavior,
restart generations and physical/SMP validation remain open; no complete-codebase
coverage claim.


### November 3: nested translators and shared recovery checkpoint

PR317 corrects TT owner/port selection across nested full-speed hubs, refuses
malformed ancestry before hardware submission, and programs hub Configure Endpoint
metadata with high-speed TT think time while retaining single-TT mode. Root EP0
and HID recovery share fresh-ring reset/dequeue tracking; HID STALL additionally
requires device clear-halt. An owned Stop proof uses a toggle-preserving dequeue
rebase. Host fixtures cover HS boundaries and both context sizes; the extended
Q35 fixture covers nested full-speed hubs and new input after stop/rebase. See
the [kernel audit](audit-kernel-isolation-2026-11-03.md) for evidence and limits.
TT buffer cleanup/shared translator recovery, bulk/network/isochronous cancellation,
multi-TT selection, USB3 hubs, restart generations and physical/SMP qualification
remain open; no entire-codebase coverage claim.


### November 3: shared TT cleanup and class cancellation

PR317 adds leased, identity-checked single-TT control cleanup to explicit routed
recovery, with both directions and hardware USB address. Unsafe TT ownership
quarantines dependents; no whole-TT reset or storage/network replay. Confirmed
bulk cancellation readiness/wake and USB mouse source/button cancellation defects
are fixed. See the [kernel audit](audit-kernel-isolation-2026-11-03.md) and
[testing guide](testing.md) for modeled HS-TT versus actual routed-FS guest coverage.
Physical HS-TT/MTT, generic class recovery and restart generations remain open.


### November 3: class-report recovery and restart refusal

PR317 fixes short HID parsing/cancellation and dropped repeated relative movement.
Actual-driver fixtures cover all short lengths, held-input cancellation, rearming
and valid-report recovery. Class-policy tests keep published storage/network,
ambiguous bulk and trackpad recovery closed; restart tests require initialization
to preserve partial and reclaimed boot-lifetime identities without hardware work.
See the [kernel audit](audit-kernel-isolation-2026-11-03.md) for evidence and limits.
Normal/GCC14/sanitizer host checks and rebuilt-kernel nested recovery, storage
retirement, netabi and full threadstress guests pass within the documented fixture
limits. Physical class recovery, multi-TT/USB3, new-generation restart and SMP qualification
remain open. Broader process-isolation review continues with the independently
owned PR334 foreground-worker spawn defect; no whole-codebase completion claim.


### November 4: owned user-frame copy preflight

PR334's unwired owned frame-copy helper has independent source and ten host-profile
checks, plus two maximum-table strict/sanitizer profiles. Duplicate destination VA
and distant physical-alias refusal preserve the destination. Current hardware and
paging state is modeled by those fixtures. The 65,536-mapping wrapper can visit
262,144 records under IRQ exclusion; target build/codegen and actual worst-case
helper latency remain pending before any production caller. The optional isolated
guest test hook has source review only. See the [kernel audit](audit-kernel-isolation-2026-11-03.md)
for exact source versions, evidence and remaining signal-delivery boundaries.


SDK source carry f88f1bd0 preserves 1,230 existing port entries and adds 55 exact
browser overlay modes/blobs; 27 root files/modes match browser185db570 after the owner corrected the confirmed
missing math fixtures. Copied runtime claims now have explicit historical attribution. Independent hyperbolic host checks pass ARM strict,
ARM sanitizer and x86 strict, not SDK/guest acceptance. Public synthetic local
commit guard remains proposed. See the kernel checkpoint for precise scope.
