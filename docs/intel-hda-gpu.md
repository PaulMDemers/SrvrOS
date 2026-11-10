# Joint display-HDA and GPU diagnostic lifetime

`intel_hda_gpu` composes the existing display-HDA inhibit owner and read-only
[GPU interval claim](intel-interval-claim.md) under one original CPU exclusion.
The separate `intel-joint` image links the core, original interval claim and
native adapter. Its v20 wake receipt and exact 21-token recipe add
`srvros.intel-joint=hda-claim` to the original re-arm recipe. Ordinary startup,
the independent HDA images and legacy GPU recipes remain separate.

Open requires the original empty GT owner, the already-held exclusion and the
original outer diagnostic clock origin. It pins that exclusion before callbacks,
opens the actual HDA owner, inhibits command6→2 and registers the original HDA
borrow plus its nested joint dependent. Successful open is active ownership,
not a completed diagnostic. Rebound copies, callback reentry and reuse refuse.
The caller must keep all objects and callback contexts alive through retirement
or retained fault. Large result storage belongs in static/heap storage rather
than a kernel stack.

Checks validate that exact HDA registration, nested counts, exclusion and GT/claim
identities, then take a fresh guarded HDA endpoint/clock check. They avoid another
full HDA census. A claim attaches once, before it opens, while the original GT
owner is awake. The actual claim still owns both six-DWORD observations and its
original re-arm child registration; the joint wrapper supplies no replacement
source, epoch or hardware authority.

The original HDA400ms and outer500ms origins never restart. Native integration
must route immediate I/O clock samples through `intel_hda_gpu_deadline`, which
advances the shared monotonic watermark and retains the first invalid raw sample
before the individual core receives refusal. `intel_hda_gpu_time` is the pure
predicate for an already-taken sample. Neither function supplies a fresh clock
or extends the original preparation100ms or GT300ms bounds. A sub-core's own
checks remain mandatory. Source failure is not relabeled as a clock failure.

Close requires the original GT owner cleanly closed (or never opened) with no
pins, borrowers or dependents. An attached claim must be cleanly closed with both
owned observations complete and bound. Only then may a separate two-pass HDA
midpoint inspection run, the nested dependent drain, HDA unpin and command2→6
restoration occur. The adapter must independently prove alias retirement before
the joint CPU pin is dropped. `gpu_complete=false` permits clean refusal cleanup;
it cannot produce a pass. An unfinished claim, lost registration, expired clock,
unknown store or failed alias retirement retains exclusion and refuses retry.

The model uses the real CPU exclusion, HDA observer/owner, GT, engine census,
takeover, preparation, held reset, re-arm and interval-claim implementations.
Its successful path has four full HDA inspections (232 reads), command6/2/2/6,
12 original-entry reads and no early restoration. Each injection uses a fresh
process because retained faults cannot be reset by a test-only production API.

```sh
make test-intel-hda-registry test-intel-hda-gpu
```

The fault matrix exercises each shared clock, HDA guard/endpoint/read and command
store position, raw drift, small cross-subsystem clock backsteps, copies, reentry,
lost dependent pointers/counts, unfinished claims and failed retirement. This is
host evidence, not native timing, DMA drain or physical tear-free acceptance.

## Native adapter and receipt

The adapter admits the root read-only, NX, UC HDA alias before either HDA or GT
stores. The same original CPU exclusion covers the whole lifetime. The GT owner,
source receipts, native provider context and claim use static storage so a
retained fault remains valid after the report function returns. The GT's initial
read-only pair is admitted only on its original published object, with the same
exclusion, an established clock and zero pins, attempts and dependents. It is
not awake authority. All later stores still need the existing native guards.

Four inspection borrows return logically; they do not unmap the root page.
The original GT/claim and GPU aliases retire, and firmware/backing are checked,
before the midpoint HDA inspection and command restoration. The HDA root alias
retires before the final CPU pin. The collector joins command6/2/2/6 phases,
including command2 in both GPU preservation samples, all 232 HDA loads, both
six-read claim observations and the original re-arm ACK/cancellation clocks.
Retained faults preserve the first failed clock and raw observation prefixes.

