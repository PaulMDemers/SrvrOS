# Broadwell System Agent GGTT invalidation

`intel_ggtt_invalidate` implements an isolated, one-use ordered register sequence.
A separately selected combined native adapter is now implemented; native
qualification and physical acceptance of that new image remain pending. It
neither changes a GGTT entry nor enables GPU mapping, submission or tear-free scanout.

## Hardware scope

Broadwell PRM Volume12, **PCIE Configuration Registers**, defines
`MPGFXTRK_CR_GFX_FLSH_CNTL_0_2_0_GTTMMADR` at **MMIO+0x101008**,32bits.
A CPU write flushes the System Agent graphics TLBs; write data is discarded and
reads return zero. The retained November2015 manual, printed page5, has SHA256
`68ea001025f0ebfe93b1f72be1a51b4debb8b6c8fe0eab138f7e3079c023f1ca`.
The [published May2015 register definition](https://www.x.org/docs/intel/BDW/intel-gfx-prm-osrc-bdw-vol12-pcie_config_registers_0.pdf)
gives the same offset/access/behavior on printed page6; it is a different
edition, not a claim that its PDF hash equals the retained one.

Pinned [Linuxv6.12 GGTT implementation](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/i915/gt/intel_ggtt.c)
uses an uncore-serialized DWORD1 at this register. `gen6_ggtt_invalidate` follows
it with a read; `gen8_ggtt_invalidate` documents the UC trigger ordering against
prior WC GGTT writes. The conservative core retains the posting read. This is
an ordering operation, **not a self-clear acknowledgement**: the register reads
zero even before a write. No polling, busy-bit inference or invented ACK is used.

This differs from the [five per-engine TLB requests](intel-bdw-tlb-protocol.md).
Their documented pipeline-flush/memory-retirement prerequisites remain unmet;
this System Agent operation must not be substituted as those prerequisites.
It does not flush GPU data caches or prove retirement of requests still inside
engines, prefetch, firmware or display readers. Native execution and the exact
resource/ordering path still need a substantive new trial before acceptance.

## Original source and failure rules

A zero-initialized descriptor binds the original owner/provider/lifetime,
root, epoch, sequence, exact MMIO resource and four original clock origins.
The conservative profile is8086:1626. The source registers its consumer before
any callback. A description or matching GPU ID is not hardware authority; the
trusted provider must independently match the actual original graph.

1. Guard and read the exact register. Require its documented zero value and
   reject floating/reserved values before a write.
2. Guard again and issue exactly one ordered DWORD1. Record definitely issued,
   definitely not issued or uncertain outcome before any further callback.
3. Only after definite issuance, guard and read that same register to complete
   the ordered posting-read sequence. Require the documented zero, with no loop.
4. Require a final guard and complete record: two reads, four guards, one attempt,
   one definitely issued write, unchanged source/callback/progress identity and
   advancing original time. Keep the source consumed after completion.

Every callback is bracketed by original binding/progress checks. Copied/rebound
objects, reentry, a lost consumer, changed resource/root/generation, callback
mutation, zero/backward/expired/frozen time and any failed I/O retain the
original lifetime. No forced unmap, retry, request clearing or release API exists.
A no-write refusal still needs its owner's normal safe retirement; it is not a
new permission to release backing. Any possible store remains recorded on fault.

Default preparation is100ms. A separately compiled child of the explicit VT-d
diagnostic selects its existing150ms policy; the core starts neither budget
afresh. GT300ms/HDA400ms/outer500ms remain unchanged. Both policy binaries must
pass each exact endpoint and endpoint+1us refusal. This defines a future
composition policy; it does not add this core to the accepted VT-d image.

## Native adapter contract and next checkpoint

The next implementation must join the exact original held child, rather than
replay a token from the earlier accepted remapper drain. It must:

- Match the actual8086:1626 resource/profile, captured MMIO BAR/extent, original
  source/root/epoch and live requester/five-engine/firmware/HDA graph. Admit the
  specific101000 page; do not widen another page's register allowlist or reuse
  the remapper's physical address as a GPU BAR.
- Register before callbacks and map a dedicated, originally empty4KiB VA as
  supervisor/writable/NX with independently derived UC policy. First-touch
  receipt storage before the excluded lifetime. Keep all original backing held.
- Validate actual alias, resource, cache and original graph after the provider's
  final clock immediately before each ordered DWORD access. Clock callbacks
  themselves supply no hardware authority. Fences/order must reach the posting
  read; an ordinary cached load or a constant zero stub is insufficient.
- Preserve the precise issued/read prefixes, original clocks, selected policy,
  MMIO/alias identity and retirement counters in a distinct strict receipt.
  Bind its checkpoint and complete result to that same original held child.
  Legacy receipts must not acquire this new scope; raw files remain unmodified.
- On success, remove the exact alias, verify absence and policy, unregister and
  require complete enclosing engine/requester/source/reset/HDA/root/private-RAM
  retirement. Any lost order/identity/return retains the original graph visibly.

Measure the combined original envelope before freezing a physical trial. The
latest accepted remapper lifetime takes102,810/150,000us preparation/return,
but its full wake bracket is342,986/500,000us and the older GT lease is already
near its300ms limit. The extra aggregate allowance does not extend that parent;
there is no promise that two separately fitting operations fit when composed.
Avoid repeating the known zero observation or extending parent limits to fit
unmeasured adapter overhead. A distinct single-operation trial or measured
shared integration must name its exact new hardware question and preserve
all original deadlines and witnesses.

## Local validation

`make test-intel-ggtt-invalidate` runs the actual C protocol under both100ms and
150ms policies,1,248 cases each. The independent model checks exact register,
DWORD value and read/write order,16 source/registration fields and46 result/
callback/progress changes at all18 callback boundaries, reentry, missing callbacks,
overlapping/copied objects, every clock failure, failed guards/reads/stores,
unknown store outcomes and all four exact/+1us endpoints. Fatal ASan/UBSan runs
both complete matrices. Sixteen compiled weakening controls cover register,
store/posting-read, known issuance, source/root/generation, progress, reserved
reads, frozen time and all four deadline extensions.

The actual native Zig flags compile the core below the2KiB frame limit. Image
plan checks keep ordinary, v35 and v36 payloads free of this new caller. These
are host/build results only. All255 hardware obligations, zero assurances/
managed GPU pages and the [current native acceptance](machines/macbookair7-2-current.md)
remain unchanged.


## Combined original-held native candidate

`--intel-ggtt-invalidate` selects `build/intel-ggtt-native/srvros.elf` and the
workflow profile `intel-ggtt-invalidate-v36`. It preserves the original15-token
pre-init VT-d recipe and wake36, but pins a distinct configuration/payload and
**version4** receipt. The old `--intel-vtd-drain` image stays version3 and never
links the new writer. Ordinary and older images keep their existing behavior.

The new child runs after the original CPU/audit/remapper work, while that same
source, requester and five-engine hold remain live. The remapper's exact alias
must already be retired and its full completed record must still match. The new
child independently registers and maps GPU MMIO+101000 at a dedicated4KiB VA,
with supervisor/writable/NX and fresh independently derived UC policy. It never
reuses the remapper's physical page or broadens another alias's access list.

Each of the two loads and single DWORD1 store admits only101008, original I/O
phase and counts, actual alias/policy/registration, then fresh full hardware
validation after its own final clock. Actual native accesses use MFENCE around
volatile32bit MMIO. Core guards retain full hardware admission as well. Success
requires exact unmap/absence, fresh policy/clock checks, registration release and
pure complete-record validation before the requester may restore Command7.
Any uncertain operation/alias/parent/return keeps the original graph retained.

A separate432byte canonical packet carries54 unsigned64bit values: original
binding and clocks, all core and native progress/first failure, exact UC alias,
map/retirement/registration state, callback counts and selected150000us policy.
Version4 requires exactly this packet; old receipts cannot acquire its scope.
The receiver independently binds its BAR to captured PCI identity and computes
its cache policy from the original complete CPU context. Raw-first collection
still preserves files before parsing. The complete original native chain, not
just either child, is required for any reviewed bounded acceptance.

Both modeled original routes complete. The new adapter's672 target/alias/cache/
clock/final-record fault cases refuse acceptance; strict receipt/checkpoint
mutations and eight compiled admission/return controls pass, as do12 fatal
sanitizer lifetimes. Actual image/VM/frozen full qualification and matching
physical timing remain the next gate. A saved safe outcome returns automatically
at1% volume; uncertain live ownership halts visibly. No input is required, no
parent deadline is extended and all255 GPU hardware obligations remain open.


The host-only follow-up uses stage configuration revision3 without changing
kernel/receiver bodies or wire4; revisions1/2 remain readable. Fast image/fixture/
staging checks include both VM profile classification and explicit Broadwell TCG
CPU selection. Exact-stage save/return and missing-workspace VMs now precede long
qualification, with receipt hashes verified again afterward. All long host gates,
ordinary/v35 exclusions and combined native link checks remain required. Earlier
failed/cancelled host runs and their payloads stay distinct; none establishes
hardware acceptance.

## First bounded native combined acceptance

Air7,2 runtime `fe3919ec` / release `451221606bdf74f7`, session
`session-0006605896ca2c5f-00`, accepts one original kind2/epoch1/mode1 combined
remapper/GGTT lifetime after all26 frozen host gates and both exact-stage VMs.
The exact101008 DWORD1/posting read, independently derived UC/NX alias and
original source/root/clock binding verify. GGTT mapping through verified retirement
takes752us, with two loads, one known store,13 native parent checks,10 cache and
12 alias checks. Full preparation/verified return is103,582/150,000us; original
parent bounds remain. Both temporary command aliases and the full original
diagnostic chain retire; private pages, files, offline USB and recovery verify.
See the [unit evidence](machines/macbookair7-2.md#native-combined-remapper-and-system-agent-invalidation-accepted).

This is ordered System Agent invalidation, not a self-clear ACK, pipeline/cache/
prefetch/reader completion proof or a private GPU mapping. Enabled-route and
repeatability acceptance remain pending. All255 hardware obligations and zero
GPU assurances/managed pages remain; no historical receipt becomes live authority.

## Original full-GT composition

The separate [wire6 full-GT memory child](intel-bdw-gt-bootstrap.md#same-owner-post-reset-memory-preparation-wire6)
composes this operation after its remapper drain on the original post-reset GT
borrow. It checks its own exact UC alias and retires both aliases before parent
return, then verifies a fresh census. Selected practical aggregate clocks are
inherited without changing older defaults. The posting read remains a posting
read; zero does not become a completion ACK or whole-device ownership witness.
Native composition acceptance is pending.
