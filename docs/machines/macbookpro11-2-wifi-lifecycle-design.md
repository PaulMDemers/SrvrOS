# Pro11,2 connection and UI handoff

November 8. Design for completion milestones5/6;no native acceptance follows.
Current shared `wifi_abi.h` version1 exposes saved discovery snapshots and
selection only. `userspace/apps/desktools/system.c` deliberately says connection
is unavailable;selecting a row must not become an implicit connect side effect.
Existing provider/io and DMA lifetimes are prerequisites,not a UI connection API.

## State and lifetime contract

Adapter lifetime and connection lifetime are separate. Each adapter has stable
registry identity while present;each connection attempt has a strictly newer
nonzero epoch. A request carries adapter identity,discovery generation and target
index,plus a unique request identity. Stale generations refuse without touching
an active attempt. A new ABI must preserve version1 layout and clients;use
separate versioned request/status structures and update kernel/library/System
and validation tests together. Do not reinterpret existing selected flags.

States:unavailable,idle,scanning,connecting,secured,addressing,connected,
disconnecting and failed. Secured means local authenticated key activation;
connected additionally requires a usable configured production IP path. Report
error categories separately from state. Registry selection alone is not link or
Internet readiness. No advertised bitrate/duplex may be invented. Snapshots are
copied immutable cached data;System's update/render path never waits on USB.

Connect is queued to the sole adapter worker. It validates selected capability,
regulatory/channel policy and fresh target before acquiring hardware ownership.
One original CCMP owner persists across ordinary TX/RX,with no key-copying into
provider state. Replies are committed once;delivery/storage is bounded/copied.
Credentials use a private explicit provisioning boundary;version1 display names
are escaped presentation strings,not trusted SSID/target identity. Do not reverse
parse them or put passwords/PMK/packets in status,logs,command lines or Git.

Disconnect first closes publication and cancels queued traffic. Pending device
leases and callback loans remain pinned until exact native completion/stop and
event-debt proofs. Erase keys and finish all independent parent restorations
before detaching old context. A fresh connection cannot borrow a prior epoch's
PN/key/lease/token or registry selection. Uncertain hardware retirement produces
contained failure with pinned resources,not optimistic success/reconnect.

Removal invalidates cached rows and starts the same retirement path. Reinserted
hardware receives a new verified device identity/generation and fresh firmware/
calibration lifetime. Never resurrect the quarantined context or consume registry
slots indefinitely on repeated reconnect. Reattach is a new initialization,not
a UI flag change. Warm initialization remains unproved until native evidence
replaces the standing cold-unplug requirement.

## UI behavior and tests

System provides Refresh/Scan,Connect,Cancel and Disconnect from cached status.
Enable only actions valid in the current state;present connecting/addressing
progress and explicit authentication/timeout/device-removed errors. Preserve
selection only within its discovery generation. Refresh must not create another
worker or reset a live connection. Pause/Resume affects observation refresh,
not hardware ownership. Error dismissal must not erase retained-fault status.

Software tests cover stale request/generation,duplicate commands,cancellation at
each phase,wrong credentials,AP disappearance,timeout,closed/pending queues,
removal during TX/RX,old completions after reconnect and UI-state transitions.
Use independent encrypted fixtures for security refusals. Hardware tests use
only authorized access points with automatic progress and saved receipts;check
successful transfer after each reconnect,and verify original user-file bytes,
metadata semantics,filesystem allocation and Ubuntu recovery. Attended UI work
opens an untimed full guest checklist and ends through saved Exit desktop.

Done means repeated normal connect/transfer/disconnect/reconnect and removal/
reattach work through production UI/stack,with evidence-backed failure recovery.
An association receipt,a selected network row or successful single diagnostic
DHCP exchange alone does not satisfy this lifecycle gate.
