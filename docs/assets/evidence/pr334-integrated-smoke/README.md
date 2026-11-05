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
paths are replaced by named tokens in copied records. The two explicitly named
review summaries omit the large input/function maps and identify their complete
originals by hash. Synthetic serial bytes are unchanged; Git binary attributes
preserve captured carriage returns and terminal control bytes. Internal hashes
refer to original records, so use the outer manifest for published files.
These saved records are evidence, not runnable build or launch helpers.

Normal production signal capabilities remain disabled. These checks do not
accept a general signal ABI, SMP, native hardware, physical entropy quality or
a full browser. Ordinary desktop/session verification is recorded separately
by the integration coordinator before merge.

A later main update (`f11ce344`, cache diagnostics and exFAT warning layout) is
being reconciled separately. These records keep their exact `0a3f5981`
attribution; they do not by themselves qualify the later kernel.
