# Broadwell private BCS execlist submission

`intel_bdw_bcs_submit` is the bounded software submit step for an already owned,
initialized private blitter context. It fills the port-level gap beneath the
[private flush/nonce protocol](intel-bdw-bcs-flush.md), without creating its own
GPU address ownership. No native image or adapter calls it. An inherited context,
stable queue sample or successful reset cannot supply its prerequisites.

## Hardware basis and ruled-out shortcut

Intel's [Broadwell PRM Volume2c](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
printed pages438–440 defines the WO32-bit Execlist Submit Port, including BCS
`0x22230`. A one-context list uses element0; the other element is invalid. The
required write order is element1 high/low, then element0 high/low. All four
DWORDs are required. The last write triggers submission.

Intel's [Broadwell PRM Volume6, Command Stream Programming](https://cdrdv2.intel.com/v1/dl/getcontent/690866),
SHA256 `603855f24d6cb55f13ed6978796ccf12cf6fef9df7ccf3d704b7c79c810d7cdf`,
printed pages15–17 requires execlist mode, available queue capacity and prior
context-image initialization, including ring registers and page directory. Even
the first list does not necessarily become current immediately. Issuing this
packet proves neither acceptance/execution nor command/context retirement.

The older MMIO sync-flush shortcut is unavailable: Broadwell RCS INSTPM bit5
is reserved on PRM2c826; VCS bits6:5 are reserved on1698. Older-generation
Sync Flush Enable recipes must not be copied here. The same manual's GDRST
reset acknowledgement on609–610 does not specify a whole-device outstanding
memory drain. The existing remapper and System Agent operations retain their
separate documented scope. This audit supplies no substitute ownership grant.

## Original source and independent provider

The one-use source binds original owner/provider/lifetime/context, root, epoch,
sequence, opaque descriptor, private ring/context/completion GPU/physical spans,
full protected fence and all original aggregate clock origins. Context storage
is two pages; ring and completion are one each. GPU spans must be disjoint,
aligned, below4GiB and outside the fence. Physical spans must be disjoint,
aligned and below4GiB. The aligned CPU root page must also fit below4GiB and
remain outside private RAM; broader physical addressing is not admitted here.
The exact8086:1626 match is a structural narrowing, not hardware admission.

Only minimum descriptor structure is checked: valid element0, matching aligned
LRCA, conservative21-bit software context ID and known reserved fields. This
component does **not** encode or certify the remaining context/addressing/cache
fields, page directory, ID allocation or executable LRC initialization. Those
are mandatory independent provider duties, not facts inferred from this input.
No bit5 cache/coherence meaning is invented from the opaque descriptor.

Before each port write and after the trigger, a fresh proof must establish:

- actual execlist mode and the original source/epoch/resource/cache/exclusion;
- the initialized private context and exact head0/tail24 command image;
- reserved queue capacity, no competing poster and known partial FIFO position;
- the actual CPU/GPU mappings, backing and generation of all private pages;
- GPU visibility of context and command contents before submission.

Proofs name the source/context/descriptor/root/epoch/sequence, exact known prefix
and a timestamp bracketed by source clocks. Prefix0–3 retains the reserved queue;
prefix4 permits that context to have begun executing, without claiming any
acknowledgement. A native provider must validate real hardware immediately before
an ordered UC DWORD store, after its final clock. Descriptor echoes are not proofs.
The trusted serialized-kernel contract is not a malicious-memory-write boundary.

## Effects, clocks and failure

The four values are0,0,descriptor-high,descriptor-low. Definitely issued DWORDs
are recorded separately from attempts and uncertainty. A trigger is recorded
only for a known fourth store. All callbacks, tables, binding/registration,
packet and progress data are sealed across callbacks, including clocks. Failed
callbacks preserve the real known-effect prefix even if they corrupted protocol
progress. Original registration and consumed state are restored on failure;
hardware state is never guessed or rolled back.

The source's admitted original-parent timestamp is the first clock floor and
start of the independent10ms port bound. Preparation/GT/HDA/outer samples use
the existing selected timing aliases:100/150ms legacy policies retain their
original300/400/500ms parents; the separate practical selection uses250/500/
750/1000ms. No origin restarts. Exact endpoints pass; zero, backward, over-limit
and wholly frozen clocks refuse. The final ownership proof counts in the bound.

An issued result means only the validated four-write sequence. Source registration
remains held on success as well as failure. No retry, cancellation, rollback,
unmap/free or retirement API exists here. A partial FIFO, unknown store, lost
post-trigger proof or timeout retains the original context/backing. Actual
command completion and context/mapping retirement must be established separately.

## Joined completion validation

`make test-intel-bdw-bcs-submit` runs2,519 callback/binding/progress/packet/proof/
clock/prefix cases under each of three policies,24 compiled weakening controls,
fatal ASan/UBSan and native compiler2KiB frames/image isolation. Lifetime controls
must preserve the ordinary positive before the complete fault matrix detects
the weakened invariant; wrong packet/order controls must fail that positive.

The actual submit and flush cores are also joined in35 cases per policy.
Publication precedes port access and the original submit-through-completion
10ms interval contains the nested operation. Tests cover immediate and256th
nonce completion, never/wrong/stale nonce, every partial/uncertain port prefix,
lost requirements and lost verification after the fourth trigger. Partial or
uncertain port outcomes cannot be translated to definitely unissued or retried.
Neither component can release the synthetic held backing. A220ms-aged original
preparation completes only under the selected practical policy; both legacy
policies refuse before publication/port access. All joined cases run fatal
sanitizers. The existing flush core retains its2,326 cases and24 controls, now
also checked under the practical selection without changing legacy values.

These are host software results. Future frozen v45 validation includes all nine
standalone/joined targets and both suites in development and qualification,
retaining all42 job slots. This does not requalify a prior frozen native run.
Native ownership, initialized private LRC/page-directory/cache policy and actual
ordered port access/queue acceptance/nonce/context retirement remain unaccepted.
All255 whole-device obligations and the first-GPU-write refusal remain. There
is no new hardware question until the real bootstrap ownership witness exists.
