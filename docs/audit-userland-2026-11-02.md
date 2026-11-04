# Userspace and ports review — 2026-11-02

Baseline: `dc0a3fef` (working tree changes described below). This is a bounded review, **not a complete manual audit** of userspace or ports.

## Scope and method

The tracked inventory contains 685 entries under `userspace/` and `ports/`: 563 first-party regular files (561 text and two binary fonts), 115 tracked vendored upstream files, and seven gitlinks. A script read every first-party regular file and classified it as text or binary; it searched text files for unsafe string-copy, memory-operation, process-launch, and TODO/FIXME markers. These searches identified 47 files with string-copy markers, 80 with memory-operation markers and 40 with process-launch markers (counts are files, not defects). This is triage, not proof that each call site is safe. The seven gitlinks were treated as external dependencies, not reviewed line by line. The 115 tracked vendored files (42 byacc, four SQLite, two stb and 67 uACPI) were read by the same per-file marker scan, and their C translation units were included in the compiler analysis described below; this does not amount to a line-by-line manual review. The threadstress/PTY guest work is owned by the parallel kernel review; threadstress files were included in the inventory and text scan, but not manually audited here.

Manual risk review sampled the libc allocator, string and path layers; the desktop file picker and application path handling; the `cp` utility; the local libuv worker/loop bridge; port manifests and Node staging; and the submodule declarations. The picker validates input lengths before its copies. The Node manifest explicitly says prior runtime milestones have not been reproduced from the restored upstream gitlink. The local libuv adapter is explicitly a limited shim, not the upstream backend. These observations do not establish correctness of the unreviewed flows, utilities, ABI wrappers, scripts or port recipes. In particular, worker creation failure and asynchronous teardown paths in the libuv shim merit focused fault-injection testing. Image handling review covered the decoder allocation/file/dimension bounds, BMP arithmetic, and worker input framing. The stb wrapper enables only PNG, JPEG and BMP; its manifest documents a local JPEG overflow patch. SQLite uses a local VFS and upstream libuv is a separate pinned dependency. No shared build or guest VM was run.

## Reproduced and fixed

1. `malloc(SIZE_MAX)` wrapped alignment arithmetic and could return a small allocation, allowing a caller to write beyond the allocation. `posix_memalign` similarly wrapped its allocation size. The allocator now rejects requests before size arithmetic can overflow or exceed the signed `sbrk` increment; `posix_memalign` rejects overflowing totals without changing its output pointer. A private host harness using a bounded stub heap returned failure for the original source and passed with the patch. A guest regression was added to `libcprobe` but has not been run in a guest.
2. The integer conversion implementation skipped the only zero in `strtoll("0", ..., 0)`, performed signed-overflow arithmetic, and routed unsigned conversion through signed conversion. Consequently it mishandled `endptr`, out-of-range values, and the full unsigned range. The parser now accumulates with checked unsigned arithmetic, saturates on range errors, and consumes the full digit sequence. A private host harness returned failure for the original and passed with the patch, including zero, signed boundaries, unsigned maximum, negative unsigned values, overflow and incomplete hexadecimal prefixes. Guest regression cases were added to `libcprobe`; they have not been run in a guest.

Host checks compiled the isolated allocator source with renamed entry points and private stubs; the host's system integer type definitions were used in place of the target `inttypes.h` to avoid macOS header conflicts. They do not validate SrvrOS syscall, threading, or ABI integration. `git diff --check` passed for the changed userland files. No full build, exhaustive manual review, or upstream dependency security audit was performed.

