# Automatic CLI port queue

Authorized November 4, 2026: implement bounded GNU diff in a separate checkout,
then independently review, publish and merge each accepted port and its OS
integration before proceeding through this finite list. Keep integration's
checkout and protected review VMs untouched. Coordinate only concrete shared
blockers, including an occupied compile/VM slot. A released slot is not a standing
reservation for the next milestone.

| Order | Application | Bounded outcome | State |
| --- | --- | --- | --- |
| 1 | GNU Diffutils 3.12 `diff` | Correct normal/quiet/unified comparison of two capped text files; long lines and 0/1/2 statuses; host patch validation and explicit local-patch subset. | Active: corrected two-boot qualification, reproducibility and Terminal pass; combined-cap target fixture, regressions and final review remain. |
| 2 | GNU Patch 2.8.1 | Apply a bounded single-file unified patch to an explicit separate output; preserve source, reject ambiguous/unsafe paths and failed hunks; qualify the diff edge cases excluded by the compact local patch. | Source-only preparation in isolated `SrvrOS-ports-patch` from `88a95f9a`; authenticated pristine2.8.1 imported. No build/VM/PR/merge before diff delivery and a fresh slot. |
| 3 | GNU Grep 3.12 | C-locale text search over bounded files/finite stdin; fixed strings and qualified BRE/ERE, useful match/count/quiet status contracts. No PCRE, recursion, color or Unicode claims. | Queued: authenticate/pin stable release and qualify regex/SDK/memory/cancellation gates. |
| 4 | GNU Coreutils 9.12 `sha256sum` | File/stdin SHA-256 and a bounded explicit verification manifest; exact known vectors and failure statuses. No broader coreutils import or automatic filesystem traversal. | Queued: authenticate/pin stable release and bound manifest/path behavior. |

Each row gets a coherent port commit/PR before the dependent OS gitlink PR,
independent source review, truthful host/guest evidence, required target/Terminal
and failure checks, reproducible CLI builds and relevant regressions. Reconcile
with actual main before merge; verify merged tree equality and port-pin
reachability. Native-host tests do not count as guest tests. Never merge a failed
or unverified gate to maintain queue momentum.

The user authorized automatic review/merge/proceed through these four rows. Stop
when all four are delivered, or when an unresolved concrete blocker prevents
safe progress. Do useful independent work while waiting on resource ownership;
never silently reduce an approved input/behavior boundary or add fake runtime
capability stubs. A shared libc/kernel need requires a minimal reproducer and
coordination before changing its contract.

Current checkpoint: isolated source includes OS main `88a95f9a` and exact merged
Apps/UI pins. Corrected two-boot GNU diff qualification passes at
`build/review/diff/20261104-160706-4um4u8va`: normal/quiet/unified references,
actual-read refusals, real calloc/malloc injection and recovery, timestamp-matched
unlabeled output, GNU host patch roundtrips, narrow local-patch subset, preserved
inputs, reboot bytes and fsck. Exact SIGINT/SIGTERM statuses 130/143 pass after
comparison progress and observed held-output stall with unchanged flags. The
pipe remains at 65534 bytes while POLLOUT stays true; this corrects the old
probe's false assumption and does not trace a kernel write wait or promise
interruption latency. The failed preceding receipts remain retained.

Two clean core/gnulib and diagnostic rebuilds match byte-for-byte against the
same SDK objects. Native upstream scripts: three PASS and the upstream-declared
`large-subopt` XFAIL. Guest upstream input fixtures are separately labeled.
Actual Terminal/less/local-patch-subset/error/readback proof and expert screenshot
inspection pass at `build/review/diff-terminal/proof-wvsivqnz`. CLI regression passes all491 commands and clean fsck; ports regression passes
with clean fsck. All five owned guests have stopped and the slice is released.

One final memory-envelope fixture now reaches 256 KiB, 8192 lines and one 64 KiB
line simultaneously, with a changed byte. Host data construction passes; target
qualification remains pending in a separately requested ≤8-minute/two-boot
slice. No production code changed. Do not merge before this gate and regressions
pass, final independent review completes and merged trees/pins are verified.

Task-owned diff/xz build/evidence trees live on `/Volumes/SrvrOSCLIQueue` after
all copied file hashes/link targets matched. Volume-root relocation manifests
retain the originals' hashes; workspace paths remain symlinks. The APFS image is
`/Volumes/miniTB/SrvrOS-CLI-queue-20261104.sparsebundle`. Build/cache/TMPDIR use
this owned volume. Recheck internal, output/temp and backing reserves (≥2 GiB)
before every heavy launch. The prior ≤25-minute/five-boot slice is complete and explicitly released. The
≤8-minute/two-boot combined-cap request is queued behind other ready owners and
is not a launch grant. Source-only GNU patch preparation in a separate checkout
may continue, but no next-port build or merge precedes diff acceptance. No
standing reservation applies to later queue entries.


Primary GNU release indexes were fetched directly during source preparation:
patch 2.8.1, grep 3.12 and coreutils 9.12 are the latest numbered tarball releases
listed. Recheck these pins at entry rather than relying on search cache freshness.
All four queue entries are authorized; pin verification is not another approval
request. No queued source is accepted until its own gates pass.

Review delivery (draft, not accepted): [SrvrPorts #11](https://github.com/PaulMDemers/SrvrPorts/pull/11)
and dependent [SrvrOS #369](https://github.com/PaulMDemers/SrvrOS/pull/369).
Both explicitly retain the failed probe/remaining target, Terminal and
regression gates. Merge order remains ports first, then OS, only after passing.
