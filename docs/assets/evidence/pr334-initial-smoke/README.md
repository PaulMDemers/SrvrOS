# PR334 initial bounded smoke evidence

This portable collection preserves the four reviewed emulator runs at OS6b5ebaf7
and Ports d99565f before current-main reconciliation. It proves normal absent/
positive strict-entropy controls, experimental native register/context/nested
wait behavior, and the eight-case WTF/footprint/GC/JavaScript suite. The actual
JavaScript case prints42 and exits0. All four owned guests stopped.

`manifest.json` records original and published hashes. Local path spellings in
metadata and the one-off supervisor are replaced by `$WORKSPACE`, `$HOST_HOME`
and `$BACKING_VOLUME`; synthetic serial logs have no private device data,
credentials or personal paths. Metadata's internal hashes still describe the
original collection, so validate published files against the outer manifest.
These saved supervisors/receipts are historical evidence, not runnable helpers.
Captured logs use Git's binary attribute to preserve their original carriage
returns, terminal control bytes and hashes across fresh clones.

The profile is diskless Q35/single TCG/1CPU/1536MiB/noNIC/noDebug. JSC uses
qemu64,+rdrand; the absent-provider control uses plain qemu64. No deterministic
seed/replay or extra device is added. Quota fallback diagnostics remain visible
in passing GC/JavaScript runs. This is no physical/general ABI/SMP/full-browser
or physical entropy-quality acceptance. Current-main integrated verification
must be attributed separately.
