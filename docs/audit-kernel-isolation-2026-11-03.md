# Kernel isolation and teardown audit — 2026-11-03

## Scope and coordination

Separate Apple Silicon audit checkout, branch
`device/apple-silicon/kernel-isolation-audit`, started at PR313 `3cd9ce86`
and advanced to merged main `56e1f950`. PR313's owner retained integration
and VM ownership; its merge includes PR252 and PR308 by ancestry and pins
SrvrPorts `5e994f53`. This follow-up changes no process, syscall, namespace,
port, or ABI implementation. It does not complete the broader audit.

## Confirmed VMM boundary defect

The public map/ensure, protect, unmap and physical-address lookup helpers
indexed page tables without first requiring a canonical virtual address.
Upper bits disappear during index extraction: `0x1000000001000` and
`0xffff000000001000` select the same entries as `0x1000`.
Thus an invalid input could return another mapping's physical address or
alter its PTE. The existing user-buffer and ELF checks already reject these
inputs at their boundaries; this finding does not demonstrate an unprivileged
syscall bypass. The low-level VMM API nevertheless failed its address contract.

All four shared boundaries now reject noncanonical addresses before table
access or allocation. Map and ensure share the same implementation. Both lower
and upper canonical addresses remain accepted. Refused operations leave the
existing PTE, absent destination slot and physical-output sentinel unchanged.
`tools/test_vmm_user_range.c` includes both malformed encodings, all five APIs,
a positive upper-half lookup, and the existing permission/large-page cases.
The baseline aborts on the physical-lookup refusal assertion; the fix passes.

## Teardown and write protection

The expanded actual-source fixture also verifies that destroying an active
user root switches to the kernel root before any tracked table frame is freed,
retires table frames in reverse order and the root last, removes registration,
and leaves zero/kernel roots alone. A host-only CR3 write hook makes the switch
observable. Production CR3 behavior is unchanged.

Source tracing covered `process_exit_gate`, worker versus initial-thread exit,
sibling cancellation before file cleanup, `finish_process_files`, final content
release, inherited-reader writeback ownership and address-space cleanup. No new
process-exit defect was confirmed. Existing process fixtures cover first fatal
cause, exit admission, mapping publication before service checkpoints, inherited
ownership, clean-space retry and sticky writeback errors. These fixtures do not
prove every scheduling interleaving or SMP behavior.

A separate disposable Q35/TCG baseline boot on this ARM64 Mac reached the monitor
with `CR0=0x80010033`: bit 16 (WP) is set. The kernel source still does not
explicitly enable WP; this boot relies on the bootloader-provided state. This
resolves the earlier unmeasured-state question for this BIOS/QEMU configuration
only, without claiming native or alternate-bootloader acceptance. Its register
and serial records are local under `build/review/isolation/baseline.*`.

## Verification

- Native ARM64 ASan/UBSan expanded VMM fixture passes. The old source fails
  the new noncanonical lookup assertion (`vmm-before.log`).
- x86_64 Darwin/Rosetta host targets pass: `test-vmm-user-range`,
  `test-process-fault`, `test-process-service`, `test-user-fault`,
  `test-syscall-user-copy`, `test-process-output`. Fault and service fixtures
  also pass ASan/UBSan.
- Freestanding x86_64 kernel build passes with warnings treated as errors.
- The rebuilt-kernel Q35 guest passes `sysabi_smoke.py`, including user-fault
  recovery (`sysabi-fixed.log`). Its initramfs and disk are retained PR313
  runtime artifacts; the kernel is newly compiled from this audit tree.
- Local Markdown checking found nine pre-existing missing license targets in
  uninitialized nested port submodules; no new documentation target is missing.
  This checkout does not claim a complete passing link check.

Local evidence: `build/review/isolation/`. The boot image uses the newly built
kernel with unchanged runtime files extracted from PR313's published image;
this is a kernel regression boot, not a complete clean image rebuild. Test
harnesses copy the source disk into disposable storage. Existing review VMs
and the primary and filesystem checkouts remain untouched. An initial ISO
boot-info replay emitted an xorriso error; its output was discarded. The next
extraction exposed read-only permissions on the extracted kernel, so that run
is not claimed as changed-kernel evidence. After explicitly making the private
kernel copy writable, the final ISO was rebuilt with the repository's boot
options and BIOS installation; its kernel was compared byte-for-byte with the
new build before the passing guest run.

## Remaining work

Deterministic fault/dirty-writeback contention through full process exit,
long IRQ-disabled operations, driver DMA/reset/removal ownership and broader
manual isolation review remain open. Future boot paths must establish or verify
WP rather than inheriting this configuration's acceptance. The historical
[coverage ledger](audit-coverage-2026-11-02.md) retains the broader gaps.
