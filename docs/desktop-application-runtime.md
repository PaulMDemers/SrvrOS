# Shared desktop application runtime

Text and Preview use the optional `userspace/desktop_runtime` archive above the
[connection and document lifecycle](desktop-application-lifecycle.md). The
[widget library](../userspace/widgets/README.md) remains pure model/controller,
layout and drawing code. The runtime allocates nothing and starts no receiver or
thread. Applications still own documents, images, accepted jobs and rendering.
This block changes no desktop protocol, kernel wait ABI or hardware gates.

## One dispatcher, bounded turns

Initialize `desk_runtime` with an existing `desk_connection`, then call
`desk_runtime_dispatch(runtime, event, context)` once per application turn.
The event callback receives the authenticated target window and a copied packet.
It returns zero to continue, one to stop the batch, or negative on app failure.
The dispatcher handles at most 32 transport polls, counting internal progress as
well as delivered events. It refuses recursive dispatch. Run job completion,
state changes and rendering after that batch, even when more GUI messages remain.
Do not combine it with another connection/app/raw GUI receiver.

Transport or callback failure marks this runtime failed; it does not close a
window, free storage, kill a worker or answer a document prompt. Drain accepted
work according to the existing owner contract, retaining uncertain GUI resources
until the established process/connection cleanup can release them safely.
Text preserves the PR170 document/allocation/cleanup timing checkpoints.

## Jobs, deadlines and one wait

After dispatch and job/render work, prepare this turn's interests with
`desk_runtime_watch` or the file-pool, file-job and image-job adapters, then call
`desk_runtime_idle(runtime, busy)`. It returns native readiness flags, or negative
after a short retry; a retry alone is not a fatal app error. Preparation is non-consuming: it does not poll
a worker, read a reply, issue a file mutation or transfer its ownership. Completion
is delivered by the application's existing job poll on the next turn. Duplicate
FDs merge read/write masks. The native wait still accepts only eight explicit
nonblocking pipe/PTY interests; invalid/full sets force a short retry rather than
silently sleeping without accepted work. Queued leases, complete buffered replies
and child retirement also retain the short retry path.

The wait deadline is the earliest supplied absolute monotonic deadline, timer
or 100 ms service heartbeat. GUI and descriptor readiness wake it earlier; the
heartbeat is not a delay imposed on input. An exhausted event batch, busy app,
pending canvas/menu publication, unavailable clock or ineligible wait uses the
existing one-tick fallback. Interests and explicit deadlines reset at the start
of each dispatch, so assemble them each turn. The connection owns cancel epochs,
FD lifetime generations and surface-publication eligibility as before.

Text supplies its shared file pool once per turn. Preview supplies both its
standalone chooser transport and image-worker transport; it can now wait when
quiescent instead of unconditionally sleeping/polling each tick. Both keep
clipboard/control retries and accepted-read retirement under their existing
owners. This is not a native latency or energy measurement, nor a fix for the
separately investigated app startup/cleanup stalls under heavy disk work.

## Caller-owned one-shot timers

A zero-initialized `desk_runtime_timer` can be armed at an absolute monotonic
microsecond deadline. Equal deadlines retain registration order. Rearming in the
same runtime replaces the old registration; attempting to move an armed timer to
another runtime is refused. Canceling or clearing timers never invokes them.
Keep each timer and its callback context at a stable address while registered.

Due callbacks run after the GUI quantum, detached before invocation. They may
cancel/free themselves or other timers, or explicitly rearm a one-shot timer.
The dispatcher rereads the list after each callback and runs at most 32 callbacks
per turn, so a timer repeatedly rearming itself in the past cannot monopolize
GUI/jobs/rendering. There is no fixed registration-count limit or implicit
periodic catch-up. Callbacks must themselves be bounded and stay on the UI thread.
Cancel a timer before freeing its owner; clear remaining timers at app teardown.

## Shared command snapshots

`desk_commands.h` describes caller-owned action IDs, enabled/checked state,
labels and shortcut bindings. The same snapshot builds a menu item and guards
keyboard/button/menu invocation. Rebuild it from current app state at invocation;
a stale enabled visual cannot authorize an action after a job/modal disables it.
Unknown, disabled or duplicate action IDs do not invoke a handler. Invalid menu
metadata leaves the destination unchanged.

Primary-modifier bindings accept Ctrl or Meta, normalize either left/right
modifier, match Shift/Alt exactly and ignore Caps Lock. Released, repeated or
canceled key events do not invoke commands. This handles app-level file/window
commands; text editing, search fields and image-local navigation remain with
their shared widget controllers.

Text's New/Open/Save/Save as/New window/Close/Quit use one command definition.
Accepted file work disables conflicting mutations while an independent new
window remains available. Quit from a document or owned picker uses the same
cancellation/application intent. Preview's toolbar, menus and app shortcuts use
one definition for Open/Cancel/Fit/Actual/Zoom/Close/Edit/Quit. Model-specific
save decisions and readonly-image cancellation stay with their owners.

## Validation and acceptance

`make test-desktop` includes the focused runtime and Text-command fixtures:

```sh
tools/host-env.sh make test-desktop-runtime test-tool-commands test-preview-app
```

The existing `desktop_wait_load_smoke.py` fixture also uses this runtime: its
three-second phase change is a registered timer, while dispatch and pooled-worker
readiness share the same turn/wait path under throttled writes. It preserves the
original workload, key/frame acknowledgements, worker retirement and integrity
checks. This is an actual-ABI consumer, not native timing acceptance.

Host tests cover event/internal-message floods, reentry, timer fairness and
removal during callbacks, deadline shortening, wait capacity and worker phases,
left/right primary shortcuts, repeated/canceled input and command state changes.
Actual Text/Preview, pointer, shutdown, crash and refused-dialog VM fixtures in
the lifecycle guide remain the consumer gates, including exact saved bytes,
retained edits, canceled reads, retirement and restored process baselines.
Machine records distinguish source/VM validation from new physical acceptance.
