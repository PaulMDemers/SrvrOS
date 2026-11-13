# Maintained original-source PCI requester control

The separate v33 `intel-source-requester` image nests one maintained PCI
requester hold beneath the [original-source transfer owner](intel-startup-transfer.md).
The original source, reset/entry epoch, both full private RAM holds and CPU
exclusion stay registered throughout the inhibited child. The older
[claim-based requester operation](intel-gpu-requester-hold.md) retains its API,
image and evidence; it restores before returning and cannot supply this hold.

This operation exercises requester configuration and lifetime. It supplies no
positive firmware, DMA, prefetch, cache, translation or display-reader assurance,
imports no GPU interval and writes no GPU PTE. Native flags stay at zero
assurances and all 255 outstanding completion obligations.

## Exact native operation

The profile remains the captured Air7,2 GPU 00:02.0, 8086:1626 revision 9,
subsystem 106b:011b and MSI90→PMd0→AFa4 chain. Validate each prefix before
reading the next capability. Four complete nine-DWORD profiles bind the actual
source and one original transaction:

1. Capture Command 7, identity, status, class, subsystem, capability chain and
   PM/AF state. Reject any malformed prefix before another target read.
2. Freshly sample the Command/Status DWORD, perform the final original-source/
   alias/requester and clock admission, then write only the Command halfword 3.
   Memory and I/O decoding remain enabled; Status is never acknowledged.
3. Capture the full inhibited profile, requiring Command 3 and unchanged other
   fields. Drain AF pending with at most 32 Command/AF samples and 10 ms, inside
   the unchanged parent budget. No child runs while AF pending remains set.
4. Run the registered child while inhibited. The current child verifies actual
   parent-close and RAM-hold interlocks; it performs no GPU memory operation.
   Re-entry and premature transfer/source/RAM return refuse.
5. Capture another full inhibited profile after the child; require AF pending
   clear. Freshly sample Command/Status and take the final original clock before
   the exact Command-halfword restoration to 7. Capture the full restored
   profile with pending clear before returning this original registration.
6. Complete the enclosing full hardware guard before transfer/source return,
   then retire reset, GPU/HDA/root/aliases and CPU exclusion. Only verified outer
   retirement permits full RAM pattern checks and exactly-once allocator release.

The PCI Command register's BME definition is in pinned Broadwell PRM Volume 2c,
printed pages 1171–1173; its MMIO-mirror discussion on pages 1075–1076 also
describes GSA behavior in terms of MSI gating. See the pinned references and
hash in the [prior operation's scope](intel-gpu-requester-hold.md#completion-scope).
Neither Command readback nor AF pending clear proves retirement of internal
graphics/display accesses. Do not convert this result into a DMA or interval
grant. Preserve the actual cooperatively yielded EFI policy and all existing
profile/firmware checks; sibling firmware remains independently unaccepted.

## Registration, clocks and failure

The one-use original object lives in static/heap storage, including faults.
It seals the original transfer/source/buffers/provider/input/root, route/epoch,
callback context and functions. Its parent transfer remains in the known
missing-authority refusal state; positive modeled range ownership is not
accepted by this diagnostic. Both original RAM holds remain pinned.
Every child timestamp advances the transfer, buffer, source, reset and GT
watermarks together. Preparation 100 ms, GT 300 ms, HDA 400 ms and outer 500 ms
retain their original origins. The 10 ms pending bound is nested inside these
limits. Zero, regressed, stalled or expired clocks refuse. No successful old
receipt or reset restarts a budget or registers a later owner.

The new adapter batches source loads and requester control between the same
opening and closing full hardware validations. Each actual target read retains
the original source, root, exact aliases, requesters and immediate clock checks;
each store also has a fresh Command/Status read and final clock followed only
by pure identity/count loads. The full closing validation runs after known
restoration, while the original source/transfer still holds its registration.
This avoids introducing another full-census bracket inside the latest measured
2,004 us preparation margin. Actual timing remains a physical acceptance step.

Unknown stores are never replayed or optimistically restored. Child, source,
profile, clock or final-guard failure retains the registered source/RAM/root
through a terminal halt. A complete requester suboperation followed by a failed
full closing guard is still an incomplete enclosing test. The receipt records
both batch endpoints, raw completed prefixes, every pending sample and both
pre-store admissions. The strict receiver requires the entire original
transfer/buffer/source/reset/outer chain before full completion.

## Build and evidence

Use the separate `build/intel-source-requester/srvros.elf`, firmware request,
wake version 33, exact 15-token `source-requester-v1` recipe and
`source-requester-preinit`. Stage with `--intel-source-requester` and this unit's
own `--laptop-probe-volume`. Existing v32 and earlier receivers reject the new
recipe. A checked internal shared-schema view supports common prerequisites;
raw logs remain unchanged and older result fields stay empty.

`make test-intel-source-requester` covers both actual-adapter modeled entry
routes, every new clock and read/admission prefix, final store boundaries,
pending cap/deadline, copied/re-entered owners, child failure, required closing
hardware validation, inherited clock origins and strict raw collection. Models
are not native acceptance. Before physical staging, complete the full local
gate, fatal sanitizer/control matrices, image/store/map/frame/log isolation,
exact-stage EFI/MBR persistence and missing-workspace checks, plus fresh local
media/recovery/capacity checks in the [Ubuntu workflow](ubuntu-device-agent-setup.md).

This implementation is under qualification; no native v33 pass, installed
candidate or reboot is implied. An eventual trial needs no human input, shows
progress, uses volume 1%, persists/unmounts and returns automatically on a safe
outcome. Unknown ownership halts visibly and remains incomplete evidence.
