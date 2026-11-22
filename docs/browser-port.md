# SrvrOS no-JIT JavaScriptCore checkpoint

The user resumed a WebCore dependency/configuration and software-backend design
audit on November 5. See [the audit](browser-webcore-audit.md) for pinned source
findings, runtime prerequisites and the proposed next slice. It adds no WebCore
compile/render or production-runtime acceptance.

The bounded experimental target is complete: real threaded signal/register
contexts, WTF suspension/retirement and memory-footprint checks, all four GC-root
modes, and actual `jsc -e 'print(42)'` pass with exit0. The initialization abort
was WTF opening unavailable `/dev/urandom`; a strict native entropy adapter fixes
it. Normal public signal dispatch remains disabled. These results apply to the
reviewed diskless single-CPU Q35 guests, with RDRAND exposed for JSC, and do not
establish general public ABI, SMP, physical hardware or browser acceptance.
Latest paired kernel qualification is at `a7f71411` / Ports `af1421b`, with
all retained SDK/JSC inputs verified byte-identical to the fresh `0a3f5981`
build. See the [integrated portable evidence](assets/evidence/pr334-integrated-smoke/README.md).
The [public packet](signal-public-v1.md) and [logical I/O supplement](signal-logical-io-v1.md)
record the reviewed contract and explicit experimental qualification boundary.

The private signal path has owner-reviewed source, host and native guest evidence,
including the [RESET peer correction](signal-publication-transaction.md).
Separate normal-kernel fixes cover pending defaults across exec, process-wide
action changes and GOT-before-TLS layout. Those historical results did not establish a new SDK/ICU/JSC build or JavaScript
execution. The older browser-checkout smoke's134 remains preserved; the fresh
matched experimental qualification below now passes.

## Imported dependency source

