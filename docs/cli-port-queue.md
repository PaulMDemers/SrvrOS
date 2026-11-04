# CLI port queue and closeout

Authorized November 4, 2026: implement bounded GNU diff in a separate checkout,
then independently review, publish and merge each accepted port and its OS
integration before proceeding through this finite list. Keep integration's
checkout and protected review VMs untouched. Coordinate only concrete shared
blockers, including an occupied compile/VM slot. A released slot is not a standing
reservation for the next milestone.

| Order | Application | Bounded outcome | State |
| --- | --- | --- | --- |
| 1 | GNU Diffutils 3.12 `diff` | Correct normal/quiet/unified comparison of two capped text files; long lines and 0/1/2 statuses; host patch validation and explicit local-patch subset. | Delivered and verified: ports PR11 `4da8a98b`, then OS PR369 `3fcf2630`; exact trees and reachable pin checked. |
| 2 | GNU Patch 2.8.1 | Apply a bounded single-file unified patch to an explicit separate output; preserve source, reject ambiguous/unsafe paths and failed hunks; qualify the diff edge cases excluded by the compact local patch. | Active in isolated `SrvrOS-ports-patch`, reconciled with delivered diff/main `3fcf2630`; Six earlier guests pass; miniport correction is frozen. Final archive-identity gate failed before QEMU; fresh full guest qualification is required on the retained final payload. No merge. |
| 3 | GNU Grep 3.12 | C-locale text search over bounded files/finite stdin; fixed strings and qualified BRE/ERE, useful match/count/quiet status contracts. No PCRE, recursion, color or Unicode claims. | Deferred by closeout instruction; authenticated source/research retained, no implementation started. |
| 4 | GNU Coreutils 9.12 `sha256sum` | File/stdin SHA-256 and a bounded explicit verification manifest; exact known vectors and failure statuses. No broader coreutils import or automatic filesystem traversal. | Deferred by closeout instruction; authenticated source retained, no implementation started. |

Each row gets a coherent port commit/PR before the dependent OS gitlink PR,
independent source review, truthful host/guest evidence, required target/Terminal
and failure checks, reproducible CLI builds and relevant regressions. Reconcile
with actual main before merge; verify merged tree equality and port-pin
reachability. Native-host tests do not count as guest tests. Never merge a failed
or unverified gate to maintain queue momentum.

The original instruction authorized automatic review/merge/proceed through
these four rows. The later November 4 closeout instruction, relayed by the
integration chat, supersedes that continuation: finish and verify the current
GNU Patch Ports12→OS370 delivery, update topic/unit documentation and status,
then stop. Grep and sha256sum are deferred; do not start another port. Do useful independent work while waiting on resource ownership;
never silently reduce an approved input/behavior boundary or add fake runtime
capability stubs. A shared libc/kernel need requires a minimal reproducer and
coordination before changing its contract.

Current diff checkpoint: all bounded gates pass on clean `5796a9cd`, including
the changed simultaneous 256 KiB/8192-line/64 KiB-line fixture and full two-boot
reference, allocation, signal, output, host patch, preservation and fsck checks.
Actual Terminal expert walkthrough, full 491-line CLI regression and ports
regression pass against identical kernel/initramfs/base-disk bytes. Two clean
GNU core/gnulib/diagnostic rebuilds match against the same SDK objects. Native
upstream scripts have three PASS and one expected XFAIL, with guest input
fixtures separately labeled. Independent source/evidence review found no blocker.
No shared libc/kernel change, full guest-suite, whole-image/SDK reproducibility,
RSS/true peak, traced syscall wait, in-call latency or physical acceptance claim.

The former held-output probe's false POLLOUT assumption is retained as a failed
receipt: corrected evidence observes 65534 stable bytes while POLLOUT stays true,
then exact signal statuses 130/143 and unchanged flags. Earlier ISO/disk-pressure
failure also remains historical evidence, without a guest claim. All owned
builds/guests stopped and every resource slice was explicitly released. There is
no standing reservation for GNU patch or later rows.

[Portable evidence](assets/screenshots/diff-manifest.json) and the verified
qualification archive under ignored `.local/cli-port-evidence/diff/` preserve
all failed/passing proofs and source/payload copies outside build cleanup.
Exact storage/restore paths are in ignored local records. Recheck internal,
output/temp and backing reserves before every heavy launch; current coordination
requires 2 GiB internal and 4 GiB backing reserve. Use external build/cache/temp
storage and one owned compiler/VM job at a time within each finite handoff.

