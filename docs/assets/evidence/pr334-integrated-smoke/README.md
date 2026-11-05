# PR334 fresh integrated smoke evidence

This collection records fresh qualification at OS
`0a3f5981cd23b8982c5a8656af55875b0aae4a40` and Ports
`af1421b735b61cf47c4e6516730c30468020abc7`, incorporating OS main
`d10a4e10` and Ports main `72f3d6fa`. It is separate from the historical
[initial smoke](../pr334-initial-smoke/README.md).

The reconciled normal SDK, normal/experimental kernels, threaded libc++, RTTI,
math, ICU79.1 and filesystem dependencies were rebuilt together. Normal runtime
guests pass17 cases, including two deliberate status134 controls; the private
filesystem guest passes two cases and leaves its probe directory empty. JSC
and four fixtures then build afresh with JIT, WebAssembly, remote inspector and
sampling profiler disabled. No resume or legacy-SDK header transition was used.

Two earlier host-fixture integration attempts remain failed. Their simulated
interrupt-state, loader signature/publication and trap-retirement seams were
corrected before the successful focused lifecycle/stream/scope and kernel builds.
The corrected assertions preserve the existing scope reservation/revocation,
exit accounting, stale-owner and loader rollback checks.

Exact review matches1,136 current identities,1,488 retained functions and5,633
named relocation targets with zero mismatches or unbound targets. It verifies
the fresh patched-source fingerprint, build identity, SDK/dependency provenance,
four ISO configurations and packed ELF payloads. This chat owns the explicitly
transferred binary signoff; integration independently reviewed source,
configuration and output hashes.

Four sequential guests pass in26.702 seconds:

- Normal/plain qemu64: strict entropy is unavailable without mutating the
  buffer; legacy flags0 remains successful.
- Normal/qemu64,+rdrand: provider bytes, bounds and CPUID checks pass.
- Experimental/+rdrand: native context restoration and nested suspend/resume
  wait behavior pass.
- Experimental/+rdrand: entropy, actual WTF suspension, utility/memory footprint,
  GC control, GP/FX/red-zone root retention and `jsc -e 'print(42)'` all exit0.
  The JavaScript case prints42 followed by exit0 and a new monitor prompt.

All four are diskless Q35/single TCG/1CPU/1536MiB/noNIC/noDebug, without a
deterministic seed, replay or added entropy device. Each guest stops and all
owned process groups retire. The separate filesystem guest uses a new private
128MiB exFAT/AHCI disk; the normal runtime guest is diskless. Page-quota fallback
messages remain visible in successful GC/JavaScript runs; quotas were unchanged.

`manifest.json` binds original and published bytes. Local workspace/home/backing
paths are replaced by named tokens in copied records. The explicitly named
review and input summaries omit the large input/function maps and identify their complete
originals by hash. Synthetic serial bytes are unchanged; Git binary attributes
preserve captured carriage returns and terminal control bytes. Internal hashes
refer to original records, so use the outer manifest for published files.
These saved records are evidence, not runnable build or launch helpers.

Normal production signal capabilities remain disabled. These checks do not
accept a general signal ABI, SMP, native hardware, physical entropy quality or
a full browser. Ordinary desktop/session verification is recorded separately
by the integration coordinator before merge.

## Later kernel qualification

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


## Normal desktop/session integration gate

The matched normal build and both guests pass at clean a7f71411/af1421b in
104.210s. Desktop shutdown exercises Cancel-exit, Cancel-save-picker, Save-and-
exit and Discard-existing-prompt-and-exit; all five expected file payloads,
pre/post generator and allocation checks and restored process baseline pass.
The fixture is matched/generated; independent fsck was not run for this fixture.
Session checks pass scopeprobe twice, live system init, lifetimetest, PTY and
syscall ABI probes. All owned processes/groups retire. The representative
[shutdown prompt](sanitized/normal-os/shutdown-prompt.png) is synthetic guest UI.

Four setup attempts remain failed in the collection: uninitialized nested
submodules (no child launched), a split host-compiler argument, a preexisting
ISO output symlink, and missing host Pillow after reaching desktop dialogs.
The final run uses a disposable Python3.14.6/Pillow13.0.0 environment; these
corrections did not change runtime source. Normal build and guest receipts
remain attributed to a7f71411, before the final documentation/port-pin changes.

The final port documentation changes only `srvros/webkit/PORT.md`, which is a
descriptive input recorded by the original JSC receipt. That original document
hash is retained as historical provenance; all build/runtime input and JSC
output hashes remain unchanged. Final OS changes after a7f71411 are documentation
and the reachable port pin, with no runtime code delta.