3. `expr` parsed the magnitude in signed arithmetic, so even the valid value `-9223372036854775808` triggered signed overflow; arithmetic operators also overflowed at integer boundaries. The parser now checks the magnitude, and arithmetic reports an error instead of performing an overflowing operation. A host UBSan harness reproduced the parser overflow before the fix; `tools/test_expr_bounds.c` now passes under UBSan for both boundaries and all five operators.
4. The local libuv bridge summed write-buffer lengths without checking for wraparound. With lengths `SIZE_MAX` and `1`, the sum became zero and the subsequent copy wrote through a null buffer. A private ASan/UBSan harness using the extracted actual function and modeled surrounding types reproduced the failure before the fix and passed afterward. The bridge now rejects overflow, nonempty null buffers, and write-queue count overflow. A null connect request with a callback also dereferenced the request in the error path; that path now returns safely. `uvdemo` contains guest regression checks, not yet run in a guest.

## Additional static and sanitizer checks

Clang's static analyzer was invoked separately on all 367 tracked C translation units under userspace and ports, including the 115-file vendored snapshot's C units, with generic freestanding x86-64 include paths and a 25-second per-file timeout. It exited successfully for 351 units, failed for 15 (missing QEMU, Limine or upstream dependency headers in this generic configuration), and timed out on the SQLite amalgamation. Thirty-four units emitted warnings, including untriaged diagnostics in vendored uACPI/byacc, desktop, utilities, stdio and the libuv bridge. These results are not equivalent to the actual per-target build configurations, do not cover all headers in every configuration, and do not establish that the warnings are reachable defects or that other defects are absent. The two libuv null-buffer analyzer warnings persisted after the bounds fix, while its null-request warning disappeared. Further triage is warranted; no claim of exhaustive manual review is made.

The existing host image-decoder and BMP tests passed with AddressSanitizer and UndefinedBehaviorSanitizer in private `/tmp` binaries. The image test exercises PNG, JPEG and BMP reference pixels, truncations, deterministic mutations and declared limits; these are bounded tests rather than exhaustive fuzzing. The BMP test covers pixel round trips, truncation and header/size limits.

## Completion lifetime and regression follow-up

The local libuv shim's work completion wrote `done` and then used the request's loop without synchronization. The loop could invoke a completion callback that released the request or loop while the worker still used it. The filesystem completion queue had the analogous wake-after-unlock loop-lifetime window. Work and filesystem completion now publish and wake while holding the loop queue mutex; the loop takes the mutex before consuming work completion. The worker pool also returns `UV_ENOMEM` if every thread creation fails instead of accepting work that can never run. This is a focused fix, not a complete proof of all bridge thread safety.

`tools/test_uv_lifetime.c` compiles the actual bridge source and blocks its wakeup while a worker is running; it checks that the loop mutex is held for both work and filesystem publication. Against the baseline source the work check exited 3, and it passes with the fix. `tools/test_uv_bounds.c` links the actual `ports/srvros/uv.c` and checks wrapped lengths, null data, queue overflow and a null connect request. Both passed private native ASan/UBSan builds, as did `tools/test_expr_bounds.c`. The three tests are registered with `test-core`; the complete target and guest regressions were not run because this review avoided the shared build and VM.

On macOS the actual-source commands used `clang -std=gnu11 -O1 -fsanitize=address,undefined -ffunction-sections -fdata-sections -I shared/include -idirafter userspace/lib/include -I ports/srvros -include tools/uv_host_compat.h tools/test_uv_lifetime.c -Wl,-dead_strip -pthread -o /tmp/srvros_uv_lifetime` and the same flags with `tools/test_uv_bounds.c ports/srvros/uv.c -o /tmp/srvros_uv_bounds`. The expr check used `clang -I userspace/lib/include -I shared/include -ffunction-sections -fdata-sections -fsanitize=undefined -fno-sanitize-recover=all tools/test_expr_bounds.c -Wl,-dead_strip -o /tmp/srvros_expr_bounds`. Each resulting `/tmp` executable was run successfully. The host compatibility header supplies declarations for macOS compilation of Linux-specific functions; the gated tests do not exercise those functions.

## Analyzer warning classification

These are classifications of the original 34 warning-bearing units, not claims that all warning paths are understood or safe:

