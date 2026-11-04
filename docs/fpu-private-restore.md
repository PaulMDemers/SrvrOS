# Private saved-user FPU replacement: proposed next boundary

Status: design for process/FPU owner review following snapshot/consume candidate
`2a3b1a89`. No implementation, replacement stub, signal hook or public context
editing is included. The user's target still requires real signal delivery and
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

At FPU initialization, retain the MXCSR support mask from the initialized,
zero-backed hardware FXSAVE image (offset28). A zero hardware mask means0xffbf;
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
