# Automatic CLI port queue

Authorized November 4, 2026: implement bounded GNU diff in a separate checkout,
then independently review, publish and merge each accepted port and its OS
integration before proceeding through this finite list. Keep integration's
checkout and protected review VMs untouched. Coordinate only concrete shared
blockers, including an occupied compile/VM slot. A released slot is not a standing
reservation for the next milestone.

| Order | Application | Bounded outcome | State |
| --- | --- | --- | --- |
| 1 | GNU Diffutils 3.12 `diff` | Correct normal/quiet/unified comparison of two capped text files; long lines and 0/1/2 statuses; host patch validation and explicit local-patch subset. | Active: genuine GNU core built; first guest passed exact output/status/refusal/fsck. Full qualification and review remain pending. |
| 2 | GNU Patch 2.8.1 | Apply a bounded single-file unified patch to an explicit separate output; preserve source, reject ambiguous/unsafe paths and failed hunks; qualify the diff edge cases excluded by the compact local patch. | Queued: authenticate/pin upstream stable version and research exact profile before implementation. |
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

Current checkpoint: source is reconciled with actual main `88a95f9a` and its
merged Apps/UI pins in the isolated checkout. First guest passed; full harness
stopped before QEMU on xorriso error 32 while internal storage was below reserve.
Both prior heavy windows were explicitly released. Task-owned diff/xz build and
evidence trees were relocated to `/Volumes/SrvrOSCLIQueue` after all file hashes
and symlink targets matched; manifests are `diff-relocation.json` and
`xz-relocation.json` at that volume root. Workspace paths remain usable through
symlinks. New builds/cache/temp images use this dedicated APFS volume backed by
`/Volumes/miniTB/SrvrOS-CLI-queue-20261104.sparsebundle`. Recheck internal, output,
temp and backing free space (at least 2 GiB) before every heavy launch. Integration relayed a fresh explicit ≤40-minute diff-only window: one matched
-j2 incremental build, two clean diff rebuilds/native selected tests and at most
six sequential 512 MiB guests. Stop/release on first failure or expiry; no
automatic extension or reservation for later queue entries.


Primary GNU release indexes were fetched directly during source preparation:
patch 2.8.1, grep 3.12 and coreutils 9.12 are the latest numbered tarball releases
listed. Recheck these pins at entry rather than relying on search cache freshness.
All four queue entries are authorized; pin verification is not another approval
request. No queued source is accepted until its own gates pass.

Current diff gate result: matched current-main build and two clean CLI/diagnostic
rebuilds pass with identical ELF bytes. Selected native GNU tests: three PASS
and the upstream-declared `large-subopt` XFAIL. Full guest run
`build/review/diff/20261104-155542-8xs5kghh` failed the held-output probe after
CPU-progress interruption, output failures, reference comparisons, allocation
refusals/recovery, timestamp-matched unlabeled output and reboot checks passed.
The failed receipt is retained. Source inspection found that atomic small writes
can stall while POLLOUT still reports a few writable bytes. The corrected probe
uses stable near-capacity pipe occupancy and has not yet been target-run. No
production or shared ABI change is needed on present evidence. The window was
released; a new ≤25-minute helper/manifest build and ≤five sequential 512 MiB
qualification/Terminal/regression boots is queued, with no current launch grant.
