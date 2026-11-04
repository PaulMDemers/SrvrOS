# Browser engine port

## Active startup milestone — November 3, 2026

User authorizes diagnosing/fixing the JSC startup abort and WTF footprint fault,
then a minimal real no-JIT JavaScript smoke. Start from OS `ccb969c3`, ports
`af466ba` and the retained verified ELF/source/proofs. Acceptance requires the
utility's mapped-byte/arithmetic/Unicode checks and an actual JavaScript result,
with honest limits on collector/concurrency qualification. Preserve old binaries,
failed guest ISOs and receipts. Own WebKit adaptations and browser diagnostics;
inspect shared SDK/kernel ownership before any runtime fix. No success stubs,
fabricated register contexts or competing suspension mechanism.

Fetched main `c90f3522` adds installation/profile/salvage/Haswell diagnostics;
its diff since the tested base does not change syscall/process, pthread, signal,
C++ or math implementation. Ports main remains `ca74951` (libuv). PR317 at
`e2457b1b` owns VMM/process/DMA hardening; PR329 owns filesystem sync/salvage,
PR330 installation I/O, PR301 display, PR19 Wi-Fi and PR306 SQLite retain scope.
Use the current matched SDK first to diagnose the exact retained failure.
Resource bounds continue: two compiler jobs, one build/VM harness, 2 GiB reserve,
6 GiB per retained JSC run; approximately 27 GiB host free at start. Protected
review VMs 61610/6233/85869 remain live. No physical boot/disk, Actions or merge.

### Confirmed startup causes and shared-runtime handoff

Fresh source build `build/browser-jsc/run-nxi_i90d` succeeds on clean source
`26e9e78f`. Receipt `receipt-1793759255161348000.json`, SHA256
`ab60967d353e2af5d7658fe69d21dcc99a7d0a48845e3d3cecbf7fe19d9924e7`,
records static ELF SHA256
`4b32bb9ce3c25b1b294714b5bd664fbf17555b6f87226d2e35742313b8c45d39`.
PT_TLS now covers all 2,096 bytes, file size zero and alignment 16; dynamic/
interpreter segments are absent. The 893/172/168 target archive members remain
ELF64 x86_64. The source fingerprint remains `81d2a8dba49cd370432260d9e221dd9a430f6b9b53f8d7618093f94a870154db`;
manifest identifier is `6f70d8b80a8434b7b2a1703d540af446f8068680389ecdcf964d937922f6a1b5`.
Fresh patch replay `replay-1793759315358086000.json` matches exactly.

The fresh Release utility is built by `tools/browser_wtf_probe.py`; its receipt
is `wtfprobe-e_vdbqob.json` in that run. The actual required-success smoke is
`build/browser-jsc/guest-5f8_ysyu/receipt.json`: the entire utility passes status
zero, then JSC exits 134 without `42`. The harness correctly records **failed**
and returns nonzero. The corrected TLS layout therefore fixes the utility fault
but does not fix or hide the independently proven signal-handler startup abort.
The VM stopped. All recorded build input/output hashes were independently checked.


The utility fix is verified in `build/browser-jsc/guest-8diqa2_5/receipt.json`:
status zero and every footprint, mapped-memory delta, UInt128 decimal and scalar
Unicode marker passes. The ELF is separately linked with the corrected source
script while the original SDK/evidence was still preserved. A second discovered
fixture defect was a Debug/Release C++ ABI mismatch: its missing `NDEBUG` made
inline CString access skip eight bytes compared with the Release library layout.
The new fixture recipe derives the real Ninja compile profile, including
`NDEBUG` and libc++ hardening, rather than approximating it. The failing decimal
assertion is retained in guests `guest-2i0vh_a1` and `guest-nyp5lqx2`; it was not
weakened. The exact successful recipe is archived beside its receipt.

`tools/browser_jsc_elf.py` now rejects missing or partial PT_TLS coverage of
SHF_TLS sections, enforces the kernel's 4 KiB template/alignment limit and
rejects dynamic/interpreter segments. It rejects both old malformed binaries
and accepts the corrected utility ELF. The build calls this gate before recording
success. A historical helper snapshot may be supplied explicitly when replaying
an old immutable ELF after validation-only helper changes; its exact recorded
hash is required and all other inputs/artifacts still have to match.