Actual OS main `70c3ea0f` is reconciled here, adding host installation tools/docs
without target runtime changes. The tested port tree and later delivery head
differ only in PORT.md. Final exact-head/tree/pin verification belongs to the
authorized port-first, OS-second delivery. [SrvrPorts #11](https://github.com/PaulMDemers/SrvrPorts/pull/11)
and [SrvrOS #369](https://github.com/PaulMDemers/SrvrOS/pull/369) record its outcome.

GNU Patch 2.8.1 source is authenticated (578 regular files match the archive) and
its adapter/tests are prepared in the separate checkout. Configuration, recipe,
link and clean rebuild gates pass. Full snapshot/output/memory/signal guest
acceptance remains incomplete at the failed filesystem artifact check described
below; do not merge unqualified source to maintain queue momentum. Grep/coreutils source signatures
and digests are prepared and must be rechecked at entry. The later closeout instruction limits remaining delivery to Patch; later rows
are deferred and have no heavy-slot reservation.

GNU Patch initial window (completed and released): ≤30 minutes for SDK bootstrap, genuine cross-config
and sequential port-local compile fixes at ≤2 jobs, then at most one 512 MiB
initial guest after production/diagnostic link. Stop/release on a shared blocker,
first runtime failure, reserve breach, expiry or completion. Full qualification,
reproducibility, Terminal and regressions require later handoffs. Caller tests
migrate in-place syntax to explicit new output while preserving source.

GNU Patch initial compatibility outcome: genuine production/diagnostic and
matched-image builds pass. Initial dirty-source proof
`build/review/patch/first-mr6qicc4` passes named hunk edge cases, relative
--version source, source preservation, exclusive-output/legacy refusals, failed
hunk status 1 with output removed and fsck. No shared runtime change. The slice
is explicitly released. The subsequent full ≤40-minute/two-clean-build/seven-512-MiB-guest
window was granted and stopped at the first failed gate below; no acceptance
or merge yet.
Pinned dependency and alternate-depth setup failures remain historical evidence;
direct owned reference stores solved them without changing older checkouts.

GNU Patch full-window checkpoint: clean OS `37139e18`, ports `de7adcea`,
matched build and two clean GNU core/gnulib/diagnostic rebuilds pass against
the same SDK objects. Native selected tests have two PASS and one expected
XFAIL (`dash-o-append`, outside the single-file profile). Two disposable boots
and guest fsck completed, but the harness failed its outside-fixture artifact
comparison at `build/review/patch/20261104-174642-s7p0g0jh`. The exact root
addition is `.srvros` containing `meta`, besides the intended `patch-test`;
Home and tmp entries are unchanged. Diagnosis is source-only; the failed
receipt remains failed, later qualification assertions and five later guests
are not accepted. The slot was released at first failure. A concrete harness
correction and a fresh finite handoff are required before another heavy run.

Grep source-only preparation: genuine GNU regex/DFA is required for BRE/ERE;
full capped input must be admitted before `-q` can exit, so later input errors
remain status 2. Prefer a measured port-local heap-arena ceiling at the actual
SDK break-growth boundary over replacing the allocator. SDK calloc/realloc and
aligned allocations call malloc within the same translation unit, so linker
wrapping allocation symbols alone does not prove coverage. Audit the final
`sbrk`/`srv_sbrk`/`brk` relocations and direct mappings; count page-rounded
high-water because reducing program break does not unmap prior pages. This
would bound heap arena, including fragmentation, rather than RSS or all process
memory. No budget value or target acceptance is asserted before measurement.
Cancellation checks must cover actual grep/kwset/DFA/regex compilation and
matching/backreference loops, not only the caller around GNU execute. No Grep
implementation, configure, compiler or guest run has started.

Closeout checkpoint: [Ports12](https://github.com/PaulMDemers/SrvrPorts/pull/12)
and [OS370](https://github.com/PaulMDemers/SrvrOS/pull/370) are drafts with truthful
incomplete verification. Harness `1ec87276` validates the entire metadata
sidecar stream, including field shape, ASCII canonical mount paths, uniqueness,
uint64 bounds and final newline. A host-only check accepts the retained 605-row
sidecar and rejects six malformed samples; it does not replace fresh guest
qualification. Integration is reviewing the finite rerun request. No heavy
launch is authorized until that handoff; no shared runtime blocker is known.

Fresh corrected evidence on clean `328c2a3a`: two-boot Patch
`build/review/patch/20261104-180150-50p8cl2l` PASS, two-boot diff interoperability
`build/review/diff/20261104-180309-0ngfiw3b` PASS, actual Terminal plus expert
screenshots `build/review/diff-terminal/proof-g_r5b65a` PASS, and full 498-command
CLI plus fsck PASS. Production/diagnostic bytes match the previous clean rebuilds.
The six guests stopped and the window was released early; ports regression was
not run after a remaining shipped miniport caller was found incompatible.
`dea65ffc` migrates that example to safe labels and explicit separate output,
builds the patched copy and checks original-source preservation. Its expected
output is updated; no runtime ELF source changed. Current main `db95fc9b` is
reconciled and adds documentation only relative to the prior base. A fresh
finite data-refresh/one-ports-guest handoff is required after other owners'
reserved turns. Compare the retained baseline logical-file manifest and all
executables/kernel/initramfs before reusing earlier scoped evidence.

Final data-refresh checkpoint on clean `d32133b3`: the matched build passes,
and kernel/production/diagnostic/probe ELF bytes are unchanged. The logical
392-entry data image differs only at `share/examples/ports-smoke-miniport.sh`.
However, the strict initramfs byte-identity gate fails before QEMU: the archive
contains that changed data image and new packing metadata (Makefile copies
`EXFAT_IMAGE` into `initramfs_root/srvros.exfat`). The failure is retained in
`build/review/patch/final-identity-failure.json`; the slot was released, and no
ports guest ran. The old initramfs has only a retained whole-file hash, with
no old member inventory/bytes, so exact member equivalence cannot be asserted.
Do not exempt this gap or reuse earlier archive acceptance. Current source/
payload copies are readback-verified outside build under the task's external
`patch/frozen-d32133b3/` directory. Request a fresh full seven-guest qualification
on that frozen final payload after the browser owner's resource release. The
earlier six guests retain their original source/data attribution. Native and
two-clean-GNU rebuild results apply to the unchanged GNU runtime source.

Fresh final-candidate preparation incorporates filesystem PR366 `7b1bfcb8`
and actual main `72fde837` through merge `a7563231`. The reviewed incoming
delta changes host offline inspection tools, host tests and docs; it does not
change kernel/userspace runtime sources or port pins. There were no conflicts.
A new matched incremental build/manifest is required, followed by freezing the
actual tar/disk/kernel/executables and complete tar-member inventory outside
build before all seven fresh guest checks. The browser currently owns the
resource slot; queue acceptance is not a launch grant. Allow under 1 GiB of
additional evidence/frozen copies while retaining all earlier failed proofs.
