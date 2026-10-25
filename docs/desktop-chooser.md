# Asynchronous file chooser controller

Text and Preview use the optional application-runtime controller in
`userspace/desktop_runtime/chooser.c`, declared by
`srvros/desk_chooser.h`. Link `libdeskruntime.a` before `libdeskwidgets.a`
and the userspace library. Generic CLI programs do not link this service.
The pure `desk_file_picker` widget still owns presentation/input only; it has no
process, filesystem, allocation or callback dependencies.

The controller borrows one picker, one `desk_file_job` and the existing listing
entry/item/detail arrays, each with `DESK_FILE_ENTRIES` capacity. Initialize it
once per owner and keep all storage at stable addresses while active or pending.
Use one application dispatcher. The job may be bound to the [shared worker
pool](desktop-connections.md#shared-file-worker-service) or use the legacy
transport. This controller neither spawns nor kills/reaps workers; it starts only
LIST requests through that job. Document/image reads, writes, overwrite guards,
clipboard, focus and repaint decisions remain application policy.

## Lifecycle

1. `desk_chooser_open` validates the mode and initial directory/name, then opens
   the widget. It refuses an active chooser, a pending canceled request or a
   busy job without changing that state. Configure the widget's geometry and
   call `desk_chooser_browse` to start the first asynchronous listing.
2. Route input to the picker and pass its result to `desk_chooser_action`.
   Redraw/clipboard flags remain available to the caller. A browse starts a LIST;
   an acceptance copies a normalized path and mode into the caller's selection
   before hiding the picker. Acceptance cannot race an unfinished listing.
3. Before polling the listing job, snapshot `pending_id`. Pass that ID and its
   terminal `desk_file_job_poll` result to `desk_chooser_finish`. Continue servicing
   GUI/input and other work between polls; the controller adds no blocking loop.
4. Cancellation hides selection immediately but retains the accepted read and
   its buffers. Keep polling and finish the canceled result; it is discarded
   without replacing the displayed listing. Reopen is refused until it drains.
   A document/window may close independently, but its pending job buffers must
   remain owned until completion. Do not reuse the job for another operation.
5. After pending work drains, the caller may detach/reuse the job and retire its
   transport under the existing worker contract. Cancellation never cancels an
   accepted file mutation, and a picker selection does not itself open/save a file.

Controller IDs increase monotonically and refuse exhaustion. They are separate
from worker wire IDs, which can restart when a legacy worker is replaced. A
stale/duplicate cookie, foreign operation or mismatched wire request cannot
replace the listing or release current ownership. A still-busy or nonterminal
poll leaves `pending` set. A matching failed listing returns an error with the
chooser still active; the user can change the path, retry when the job is usable,
or cancel. Transport recovery belongs to the job owner: pooled jobs can acquire
a replacement worker, while Preview recreates a broken legacy transport on its
next picker open. Never reinitialize a job while its chooser remains pending. Validated
listings are decoded into caller-owned storage before the widget borrows them.

```c
if (chooser.pending) {
    uint64_t id = chooser.pending_id;
    int done = desk_file_job_poll(chooser.job);
    if (done) {
        int result = desk_chooser_finish(&chooser, id, done);
        /* READY: repaint the listing; ERROR: repaint its error message.
         * DISCARDED: canceled read drained; STALE: no state was changed. */
        handle_chooser_completion(result);
    }
}
```

The controller remains presentation-independent. Text and Preview host its widget
in a [document-owned native dialog](desktop-application-lifecycle.md) through the
shared connection presenter, with an embedded admission-refusal fallback. It does
not provide cross-process chooser portals, a filesystem permission boundary or
an independent modal-event loop.

## Verification

`make test-desktop-chooser test-file-picker test-preview-app` exercises pending
cancellation/reopen, worker replacement with reused wire IDs, stale/foreign/
duplicate completion, request-ID exhaustion, invalid listing preservation,
transport errors, normalized path snapshots and retained Text/Preview contents.

The opt-in `desktools text --chooser-capture` consumer submits real LIST traffic
before cancellation, drains it, reopens and rejects an old completion. It runs
with both legacy and pooled workers and checks retirement. The Preview VM
harness runs it before the ordinary PNG/JPEG/BMP, picker, clipboard, cancellation
and image-retention workflows. Text/Open/Save/Save As/multi-document fixtures
exercise the pooled consumer. Read the [machine record](machines/macbookair7-2.md)
for exact-source acceptance; a new shared API does not imply new physical input
acceptance.