The selected recipes pin ICU79.1, WPEWebKit2.54.2 and Zig0.15.2. The source carry
starts at SrvrOS main's ports pin `d760e5413edc8badd5c87d9b02b6c815f7169721` and
adds55 libc++/ICU/libm/WebKit overlay blobs from
`af466ba6888ad29904d1586bf574702ef53b9bed`, preserving all other base tree entries
apart from a narrow README addition. See [the source integration PR](https://github.com/PaulMDemers/SrvrPorts/pull/9).
The matching OS carry includes threaded C++ lifetime/guards, real hyperbolic float
math, C++ linkage declarations and the original build/proof fixtures.

Acceptance statements and ignored run names in the imported libc++/ICU port
manifests describe historical browser-checkout evidence. Their logs are not a
fresh PR334 SDK acceptance. The earlier browser work is retained at
[source185db570](https://github.com/PaulMDemers/SrvrOS/blob/185db570/docs/browser-port.md).
Licenses, pinned source digests and original patch bytes are preserved.

## Matched verification workflow

For each changed candidate, build the SDK and threaded libc++, RTTI, math,
filesystem and ICU dependencies against matched headers/archive/linker script.
Retained artifacts require exact verification of their complete build inputs and
outputs with the original tested source attribution. Regenerate affected receipts
and run the normal kernel with runtime, filesystem, TLS and signal-boundary probes.
Then rebuild the pinned no-JIT JSC/WTF fixtures, qualify bounded suspension and
the real conservative register/stack view, and run the minimal JavaScript smoke.
A static link, old receipt or private signal hook cannot substitute for that run.

Record task-owned output and backing storage for each run. Preserve existing
images, VMs and checkouts while they are required; check host, output, temporary
and sparse-image backing reserves before and throughout heavy work. Maximum concurrency is two compiler
jobs or one private VM. No physical boot or hosted CI is authorized by this work.


The experimental build flag is `--experimental-signal-context` on a fresh
`browser_jsc_build.py` run. It records/applies the separate WTF overlay after the
existing patches and does not enable kernel production capabilities. The first
matched guest profile permits only console, rights-free simple pipes, bounded
zero-FD waits and threading. Unqualified storage/socket paths fail before effects.
A JavaScript failure that requires another path is evidence for the next bounded
qualification; it must not be bypassed by broadening the gate without review.

Historical public-context checkpoint: the owner independently accepted the bounded native context
and nested-wait functional probe after source, linked-code, stack and exact guest
configuration review. The corrected image exits0 with both required markers;
boot serial proves no network device/worker before the payload, and the owned
guest stops. This does not establish general public ABI, SMP or only-root GC
acceptance. Fresh matched SDK, threaded libc++, RTTI, math, ICU, filesystem and
runtime-extension builds and normal guest checks pass on the task APFS volume.
The experimental-overlay no-JIT JSC and actual-WTF, utility and GC-root fixtures
compile and link. The original seven-case experiment stopped at WTF134 after
only the main-context marker. An unchanged-ELF diagnostic also reproduced134
without locating the abort. Both failures and their stopped-guest evidence remain
preserved.

The owner independently reproduced a Running-to-Idle TLS-retirement race. The
new `signal-retirement.patch` retains the worker's TLS/group reference until its
controller finishes the matching acknowledgement and publishes Idle. Resume
updates its suspend count before that final publication. The actual destructor
and resume method pass35 deterministic cases in strict and ASan/UBSan builds;
the original source rejects both valid posted/consumed acknowledgement schedules.
The INFO handler is unchanged. The owner independently accepted the source
correction. The user transferred the remaining binary/configuration review to
this chat after the owner's chat became unavailable. Exact input, archive-delta,
544-function/1,949-relocation binding and packed-ISO checks pass.

The corrected prepared suite then passes all four WTF markers with exit0 and the
utility/memory-footprint case with exit0. GC control exits134 after three native
page-quota messages at65,536 mappings. Those messages may be the structure-heap
allocator's ordinary shrinking fallback and do not establish the final abort's
cause. The suite stops there; GP/FX/red-zone GC modes and JavaScript are NOT RUN.
The owned guest stops and its resource window is released. A bounded C-API
initialization diagnostic is prepared for a separately coordinated build/review/
guest window. No kernel quota, mapping contract, I/O gate or GC bypass was changed.

## Strict entropy and successful no-JIT smoke — November 4, 2026

The initialization diagnostic identifies `crashUnableToOpenURandom` during
`JSContextGroupCreate`. The 128 MiB structure-heap fallback succeeds first; quota
messages alone were not the abort. Shared review assigns the bounded entropy
prerequisite to PR334. Existing SYS_RANDOM92 gains the named provider-only mode
in [the native contract](random-entropy.md); flags0 and POSIX behavior remain
unchanged. WTF fails closed on refusal/nonprogress and accepts positive short
reads. No VFS, quota, mapping, worker or device gate is widened.

The generated WTF ABI copy is byte-identical to the authoritative native header;
the builder checks parity before/after each build and binds the retained SDK's
transitive syscall header. The retained SDK/libc++/ICU artifacts stay unchanged.
A C-linkage companion fixes the observed mangled-native-symbol link failure;
actual SDK native headers now replace the host test's synthetic declaration.
Actual native/WTF strict and ASan/UBSan tests pass, target constants compile, both
kernels and no-JIT JSC/four fixtures build and link. Earlier failures stay frozen.

Exact review passes757 frozen/current identities,839 retained function bindings,
3,460 named relocation targets, the expected RandomDevice/cache-ID archive delta,
all four ISO/CPU configurations and packed ELFs. The coordinator independently
accepts source/configuration scope; this chat performs the human-transferred
binary signoff. Four separately indexed guests pass in27.864s total:

- Normal kernel/plain qemu64: unavailable provider returns-11 without mutation;
  legacy flags0 still succeeds.
- Normal kernel/qemu64,+rdrand: CPUID,257 provider bytes and bounds checks pass.
- Experimental kernel/+rdrand: native GP/FX/x87/XMM/DF/TLS/SP/red-zone restoration,
  public-context destruction and nested temporary-mask wait regressions pass.
- Experimental kernel/+rdrand: entropy, actual WTF, utility/memory footprint,
  GC control/GP/FX/red-zone and `jsc -e 'print(42)'` all exit0. GC control collects;
  each selected root retains the object until its target retires, then collects.
  JavaScript prints42 followed by exit0 and a new shell prompt.

All guests prove no network device/worker, stop, and leave empty owned process
groups. The resource reservation is released. Page-quota fallback diagnostics
remain observed during passing GC/JavaScript runs; quota behavior is unchanged.

Tested source6b5ebaf7/Ports d99565f; JSC SHA256
`d7caf7ddc2d144c8ed71618fde76fee73d0b4cff5eb92034c85a5b5de63b25a2`.
The [portable initial smoke bundle](assets/evidence/pr334-initial-smoke/README.md)
preserves the reviewed synthetic logs and original/published hashes.
Durable local result receipt `.local/pr334-entropy-guest-results/receipt.json`
SHA256 `ce0ce2147666002e4deacb1b13b9a5e6a0f8183da54f13a2107b528ebe8aa6be`;
exact input packet74470aae and signoff6acb68dc are retained outside build output.
At that historical checkpoint PR334/Ports9 remained drafts and current-main
reconciliation was still required. The integrated qualification below supersedes
that pending status. This historical result makes no production capability,
main-merge, physical-machine, full-browser or physical entropy-quality claim.


## Fresh current-main qualification — November 4, 2026

OS `0a3f5981` / Ports `af1421b` reconciles actual OS main `d10a4e10` and
Ports main `72f3d6fa`. Source review combines session-scope accounting and file
stream168 with isolated signal164–167. The experimental gate refuses session163
and stream168 at the outer dispatcher before namespace admission, IRQ changes,
wait bookkeeping or backend effects. Normal builds retain those APIs. Actual
dispatcher strict/ASan/UBSan cases and focused process/thread/signal/output/
lifetime/stream/scope-service/loader regressions pass. Two earlier host-fixture
integration failures remain recorded; their test seams were corrected without
changing production behavior or weakening scope/trap assertions.

The SDK and both kernels were rebuilt, then threaded libc++, RTTI, math, ICU79.1
and filesystem dependencies were rebuilt against those exact SDK bytes. The
fresh SDK exports the native entropy header. Normal guests pass17 runtime cases
(15 exit0 and two intentional134 controls) and two filesystem cases, with an
empty probe directory afterwards. Fresh JSC and four fixtures then compile/link
with JIT, WebAssembly, remote inspector and sampling profiler disabled. No
incremental resume or retained-SDK header transition is used in this fresh run.

Exact review binds1,136 current identities,1,488 retained functions and5,633
named relocation targets with zero mismatches or unbound targets. It recomputes
the patched-source fingerprint and build identity and verifies dependencies,
four ISO/configuration records and packed ELFs. Integration independently
accepts the source/configuration/output review; the browser chat owns the
user-transferred binary signoff.

Four sequential guests pass in26.702s: normal strict-entropy absent/positive
controls, experimental native context/nested waits, then all eight entropy/WTF/
utility-footprint/GC-control/GP/FX/red-zone/JavaScript cases. Every case exits0;
actual `jsc -e 'print(42)'` prints42 and returns to the monitor. Every guest stops
and every owned process group retires. The quota fallback diagnostics remain
visible; no quota, VFS, device or I/O gate was widened.

The fresh JSC ELF SHA256 is
`08dd2679eee6bac5ce701877005cd63e33bff5b28a5a3bf75e709f94f7ddf5a9`;
its patched-source fingerprint is
`35085fbc78b8cb816d59fc666a25edcd84d5349c8f259d761842a1b52ac051f4`.
The [integrated portable evidence](assets/evidence/pr334-integrated-smoke/README.md)
records exact SDK/kernels/fixtures/configurations and original/published hashes,
separately from the historical initial smoke. Normal production signal
capabilities remain disabled. General ABI, SMP, physical hardware/entropy and
full-browser acceptance remain unclaimed.


## Latest-main kernel qualification — November 4, 2026

The later actual-main update `f11ce344` adds read-only cache diagnostics, an
AP-probe-only boot branch and exFAT conditional line-layout fixes. It changes
no shared/userspace SDK, signal/process/scheduler or port implementation. The
conflict-free merge and evidence record produce clean `a7f71411` / Ports
`af1421b`. The new cache/context tests, actual dispatcher strict/ASan/UBSan
checks, focused lifecycle/stream/scope/loader checks and both kernel builds pass.

An exact audit verifies715 SDK, dependency, JSC and fixture input/output
identities, including the original patched-source fingerprint, with no changed
bytes. The fresh JSC build at0a3f5981 is retained with its original attribution.
New normal and experimental kernels are respectively SHA256
`da470d92d4ae3921405f6fb14c40c39a76efc47213f994552d8b89328e3ccee2`
and `796fe4092b514c0066b178315d7d5dddddf4b23bd81ba30bd83a0254a368964d`.

Four newly prepared and exact-reviewed guests pass in28.022s on those kernels:
normal absent/positive entropy, native contexts/nested waits, and all eight
actual WTF/footprint/GC/JavaScript cases. The retained JSC prints42 and exits0.
All owned guests stop and groups retire. Exact review again binds1,136
identities,1,488 retained functions and5,633 named relocation targets with zero
mismatches/unbound. Original A–E records remain separate from this later-kernel
qualification in the portable evidence collection. Production signal capability
and quota behavior are unchanged; no physical/SMP/full-browser claim is added.

Integration delivery is tracked by [Ports9](https://github.com/PaulMDemers/SrvrPorts/pull/9)
and [OS334](https://github.com/PaulMDemers/SrvrOS/pull/334), in that order.
The user-authorized final workspace reset removes the local build/VM objects;
the portable repository bundles retain the selected synthetic logs, receipts,
original hashes and explicit limitations.


## Normal desktop/session verification

At clean a7f71411/af1421b the matched normal build, desktop shutdown and
session guests pass in104.210s. All four shutdown choices, five expected file
payloads, pre/post generator/allocation checks and restored process baseline
pass. The matched/generated fixture has no independent fsck claim. Session
checks include scopeprobe twice, live init, lifetime, PTY and syscall ABI. All
guests stop and groups retire. Host setup failures (uninitialized submodules,
compiler argv, ISO symlink, missing Pillow) remain failed and separately saved.
The successful host test uses disposable Python3.14.6/Pillow13.0.0. Portable
results are in the integrated smoke bundle; final docs/pin changes retain the
runtime attribution above. No physical or broader browser acceptance is added.