The user authorized a direct owner handoff to “Review SrvrOS repository context”;
the confirmed abort backtrace and proposed context/suspension contract were sent.
The audit owner agreed that this chat can own the dependency in a separate OS
branch and draft PR, with ABI/delivery review before implementation and retained
process/VMM/teardown integration review. The isolated `SrvrOS-signal-context`
checkout, branch `device/apple-silicon/browser-signal-context`, merges main
`89c210db` with PR317 `c9f4f306` as `af470fff`. [Draft PR334](https://github.com/PaulMDemers/SrvrOS/pull/334)
contains design commits `8c8dc952` and `76d70a49`, including independent review
invariants and the JSC handshake failure rule. It contains no signal/runtime
implementation yet. PR310 must consume the reviewed dependency; its code remains
separate. The managed-worktree tool rejected the nested-repository workspace,
so a separate local checkout was used without changing the integration checkout.
Matched SDK/runtime/ICU rebuild now passes. All 17 combined runtime cases pass
in `build/browser-runtime/guest-_liv6x0k/receipt.json`, including both new TLS/GOT
thread-isolation cases; both writable filesystem runs pass in
`build/browser-filesystem/guest-g8tllct7/receipt.json`. Both guests stopped.
The current structural proof `build/browser-tls-got/proof-7252x0jm/receipt.json`
rejects both old layouts and accepts complete PT_TLS coverage for both corrected
layouts (zero-initialized and mixed initialized/zero TLS). Old SDK/dependency artifacts were hashed and copied to
`build/browser-integrated/startup-baseline` before refreshing the installed SDK.

CPU trace `build/browser-jsc/guest-m3bl4ghw/interrupts.log` records a CPL3 page
fault at `signal_dispatch_pending` reading `%fs:0` with FS base zero. The first
raw phase marker's `write()` triggers this during its post-I/O signal dispatch,
**before the footprint query is called**. This supersedes the earlier interval
inference attributing the utility fault to its first footprint call. The ELF's
`.tbss` is 2,084 bytes but its PT_TLS memory size is zero. JSC similarly has a
2,096-byte `.tbss` but advertises only 40 bytes in PT_TLS (its orphan GOT).
A bounded target-only TLS/GOT regression reproduces the linker-script defect;
explicit GOT placement before TLS is under verification.

A hardware breakpoint on the original JSC's `abort()` gives the exact backtrace
in `build/browser-jsc/debug-t2rszujx/lldb.log`: `main` → `WTF::initialize` →
`Thread::initializePlatformThreading` → `RELEASE_ASSERT(signalIsInstalled)` at
`ThreadingPOSIX.cpp:237`. The SDK rejects its `SA_RESTART | SA_SIGINFO` handler.
No missing memory query, arithmetic failure or random-source failure has been
established by these observations.

Proposed shared signal/context work, for agreement with the runtime owner:

- Deliver a directed signal asynchronously on the target user thread with its
  interrupted x86_64 general registers, instruction/stack pointers and masks.
  Preserve the scheduler's CR3/TLS/FPU and I/O admission/teardown contracts.
- Supply a real public machine-context layout, validated signal frame and
  return/trampoline path; preserve masks, nesting, atomic suspend/resume and
  thread-exit race handling. Do not merely accept the flag or invent registers.
- Prove async-safe semaphore publication/waiting and the suspension handshake
  with a CPU-bound target, a root held only in a register, stack roots, repeated
  suspend/resume, masking, exit races and refusal/failure paths before GC use.
- Connect WebKit to that tested context. Its present `HAVE(MACHINE_CONTEXT)=0`
  fallback captures only an approximate stack pointer and is insufficient for
  conservative register roots even if handler installation starts succeeding.

Pinned-source review found no supported option that removes this requirement:
`useConcurrentGC=false` selects a stop-the-world scheduler (`Heap.cpp:461-469`),
and one marker removes parallel markers (`HeapHelperPool.cpp:48`); machine-root
scanning still suspends registered mutators (`MachineStackMarker.cpp:145-230`).
The first owner handoff should agree the kernel/process/pthread implementation
branch and how PR317's live process/VMM changes are consumed. No competing
suspension implementation is being integrated in this browser branch.

## Active compilation checkpoint — November 3, 2026

The compile/link target is achieved: pinned WPE WebKit 2.54.2 produces a real
static ELF64 x86_64 `jsc` with JIT, WebAssembly, remote inspection and sampling
profiling disabled. The build uses the static LLInt assembly interpreter;
`ENABLE_C_LOOP` remains off. This is compilation acceptance, with runtime/GC
acceptance still subject to the shared suspension/root-capture contract below.

The retained run is `build/browser-jsc/run-w8vtuwcr`. Its first successful
receipt is `receipt-1793755801803185000.json`; it records actual compilation,
target archive creation and executable linking, rather than configure alone.
The exact executable is `cmake/bin/jsc`, 271,759,848 bytes including debug data,
SHA256 `c2b8f14cb3a2001ef1dd87bc64604afa2075b2fc320c1c8f68c54f4b6f4d46d4`.
Independent ELF inspection finds no interpreter or dynamic segment and no
unresolved strong symbols. The only undefined weak symbol is the optional
`__posix_pthread_create_trace`, whose SDK caller checks for null. Loadable
segment memory totals 51,661,331 bytes and the TLS template is 40 bytes; these
are artifact bounds, not peak working-set measurements.

| Target archive | Validated ELF64 x86_64 members | SHA256 |
| --- | ---: | --- |
| `libJavaScriptCore.a` | 893 | `9dadb1d28312b5e5524b8b9e4e1169dc64cc7e67b95d94b11c9515ad4674369d` |
| `libWTF.a` | 172 | `66578054b2d630c331a51953964ba68a6dc8b6e17c5a215d24c10cb046fbc52c` |
| `libbmalloc.a` | 168 | `4b61ddf4f4fb57f8d95687cf536ce348d13d4119da6825ccceaa6a29925205cf` |

The fresh source replay in `replay-1793755880352653000.json` extracts the pinned
archive and reapplies the full ordered patch stack with zero fuzz. All retained
and freshly replayed source paths, modes and contents match fingerprint
`81d2a8dba49cd370432260d9e221dd9a430f6b9b53f8d7618093f94a870154db`.
The bytecode-cache manifest identifier is
`446d514d0b38fcba7c47042be4e228ec941427d5c938a5a2a38375b1bf7dc772`;
it does not establish persistent cache compatibility. Compiler/link failures,
disk-pressure pauses and controlled source/SDK transitions remain separate
receipts. Global date/time warnings are preserved.
The post-doc verification receipt `receipt-1793755971341283000.json`
also checks remote inspection is off and hashes the final helper/port inputs;
its SHA256 is `d6e16a02f56af39d78af62a1d3d07801cc1f864e737785e300a42007727fdee8`.

The first private diskless guest observation is
`build/browser-jsc/guest-p9yc2eek/receipt.json`. Both ELFs entered userspace:
`jsc -e print(42)` exited 134 without printing `42`; the separate WTF fixture
exited 139 without its success marker. Its serial contains no conclusive
assertion/fault diagnosis. These failures do not establish which prerequisite
was reached first, and cannot be attributed conclusively to the independently
reproduced `SA_SIGINFO` limitation. No interpreter, garbage collector, Unicode
utility or browser runtime pass is claimed from this run. The guest stopped.

One separate raw-write diagnostic in `build/browser-jsc/guest-89v6l2z0/receipt.json`
reaches `wtfprobe: entered` and exits 139 before `wtfprobe: footprint-before`.
The interval contains the first `WTF::memoryFootprint()` call; its underlying
cause remains unidentified. The later arithmetic/formatting/Unicode checks were
not reached. The original fixture source and binary were reproduced exactly;
the diagnostic uses separate saved source, binary and receipt. Both guests
stopped. The final no-work compile verification is
`receipt-1793756232569714000.json`, SHA256 `ededab3ba87003a15996839823df3c4a4e2d3d42247b6390230d2bfe06d82300`.
All its recorded input/output hashes match the published source and retained
artifacts. Raw guest harness sources are preserved with the diagnostic evidence;
the published harness additionally derives its run directory from the selected
receipt instead of a fixed local run name.

User authorizes actual no-JIT JavaScriptCore compilation and resolving build
blockers. Start at OS `4f84d500` and ports `e866ff5`; pinned WPE 2.54.2 and
ICU 79.1 remain the inputs. Retain a source/build tree and actual compiler logs.
Own the SrvrPorts WebKit overlay/build recipe, dedicated build evidence and
this topic record. Shared SDK additions require inspected ownership and real
behavioral tests; no success stubs or parallel pthread/kernel implementations.

Fresh main is `56e1f950`, including merged PR252 and filesystem namespace work.
Inspect and consume relevant published dependencies before shared runtime fixes;
early compiler exploration can use the prior verified SDK but cannot claim
current-main acceptance. PR317 owns VMM range/teardown; PR315 owns installed
paths, PR301 display, PR19 Wi-Fi, and PR306 SQLite retain their independent scopes.

Target: compile and link the static no-JIT JSC target where the supplied runtime
permits; resolve local build/platform errors and publish concrete shared-runtime
requirements when an owner contract is necessary. Configuration alone is not
acceptance. Keep JIT, WebAssembly and remote inspection disabled. A runtime shell
smoke is appropriate only after a valid ELF link; browser rendering/networking
remain later milestones. Initial budget: 14 GiB free, at most 6 GiB additional
artifacts, two compiler jobs, one build/VM harness, 2 GiB disk reserve. Preserve
review VMs 61610, 6233 and 85869. No physical install, Actions dispatch or merge.

### Compilation integration checkpoint

Consumed main `56e1f950` through merge `95510aba`; ports merge `04092809`
retains both the browser work and main's PR252 libuv/uACPI fixes. The previous
build proof remains tied to its older SDK. Rebuild all runtime/ICU evidence and
matched kernel/init before continuing the Release JSC build. The matched rebuild and all 11 guest
cases pass in `build/browser-runtime/guest-0klm2ysr/receipt.json`; its VM stopped.
Integration logs are under `build/browser-integrated/run-H97H4c`. Initial retained
run `build/browser-jsc/run-znpq988j` configured successfully and generated host
derived sources, then failed on bmalloc's missing direct standard-library
include. It is failed compilation evidence, not an engine build pass.

### Concrete compiler and runtime dependencies

Release compilation passed the bmalloc header error and exposed additional
portable adaptations: avoid an unavailable context header while retaining
`HAVE(MACHINE_CONTEXT)=false`, and select upstream's 512 MiB available-memory
guess when `_SC_PHYS_PAGES` is unavailable. That value is a heuristic, not a
system-RAM or per-process capacity measurement.

WTF FileSystem then failed: header presence enabled `HAVE_STD_FILESYSTEM`
despite failed CMake link probes, while the configured libc++ excluded filesystem
operations. The port now uses pinned upstream filesystem support against the
existing SDK calls and merged namespace semantics, plus actual feature detection.
The pinned filesystem archive links and its initial private exFAT guest passed two
consecutive create/write/read/copy/overwrite/error/rename/iteration/cleanup runs:
`build/browser-filesystem/guest-178phxcj/receipt.json`. Copy uses real POSIX
read/write without enabling localization. A minimal SDK `statvfs` declaration
fix supplies its existing C ABI to C++ callers; it adds no filesystem policy.
The changed header is covered by fresh ICU run `run-t8u158dv` and all 11
combined guest cases in `build/browser-runtime/guest-sknzuzqp/receipt.json`;
the guest stopped before JSC restarts. Runtime/provider hashes and patched-source fingerprints are recorded.

Disk reserve pauses preserve failed runs; only inactive generated trees are
reclaimed while their command logs/receipts and pinned source inputs remain.
The current JSC source state is retained until a replacement build is ready.

Independently, compiling the fallback register type does not qualify JSC GC.
ThreadingPOSIX uses signal suspension and register capture; current SDK
`sigaction` explicitly rejects `SA_SIGINFO`. MachineStackMarker suspends/scans
other threads. The runtime owner must supply a supported suspension/root-capture
contract (or an upstream configuration proven to require none) before accepting
collector execution. No fabricated context layout, successful suspend stub or
new competing pthread/signal implementation is permitted by this checkpoint.

### Matched link dependencies and ownership

The current candidate consumes main `56e1f950` and ports main `5e994f5`.
A later inspection observes OS main `1a5a6dee` (installation, profiles and
salvage tools) and ports main `ca74951` (libuv spawn result); these do not overlap
the two SDK linkage headers or C++/math providers. They were inspected, not
imported into this candidate or claimed as tested. PR317 at `7240b5ce` owns
VMM/process/DMA hardening, rather than the signal-suspension implementation.
Review continues through draft PR310 and SrvrPorts PR4; no competing kernel,
pthread or signal mechanism was introduced.

Real final-link failures required C linkage guards around existing declarations
in `semaphore.h` and `regex.h`, upstream libc++ `algorithm.cpp` and `hash.cpp`,
and 17 scalar math functions. The two headers retain identical target-C
preprocessed declarations. Separate archive objects for `std::terminate` and
the no-exceptions system-error fallback let the real filesystem `system_error`
provider link without duplicate strong definitions. The shared compiler wrapper,
base libc++ feature profile and thread ABI stay matched.

The [math supplement](../ports/srvros/libm/PORT.md) builds the pinned Zig 0.15.2
musl sources, with privately prefixed accurate dependencies. It does not replace
the existing SDK public `exp`, `log` or `sqrt`. An observed large-negative
`acosh` cancellation case requires a domain guard before the pinned algorithm;
inputs below one use musl's real invalid-operation helper. Host edge/domain/
threshold fixtures and 10,000 deterministic binary64 patterns pass the eight-ULP
bound; target guest vectors pass. This is sampled numerical evidence under the
tested environment, not full rounding-mode, errno or floating-point-exception
conformance. The older public SDK math accuracy remains a separate JSC runtime
correctness requirement.

Published dependency sources are SrvrPorts `9451133`. The receipts below retain
the actual dirty-source observations atop OS `16527164`; the later publication
commits do not relabel these checks as clean-commit builds.

Fresh matched evidence after both header changes:

| Check | Retained evidence |
| --- | --- |
| Writable filesystem | `build/browser-filesystem/guest-xqe688nf/receipt.json`: both runs pass, private directory cleaned, VM stopped. SHA256 `d9c73b8998896f05c0c7c4c47ea08ab023da7f3328f66e5db9de5e3bcbb67375`. |
| ICU refresh | `build/browser-icu/receipt.json`: successful rebuild, SHA256 `00bbaee3895d23190f29875c1326e7cbeae5a737857f981a45805da9256a5c39`. The explicit previous-install option preserved mtimes of 203 headers only after identical content and mode checks; no header contains `__TIMESTAMP__`. Libraries retain their fresh metadata. |
| Combined runtime | `build/browser-runtime/guest-9cufz_rw/receipt.json`: all 15 cases pass, VM stopped. SHA256 `48452281071b9f03f3b2c50a666c6802f3445ae59676306420d885086a0e04d6`. Includes prior thread/TLS/exhaustion/RTTI/ICU/math checks, real C++ semaphore/regex calls, sort/rehash and the signal boundary. |
| Supplemental math | `build/browser-libm/receipt.json`: SHA256 `3c642d8d6d3b003b0f79324271bd7ddbcedf734668558e42f86e1320c7d52d7c`. Archive SHA256 `47bd38bb6adcdf83d693a8ac5718034b877d591886a820c80f7394404fffcb9a`; private dependency closure has no unresolved references. |

The signal-boundary case succeeds by reproducing the known limitation:
`SA_SIGINFO | SA_RESTART` returns `EINVAL` and preserves the previously installed
handler and caller output. Ordinary one-argument self-signalling still works.
It does **not** accept async suspension or register capture. WTF's POSIX
suspend/resume path requires per-thread asynchronous delivery with a valid
interrupted register context, correct masks, an atomic handshake, signal-safe
synchronization and exit/race handling. JSC's collector scans suspended mutator
stacks and registers. That contract belongs to the shared signal/process runtime
owner; an independently proven safe safepoint design is an alternative.
`tools/test_browser_signal_contract.c` records the concrete source handoff.
No successful suspend stub, fabricated context or uncaptured-root claim is used.

### Reproduce the compilation checkpoint

Published engine adaptations are SrvrPorts `af466ba`. Use the matching OS
branch and ports gitlink, verified WPE 2.54.2 archive and
ICU 79.1 archives described in the earlier checkpoint. Run one command at a time:

```sh
sh tools/macos/build.sh -j2 srvros-sysroot build/kernel/srvros.elf build/userspace/init.elf build/limine-binary/.ready build/userspace/cxxprobe.elf build/userspace/cxxstlprobe.elf
python3 ports/srvros/libcxx/build.py
python3 ports/srvros/libcxx/build_rtti.py
python3 tools/browser_math_probe.py
python3 ports/srvros/libcxx/build_filesystem.py
python3 ports/srvros/libm/build.py
python3 ports/srvros/icu/build.py
python3 tools/browser_extension_probe.py
python3 tools/browser_signal_probe.py
python3 tools/browser_filesystem_smoke.py
python3 tools/browser_runtime_smoke.py --icu --runtime-extensions --signal-contract
```

For an existing ICU install, move it aside first; optionally supply that directory
via `--previous-install` to preserve only identical installed-header metadata.
Use the new guest receipt paths printed by the two harnesses in:

```sh
python3 tools/browser_jsc_build.py \
  --filesystem-guest-receipt build/browser-filesystem/guest-.../receipt.json \
  --math-guest-receipt build/browser-runtime/guest-.../receipt.json
python3 tools/browser_jsc_replay.py build/browser-jsc/run-...
```

The dots are placeholders for this run's output paths. Default resume rejects
changed source, toolchain, cache or dependency proofs. The narrowly checked
historical SDK/package transitions are documented in the WebKit port manifest;
they are not permission to reuse arbitrary stale objects. Generated artifacts,
logs and receipts remain local under ignored `build/`; committed recipes and
published source revisions provide the portable handoff.

## ICU checkpoint — November 3, 2026

User authorized a pinned SrvrOS ICU port and advancing the existing no-JIT
JavaScriptCore configure to its next dependency. Continue draft PR310 and
SrvrPorts PR4 in the assigned `SrvrOS-browser` checkout, starting at OS
`ddb841d2` and ports `4d47276`. Freshly fetched main is `3db1a1d3`: its new
installation PR311 changes only provisioning tools/docs, not this SDK. Retain
the already tested runtime base for this bounded library experiment; do not
claim the new installation implementation is tested here.

Own `ports/srvros/icu`, browser dependency/guest tooling and this topic record.
ICU 79 requires RTTI; extend the owned C++ port with a separate pinned upstream
libc++abi RTTI archive and focused cast tests, without changing the existing
OS guard/TLS implementation or adding duplicate ABI providers. Executable ICU
linking also exposes missing `tanhf`; the bounded SDK math addition supplies
`tanhf` and its `expm1f` dependency from the pinned musl source with numerical
edge tests. Fresh inspection of PR252 file changes shows no overlap in math
sources/header. Keep this shared SDK addition reviewable in PR310; no parallel
ICU-local libc implementation or pthread/kernel change is authorized by it.
Consume the existing SDK and threaded libc++ slice. Shared process, I/O,
pthread, resolver and kernel changes remain owner-coordinated dependencies;
PR252 `badfeb37`, Wi-Fi PR19 `993e920a`, display PR301 `4c9fc184`, SQLite
PR306 `5028871b` and filesystem/profile PR308/309 retain their scopes. No new
runtime stubs or duplicate implementations to pass configuration.

Pin official ICU4C 79.1, source archive 28,162,455 bytes, SHA256
`06fa63ecb521048f8082f12ad988f6b678d90cbd88aafce24cbc94902b191d99`.
The [official release](https://github.com/unicode-org/icu/releases/tag/release-79.1)
and [ICU release index](https://unicode-org.github.io/icu/download/) were checked
November 3. Build target common/i18n/data static libraries and meaningful guest
Unicode/data/concurrency checks, then rerun real executable CMake detection.
Any data reduction or unsupported ICU facility must be explicit; no host ICU
may satisfy the target. A complete JavaScriptCore build remains out of scope.

Resource envelope: approximately 7.2 GiB free at start, at most 3 GiB additional
artifacts, two compiler jobs, one build/guest harness at a time. Protected QEMU
PIDs 61610, 6233 and 85869 remain untouched. No physical disk, host reboot,
Actions dispatch or merge. Publish ports before the OS gitlink; preserve exact
hashes/logs and distinguish compiler/link checks from guest acceptance.

### ICU checkpoint result and evidence

Published SrvrPorts commit `e866ff5` extends
[draft SrvrPorts PR4](https://github.com/PaulMDemers/SrvrPorts/pull/4); matched SDK
math, probes and the gitlink are carried by
[draft SrvrOS PR310](https://github.com/PaulMDemers/SrvrOS/pull/310).

The bounded milestone is complete: pinned ICU 79.1 common/i18n libraries and
its full 33,558,160-byte little-endian data image build as static SrvrOS archives.
The matched guest passes Unicode and locale fixtures. No-JIT JSCOnly CMake
configuration and generation now **complete successfully**; no additional
required dependency is reported at configure time. The next gate is actual
JavaScriptCore compilation and then executable/runtime acceptance, not another
claimed missing package. No engine source was compiled or executed here.

ICU requires RTTI, so the port builds a separate real libc++abi RTTI archive
from pinned Zig 0.15.2 sources. ICU also required `tanhf`; the SDK now supplies
it and `expm1f` using the pinned musl algorithms, with MIT/Sun notices retained
in `posix_math_hyperbolic.c` and indexed in `THIRD_PARTY.md`. The host numerical
comparison passes signed zero, infinities, NaNs, subnormals, threshold neighbors
and 200,000 deterministic float bit patterns within its three-ULP bound. This
is bounded numerical evidence under the tested rounding environment, not a
claim of full libm conformance.

| Acceptance | Recorded result |
| --- | --- |
| Clean ICU recipe | `build/browser-icu/run-_0xar8ww/receipt.json`: successful common/i18n/data build and selected executable link; 245 hashed files and dependency receipts. Current receipt SHA256 `7fc26c99e22524443cbb968b81ac0e676bf45ab399a855367482a85c71ed084b`. |
| Matched guest | `build/browser-runtime/guest-f3hw_u3w/receipt.json`: all 11 cases pass; own diskless/networkless Q35 VM stopped. Math edges, RTTI down/cross/failed pointer casts, expected fatal reference cast, ICU twice, existing C++ probes, TLS ordering, thread exhaustion and recovery. |
| ICU guest behavior | Initialization, concurrent NFC normalization, German case expansion, invalid UTF-8 rejection, English word boundaries, Swedish tailored collation, German and English number formatting, UTF-32 supplementary-character roundtrip and cleanup/reinitialization. |
| Executable bounds | Unstripped ELF 50,721,616 bytes; loadable segment memory totals 36,110,325 bytes, TLS template 2,084 bytes (below existing 4,096-byte limit). These are artifact sizes, not peak working-set measurements. |
| No-JIT configure | `build/browser-platform/run-uhaxo369/receipt.json`: status zero, configuration and generation complete, 40.672 seconds including bounded extraction/controls. ICU 79.1 headers and data/i18n/uc resolve exactly to the verified target install. |
| Negative proof check | A changed ICU library digest is rejected before compiler or VM launch. Runtime, RTTI and math receipts must share the SDK header digest; extra unrecorded installed files are refused. |

The first post-ICU configure found all three libraries but stopped because the
WPE release archive omits `Tools/Scripts/hmaptool`. The final probe uses the
upstream-supported `USE_HEADER_MAPS=OFF` path (also the WPE/GTK default), leaving
ordinary include paths. The failed receipt `run-aty0yc_0` remains failed evidence.
No missing target API was stubbed and no thread/ICU discovery result was forced.
The probe's temporary WebKit extraction is removed afterward; the recorded
CMake graph is configure evidence, not a retained ready-to-build source tree.

The separate whole-archive audit explicitly disables section garbage collection.
It still fails on unsupported exception/future-related symbols referenced by
unused sections of the existing libc++ `future.o`. This does not identify a new
ICU API requirement; the normal selected-object link with garbage collection
passes. It also does not establish complete ICU API or full C++ runtime link
closure. Preserve this audit limitation rather than enabling untested futures,
exception handling or thread-exit notification behavior. Those prior pthread
teardown-order handoffs remain open. The ICU wide-string API explicitly uses
UTF-32 on the four-byte SrvrOS wchar ABI; libc's byte-oriented multibyte routines
remain a separate shared-runtime limitation.

Final coordination fetch observes OS main `4c4c37a5` and SrvrPorts main
`5e994f5`. Newly landed installation/profile/desktop and uACPI/libuv work does
not overlap this math/header or C++ port implementation. PR252 remains open at
`badfeb37`. These newer integration changes were inspected but not imported into
this tested SDK; the evidence is for the dirty implementation atop `ddb841d2`.
Integration must retain the other owners' changes and rebuild/retest the merged
SDK. No pthread, kernel, descriptor or resolver implementation changed in this
ICU checkpoint. All three protected review VMs were retained. Disk-pressure
pauses and cleanup removed only our regenerable source/build trees, retaining
pinned archives, failure logs and receipts; the user restored working space.

### Reproduce ICU and the advanced configure

From the matching OS and ports revisions, with the pinned WPE archive already
present as described below:

```sh
sh tools/macos/build.sh -j2 srvros-sysroot build/kernel/srvros.elf build/userspace/init.elf build/limine-binary/.ready build/userspace/cxxprobe.elf build/userspace/cxxstlprobe.elf
python3 ports/srvros/libcxx/build.py
python3 ports/srvros/libcxx/build_rtti.py
python3 tools/browser_math_probe.py
mkdir -p build/browser-icu
# First download only; subsequent runs reuse the verified archives.
gh release download release-79.1 --repo unicode-org/icu --dir build/browser-icu --pattern 'icu4c-79.1-sources.tgz' --pattern 'icu4c-79.1-data-bin-l.zip'
python3 ports/srvros/icu/build.py
python3 tools/browser_runtime_smoke.py --icu
python3 tools/browser_platform_probe.py --icu
```

The ICU recipe extracts a fresh source tree, generates upstream cross-build
configuration, builds with two jobs and a 1.5 GiB free-space reserve, and installs
under `build/browser-icu/sysroot`. Move an existing install aside before a fresh
rebuild; the recipe refuses to overwrite it. No host ICU can satisfy target
library/header detection. Math tests use the host libm solely as a comparison
oracle for privately renamed functions; guest math links the actual SrvrOS SDK.
The installed ICU license and every installed file are covered by the receipt.

Syntax, focused new links and whitespace checks pass. The global documentation
checker still reports the same nine existing missing vendor-source/license
links in uninitialized nested submodules; it is not a pass. No full engine
build, browser rendering, network browsing, physical installation or merge is
claimed. This checkpoint is for draft review, with compilation as the next
browser milestone.

## Second checkpoint — November 3, 2026

User authorized explicit SrvrOS platform support, a real threaded C++ runtime,
and advancing no-JIT JavaScriptCore configuration to its next actual dependency.
Main remains `a936baeb`; PR252/19/301/306 heads were freshly fetched and unchanged.
This continues draft PR310. The clean assigned checkout and all three protected
review VMs are retained. Free storage was approximately 16 GiB at the start of this checkpoint.

Own the new SrvrPorts `srvros/webkit` and `srvros/libcxx` recipes, dedicated
configuration/guest probes, and necessary C++ ABI guard/TLS adaptation in
`userspace/lib/src/cxx_runtime.cc`. Actual PR252 diffs do not touch that C++
runtime or pthread implementation. Consume existing pthread/process semantics;
do not change the kernel, pthread library, descriptors, resolver or ownership
limits. Any discovered requirement in those owned subsystems must be published
as a dependency rather than implemented independently. Other platform tracks
retain their namespace/profile/installation scope.

Acceptance: explicit `OS(SRVROS)` without Linux impersonation; actual upstream
libc++ thread/mutex/condition-variable/once code linked and exercised in a matched
disposable guest; thread-local destruction and static initialization correctness;
documented no-exceptions failure at worker exhaustion; executable (not archive)
CMake feature checks exposing the next true Threads/ICU dependency. Do not force
thread discovery success or add missing pthread cancellation/fork-handler stubs.
No full JSC/WebCore build, browsing, JIT, physical installation or merge is in
this checkpoint. Keep experiments below 3 GiB additional storage, two compile
jobs, and one build/VM harness in this checkout. The bounded implementation and guest acceptance are complete; publication
records and the configure result follow below. SrvrPorts commit
`4d47276917a9c8738c920cd21208b04fdcd24d02` is published in
[draft SrvrPorts PR4](https://github.com/PaulMDemers/SrvrPorts/pull/4); the OS
ABI, harnesses and matching gitlink continue in
[draft SrvrOS PR310](https://github.com/PaulMDemers/SrvrOS/pull/310).
Tests ran on the recorded dirty implementation atop `3863241d`; the input
hashes identify the tested source, not a retroactively claimed clean commit.

### Implemented and measured

The SrvrPorts overlay defines `OS(SRVROS)` and generic `OS(UNIX)` without
`OS(LINUX)`. Its driver links real SDK executables for CMake checks. C++ uses
hosted language semantics for ordinary `main` linkage and implicit return zero,
while retaining the freestanding SrvrOS target, explicit SDK headers/startup and
static libraries. This does not establish a complete hosted C++ environment.

The separate threaded libc++ archive uses pinned Zig 0.15.2 upstream thread,
mutex, condition-variable, once and supporting sources. OS changes are confined
to `userspace/lib/src/cxx_runtime.cc`: synchronized static-initialization guards,
per-thread LIFO TLS destructors, bounded DSO-filtered global destructor records,
and main-thread TLS-before-global teardown. Existing pthread operations remain
the backend. No Node threading shims or success-returning error stubs are used.

Matched SDK, kernel/init, libc++ recipe and existing C++ regression builds pass.
The diskless, networkless, single-CPU Q35/TCG guest receipt is
`build/browser-runtime/guest-1550di15/receipt.json`. It records all six passing
cases: existing `cxxprobe` and `cxxstlprobe`; TLS-first registration/teardown;
threaded synchronization, call-once, concurrent local-static construction and
TLS destruction; eight-worker exhaustion with real error 11 and expected abort
status 134; and a successful fresh normal run afterward. The last run proves
worker resources were released after the aborted process. The harness stopped
its own guest. Existing review VMs 61610, 6233 and 85869 remain untouched.

The no-JIT configure passes C/C++ executable validation, explicit platform
identity and a real `std::thread`/mutex link control. CMake's broad libc pthread
check fails on undeclared `pthread_cancel`; its upstream `-pthread` fallback
successfully compiles and links actual `pthread_create`/`pthread_join` against
the SDK, producing `Found Threads: TRUE`. Nothing pre-seeds this result.
Configuration then requires target **ICU >=70.1**, with `ICU_INCLUDE_DIR`,
`ICU_LIBRARY`, `data`, `i18n` and `uc` missing. Host ICU is not substituted.
Final configure receipt: `build/browser-platform/run-7ik21jgi/receipt.json`
(41.050 seconds, pinned archive extraction 478,809,532 bytes). The probe exits
zero for the recognized dependency stop; CMake itself exits one. This is
dependency discovery, not successful configuration or engine execution.
CMake still warns that its built-in platform catalog lacks SrvrOS; the explicit
toolchain and WebKit patch supply this probe's target settings.

The SDK/runtime receipt hashes sources, SDK archive/startup/linker script,
headers and resulting ELF/archive files. Both harnesses reject stale runtime
inputs. Guest kernel/init hashes identify the artifacts actually booted;
Git HEAD is labeled an observation, not an assertion that uncommitted changes
were absent. Raw logs/receipts remain local under `build/browser-runtime/` and
`build/browser-platform/`. Failed exploratory runs remain failed evidence:
monitor echo parsing, same-translation-unit TLS fixture initialization, ordinary
C++ main linkage, and an ICU diagnostic-format mismatch were corrected before
final acceptance. No old receipt is relabeled as a pass.

### Supported slice and owner handoff

This is a real, bounded threaded runtime, not the full C++ standard library or
ABI. Eight workers, 4096-byte ELF TLS templates and 128 global destructor records
remain limits. No exceptions/unwinder, dynamic unloading, recursive static
initialization recovery or initializer-thread-death recovery is supplied.
Concurrent finalizers may execute distinct callbacks concurrently; global LIFO
completion is only established for single-caller finalization. Aborts and forced
termination intentionally skip C++ teardown.

`notify_all_at_thread_exit` and promise/future readiness at thread exit are
excluded. The existing pthread code allocates keys and drains destructors in
ascending key order; libc++ can therefore notify before user C++ TLS destruction.
Supporting these APIs needs an agreed pthread teardown-order contract with the
shared runtime owner. This finding is a dependency handoff, not authorization to
change pthread lifecycle independently. PR252's process/I/O/socket/resolver
changes, PR19 Wi-Fi, PR301 display and PR306/SrvrPorts PR2 SQLite remain owned
interfaces. No competing kernel, pthread, descriptor or resolver implementation
was added. Existing C `atexit` registration is not a general concurrent teardown
facility.

The next browser milestone is a pinned SrvrOS ICU recipe and then the next
no-JIT JSC dependency/compile failure, with runtime requirements handed back to
their owners. No full engine build, browser rendering, network enablement,
physical installation or merge is accepted by this checkpoint.

### Reproduce the second checkpoint

From the matching OS branch and published ports gitlink:

```sh
git submodule update --init ports userspace/widgets userspace/apps
sh tools/macos/build.sh -j2 srvros-sysroot
python3 ports/srvros/libcxx/build.py
sh tools/macos/build.sh -j2 build/kernel/srvros.elf build/userspace/init.elf build/limine-binary/.ready build/userspace/cxxprobe.elf build/userspace/cxxstlprobe.elf
python3 tools/browser_runtime_smoke.py
python3 tools/browser_platform_probe.py
```

The final command requires the pinned archive from the first checkpoint at
`build/browser-probe/wpewebkit-2.54.2.tar.xz` (URL and digest below). The historical
`browser_probe.py --fetch` can acquire it but also runs the old expected-gap
checks; it is not the acceptance command for this updated runtime. CMake/Ninja,
QEMU and xorriso must be installed. Configure subprocesses have 60-second
bounds; extraction is capped at 1 GiB and requires 2 GiB free. The guest uses a
private serial socket and read-only boot ISO with no writable disk or network.
Run only one build/guest harness in this checkout at a time.

Python syntax, focused document links and diff-whitespace checks pass.
Negative controls reject missing/stale runtime artifacts and unsupported driver
target/shared/C++ language modes before compiler or guest invocation. The global `check-docs` gate was
rerun and still fails on the same nine existing missing vendor-source/license
links in intentionally uninitialized nested submodules. That gate remains a
review dependency for a fully initialized integration checkout.

## Historical first checkpoint — November 3, 2026

Target: select an engine from current primary-source requirements and deliver a
bounded, pinned-source configure/compile probe against the real SrvrOS SDK.
This checkpoint does not deliver a browser or accept arbitrary web content.

Base: `a936baeb4a20ff28304a13252bf20345dee13bf1`, freshly fetched main.
Branch: `device/apple-silicon/browser-probe`, in isolated `SrvrOS-browser`.
Own only this document and browser feasibility tooling. Do not edit the
integration roadmap, machine record, kernel or libc.

Inspected live heads: PR252 `badfeb37` owns process/I/O and socket/resolver
lifetime; PR19 `993e920a` owns Wi-Fi; PR301 `4c9fc184` owns native display
readout; PR306 `5028871b` and SrvrPorts PR2 own SQLite. Their implementations
are dependencies, not permission to duplicate shared runtime work. Filesystem,
profile, SrvrApps and SrvrUI contracts retain their independent owners.

Host preflight: ARM64 macOS, 32 GiB physical RAM, approximately 31 GiB free
storage. Three existing review QEMU processes (61610, 6233, 85869) remain live
and untouched. No physical install, host reboot or VM mutation is needed.

Experiment envelope: WPE WebKit 2.54.2 release archive, 49,565,492 bytes;
SHA256 `f3284e19e47fcb5ac8c63c2e1c113dbd65b2423eb8a545f5b345707829004f56`.
At most 2 GiB of source/probe artifacts, two compilation jobs, no full engine
build. Build only a matched SDK and bounded probes; preserve all external
checkouts and images. This envelope produced the evidence below; no engine runtime was built.

Status: first feasibility checkpoint published in [draft PR310](https://github.com/PaulMDemers/SrvrOS/pull/310),
implementation commit `84b25354`. Locally verified for the bounded draft scope. Next implementation gate is explicit platform/threaded-runtime support
and a no-JIT JSC configure reaching its next dependency. Actions stay disabled.

## Engine decision

Recommend **WebKit/WPE as the first feasibility track**, starting with static,
no-JIT JavaScriptCore and then an offline WebCore software-rendering slice.
This is a conditional engineering choice, not a claim that WPE currently runs
on SrvrOS. The advantage is a documented embedder/platform boundary and an
existing C/C++ SDK. An upstreamable SrvrOS port is preferable to a permanently
outdated fork. Stop and reconsider before a full engine build if the graphics
and runtime contracts cannot be made tractable.

| Candidate | Current upstream evidence | Consequence for SrvrOS |
| --- | --- | --- |
| WebKit/WPE 2.54.2 | Release archive and SHA256 are pinned below. `Source/cmake/OptionsCommon.cmake` requires C++23; `OptionsJSCOnly.cmake` requires threads and ICU >=70.1. WPE requires GLib >=2.70, HarfBuzz >=2.7.4 with ICU, FreeType >=2.9, libsoup3, libgcrypt, libxml2, SQLite, image libraries, Epoxy and xkbcommon. | Existing Zig/Clang and C++ work provide a starting point, but the current SDK is not a complete threaded libc++/WebKit platform. A JavaScript shell is a prerequisite experiment, not HTML rendering. |
| Chromium/Blink/V8 | Official Linux build instructions require at least 100 GB free disk, recommend >16 GB RAM, and support Clang/libc++; Linux instructions assume an x86-64 host. GN/depot_tools, large dependency trees, multiprocess IPC and platform sandbox backends are involved. | No full checkout/build on this 31 GiB-free host. A macOS host build would not establish a new SrvrOS target. Existing Node/V8 historical notes do not establish a current Chromium runtime. |
| Servo | Official tree uses Rust 2024, `rust-version = 1.88.0` and currently pins Rust 1.97.1; build docs use rustup, Cargo, Python/mach and native C/C++ dependencies. WebRender/graphics, fonts, SpiderMonkey and OS integration still matter. | Viable alternative to revisit, but requires an SrvrOS Rust standard-library target and runtime validation in addition to the C/C++ dependencies. No such matched SDK is supplied here. Rust alone does not remove VM, threads, IPC or graphics requirements. |

Primary sources checked November 3, 2026:
[WPE releases](https://wpewebkit.org/releases/),
[Chromium Linux requirements](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md),
[Servo build instructions](https://book.servo.org/building/linux.html),
[Servo workspace](https://github.com/servo/servo/blob/main/Cargo.toml), and
[Servo toolchain](https://github.com/servo/servo/blob/main/rust-toolchain.toml).
Moving branch references describe the inspected date; the implemented probe uses
an immutable archive digest, not these moving branches.

WPE 2.54 makes WPEPlatform stable/default and deprecates the legacy libwpe API;
new application work should follow WPEPlatform, not start a new Cog fork.
Its Skia compositor does **not** imply framebuffer-only integration is solved.
The current documented new-API transport is DMA-BUF, with separate web-process
rendering and UI-process presentation. SrvrOS has neither that Linux transport
nor a demonstrated equivalent. Software painting and transporting/presenting the
result are separate dependencies. See the
[2.54 announcement](https://wpewebkit.org/blog/2026-09-16-wpewebkit-2.54.html) and
[graphics architecture](https://docs.webkit.org/Ports/WebKitGTK%20and%20WPE%20WebKit/Graphics.html).
Chromium's [SwiftShader](https://chromium.googlesource.com/chromium/src/+/main/docs/gpu/swiftshader.md)
is similarly not a replacement for an OS port or sandbox; its WebGL fallback
has security caveats. No candidate's guest RAM, frame time or binary size is
measured by this milestone. The 1 GiB review VM configuration is not a proven
browser memory budget.

### Maintenance and licensing

WebKit is mixed BSD/LGPL by file; a static distribution needs a concrete
license inventory and fulfillment plan for applicable source/relink obligations,
including dependencies and fonts. Servo uses MPL-2.0 for its project code, with
separately licensed dependencies. Chromium also carries many third-party
licenses; its top-level license does not cover the whole distributed bundle.
These are engineering obligations to review for the chosen build, not a blanket
license conclusion. Keep upstream notices with fetched source and patches.
See [WebKit licensing](https://docs.webkit.org/Other/Licensing.html),
[Servo license](https://github.com/servo/servo/blob/main/LICENSE), and
[Chromium third-party policy](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md).

A port owner must track releases, security fixes, dependency updates and patch
rebases continuously. WebKit explicitly distinguishes maintained upstream ports
from downstream ports that can lag; see its
[port maintenance guidance](https://docs.webkit.org/Ports/Introduction.html).
This checkpoint does not promise that maintenance capacity or upstream acceptance
already exists. No independently maintained adapter exists yet, so creating
SrvrWeb is premature. Reusable upstream recipes/patches belong in SrvrPorts;
the eventual browser shell belongs in SrvrApps.

## SDK dependency map at the first-checkpoint base

Headers and a successful static link are weaker evidence than runtime semantics.
The table separates source findings from the focused tests below. Pending PRs
are not part of the tested SDK.

| Area | Actual SrvrOS evidence | Required milestone / owner |
| --- | --- | --- |
| Toolchain and C++ | Zig 0.15.2, Clang 20.1.2, freestanding x86_64, static ELF SDK. Existing `USER_STL_CXXFLAGS` are C++20 with a no-threads compatibility setting and Node shims. WebKit needs C++23 and a real threading configuration. | Explicit SrvrOS platform registration, threaded libc++ configuration and required ABI symbols. Publish port patches through SrvrPorts; coordinate SDK changes rather than inventing a parallel libc. |
| Threads, atomics and TLS | Real pthread mutexes/condition variables, ELF TLS and futexes exist. `kernel/src/process.c` caps spawned user threads at 8/process, futex entries at 64 globally and TLS image size at 4096 bytes. Public pthread keys are capped at 128. Ordinary scheduling is CPU0. | Measure engine worker/TLS demand, atomics, parking, termination and exhausted-resource behavior in a matched guest. No SMP or unlimited capacity claim. |
| VM and JIT | Private anonymous/file mappings and page protections exist. `posix_mman.c` rejects MAP_SHARED, strips MAP_NORESERVE and accepts discard advice without performing it. Kernel MAP_FIXED requires an unused range. File mappings are private snapshots. | Test reservation/commit/guard replacement/decommit contracts against JSC allocator expectations. Keep JIT and WebAssembly disabled initially; executable mappings alone do not validate JIT or W^X policy. Kernel/VM corrections need explicit shared ownership. |
| Processes and IPC | Native spawn, pipes, socketpairs and SCM_RIGHTS exist, with four transferred FDs per message. Fork refuses live workers; pseudo-socket/adapter inheritance is restricted. `dlopen`/`dlsym` explicitly fail. | Static initial build, explicit process launcher and audited stream/FD transfer semantics. PR252 owns lifetime and rights/payload corrections; consume its reviewed integration, do not duplicate it. |
| TLS, DNS and certificates | Selected IPv4 TCP/UDP and resolver support exist; no established browser TLS/certificate/trust-store stack. `tlsprobe` means thread-local storage, not TLS networking. Kernel entropy can fall back to timestamp/PID-derived SplitMix64 if hardware/virtio entropy is unavailable. | Stay offline. Before HTTPS, require a vetted TLS provider, certificate/hostname/clock checks, trust-store updates and a cryptographic entropy contract that fails closed. Resolver/socket changes remain PR252-owned. Wi-Fi stays PR19-owned. |
| Unicode and fonts | SrvrUI's bitmap atlas and density support do not provide browser Unicode layout, shaping, bidi or general font selection. WPE's ICU/HarfBuzz/FreeType dependency graph is absent from the matched SDK. | Port pinned text libraries and fonts via SrvrPorts; validate mixed scripts, combining marks, fallback and text metrics before claiming text rendering. |
| Storage | exFAT and libc support bounded regular-file operations. Pending SrvrPorts PR2 is a single-threaded SQLite VFS, one persistent connection, with WAL and ATTACH disabled; clean reopen is not crash durability. | Begin with memory-only offline documents. Browser profile/storage design consumes filesystem/profile contracts; assess engine SQLite concurrency and transaction requirements with PR306/ports owner. |
| Graphics and input | SrvrOS copies pixel rows into owned GUI surfaces; no WPE DMA-BUF/EGL/GBM backend. SrvrUI exposes logical coordinates and 1x/2x backing density, not a browser renderer. | Start with an offscreen software buffer and explicit copy/presentation lifecycle. Consume existing SrvrUI input/density and SrvrOS surface contracts, preserve stride/clipping/budgets, and test resize, input cancellation and scale changes. PR301 display readout is not an engine backend. |
| Sandboxing | Separate address spaces exist; the architecture does not claim production isolation. Linux seccomp/namespaces and browser-specific service restrictions are not supplied by header compatibility. | Offline, trusted fixtures only. A multiprocess or no-JIT prototype is not safe for arbitrary browsing. Network enablement requires independent isolation and hostile-content review. |

Local sources: [supported POSIX profile](posix-supported-profile.md),
[architecture](architecture.md), [ports/VM contracts](ports.md),
[application runtime](desktop-application-runtime.md),
[SrvrUI density contract](../userspace/widgets/docs/render-density.md),
[libc mapping adapter](../userspace/lib/src/posix_mman.c),
[dynamic-loading adapter](../userspace/lib/src/posix_dlfcn.c),
[kernel process implementation](../kernel/src/process.c), and
[kernel entropy implementation](../kernel/src/arch/x86_64/syscall.c).
The pending SQLite contract was inspected directly at
[SrvrPorts 8cbf17b](https://github.com/PaulMDemers/SrvrPorts/blob/8cbf17b68b711ab8cf59ee2e5b98657ec9b8968c/srvros/sqlite/README.md).

## Ordered implementation gates

The bounded platform/threaded-runtime and ICU checkpoints are accepted above;
no-JIT configuration now completes. JavaScriptCore compilation and the broader
runtime/browser gates remain open.

1. **SDK/platform recognition and threaded C++ runtime.** Add an explicit SrvrOS
   cross-build description in a SrvrPorts patch series after dependency review.
   Preserve target/host separation; do not define Linux merely to pass checks.
   Require real C++23 atomics, mutex/condition-variable, TLS and thread teardown
   compile/link/guest tests, including exhaustion. The immediate next experiment
   is a pinned `JSCOnly` configure that gets past OS recognition and reports its
   next actual dependency. ICU must be built for SrvrOS, not accidentally found
   in Homebrew.
2. **No-JIT static JavaScriptCore.** Port the minimal ICU/WTF/runtime dependencies;
   use the generic event loop and system allocator where upstream permits.
   Explicitly validate reservation, guard-page and discard behavior, failure
   paths and thread budgets. Run deterministic arithmetic/string/GC fixtures in
   a disposable matched guest, record heap/peak memory and time, and terminate
   cleanly. Disable JIT, WebAssembly, remote inspection and networking. A passing
   shell still does not demonstrate WebCore or a browser.
3. **Offline HTML/CSS/text to software pixels.** Bring in WebCore plus the pinned
   font/shaping/image subset. Define a supported software-buffer platform path;
   the current WPE DMA-BUF default cannot be assumed usable. Render local fixed
   fixtures at a fixed viewport: block/inline layout, CSS colors, line wrapping,
   combining marks, bidi and font fallback. Check images/text geometry and
   memory limits. Record unsupported features explicitly. If the platform API
   cannot transport these buffers without large divergence, revisit the engine
   choice at this gate.
4. **SrvrApps browser shell and lifecycle.** Use SrvrUI controls, logical input
   coordinates and physical-density backing with SrvrOS application/surface
   lifetimes. Validate scroll, focus, keyboard/pointer routing, resize, 1x/2x,
   cancellation, close/reopen and bounded resource retirement using local files.
   No new competing widget, event-loop or kernel process implementation.
5. **Persistence and service processes.** Consume the filesystem/profile contract
   and reviewed PR252 semantics; determine SQLite multi-connection/WAL needs
   rather than assuming the pending CLI VFS is sufficient. Test crash/error
   recovery, storage quotas, IPC data/rights association and worker exit. Keep
   runtime caches separate from portable settings and exclude secrets from test
   fixtures and source control.
6. **Network and hostile-content readiness.** Only after TLS/trust/entropy/DNS
   and real isolation contracts exist, test controlled HTTPS endpoints, bad
   certificates, redirects, cancellation and parser/resource attacks. Define a
   security-update/release owner and maintenance policy. Arbitrary browsing and
   JIT remain separate acceptance decisions, not consequences of a screenshot.

There are no cross-chat messages or competing shared implementations in this
checkpoint. Ownership conflicts should be recorded with exact files and PRs;
unblocked probe work can continue. No new repository, submodule or hosted CI
was created for the feasibility slice.

## Historical first-checkpoint reproduction

From this isolated checkout on the documented macOS host (CMake/Ninja and
Rosetta are also required for the configure and x86_64 host fixture):

```sh
git submodule update --init ports userspace/widgets userspace/apps
sh tools/macos/build.sh -j2 srvros-sysroot
python3 tools/browser_probe.py --fetch
# Later runs reuse the verified archive without network access:
python3 tools/browser_probe.py
```

`--fetch` downloads only the pinned WPE release. The tool checks size and SHA256
before extraction, requires 2 GiB free space, caps extracted regular data at
1 GiB, excludes links/special files from materialization and uses a temporary
source directory removed after the run. It preserves commands, diagnostics,
input/binary hashes and a JSON receipt in unique ignored
`build/browser-probe/run-*` directories. It builds no full engine or disk image,
starts no VM and does not modify upstream or SDK sources. Compiler/configure
subprocess groups have a 60-second deadline. The initial manual source
inspection occupied about 583 MiB; the SDK/toolchain occupied about 401 MiB.
The temporary extraction adds about 457 MiB while the probe is running.

Exit zero means the positive controls and **expected gap signatures** matched.
It does not mean engine configuration succeeded. A changed diagnostic or failed
control exits nonzero for investigation; do not weaken it merely to regain zero.
CMake uses a real `SrvrOS` system name, the freestanding target and SDK-only
library/header searches. Its compiler checks use static archives, so they do not
prove a JavaScriptCore executable link. On macOS, upstream's early Xcode SDK
lookup still appears in the log; it is not the target SDK. No Linux platform
macro, Node shim, fake mutex or success-returning runtime stub is added.

The host memory fixture compiles the actual libc mapping adapter with private
symbol names and an always-failing modeled kernel boundary. It establishes
adapter rejection/flag forwarding and unchanged local bytes after advice;
it does not simulate working kernel mappings or prove guest memory protection.

## Historical first-checkpoint acceptance and limits

SDK build: `sh tools/macos/build.sh -j2 srvros-sysroot` passed on base
`a936baeb4a20ff28304a13252bf20345dee13bf1`, Zig 0.15.2. The source SDK
and pinned first-party submodules were built in this isolated checkout.
Final first-download run `run-n3i5r6nq` took 18.514 seconds, including download,
archive checks, extraction, configure and probes. Extracted regular data was
478,809,532 bytes. The unrelated Skia fontations Cargo.toml symlink was not
materialized and is named in the receipt. These are this host's observations,
not engine build-time or memory benchmarks.

| Probe | Observed result | What it establishes |
| --- | --- | --- |
| Minimal C program | Compile and static x86_64 ELF link pass | SDK startup/archive/linker script can form an executable; it was not run in a guest. |
| mmap/socketpair/pthread mutex calls | Compile and static ELF link pass | Real symbols resolve from the matched SDK, not proof of complete POSIX behavior. |
| C++23 builtin atomic | Compile and static ELF link pass; 32-bit atomic asserted lock-free | Language/basic target-code support, not contended atomics or a C++ runtime qualification. |
| `std::atomic<int>` | Compile passes | This header/operation is usable with the probe configuration; no full STL or thread claim. |
| Upstream `wtf/Platform.h` | Compiles; explicit `OS(UNIX)` assertion fails | The headers are reachable, but freestanding SrvrOS is not recognized as a Unix target. |
| `std::mutex` | Compiler reports `no type named 'mutex'` | Current libc++ configuration lacks the needed threaded surface. No Node mutex shims were included. |
| Upstream `wtf/Threading.h`, system allocator selected | Missing `unicode/utypes.h`; also an `mbstate_t` platform diagnostic | ICU and libc++ platform adaptation are real next prerequisites. Correct header ordering removes the initial experimental include-order error. |
| Static no-JIT `JSCOnly` configure | Fails with `Unknown OS 'SrvrOS'` | Upstream lacks the target definition. ICU discovery and engine compilation were not reached. |
| x86_64 macOS host libc fixture | Passes all three observed-gap checks | MAP_SHARED rejects before any syscall; MADV_DONTNEED preserves bytes; MAP_NORESERVE is stripped before the modeled failing syscall. |

Final probe-script SHA256:
`03b6f0acdcecd4a04c034f3e9523c5ecdb6a7e6165300c43be428f7a9e81c39b`.
The receipt records the uncommitted source state, script/fixture/SDK/header hashes,
full command arrays and diagnostics; publication does not retroactively claim
that a different Git commit was built. ELF hashes are per-run receipts and may
vary with debug paths. Raw evidence and the SDK build log remain in ignored
`build/browser-probe/` storage. The corrected explicit-download path and a
separate cached rerun (`run-us2tia5n`, 15.214 seconds) are verified;
the earlier failed download receipt is kept. A corrupt-archive negative control
exited 2 before any compiler invocation.

`python3 -m py_compile tools/browser_probe.py` and focused browser-document links
pass. `make check-docs` was run via the macOS wrapper and reports nine missing
existing third-party license/source links because nested upstream submodules
(zlib, Lua, cJSON, inih, linenoise, libuv and Node) were deliberately not fetched.
No missing link points into this new document; the global docs gate is **not a
pass** in this bounded checkout. It should be rerun in a fully initialized
integration checkout during review. No full OS build, VM boot, engine link,
HTML/CSS rendering, guest network browsing or physical-device acceptance is claimed.

The first checkpoint is complete for draft review: a conditional engine choice,
repeatable positive controls and exact early failures, tested libc semantic gaps,
source/resource bounds and owned next milestones. Modern browsing remains
unimplemented. Existing review VMs/disks and the integration checkout are
unchanged; no new VM is left running.