- **Reproduced/fixed in this review:** `ports/srvros/uv.c` had the null-request path and a separately reproduced write-length overflow; the analyzer's negative-connect-fd and residual copy warning remain unproven. `userspace/expr/expr.c` had separately reproduced signed overflow; its analyzer null warning has not been reproduced.
- **Intentional fault or negative tests:** `userspace/guifail/guifail.c` deliberately faults in its requested fault mode. `userspace/threadstress/file_errors_test.c` deliberately passes `-1` to `lseek`. The reported stream leaks in `userspace/threadstress/orphan_test.c` and `userspace/threadstress/stdio_access_test.c`, and demo-only stream warnings in `userspace/posixdemo/posixdemo.c`, remain to be checked against test intent; the latter also reports an errno warning.
- **Dead stores:** `userspace/deskd/deskd.c`, `userspace/fileman/fileman.c`, `ports/upstream/uacpi/source/event.c`, `ports/upstream/uacpi/source/sleep.c`, and `ports/upstream/uacpi/source/uacpi.c` produced dead-store warnings. These alone do not establish runtime corruption.
- **Vendored warnings requiring upstream/configuration triage:** `ports/upstream/byacc/output.c`, `ports/upstream/byacc/reader.c`, and `ports/upstream/uacpi/source/interpreter.c`, `io.c`, `namespace.c`, `tables.c`, and `utilities.c` produced uninitialized, null, shift, or dead-store diagnostics. They were analyzed in a generic configuration and not reproduced; no vendored source was changed on that basis.
- **Image and allocation:** `userspace/imageworker/decode.c` produced allocation and potential uninitialized-pixel warnings; the bounded PNG/JPEG/BMP sanitizer tests passed, but those tests do not disprove the warnings. `userspace/lib/src/posix_alloc.c` produced a possible null-tail warning; its list invariant was inspected but not exhaustively proved.
- **Remaining runtime paths:** `userspace/diff/diff.c` (stream state), `userspace/init/init.c` and `userspace/webd/webd.c` (possible uninitialized input), `userspace/lib/src/posix_stdio.c` (snapshot and realloc ownership), `userspace/lib/src/posix_time.c` (possible uninitialized time), `userspace/patch/patch.c` (error-path allocation leak), and `userspace/tap/tap.c` (open flags) remain unconfirmed and warrant targeted reproductions. `userspace/libcprobe/libcprobe.c` has a possible null string argument in the probe itself.
- **Cross-file diagnostic limitations:** `userspace/head/head.c`, `userspace/lib/src/gui2.c`, `userspace/make/make.c`, `userspace/tail/tail.c`, and `userspace/xargs/xargs.c` reported null/uninitialized warnings in shared `cli.h` without a complete cross-file path in the selected analyzer output. These require target-specific follow-up.

For desktop storage, `userspace/deskfs/deskfs.c` checks request magic, termination, operation and length before consuming payload; `desk_image_job_start` and `desk_capture_job_prepare` reject paths at the 160-byte limit before the corresponding Sketch copies. These checks were inspected in source; cancellation, interrupted I/O, all path races, and every GUI application flow were not exhaustively tested.

## Runtime warning follow-up

The following results refine and supersede the corresponding preliminary classifications above:

