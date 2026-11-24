# Broadwell private BCS command completion

`intel_bdw_bcs_flush` implements a command packet and bounded completion protocol
for a **previously owned** blitter ring, context and completion mapping. It is
host-qualified software. No native adapter or guest image calls it, no inherited
ring is submitted, and existing Air7,2 acceptance remains unchanged.

This supplies the command/completion layer described in the
[managed-memory milestones](intel-managed-memory-plan.md#ordered-implementation-milestones).
It cannot create the address-space ownership required to execute its own packet.
The next separately selected [private command-memory trial](intel-command-memory.md)
prepares actual CPU-owned ring/context/completion backing and aliases. GPU
translations and executable context/submission authority remain separate.

## Exact packet

Intel's [Broadwell PRM Volume2a, November2015](https://cdrdv2.intel.com/v1/dl/getcontent/690783),
SHA256 `b035c834a6e1dce2c7057165f2157f8597748bd2b0ccf63a982e0fc4f2dc95b5`,
printed pages834–837, defines BlitterCS `MI_FLUSH_DW`. The pinned Linux v6.12
[non-render flush emitter](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/gen8_engine_cs.c)
and [command definitions](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_gpu_commands.h)
are independent references for command barriers and the generation's fields.

The conservative codec emits six DWORDs:

| DWORD | Value and meaning |
| --- | --- |
| 0 | `0x13044003`: MI opcode26, immediate post-sync QWord, BCS TLB invalidate, five-DWORD command |
| 1 | Direct GGTT completion address with bit2 set; no status-page indexing |
| 2 | Zero high address, restricting this implementation to below4GiB |
| 3–4 | Fresh nonzero64bit completion nonce, low DWORD then high DWORD |
| 5 | `MI_NOOP`, aligning the submission tail to24bytes |

Unlike Linux's status-page-indexed four-DWORD packet, this uses the PRM's direct
GGTT five-DWORD QWord form. There is no notification, interrupt, timestamp,
protected-memory bit, indirect address, arbitrary batch or jump. The completion
address is64byte aligned, avoiding the documented QWord destination-bit5 hardware
restriction. Zero/all-ones nonce and invalid/unaligned/out-of-range destinations
refuse without modifying output. The protocol uses offset0 in a private4KiB
completion page and a separate private4KiB ring. No wraparound is needed.

## Original owner and lifetime

The address-sealed, consumed-once source binds the original owner, provider,
lifetime, context, root, epoch, sequence, nonce, GPU/physical ring and completion
addresses, aperture, protected fence and four enclosing clock origins. Both
pages must be disjoint, aligned and outside the protected fence; physical pages
are conservatively below4GiB. Exact8086:1626 matching narrows the codec; it does
not admit a hardware resource or a sibling Mac.

Before **every** operation, the trusted provider must independently validate
the registered original source and its maintained ownership of:

- a stopped, empty private ring and the exact context that will execute it;
- the exclusive private completion page and fresh sequence/nonce;
- all actual GPU/CPU mappings, physical backing and generations;
- the applicable CPU/GPU cache policy and ordered visibility mechanism.

The proof names that exact source/owner/provider/lifetime/context/root/epoch/
sequence, includes all five prerequisite bits and has a fresh bounded timestamp.
Echoing the descriptor is not hardware evidence. Existing readiness/AF/cache
observations, the accepted remapper drain and System Agent operation supply none
of these ring/context ownership prerequisites by themselves. All prerequisites
must already hold before the first callback; this command cannot bootstrap them.

The provider is a trusted kernel adapter, not a security boundary against arbitrary
memory corruption. The core detects changed identities, callback tables,
registration, packet and progress across every callback, including clocks and
pauses. Reentry, copied sources, overlapping storage, incomplete callbacks and
reuse refuse. No operation returns a hardware token or a handoff assurance.

## Publication, submission and completion

1. Read the actual admitted completion location with acquire ordering. It must
   initially be zero; an already matching nonce is stale and cannot complete it.
2. Publish exactly the six packet DWORDs into the stopped private ring.
   `DONE` means the actual bytes are GPU-visible under the independently admitted
   cache policy. Partial/uncertain publication keeps backing pinned and forbids
   submission. No guessed CPU fence stands in for visibility.
3. Submit exactly that owned context/ring with tail24. Known issuance is separate
   from completion. A missing/uncertain store is terminal; never retry it.
4. Acquire-read the exact completion address until the full nonce matches. Zero
   remains pending; any other value refuses. Immediate completion is allowed
   only after known issuance. The256th poll may complete; no257th poll is allowed.
5. Revalidate the full original ownership and complete packet/progress/count
   record before reporting this request complete. The enclosing owner still
   must retire the command/ring/context/mappings before releasing any backing.

The default100ms and selected150ms legacy preparation policies retain their
GT300ms, HDA400ms and outer500ms origins. Named aggregate aliases also select
the existing separate practical250/500/750/1000ms preparation/GT/HDA/outer
policy; no legacy value changes and no origin restarts. An additional **10ms submit-through-completion bound**
starts before the submit callback; a slow submit cannot buy a fresh wait interval.
All bounds include the final ownership validation. Zero/backward/frozen clocks
and exact-limit-plus-one refuse. No existing native deadline is changed.

Every uncertain or failed started lifetime is retained with its first clock
failure and known issuance prefix. There is no automatic retry, cancellation,
rollback, unmap or free callback. Source registration stays consumed after
success or failure. Success is only this BCS command: it does not flush render/
video engines, retire firmware/display readers or certify a completed flip.
The PRM's display-flip caveat on printed page825 still applies.

## Validation and next native boundary

`make test-intel-bdw-bcs-flush` runs the independent actual-C device model under
the100/150ms legacy and250ms practical policies:2,326 cases each. It covers exact encoding, bad descriptors,
all21 binding fields and20 progress/identity fields at all39 callback boundaries,
packet/callback/registration changes, all proof fields, stale/wrong/never-written
completion, write uncertainty, immediate/256th-poll completion, reentry and all
24 clock failures. All five deadlines have exact/+1us cases. Both complete
matrices pass fatal ASan/UBSan;24 compiled weakening controls must fail. Actual
native compiler flags pass the2KiB frame limit under all three policies; ordinary,
audit, legacy VT-d and combined GGTT image plans exclude this caller.

Next establish a genuine private ring/context/status/completion mapping owner,
including initialization and required visibility **before submission**, and
its original-source retirement contract. Then integrate ordered native callbacks,
raw receipts and complete failure/return gates in a separate image. A fixture
or a saved earlier receipt cannot substitute for that live prerequisite.
No physical trial is scheduled merely to demonstrate the known missing owner.
The [tear-free roadmap](intel-tearfree-roadmap.md) still requires every remaining
DMA/prefetch, GPU pipeline/cache/translation and firmware/display-reader witness
before a managed mapping/publication grant. All255 obligations remain open.


The [private BCS execlist submit protocol](intel-bdw-bcs-submit.md) now supplies
the host-qualified four-DWORD port operation beneath the future submit callback.
Actual joined-core tests keep nonce completion separate from port issuance and
retain backing on every partial/uncertain/late outcome. This still requires
already owned initialized context/mappings; it cannot bootstrap their transfer
or certify other engines, firmware or display readers. No native adapter exists.

## Physical Ubuntu reference probe

`tools/intel_bdw_bcs_host.c` uses the integrated GPU's Linux i915 render node.
It requires Broadwell `8086:1626`, blitter support, a shared LLC and full
per-process graphics translation. Each case creates a private context, batch
and result page. i915 resolves both address relocations within that context.
The positive case issues `MI_STORE_DATA_IMM` and then `MI_FLUSH_DW` with
immediate QWord post-sync data on a separate cache line. Both halves of the
marker are fresh and nonzero. The probe reads the prefaulted write-back
mapping and checks the earlier value when it observes the marker, before it
calls the userspace fence wait. After the bounded wait it checks the whole
result region, including zero padding. The control ends after the first
write and leaves the marker at zero.
[Intel Broadwell PRM Volume 2a, pp. 834–837 and 915–917](https://cdrdv2-public.intel.com/v1/dl/getcontent/690783).

```sh
mkdir -p build/tests
cc -std=c11 -O2 -Wall -Wextra -Werror tools/intel_bdw_bcs_host.c -o build/tests/intel-bdw-bcs-host
timeout 15s build/tests/intel-bdw-bcs-host
```

On 2026-11-18, MacBookAir7,2 completed 20 positive cases and 10 controls.
The host observed the marker after 0–135 µs of polling after the submit
ioctl returned. A fatal ASan/UBSan build repeated one set successfully.
The wait and other scheduling work in i915 can affect these times. The
probe exercises private PPGTT addressing without requesting TLB invalidation.
Native GGTT addressing, ring and status memory admission, firmware exclusion,
and completion across other engines and display readers remain separate.

## Five-engine private queue test

The Linux comparator uses the SrvrOS Gen8 packet encoder with private
32-bit GPU addresses. i915 pins the result and batch pages at the specified
addresses. The command stream needs no relocation. RCS uses `PIPE_CONTROL`
with CS stall, render target, depth and DC flush plus a post-sync QWord.
BCS, VCS0, VCS1 and VECS use `MI_FLUSH_DW` with a post-sync QWord.
The native GGTT blitter trial uses a separate packet with TLB invalidation.
[Linux Gen8 command emission](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/gen8_engine_cs.c)

Build on the Air's Ubuntu installation. The privileged shell opens a fresh
software timeline. The render user runs the probe and opens the DRM devices.

```sh
make build/tests/intel-bdw-engine-drain-host
sudo sh -c 'exec 3<>/sys/kernel/debug/sync/sw_sync; exec setpriv --reuid="$SUDO_UID" --regid="$SUDO_GID" --init-groups timeout 20 "$1" 3' sh "$PWD/build/tests/intel-bdw-engine-drain-host"
```

Each engine writes data and then its marker on a second cache line. The
CPU checks those private mappings before waiting on the completion fence
and verifies the results with an independent GEM read. Each successor
waits on the same software fence. The internal panel must advance on
its original CRTC while all five successors remain pending with both
locations untouched. After release the CPU verifies all successor data
and markers. This exercises private work scheduled by i915. Native
global admission, a power-manager L3 flush and slot ownership still
require independent evidence.

Append `--active-range` after the timeline fd (`3`) in the command above
to compare two requests for the same private GPU address. A software fence
holds the first blitter job. A render job at an unrelated address must finish.
A second render job requests the held address with different backing and no
input fence. The observer checks that the submitter is inside the execbuffer
ioctl, or that i915 explicitly declined the binding. After release it checks
both buffers' contents and advancement of the original eDP CRTC. This
test uses a process-private page table. Native GGTT ownership is a separate
milestone.

## Device-side queue hold through slot 8704 rollback

On the Ubuntu Air7,2, build the binary as above and run the observer once.
Its client uses the render user's DRM nodes. The observer reads the GGTT
entry, Pipe A and cache/traffic registers under i915 forcewake and uses an
isolated tracefs instance to identify the client's GGTT bindings.

```sh
sudo timeout 12s python3 tools/intel_bdw_slot_loan_host.py \
    --user "$(id -un)" "$PWD/build/tests/intel-bdw-engine-drain-host"
```

1. All five engines write fresh data and a post-sync marker. The four
   non-render queues park on a PPGTT semaphore. The CPU verifies the first
   render fence, then submits to a newly created render context. Its fresh
   marker proves RCS ran in the second context after all five initial markers.
   A second semaphore lets RCS execute its final `PIPE_CONTROL`. The CPU
   checks that marker, all five pending fences and untouched successor markers
   before admitting the replacement. The observer checks that RCS has the same
   enabled indirect context batch at all three held checkpoints.
2. i915 unbinds and rebinds two private 40 MiB objects. Their traced ranges
   must cover address `0x02200000` in the same GGTT. The observer follows
   both object identities, reads the changing slot 8704 PTE and matches
   its exact original value on return. The client verifies all 1,024 words
   of the target page against the GEM backing, including the original
   contents after restoration. i915 changes whole object bindings here.
3. Pipe A must stay enabled on the original programmed and live surface at
   each check. Its frame counter must advance by the time rollback finishes.
   Only then does the CPU release the common semaphore and verify all five
   successor markers and fences. A separate process releases both semaphores
   if the client exits or 1.5 seconds elapse. Watchdog expiry fails the test.

Before the render-context change, the Air completed two of eight trials. Six stopped on the panel
surface check. Removing all waits allowed an engine to escape; removing
only the first render wait exposed an early final flush. Both controls
stopped before the replacement. These results cover the five submitted
queues and their private GEM objects in a running i915 desktop.

In the context-switch comparison i915 kept a 128-byte RCS indirect batch
programmed at `0xffffd000`. On Broadwell its context-restore workaround saves
L3SQCREG4, sets bit 21, executes a stalled DC flush and restores the original
policy before ordinary batch execution. The observed context switch, indirect
length and fresh RCS markers establish ordering. The observer samples neither
the transient policy nor a power-manager L3 acknowledgement during the switch.
[Linux Broadwell context restore](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_lrc.c#L1619-L1702)

In the complete runs the selected bank returned `L3SQCREG4=0x40400000`.
Bit 21 was clear, so the pipeline flush covered noncoherent lines and
left coherent L3 lines outside its guarantee. `SCRATCH1[4]` selects a
different policy for the power manager's flush. The observer leaves
register steering and both policies untouched. Broadwell replicates this
configuration per slice and bank. [Intel Broadwell register reference,
pp. 845 and 863](https://cdrdv2.intel.com/v1/dl/getcontent/690789)
· [Intel Broadwell MMIO, p. 3](https://cdrdv2-public.intel.com/690867/intel-gfx-prm-osrc-bdw-vol-13-mmio.pdf#page=7)

SrvrOS needs exclusive submission and mappings for its command and
semaphore pages. For a command-streamer drain it must also establish
coherent-line flush coverage on every active bank before issuing the final
render flush and exclude the separate WiDi requester through translation
retirement. The measured GAM request and WiDi registers stayed open under
the private waits. The inherited slot writer continues to require a
device-wide hold. [Native writer requirements](intel-bdw-edp-recovery.md#reset-free-use-of-the-same-traffic-checks)

### Private command pages on Broadwell

The Air submission diagnostic builds seven pages intended for the global
graphics table and twelve for a legacy 32-bit per-process graphics table.
The global pages contain the BCS ring, its two context pages, completion
storage, the RCS indirect batch, its private register-save scratch and a
dedicated engine hardware status page. On Air7,2 the active i915 blitter used
a global engine status address distinct from its per-request timeline status
address. The driver reserves a full zeroed page for the engine.
Only command words are copied to the RCS batch page. Its pointer stays in
the context metadata.

In the five-engine image, VA `0x1000` holds five batches at 256-byte
intervals. VAs `0x2000` through `0x6000` each hold one writable completion
page. VA `0x7000` holds the CPU-controlled semaphore. Commands and the
semaphore are read-only to the GPU. Four aligned page directories and one
leaf table complete the allocation. Only the first directory has a present
entry. The blitter context names all four directories, enables legacy PPGTT
and starts its held batch at VA `0x1100`. Broadwell still fetches that context
and the ring through the global table. Activating it requires the global
bindings, compatible GPU private PAT entry 3, the correct DMA domain and
exclusive submission. [Intel Broadwell context descriptor, pp. 107–110](https://cdrdv2-public.intel.com/690870/intel-gfx-prm-osrc-bdw-vol-02d-commandreference-structures.pdf)

Receipt version 7 adds the engine status page address and covers all 1,216
cache lines of the 19-frame allocation. Version 6 recorded the physical
allocation, PDP0, CPU cache lines flushed and return of its 18 frames. The driver checks the DMA tag on every
frame and the effective write-back policy of its actual kernel alias. It
checks the complete images against the fresh interval three times. The
collector preserves earlier receipt formats.

To execute the same batch bytes and virtual addresses using the Ubuntu GPU
driver, build the five-engine tool above and run it from the desktop user
account.

```sh
build/tests/intel-bdw-engine-drain-host --private-image
```

Each engine gets its own fresh marker and scratch page. The multi-engine
render batch starts at VA `0x1500`. After the CPU sees all five drain markers,
it opens the render semaphore. RCS then copies both halves of each sibling’s
64-bit marker into its own scratch page at `0x2100`–`0x211f` and issues its
final PIPE_CONTROL flush. The host requires the destination to be empty before
opening the semaphore and to match all four producers after the flush. Both
source and destination addresses use the private GPU page table.
[Intel Broadwell instruction reference, pp. 817–818](https://cdrdv2.intel.com/v1/dl/getcontent/690783)

The host also checks that eDP advances while all five fences remain pending,
then releases the common semaphore and checks every scratch word. The watchdog
releases both waits if the client fails. The i915 slot-loan comparison runs
the same render batch in its second context. It rechecks the copied values
before replacement, after replacement and after restoring slot 12291.
[Slot-loan procedure](intel-bdw-primary-flip.md#independent-slot-comparison-under-i915)

This exercises the engine commands on silicon. Native page-table installation, WiDi and
the power manager's L3 acknowledgement require separate physical evidence.

### First native blitter request

The isolated Air submission image uses a two-page private address space for
its first request. The ring dispatches to VA `0x1700` on a read-only command
page. The blitter stores a 64-bit value at `0x3000`, then uses MI_FLUSH_DW
to store its complement at `0x3040` and ends the batch. This request can
finish before a CPU release gate or any other engine exists.

The kernel driver takes a live GPU range transfer and a pinned, uncached
GGTT register page. It compares each original entry before installing seven
global mappings. The provider must complete the translation invalidation
and maintain hardware ownership. The driver then pins the published range,
checks all seven live entries, verifies execlist mode and selects the private
engine status page. Four ordered DWORD stores post the private context to the
BCS submission port. A fresh context-complete/active-to-idle event,
the expected context ID, an idle queue and a complete scratch-page comparison
are required before restoring the original engine status address. The driver
waits for reader retirement before restoring the original entries. An
uncertain descriptor or completion keeps the range and backing registered.
[Broadwell register reference, context status and execlist status](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

Run `make test-intel-bdw-blitter` for the host driver tests and
`make smoke-intel-bdw-blitter` to run the same driver with modeled registers
inside a SrvrOS Q35 guest. For the physical instruction comparison on
Air7,2 under Ubuntu, use the regular desktop account for the client.

```sh
make build/tests/intel-bdw-engine-drain-host
build/tests/intel-bdw-engine-drain-host --first-marker
build/tests/intel-bdw-engine-drain-host --marker-dispatch
sudo python3 tools/intel_bdw_slot_loan_host.py --marker-csb --user "$(id -un)" build/tests/intel-bdw-engine-drain-host
```

The third invocation samples the six hardware status slots around the fence
using separate 32-bit MMIO reads. i915 owns the live rings, address tables and
context throughout this comparison. The exclusive device owner and first
cold-boot ELSP submission still require acceptance under SrvrOS.

#### Cold entry after a full GT reset

The Air flush adapter claims the blitter submission port before the reset.
It records the lender in the reset binding and checks the claim during the reset
protocol. The kernel driver can use this path when the same owner supplies a
live transfer for the command pages. The original GT borrow must still be active.

The driver compares the live blitter's ring registers, execution-list status,
active context and parser mode with the driver's post-reset census. With the
legacy ring disabled, the parser idle, status interrupts masked and the old
status-page address zero, it installs the private status page. It initializes
the six-slot completion cursor, enables execution lists with a masked write
and clears STOP_RING if needed. These writes precede the first descriptor.
Broadwell requires the scheduling-mode change to occur after full reset and
before submission. [Intel Broadwell register reference, pp. 157 and 177](https://cdrdv2.intel.com/v1/dl/getcontent/690789)
[Linux i915 execution-list initialization](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_execlists_submission.c#L2631-L2667)

On successful cold submission, `intel_bdw_blitter_map_and_run` leaves the
request in `BDW_BLITTER_STATUS_PINNED`. The GPU can still address its status
page after the command completes. Keep the request, DMA backing, FIFO loan
and transfer alive. To return the mapping, pass the next full reset and its
post-reset census to `intel_bdw_blitter_detach`. The driver checks that the
same owner has disabled the queue, detached the status page and drained its
readers. It rechecks the reset epoch while restoring each original entry.
A lost epoch during rollback retains the remaining mappings and the port.

The physical reset capture on this Air recorded an idle parser (MI_MODE
`0x200`), disabled ring, execution-list status `1`, and zero active context
and BLT_MODE before and after PCI bus mastering reopened. That capture did
not sample HWSTAM or HWS_PGA. Both registers are checked live at admission.
Executing this sequence under SrvrOS still depends on the device-wide hold
that permits publishing the seven command-page translations.

## Seven-page command handoff

The Air7,2 reset owner imports the seven observed firmware PTEs at slots
11744–11750, outside the boot framebuffer fence. It pins a separate 19-frame
DMA allocation for the blitter image and private address space. Before
reset, after memory maintenance, and after bus mastering reopens, it checks
every original PTE with a high/low/high read and verifies the private image.
It also pins seven read-only, uncached CPU aperture aliases with the five
engine-register aliases. With PCI bus mastering enabled, it reads the last
word of every original page before reset and again after reopening. The
second sample requires the same reset epoch and the completed native GGTT
invalidation between the two observations. The receipt counts the aliases
mapped, pinned and retired and the two completed aperture observations.
These samples check preservation of the inherited aperture contents. The
private-frame challenge below checks a replacement translation.

A successful reservation trial returns the child to the firmware lender and
frees the private allocation. A changed entry or uncertain backing retains
the resources for recovery. New trials require an `intel-command-slots`
receipt in the matching session.

The full-reset binding carries the original transfer object and epoch.
`intel_bdw_blitter_map_and_run` requires that binding, its live GT borrow,
the same display exclusion and submission owner, and independent transfer
assurances. The native diagnostic currently supplies the reservation. Device
exclusion of prior GPU and firmware users and completion of their traffic
remain prerequisites for promoting it.

On a granted transfer, the driver writes all seven PTEs before issuing a
single GGTT invalidation at BAR0 offset `0x101008` and a posting read. The
driver then places a different temporary word in each private CPU frame,
flushes each cache line and reads the corresponding word through each
aperture page. It restores the private words and verifies them through the
aperture before it installs the status page or posts the descriptor. On
rollback it invalidates again and checks that the original aperture words
remain visible while the private frames carry different markers. A stale
translation or failed posting read keeps the publication and RAM pinned.
The aperture reads verify CPU-visible translations. The submitted blitter's
completion and scratch values furnish the subsequent engine-side evidence.
[Linux i915 Gen8 GGTT invalidation](https://code.googlesource.com/linux/torvalds/linux/%2B/f032c53bea6d2057c14553832d846be2f151cfb2/drivers/gpu/drm/i915/gt/intel_ggtt.c#202)

Run `make test-intel-bdw-blitter`, `make smoke-intel-bdw-blitter` and
`make test-intel-gt-flush` for the host, guest and reset-owner checks.
On the Air running Ubuntu, build `build/tests/intel-bdw-engine-drain-host`
and run it as the render user with `--command-page-witness`. It checks
CPU writeback and seven translated aperture reads on a private GEM object,
restores the words and verifies that the original eDP CRTC advances. i915
owns the mappings in this comparison.


### Command submission while WiDi stays excluded

The reset owner now binds the captured wireless video fuse, video enable,
audio routing and powered codec identity to the seven-page transfer. Its VMM
guard excludes CPU mappings of the WiDi tail, video and audio register pages.
The driver checks that guard on the original root while using the captured
values. Reading the excluded pages again would violate the exclusion.

The blitter also checks the CPU submission ports for render, both video engines
and VEBox. At each side of PCI bus-master reopening, the native reset census
checks that the four pages belong to its active VMM guard. Each has a single
read-only kernel mapping. The command path checks all four together against
the original address space with supervisor write protection enabled. It does
this before requesting the drain, across device observations and immediately
before each descriptor store. If protection is lost after any descriptor
word, the request and its pages stay pinned. Outstanding device accesses
still require the separate engine and L3 drain below.

For the blitter itself, the driver takes a second CPU mapping loan on the same
root and VMM guard. It binds the exact virtual register address to
BAR0+0x22000 before its first queue read. The VMM walks the root to establish
that this 4 KiB supervisor mapping is the sole writable alias, has NX set and
uses the register cache policy. Public VMM calls cannot replace it, remove it
or map another alias while the command owns it. The GGTT register loan can
remain active independently. The blitter keeps its loan across cold status-page
detachment, checks it after restoring the translations and returns it after
the device reopens. A failed closing check retains the restored pages and hold.

Wire v4 of `intel-slot-reopened` exercises this loan with PCI bus mastering
off, after the other four engine ports have been protected. It records the
blitter's HWS_PGA and HWSTAM before returning the loan and protecting the fifth
port. The `writer` field advances from 0 to 1 on acquisition, 2 after both
reads, and 3 on return. A mask of all ones is a valid HWSTAM observation.
The later queue samples still require all five ports to be protected.

The blitter requires a controller lease before it can promote the transfer.
It checks the enabled primary surface, all eight L3 bank policies, and open
engine and arbiter replies before requesting a drain. It checks the five
fresh engine flush acknowledgements and brackets the L3 flush pulse with
WiDi GO and busy reads. After all seven translations have been installed and
verified, the owner must independently hold the WiDi device and display
proxy before reopening global memory traffic. BCS can then fetch its command.
Rollback requires a second drain and a fresh L3 pulse. A lost lease after a
store keeps the publication and its backing pinned.

Broadwell's power manager sends the same GO value to all clients. BCS uses
request bit 4 and message offset `0x800c`. WiDi uses bit 6 and offset
`0x8014`. CGMSG bit 5 at `0x8104` requests WiDi clock gating and the clocking
unit reports the result at `0x802c`. The native reset diagnostic records this
pair with the WiDi GO and reset-flow observations at four boundaries.
[Register reference, pp. 234–236, 587–592, 805 and 815](https://cdrdv2.intel.com/v1/dl/getcontent/690789)

On the Air under i915, the host observer sampled both clock registers at
`0x7f` and WiDi at `0x1f` before and after three completed BCS requests. Each
request preserved the original primary surface. Ordinary idle clock gating
can therefore coexist with open WiDi GO. The hardware must keep the independent
WiDi exclusion across global GO reopening. The GPM request, its continuous
ownership and that independent exclusion still need native implementations.
The Q35 test supplies a modeled controller. The Air diagnostic only reads
the clock and acknowledgement registers.

Run the host comparison with the render user's marker binary. The observer
uses i915 forcewake and read-only BAR mappings. A pending primary flip rejects
the sample.

```sh
sudo python3 tools/intel_bdw_slot_loan_host.py build/tests/intel-bdw-engine-drain-host --user "$USER" --command-traffic
```

## EFI handoff on the Air7,2

The Air flush loader runs while the boot display driver is still attached.
It inventories the GPU controller protocols and PCI I/O driver opens, including
firmware file GUIDs when the binding exposes them. It samples the seven GGTT
entries twice, brackets each sample with the primary plane state, and matches
the live surface to a GOP framebuffer. It leaves the display mode in place.
[UEFI PCI I/O](https://uefi.org/specs/UEFI/2.10/14_Protocols_PCI_Bus_Support.html)
[UEFI graphics output](https://uefi.org/specs/UEFI/2.10/12_Protocols_Console_Support.html)

When firmware exposes the EFI ACPI table protocol, the loader reserves 19
ACPI NVS pages and seeds each page. It installs the
checksummed OEMG table with ID `BDWBOOT1` and returns control to GRUB. GRUB starts Limine from the release directory.
SrvrOS verifies the memory map, physical translations and contents before
using that backing. The kernel compares the panel and command slots with the
same boot's firmware snapshot across the reset and bus-master transition.
On a clean return it reseeds the private backing and retains the NVS reservation.

On the reference Air the helper completed the capture but the protocol lookup
returned `EFI_NOT_FOUND` before page allocation. GRUB continued to Limine,
and the kernel reported `status=absent catalog=17 oemg=0`.
On that lookup result the new helper allocates one additional NVS page to
hold a copy of the record and installs its address under a private EFI GUID.
SrvrOS reads the configuration directory via Limine, verifies the memory type
and translations, requires a unique record adjacent to its 19-page reservation,
and applies the same checksum, contents and GPU admission checks as the ACPI
path. A duplicate or malformed record stops the import.
[Physical run](machines/macbookair7-2-current.md#current-air72-work-on-pr-301)

Release `4ee4688f2965deff` verified the configuration-table fallback on the
reference Air. The kernel imported the bound firmware driver identifier,
all seven original PTEs and the 19-page reservation. The post-reset and
post-reopen checks matched the original mappings, and the reservation
returned to its boot owner. Device-wide traffic exclusion, fresh engine and
L3 acknowledgements, and continuous WiDi exclusion remain prerequisites
for native submission.
