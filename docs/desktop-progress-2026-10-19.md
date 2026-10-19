# Desktop reliability and idle work — 2026-10-19

Work starts from shared baseline `3b0b43a` on
`device/macbookair7-2/reliability-input-power`. Physical observations below remain
separate from host/VM results. Normal startup continues to exclude optional AML
probing. The accepted installed desktop is still `b427b78f21644bd2`.

## Physical secondary click

The trackpad now chooses the button at the initial physical press: one contact
means primary, two contacts mean secondary. That identity stays latched through
contact changes until physical release. This preserves the established gesture
of holding one finger down and adding another to steer a primary drag. Palm and
cancel suppression require release before rearming. Physical clicks are independent
of tap preferences; Settings explicitly labels the two-finger option as a tap.

Validation: `test-pointer` and `test-xhci` pass, including raw Type 3 reports with
bulk I/O pending, contact-count transitions, cancellation and no duplicate tap.
`all test-desktop` and the context-menu VM pass. The VM verifies secondary-button
routing and menus, not physical finger recognition. Native acceptance remains
pending the next human test.

## Input delivery timeout

The compositor previously checked queued-input age before attempting delivery.
A delay in the compositor could therefore close a healthy app without testing
whether it could accept the next event. Delivery is now attempted first; the
timeout starts with an actual failed send and resets on successful progress.
Timeout logs include blocked duration, event type and frame-progress age.

A regression reproduces a server delay longer than the old deadline, recovery
after IPC backpressure, ordered delivery, and bounded closure for a client which
continues refusing input. `all test-desktop`, context-menu and System VM checks
pass. This fixes a demonstrated timeout error but does not prove it caused the
System closure in the previous physical session. Frame/configuration lifecycle
deadlines remain in place; native follow-up is required.