- `userspace/tap/tap.c` passed `O_CREAT` without the required mode argument. The target libc reads that missing variadic argument, so file permissions were indeterminate. The call now passes `0666` (subject to umask); `tools/test_tap_create.c` invokes the actual command with umask zero and checks the resulting mode.
- `userspace/diff/diff.c` treated a read error as EOF, allowing two unreadable/empty inputs to compare equal. An actual-source host test comparing a directory read against an empty file returned success before the fix; it now returns error status 2. Reading an already-EOF stream in the original analyzer trace is otherwise a redundant read, not evidence of a second memory error.
- `userspace/patch/patch.c` leaked a partially constructed line vector when allocation failed while splitting input. An actual-source allocator fault-injection test failed before the fix and passes afterward; the helper now frees its partial vector for both patch and target inputs.
- The `times()` analyzer trace assumes a positive return from `srv_usage(SRV_USAGE_SELF)`: it then skips the children query without taking the error path. The target kernel's `process_usage` returns exactly zero on success and a negative error otherwise, so that trace violates the current syscall contract. It is not a proof for a different syscall implementation.
- The `init` list-path and `webd` stat warnings arise because the analyzer cannot see writes through their `int 0x80` wrappers. On successful `SYS_LIST`, `syscall_list` writes a terminated path and size; on successful `SYS_STAT`, `syscall_stat` copies its initialized result. These observations depend on the inspected kernel ABI implementations. The common `cli.h` warnings in `head`, `tail`, `make` and `xargs` similarly assume a null element within the range `argv[0..argc-1]`; their entry contract supplies non-null arguments. `gui2.c`'s independent uninitialized-data warning remains unresolved.
- `snapshot_streams` holds the registry mutex and returns either an allocated array with its positive count, null with count zero, or null with count `SIZE_MAX` on allocation failure. Both callers recognize the sentinel. The analyzer assumes null with an unrelated positive count, which the helper does not produce under the registry and allocation invariants. The `getline` use-after-free trace begins by assuming the caller passed an already released buffer; its realloc path retains the old pointer on failure and publishes the new one on success. These source-level arguments do not establish absence of separate concurrency or malformed-caller defects.

The three command regressions are registered under `test-core`. They were compiled and run privately with `clang -D_DEFAULT_SOURCE -std=gnu11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I shared/include -idirafter userspace/lib/include tools/test_NAME.c -Wl,-dead_strip -o /tmp/srvros_NAME`, using `diff_errors`, `patch_alloc`, and `tap_create` for `NAME`. The patch test also passed with ASan/UBSan. No shared test-core build or guest run was performed.

The libuv lifetime follow-up also found a data race and a lost-send window between `uv_async_send` and loop dispatch. Pending notification publication/consumption now uses atomic operations, and the async handle is marked active during initialization rather than written by the sending thread. The source-based lifetime test checks initialization and a callback that re-arms itself. The wake pipe is now created nonblocking during loop initialization: lazy creation could miss a completion published immediately before the loop started polling, and separately setting nonblocking mode could fail before a worker wrote while holding the queue mutex. Initialization now reports a pipe creation failure. The source-based test checks the initialization flags. Queue callbacks run after the mutex is released, permitting ordinary callback submission and loop-close attempts; queued work remains linked until dispatch and filesystem pending counts remain nonzero through callbacks. This review did not prove behavior for recursive `uv_run`, concurrent handle close/send, or every loop operation.

The previously unresolved `gui2.c` warning is actionable. A directory path occupying `CLI_PATH_MAX-1` bytes leaves no space for the trailing slash and terminator; `cli_join_path` fails without terminating its partial output, but the file dialog previously used that output as a string. `gui2_fd_refresh` now clears the displayed listing and reports `Path too long` on join failure. `tools/test_gui2_path_bounds.c` invokes the actual source with a maximum-length directory and a stub list syscall; the original called the list operation despite the failed join, while the fixed implementation stops. The test is registered with `test-core` and passed privately under ASan/UBSan. The follow-up analyzer traces for `make`, `tail` and `xargs` confirm their null `cli_starts_with` arguments came from the assumed-null argv elements described above.

A final ownership pass found the same publish-before-wake lifetime window in `uv_async_send`: a loop callback could run and close the handle/loop while the sender still used the loop to wake it. Async publication and dispatch now also synchronize through the queue mutex, with callback invocation after unlocking. The actual-source lifetime test gates an async sender in the wake operation and checks that the loop mutex remains held until it returns; it also tests callback re-arming. These checks passed privately with ASan/UBSan. They do not establish safety of callers sending concurrently with or after handle close.

