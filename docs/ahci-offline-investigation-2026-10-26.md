# AHCI disk-offline investigation — 2026-10-26

## Disposition and evidence boundary

The follow-up repeat campaign below passed all nine fresh VMs without a
spontaneous AHCI failure. Per the user's direction, active pursuit of the two
historical incidents is shelved as non-reproduced in this bounded campaign.
Their cause remains unknown; retain the original evidence and reopen only if
a recurrence or new directed reproduction supplies useful diagnostics. The
confirmed driver corrections and regression coverage merged in PR178 at
`8bd43adc`; this investigation is closed pending new failure evidence.

Investigation starts from merged main `f0cd6409` on
`device/apple-silicon/ahci-offline`, on the ARM64 macOS build host with x86_64
QEMU TCG. Two historical full-thread runs reported READ DMA EXT (`0x25`) failure
and permanently offlined the disk: the sleep-fix run failed during rename and
the futex run failed during fork. The old diagnostic had no phase, bound or
register information. Their original captures remain unchanged under local
`build/review/sleep-fixes/guest-all-final.log` and
`build/review/futex-audit/guest-all-harness-fixed.log`.

Two fresh full-thread runs with diagnostics added but the old failure decisions
preserved both passed. Thus the original spontaneous failures have **not** been
reproduced with enough evidence to assign their cause. The host fixture does
reproduce a confirmed false-offline defect in the same driver path; this is a
plausible contributor, not proven attribution of either historical event.

## Confirmed defects and corrections

The command loop read `PxCI` as active, then read the clock and tested its bound.
If the device completed during that clock read, the old code still took the
failure branch and permanently offlined a successfully completed disk. A
source-level host test makes precisely that transition and fails against the
old driver. Clearing `PxTFD` busy/DRQ during the readiness deadline check has the
same problem; completion/readiness on the final frozen-clock service turn also
hit a stale decision.

At each deadline, poll or service-turn bound the driver now rechecks the phase's
hardware predicate once. A cleared readiness predicate permits submission; a
cleared command bit permits the existing completion fence and final task-file
error check. TFES still rejects completion. Commands still pending at a bound
still fail, with their DMA storage pinned and the disk quarantined. No retry,
controller reset, longer deadline or DMA-buffer reuse after failure is added.
For these non-NCQ commands, the completion interpretation follows the
[Intel AHCI specification](https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/serial-ata-ahci-spec-rev1_2.pdf).

The completion phase also now resets its stalled-clock service-turn budget when
its timer/poll budget starts. Previously service turns used before submission
could consume the new command's budget. A frozen-clock test needs three readiness
turns followed by four completion turns: it passes with independent phase
budgets and fails when the old shared budget is restored. The two-second
per-phase deadline, 100-million poll limit and 4,096 stalled-clock service-turn
limit remain unchanged. Host fixtures reduce the latter limits for bounded tests.

## Failure diagnostics

The existing `failed; disk offline` marker remains, followed by:

- `phase`: readiness (`ready`) or issued-command completion (`completion`).
- `reason`: terminal branch (`deadline`, `poll-limit`, `service-limit` or `task-file`).
- `lba`, `elapsed_us`, `spins`, `windows`: requested sector and the last elapsed
  sample, poll count and consecutive service turns without clock progress.
- `ci`, `is`, `tfd`, `serr`: observed port-register values when reporting failure.

These are sequential observations, not an atomic hardware snapshot. A device
can change registers between the failure decision and logging. `elapsed_us` is
the most recent clock sample for that phase, not a new timestamp taken while
printing. Use the reason together with register state; do not infer an earlier
controller state from a later sample. Subsequent requests remain rejected, so
only the first uncertain command is reported as a new failure.

## Reproduction and regression coverage

`tools/test_ahci.c` compiles the real driver with modeled MMIO, clock and service
callbacks. New cases cover completion and readiness during deadline observation,
TFES coincident with completion, completion/readiness at a stopped-clock service
bound and independent pre/post-submission budgets. Existing cases preserve DMA
and flush descriptors, genuinely stuck busy/CI, error handling, quarantine,
address-space restoration, bounce-buffer ownership and fast-command service.
The fixture also checks the new failure phase/reason/register receipts.

Four negative controls restore the diagnostic-only old behavior, remove the
completion recheck, remove the readiness recheck or restore the shared phase
budget. All must compile and fail their intended assertions. A separate original
pre-fix probe reproduces false offline without the added diagnostics.

On this ARM64 Mac, the existing x86 driver fixtures run under Rosetta:

```sh
cc -arch x86_64 -std=c11 -O2 -Wall -Wextra -Werror \
  -I shared/include -I kernel/include -I build/limine-protocol/include \
  tools/test_ahci.c -Wl,-dead_strip -o /tmp/srvros-ahci-host
/tmp/srvros-ahci-host
```

On the existing Linux host workflow, `make test-storage` runs the AHCI and block
fixtures; no new target is needed. Guest validation uses a matched kernel and
userspace build and disposable copied disks:

