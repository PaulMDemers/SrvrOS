# Original-source GPU interval transfer owner

The separate v32 `intel-startup-transfer` image connects the accepted original
source/private-buffer lifetime to `scanout_transfer`. This closes a software
lifetime gap: a provider-facing address owner could previously exist without
keeping that original source registered. It does not establish native firmware,
DMA, cache or reader ownership. Native evidence is `{assurances=0, obligations=255}`,
so native transfer admission refuses before provider, transfer clock, arena or
GPU-visible address activity. No GPU PTE writer is added.

## Original registration and deadlines

`intel_source_transfer` is a one-use original object in static/heap storage.
Inside the live buffer child it captures the actual source, provider, input,
entry kind, reset epoch, post mode, original one-page entry, original two RAM
objects/tokens/owner IDs/serials/full extents and enclosing root. Its CPU
exclusion token must match the live source's GT owner and remain held. Its
registration blocks buffer/source return. Both full 1,356-page RAM holds remain
registered beneath that original root; closing or attempting an early return
cannot free them.

Guard and clock callbacks revalidate the original chain before and after each
call. Every measured clock advances the transfer, buffer, source, reset and GT
watermarks together. The preparation 100 ms and GT 300 ms origins are never
restarted by generic transfer phase clocks, and the existing HDA 400 ms/outer
500 ms limits stay fixed. Zero, regressed, expired or stalled clocks refuse.
Busy guards reject callback re-entry. Copies, changed resource/provider/token
identities, lost registration and uncertain owner return retain the original
source/RAM/root through a terminal fault.

## Admission, live checks and close

The exact first candidate page lies outside the complete inherited protection
fence. The input is an observed original, not an available address grant; neither
its value nor an empty entry supplies ownership. Both full private RAM buffers
remain pinned, but the address proposal covers one page only. Full-buffer GPU
imports still require complete original vectors and hardware witnesses.

A trusted future adapter must independently establish all five hardware
assurances and close every completion obligation for that exact interval. The
bridge calls the actual generic transfer owner; it does not echo stable entries
back as positive hardware proof. Admission, live and terminal callbacks are
bound to the same original source/epoch/vector and same parent budgets.

Known missing-authority refusal is an explicit live state. It owns no imported
range and performs no transfer-provider or transfer-clock callback, but keeps
its source registration until final guarded close. A positive **model** provider
can open a real software address lease, run maintained checks, stop reservation
admission and drain children before terminal original-state validation. Pending
leases keep both the transfer and original source registered; existing children
may retire while closing. Unknown provider/clock/exclusion or retirement state
is terminal, never an optimistic close or a new admission attempt.

After clean transfer close/refusal, its registration returns to the original
buffer child, then source, reset and outer owners retire. Full private patterns
and exactly-once RAM release remain outside the hardware budget, after verified
outer retirement. No saved result can authorize a later live operation.

## Image and evidence isolation

Build `build/intel-startup-transfer/srvros.elf`; stage with
`python3 tools/hardware_boot.py stage --laptop-probe-volume LOCAL_UUID --intel-startup-transfer`.
The image has its own firmware request, wake version 32, exact 15-token
`srvros.intel-ring=startup-transfer-v1` recipe and `startup-transfer-preinit`.
Ordinary boot and the accepted v31 image keep their existing APIs and recipes.

A distinct `intel-source-transfer` row records original bindings, clocks,
registration/return, refused/opened state and provider/clock/arena activity.
The strict `intel_startup_transfer` collector requires the entire original
buffer/source/reset/outer proof, exact recipe/checkpoint and complete return.
Its checked internal shared-schema view is never saved as raw native evidence
or exported as v31/v30 acceptance; those result fields remain empty. Positive
model-provider flags, missing/duplicate rows, spliced source/epochs and changed
counts are rejected by the native receiver. All raw alerts remain available.

## Qualification and next trial

Actual-core/adapter tests cover both startup routes, every new and original
clock, source/hold/provider/registration changes, callback re-entry, each missing
assurance and open obligation, positive modeled admission/close and pending
child drain. A modeled provider is not native hardware acceptance. Host/VM and
native qualification results are recorded separately in the unit ledger.

The next native diagnostic tests this registered provider-facing lifetime and
its timing inside the real original source. It must show zero transfer/provider/
arena/GPU-store activity while native authority remains absent, then clean root
and RAM retirement. It is not a trial of a GPU mapping or tear-free desktop.
Follow the [Ubuntu workflow](ubuntu-device-agent-setup.md): exact-stage VM,
independent banks, local USB/recovery checks and durable one-shot run record.
No input is required; keep volume 1%, display progress, save/unmount and return
automatically on safe outcomes. Retained uncertainty halts visibly. Collect the
matching session and media integrity before any further candidate.