## Combined guest verification

The root coordinator's rebuilt matched image passes the full libc probe,
local `uvdemo` bridge suite and upstream libuv suite in separate x86_64 QEMU
guests on the ARM64 host. The libc probe executes the allocation and integer
boundary cases. The local bridge probe executes the new invalid-buffer and
length cases. The actual-source UV bounds and lifetime tests also passed an
independent native ARM64 ASan/UBSan run after the final edits. Earlier notes
about guest checks being pending describe the private review phase.

The full CLI and staged-port guest suites subsequently passed as well. Their
post-run exFAT checks reported zero leaks, FAT errors and filesystem errors.

## Tracked dependency follow-up

A source-based sanitizer regression reproduced a null parent dereference in
uACPI 7.0 namespace resolution: a null scope selects the root for the initial
lookup, but the missing-relative-name fallback used the original null parent.
The local patch normalizes both pointers to the root. The real-source host
fixture covers found and missing names, absolute lookup, upward lookup and the
root boundary. It passed native ARM64 ASan/UBSan and the Linux GCC 14 Make
target. The full offline namespace suite also passed all 30 cases.

Additional analyzer traces were checked against the tracked source. The
interpreter's package-length marker is derived from a byte's top two bits and
therefore lies in 1..4, all initialized switch cases. Its top-level return
object is allocated only when an output pointer exists, and no other assignment
to that context field was found. Immediate parse operands are allocated and
checked before dispatch according to the static parse-op metadata. These
invariants explain the corresponding warnings; they do not establish all AML
execution paths. Byacc's semantic-action line state is initialized and consumed
under the same command-line flag, and its invalid-symbol branch terminates via
`syntax_error`. Other unconfirmed dependency leads remain subject to deeper
configured testing; no generic-analyzer clean bill is claimed.

The expanded interpreter trace for the inline-array warning assumes a zero
call-stack depth immediately after `push_new_frame` successfully allocated its
first frame. `stack_unwind` therefore appears to retain a null operation context
in that infeasible trace; the actual array size is nonzero after the push.
The HID warnings likewise cross `uacpi_eval_typed`'s type guarantee: the integer
and string cases either allocate an ID or return an error before publishing it.
The absolute-path helper requires a live node attached to the namespace root;
a null or detached foreign node does not meet that internal contract. Byacc's
saved-line copy is reached after parsing a parameter line; EOF terminates the
parser before the save. These are source-invariant classifications, without
a new runtime proof for every caller. The detailed interpreter trace is kept
locally in `build/review/whole/uacpi-interpreter-trace.log`.

A second actual-source reproduction found physical table installation always
returned success after its cleanup path, hiding an allocation failure and
allowing `uacpi_initialize_tables` to publish an uninitialized FACS pointer.
The installer now returns the installation result; tests check allocation
failure and denial without output/global publication, failed and successful
overrides, successful installation and optional firmware-table enumeration.
The baseline fixture aborts at the expected error assertion; the patched fixture
passes GCC 14 and native ARM64 ASan/UBSan. Both new Make targets and the full
30-case offline namespace suite passed together on Linux. Local vendor patches
and upstream archive provenance are recorded in `ports/srvros/uacpi/PORT.md`.

The initial optional-table skip was independently found to conflate a direct
host rejection of an XSDT entry with a required DSDT rejected while installing
its parent FADT. A private direct-denial flag now distinguishes them. The added
required-child fixture fails against `699056f6` and passes afterward; the three
Linux table/namespace targets and native sanitizer fixture passed again.

