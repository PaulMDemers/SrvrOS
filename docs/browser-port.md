# SrvrOS no-JIT JavaScriptCore checkpoint

The current target is real threaded signal/register-context support followed by
a minimal JavaScriptCore execution with exit0 and output42. That target is not
complete. Public INFO registration, atomic temporary-mask waiting and libc adapters have
a guest-gated source implementation under target/owner qualification. Normal
public dispatch remains disabled. The bounded WTF handshake overlay compiles,
but its first actual-WTF guest fails; conservative-root and JavaScript tests
remain unrun.
The [public packet](signal-public-v1.md) and [logical I/O supplement](signal-logical-io-v1.md)
record the reviewed contract and explicit experimental qualification boundary.

The private signal path has owner-reviewed source, host and native guest evidence,
including the [RESET peer correction](signal-publication-transaction.md).
Separate normal-kernel fixes cover pending defaults across exec, process-wide
action changes and GOT-before-TLS layout. These results do not establish a new
SDK/ICU/JSC build or JavaScript execution. The retained browser-checkout JSC smoke
still exits134 without42 because INFO/context support is unavailable.

## Imported dependency source

The selected recipes pin ICU79.1, WPEWebKit2.54.2 and Zig0.15.2. The source carry
starts at SrvrOS main's ports pin `d760e5413edc8badd5c87d9b02b6c815f7169721` and
adds55 libc++/ICU/libm/WebKit overlay blobs from
`af466ba6888ad29904d1586bf574702ef53b9bed`, preserving all other base tree entries
apart from a narrow README addition. See [the source integration draft](https://github.com/PaulMDemers/SrvrPorts/pull/9).
The matching OS carry includes threaded C++ lifetime/guards, real hyperbolic float
math, C++ linkage declarations and the original build/proof fixtures.

Acceptance statements and ignored run names in the imported libc++/ICU port
manifests describe historical browser-checkout evidence. Their logs are not a
fresh PR334 SDK acceptance. The earlier browser work is retained at
[source185db570](https://github.com/PaulMDemers/SrvrOS/blob/185db570/docs/browser-port.md).
Licenses, pinned source digests and original patch bytes are preserved.

## Required matched verification

After the public contract is approved and implemented, rebuild the SDK and the
threaded libc++, RTTI, math, filesystem and ICU dependencies against the exact
same headers/archive/linker script. Regenerate receipts and run the current
normal kernel with the runtime, filesystem, TLS and public signal/context probes.
Then rebuild the pinned no-JIT JSC/WTF fixtures, qualify bounded suspension and
the real conservative register/stack view, and run the minimal JavaScript smoke.
A static link, old receipt or private signal hook cannot substitute for that run.

Heavy output uses the task-owned external APFS volume. Preserve existing images,
VMs and checkouts; check host, output and sparse-image backing reserves and keep
temporary files/caches on the output volume. Maximum concurrency is two compiler
jobs or one private VM. No physical boot or hosted CI is authorized by this work.


The experimental build flag is `--experimental-signal-context` on a fresh
`browser_jsc_build.py` run. It records/applies the separate WTF overlay after the
existing patches and does not enable kernel production capabilities. The first
matched guest profile permits only console, rights-free simple pipes, bounded
zero-FD waits and threading. Unqualified storage/socket paths fail before effects.
A JavaScript failure that requires another path is evidence for the next bounded
qualification; it must not be bypassed by broadening the gate without review.

Current checkpoint: the owner independently accepted the bounded native context
and nested-wait functional probe after source, linked-code, stack and exact guest
configuration review. The corrected image exits0 with both required markers;
boot serial proves no network device/worker before the payload, and the owned
guest stops. This does not establish general public ABI, SMP or only-root GC
acceptance. Fresh matched SDK, threaded libc++, RTTI, math, ICU, filesystem and
runtime-extension builds and normal guest checks pass on the task APFS volume.
The experimental-overlay no-JIT JSC and actual-WTF, utility and GC-root fixtures
now compile and link. The owner reviewed the exact prepared seven-case payload
and cleared its bounded guest experiment. Its first WTF case exits134 after only
the main-register/red-zone marker; the remaining six cases are not run. An
unchanged-ELF diagnostic reproduces134 without locating the abort. Original
failure and diagnostic evidence are preserved, and both owned guests stopped.

The owner independently reproduced a Running-to-Idle TLS-retirement race. The
new `signal-retirement.patch` retains the worker's TLS/group reference until its
controller finishes the matching acknowledgement and publishes Idle. Resume
updates its suspend count before that final publication. The actual destructor
and resume method pass35 deterministic cases in strict and ASan/UBSan builds;
the original source rejects both valid posted/consumed acknowledgement schedules.
The INFO handler is unchanged. This source correction awaits owner review and
matched target/guest verification; it does not identify the original abort or
establish only-root GC survival or JavaScript output42.
