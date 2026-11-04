# Private saved-user FPU replacement: proposed next boundary

Status: owner-approved private contract following qualified snapshot/consume
candidate `2a3b1a89`. Implementation is ready for independent source/host/codegen
review. No production caller, signal hook or public context editing is included. The user's target still requires real signal delivery and
return plus the WTF adapter and a successful no-JIT JavaScript smoke.

## Authority and provenance

Propose one private architecture operation:

```c
bool fpu_restore_saved_user_trap(const struct isr_frame *current_frame,
    const struct fpu_user_snapshot *saved);
```

It requires the same live common-entry proof and fresh strict return observation
as snapshot/consume, including process/worker, immutable scheduler identity,
committed image generation, exact selected user/kernel FPU, FS/stack, actual
registered user root, active-root no-store preparation and supported actual CPU
policy. An already-consumed witness refuses. The helper never executes FXRSTOR;
the existing final assembly return restores the selected user record.

`saved` is a resident, aligned, caller-owned typed private kernel object containing
an unmodified image previously captured by `fpu_snapshot_user_trap`. Its lifetime
covers the operation. It must never originate from user context bytes, a public
frame, a syscall pointer, or a caller-selected wire address. Numeric canonicality
is not allocation/provenance. The future process caller must prove the exact top
private record belongs to this process/image/scheduler lifetime and current
handler frame/token, then supply only that record's saved image. That process
validation and its tests require review before any production caller is added.
The architecture helper alone cannot reconstruct capture provenance from512
bytes and does not claim to do so.

## Legal image and alias policy

At each FPU initialization attempt, invalidate the retained mask and availability
before feature checks. Explicitly zero all512 initialization bytes before the
hardware save and publish mask-valid only after FXSAVE completes. Failed initial
or repeated initialization leaves the mask invalid; a later successful attempt
samples fresh state. Retain the MXCSR support mask from this initialized
hardware FXSAVE image (offset28). A zero hardware mask means0xffbf;
never take the mask from `saved`. Refuse a saved MXCSR (offset24, little endian)
with any bit outside that hardware mask, and refuse reserved high bits16–31.
Read the fields with scalar bytes or aligned-safe integer accesses. Do not mask
or silently normalize the requested value. This follows
[Intel SDM volume1 section11.6.6](https://cdrdv2-public.intel.com/789574/253665-sdm-vol-1.pdf)
and the [FXRSTOR instruction reference](https://cdrdv2-public.intel.com/812383/253666-sdm-vol-2a.pdf).
All other bytes retain the original hardware-produced x87/SSE image; user code
cannot edit the private copy. No trial restore runs while C executes. No XSAVE,
AVX, altered CPU policy or arbitrary user-supplied register restoration is added.

Before any source byte read or destination store, validate the entire512-byte
source extent: nonnull,16-byte alignment, no overflow and canonical upper half
through the last byte. Refuse any exact/partial overlap with the whole current
common reservation/ISR frame or either entire selected FPU object, including
association metadata. A private old record outside those ranges is permitted;
caller lifetime/ownership is mandatory. Refusal leaves source, proof, both FPU
objects and association byte-identical. No allocator, callback, service wait,
IRQ reenable, scheduling or user-memory access occurs.

## Commit and caller transaction

After all identity/policy/extent/alias/MXCSR checks succeed in the existing IF0
continuation, copy exactly512 bytes into the current selected user FPU prefix
and consume the current proof as one infallible private commit. Preserve both
FPU metadata objects, kernel image, FS/TSS/CR3 and association pointer until normal
common exit clears it. The post-copy consumed witness refuses further operations.
No ordinary query, recoverable error or service window may follow that commit
before the caller finishes its prevalidated frame/mask/top-record update.

The future sigreturn transaction must validate the top record and original
private GPR/iret fields, arrange the bypass of syscall RAX result writeback, and
perform all potentially failing checks before this operation. It then commits
only nonfailing local frame/mask/depth changes and returns to the normal restore.
It must not pop/release the private record before a refused replacement, accept
public context edits, or reuse the proof for another delivery on the same return.
Exact process-level ordering remains a later review gate.

## Qualification

Extend actual-source real-FXSAVE fixtures with a second independently captured
legal image, restoration after nested handler/kernel state changes, and main/
worker A→B→A ownership. Assert exact512-byte replacement, unchanged metadata and
kernel image, one-shot consumption, and real final FXRSTOR result. Cover zero
hardware-mask fallback, supported optional DAZ, every unsupported/reserved MXCSR
bit, malicious input mask, first/last-byte alias cases, guards, current-owner and
image/root/lifetime changes, policy changes and consumed/stale witnesses. Invalid
images are only validated/refused, never loaded into hardware. Strict/no-SSE and
sanitized operation validation must preserve the existing boundary exclusions.
Inspect actual target codegen and run matched main/worker/full regression guests.
No production caller or signal/JSC success follows from these helper tests alone.


## Implementation verification pending independent acceptance

Strict Rosetta and ASan/UBSan tests pass the real-FXSAVE operation fixture and
separate modeled initialization fixture. Operations use a prior actual private
snapshot, enter a later trap with different legal hardware state, replace the
saved prefix, consume once and verify the actual final restore. Refusals cover
all32 unsupported/high MXCSR bits (high bits rejected even with a permissive
modeled mask), forged source masks, optional DAZ absence/support, no sampled mask,
policy/owner/root/image/lifetime changes, first/last partial FPU-object aliases,
current frame/proof overlap, null/misaligned/overflow/noncanonical inputs and
unchanged source/proof/full user/kernel objects. The legal source ends at a
protected page. Optional DAZ is actually restored only if host FXSAVE advertises
it. The fixture continues to pattern XMM15, not all16 independently.

Initialization hooks model CPUID, control registers and the capture result;
they verify initial failure without hardware reads/writes, full zeroing before
capture, zero-mask fallback, failure invalidation and fresh successful resampling.
These model checks are separate from actual host FXSAVE and actual target init
codegen. No fully instrumented pre-save boundary is claimed. Target full kernel
builds, with no compiler SIMD operands; disassembly shows scalar zeroing before
init FXSAVE, mask publication afterward, complete helper validation before its
first saved-source read, exact512-byte copy before consumption and no calls or
FXRSTOR after copy begins. Common entry/exit retains its qualified ordering.
Matched kernel register/thread/sysabi/I/O guests remain to be run for this slice.


Owner independently accepted `c27d018f` source/host/codegen and final matched
regression qualification. Host/codegen receipt
`build/signal-context/fpu-restore-host-codegen-receipt.json`, SHA256
`2f6388d70d8aa2d4e8196a90b972f16481aef6db2d6efb396e2984a3e49fb2b7`;
combined VM receipt `build/signal-context/fpu-restore-vm-receipt.json`, SHA256
`377c5a8062bf44a3579dc0d9b8126c74586a47900142e9fffe584584564ca224`.
Owner reran all4 host profiles and verified14 source/host/codegen and24 combined
plus4 nested guest hashes. The all16-pattern main/worker guest, full threadstress,
sysabi and OVMF storage contention pass; same-guest CR4/EFER remain0x620/0xd00.
The private restore operation itself is real-FX **host-tested**, remains unwired,
and is not exercised by a production or guest private-operation caller. Original
17 packed apps are unchanged; all task guests stopped and protected review VMs
preserved. Exact accepted14 artifacts are frozen under
`build/signal-context/accepted-fpu-restore-host/` with retention mapping so later
source/docs/builds cannot overwrite the qualified bytes.