The GAS follow-up confirmed three separate defects: encoded DWORD/QWORD
access sizes were decoded incorrectly, mappings truncated partial/aligned access
spans, and memory unmaps used one access width rather than the original full
span. Independent modes of `tools/test_uacpi_gas.c` fail against the old source
for each defect. The fix follows [ACPI 6.6 Table 5.1](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html),
checks register width/offset bounds before deriving access width, and maps and
unmaps the same aligned span. Existing offset behavior, FADT width preference
and the SystemIO 32-bit cap are retained. The actual-source test links the real
bit-scan helper and uses private register mocks to enforce bounds, widths and
exact memory unmap lengths. Independent native ARM64 ASan/UBSan runs passed
all three modes. No real MMIO or port I/O is performed by this fixture. The
original mapped-shift warning is bounded for successfully validated mappings:
access widths are nonzero powers of two and aligned total width is at most 64.
This is not a proof for callers constructing invalid internal mapped objects.

## Configured analyzer completion

The SQLite amalgamation completed a 210-second Clang analyzer run with the
actual OS_OTHER/threading/feature defines, the SrvrOS headers and an x86_64
freestanding target, without diagnostics. A Darwin-host attempt first failed
because it selected host sysctl headers; that was a configuration failure.
Fourteen of the fifteen previously missing-header units also completed after
using their build include paths/defines (or the native battery target headers).
Zig's `-march=x86_64` spelling was translated to Clang's `-march=x86-64`.
Commands and diagnostics are retained in
`build/review/whole/configured-analyzer-followup.json` and
`sqlite-target-analyzer.json`/`.log`.

The new JSON-demo warning crosses `cJSON_IsNumber`'s explicit null rejection.
The native battery fixed-address reports follow a checked VMM mapping and the
platform's exact memory allowlist; source inspection does not establish physical
acceptance. The shell control-parser traces assume a matched keyword even when
its required prefix bytes are not known initialized; `shell_keyword_at` checks
each prefix byte before dispatch. Parameter bodies are copied and terminated
before parsing; the reported name-loop traces lose the terminator invariant.
The remaining reported stores are redundant assignments. These classifications
are source reasoning, not exhaustive shell or firmware verification.

The custom QEMU LPSS model still needs its separate QEMU source/build headers
for configured analysis. Its 511-line model was manually read: packet slots,
transfer indices and private-register indices stay bounded, reset clears model
state, and unrealize unregisters its input handler. The model deliberately drops
oldest queued packets under pressure, implements only a small register subset
and stubs DMA. No custom-QEMU build, migration or device-lifecycle runtime
validation was performed in this pass.

## Shell arithmetic follow-up

Manual review of the configured shell trace found unchecked signed arithmetic
in literal parsing, negation, addition, subtraction, multiplication and the
`INT64_MIN / -1` and remainder cases. The source-based regression reproduces
the old addition overflow under UBSan. The parser now reports an evaluation
error for overflow and out-of-range numeric variables, supports the direct
minimum signed literal without negating it, and rejects an empty hexadecimal
literal. Minimum-value formatting was already safe and is retained in the test.

Logical short-circuiting previously skipped the parser call itself, leaving
RHS text unconsumed: even `0 && 1` and `1 || 2` failed. The parser now consumes
all syntax while suppressing evaluation in skipped branches. Skipped division
by zero, overflow and variable conversion do not fail; malformed syntax still
does. Nested cases and evaluated errors are covered. The actual-source host
fixture passed native ARM64 and Linux ASan/UBSan with strict warnings. The CLI
fixture now checks boundary output, skipped division and status 2 after numeric
overflow, division overflow and out-of-range variables.

The final full CLI guest passed all 491 commands, including the new exact
boundary-output and error-status markers. Its post-run exFAT check reported
zero leaks, FAT errors and filesystem errors. An independent baseline host run
also reproduces the minimum-literal signed overflow under UBSan; its log is
`build/review/whole/shell-arithmetic-before.log`. The final host command passed
all 173 core targets plus the 30-case offline namespace suite, with 415 Python
cases across the combined run.

The final staged-port guest also passed after the shell changes, with a clean
post-run filesystem check. Its transcript is
`build/review/whole/ports-shell-final.log`.