`intel-joint-hda` has a canonical 416-byte little-endian buffer per started
phase, including uncompleted sample buffers and stream-control bytes. It keeps
physical root map/retirement counts separate from logical inspection counts.
The original framebuffer PTE vector uses v3 chunks: a shared upper DWORD is
stored once when every entry in the chunk matches it; otherwise all 64 bits
of each entry are encoded. The exact native joint admission requires the
captured below-4-GiB vector before any store. A changed upper word is still
encoded losslessly and refuses the new operation. The limit stays at 65,536
bytes with the same 1,536-byte reserve. Older PTE encodings are unchanged.

```sh
make -j2 all build/intel-joint/srvros.elf test-intel-joint test-intel-hda-gpu
python3 tools/hardware_boot.py stage --laptop-probe-volume YOUR_UUID --intel-joint
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --workspace-table mbr
tools/host-env.sh python3 tools/hardware_boot_smoke.py --probe-check --probe-missing-workspace --workspace-table mbr
```

Use the unit's actual workspace and partition table. The exact stage must
refuse Q35 with no native stores, save the checkpoint and results, unmount and
return; missing workspace halts. This cannot accept native timing or ownership.
The physical trial is unattended and silent. It returns after a safe save and
unmount; retained uncertainty or persistence failure halts. Follow the
[Ubuntu device workflow](ubuntu-device-agent-setup.md) for physical testing.

No PTE, command submission, flush or invalidation callback is introduced here.
HDA inhibition remains a cooperative submission policy; it does not prove posted
writes, prefetch, firmware/SMM, cache/TLB or other-reader completion. All eight
GPU completion obligations remain open (mask255), assurances and managed pages
remain zero. A persistent desktop GPU service and page flipping need those
independent witnesses and their own maintained runtime lifetime.

## Exact-stage qualification

Release `116ba4fb62bd563f` is staged from source `2f114e64` on main
`8271c254`. The local host/core/build/docs/USB image gate and 25-image linkage
check passed at `6be4ca07`. The final host firmware decoder fix passed targeted
regressions and left the payload bytes unchanged. The new adapter, firmware
request and interval claim link only in the joint image.
Installed-root tests were omitted because the installed payload and recovery
stay in place. The actual PCI command is a halfword at offset 4. Native entry
loads and re-arm stores are DWORD accesses; the retained contexts are static
and the checked functions fit the 2,048-byte frame limit.

Fatal ASan/UBSan passed 7,732 lifetime cases, 10 registry cases and
1,902 HDA fault prefixes through the actual adapter. The adapter matrix also
covers 108 original-entry and source faults, 10 final admission or store
faults, and map, retirement and admission refusals. The tests detected all four
compiled negative controls. The local budget projection using the last
native vector is 61,246 bytes on the normal path and 59,460 bytes for a
wide-PTE refusal, including the original reserve.

The exact stage passed Q35 zero-store refusal, checkpoint provenance, durable
results, clean unmount and automatic return, plus a separate missing-workspace
halt. The stage is ready for an unattended physical test. No installation or
one-shot was performed. Native timing and successful native retirement still
need physical evidence.

## Earlier software qualification

Source`36788a91` is qualified on integrated main`72d49457`. The local matched
build/check-host/core/docs/USB-image gate and all24 existing boot images pass;
installed-root tests are explicitly omitted for this diagnostic-only change.
The actual-core joint matrix passes6,121 cases over1,084 shared clock samples,
75 HDA guards, every endpoint/read/store position and the directed lifetime
failures above. Ten original-registry cases also pass. Fatal ASan/UBSan passes
all6,131 cases, and all nine compiled behavioral controls are detected.

Both cores compile under the actual freestanding flags with a2,048-byte function
frame limit. Actual C ABI and object dependency checks pass; neither the joint
core nor interval claim is linked in any of the24 images. The joint object itself
is2,496 bytes on the measured x86_64 ABI, reinforcing the static/heap storage
requirement. This is a function-frame check, not an assertion that the whole
result fits in2KiB or that a combined native adapter has been qualified.

At that checkpoint, the installed accepted source`f2a6c116`/release
`e83571c874bc17f1`, recovery and banked payloads verified. No new payload was
staged or installed. Those host/object results do not accept the later native
adapter or change the original HDA400ms, preparation100ms, GT300ms and full500ms
limits. See the [unit record](machines/macbookair7-2.md) for the current state.