```sh
sh tools/macos/build.sh -j2 all
python3 tools/thread_smoke.py --boot-wait 60 --run-wait 300
TMPDIR=/tmp python3 tools/storage_service_smoke.py \
  --ovmf /opt/homebrew/share/qemu/edk2-x86_64-code.fd --boot-wait 180
```

The storage-service suite exercises throttled writes with USB input, cancellation,
copy comparison, fsck, executable relaunch and a deliberately over-deadline
request. It now requires the forced timeout's `completion`/`deadline` receipt,
elapsed time of at least two seconds, CI still pending and no TFES, followed by
exactly one quarantine event. This fault injection validates real timeout handling;
it does not reproduce the historical spontaneous timing or the host's exact
completion interleaving.

## Validation and remaining investigation

Final receipts are recorded below. All raw logs, source variants, image hashes
and failed attempts stay in ignored `build/review/ahci-offline/`. This work does
not accept native SATA hardware, general SMP, automatic controller recovery or
full Linux/core coverage. No persistent VM disk or physical installation changes.
If spontaneous offline recurs, retain its new diagnostic and matching image/logs
before another run. The historical incidents remain unattributed until such
evidence distinguishes a real stall, task-file error or completion-boundary race.


The candidate's storage-service VM passes throttled writes, USB typing, Ctrl-C,
copy comparison, fsck, three executable relaunch cycles and deliberate quarantine.
The injected WRITE DMA EXT (`0x35`) timeout reports `phase=completion`,
`reason=deadline`, `elapsed_us=2000000`, `ci=1`, `is=0`, `tfd=0x50`, `serr=0`.
The disk records exactly one failure, rejects subsequent storage and the shell
remains responsive. This is the expected pending-command failure, not a false
completion success. AHCI/block host fixtures and AHCI ASan/UBSan pass; all four
source-copy negative controls fail at their intended assertions.


Final candidate full-thread suite passes on its first run, with no spontaneous
AHCI failure. Documentation checks 648 local inline links with zero errors;
whitespace checks pass. Matched build, AHCI/block host checks, sanitizer and
storage-service receipts are saved with source/image/log hashes. The initial
diagnostic build rejected duplicate unused local declarations introduced while
adding diagnostics; those were removed before VM tests. That failed attempt is
retained alongside the passing build. All disposable VMs are stopped.


## Repeat campaign — 2026-10-26

The follow-up uses runtime source `8f7a9434`, QEMU TCG on the same ARM64 Mac
and sequential fresh 512 MiB Q35 guests. The macOS build wrapper passes and
records that source; the only dirty file at build time is the host scope note.
Kernel, initramfs and template disk hashes match the first candidate campaign.

The bounded plan is six complete thread suites, two complete storage-service
suites and one ten-cycle filesystem/relaunch suite. Use the commands above for
each full suite; the final read/load stress command is:

```sh
TMPDIR=/tmp python3 tools/storage_service_smoke.py \
  --ovmf /opt/homebrew/share/qemu/edk2-x86_64-code.fd \
  --boot-wait 180 --dump-memory --relaunch-only 10
```

Use a fresh evidence directory for each campaign. Save each harness's output,
and copy `build/review/storage-service-serial.log`, `storage-service-qemu.log`
and any new screenshot into that directory before the next storage run: those
paths are reused. `--dump-memory` additionally captures guest RAM on a storage
wait failure; the harness retains the failed scratch disk. Stop at any failed
run and inspect its original evidence before retrying. The complete storage
suite deliberately quarantines the disk once at its final timeout step; thread
and relaunch-only runs must have no offline receipts.

All nine guests passed, with exit status zero and their required completion
markers. Six full-thread runs include successful rename and fork tests. Both
complete storage runs pass throttled writes, USB input, cancellation, byte
comparison, fsck and three relaunch cycles apiece. The final guest passes ten
additional fsck/shell-load/executable-comparison cycles. No spontaneous offline
receipt or new runtime defect was found; no additional driver change or
regression case is justified by this campaign. Existing deterministic regression
tests and their negative controls from the first investigation remain applicable.

| Run group | Passed | Offline receipts |
| --- | --- | --- |
| Full thread suite, fresh BIOS guests | 6/6 | 0 |
| Full storage-service suite, fresh UEFI guests | 2/2 | One deliberate timeout per guest |
| Ten-cycle relaunch-only UEFI guest | 1/1 | 0 |

The two intentional WRITE DMA EXT failures report completion/deadline at
2,000,003 us and 2,000,000 us respectively, with CI=1, IS=0, TFD=0x50 and SERR=0.
Each guest records zero failures before injection, exactly one afterward,
refuses subsequent sync and remains responsive. These expected faults confirm
the diagnostics and quarantine still work; they do not identify the historical
READ DMA EXT failure cause.

Per-run commands, durations, log hashes, image hashes and QEMU version are in
`build/review/ahci-repeat/results.json`; separate serial/QEMU logs and screenshots
are retained beside it. The original `ahci-offline` evidence is unchanged.
All disposable guests stopped, and template hashes verified unchanged afterward.
This closes the repeat campaign without claiming absence of all timing races,
native SATA acceptance or a proven historical fix. Continue other work; a new
spontaneous receipt is the trigger to reopen this investigation.
