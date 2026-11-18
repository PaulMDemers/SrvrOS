# Broadwell primary-plane address envelope

`intel_bdw_primary_range` conservatively classifies two raw snapshots of one
primary-A route. It computes the complete padded image and required translation
tail, joins shared programmed/live/control fields, and checks a candidate against
the **entire protected fence**. The arithmetic has no I/O, callbacks, transfer
token or GPU write. The isolated wire10 diagnostic links it to a fresh native
capture under the original full-GT owner.

This resolves a layout prerequisite for the [command mapping transaction](intel-command-mapping.md).
It does not resolve the remaining [native ownership protocol](intel-tearfree-roadmap.md).
In particular, a positive result cannot clear any of the255 completion obligations
or supply a `HANDOFF_*` assurance. Input snapshots alone are never live authority.

## Register interpretation and references

Intel's [Broadwell PRM Volume2c, November2015](https://cdrdv2.intel.com/v1/dl/getcontent/690789),
SHA256 `09b48addae09923e35dcbf0d07fcf8d578f060ac68d58c42bb9b66a76a7e0742`,
defines PIPE_SRCSZ on printed pages1218–1219, PRI_CTL on1247–1250,
PRI_OFFSET on1252–1253, PRI_STRIDE on1254 and PRI_SURF on1255–1256.
The PRI_SURF workaround on1256 requires128 extra PTEs after the displayed surface;
the same workaround requires256KiB surface alignment, which the classifier
also checks. The manual explicitly distinguishes these translations from
backing pages.

Intel's [Broadwell PRM Volume5, Memory Views](https://www.x.org/docs/intel/BDW/intel-gfx-prm-osrc-bdw-vol05-memory_views_0.pdf),
SHA256 `235a5359a66f17d4d037d439f644de540a362deb37ca0db50dc2706e99f7c43c`,
printed pages9–10 describes4KiB X tiles as512bytes by8rows. The classifier
rounds the height up to8rows for X tiling and uses the full stride for every row.
Linear rows are not tile-rounded. It then page-rounds the image end and reserves
another128 translations. The tail must fit inside both the protected fence and
actual GGTT capacity. It is never exposed as private command address space.

## Conservative supported input

Input contains two source-size/9-word pipe/13-word reader snapshots using the
existing [display capture](../kernel/include/srvros/intel_display_capture.h) and
[pinned-reader layouts](intel-pinned-reader-audit.md). The caller must already
have qualified the machine, register power/resources, full fence and capture
lifetime. This pure function does not certify that those reads happened or that
the inputs share an original owner. It never changes them.

Only primaryA may be enabled:32-bit BGRX8888, linear or X-tiled, zero panning,
no rotation or asynchronous updates. All programmed/live right/left primary
eye addresses must coincide. A zero left base does not prove stereo is disabled;
a nonzero right base with zero left base refuses. The classifier does not
certify stereo signalling or any other uncaptured mode. Gamma/CSC flags do not
change storage size and are accepted; unsupported control/reserved bits refuse.
Other primary, cursor and sprite controls must be zero. Enabled readers on a
second pipe are unsupported, even if their bases appear far away.

Both raw samples must agree on source, geometry and every reader word. Shared
control/programmed/live/cursor fields must agree between the two register layouts.
Scan-line/frame counters are deliberately excluded from geometry equality.
Floating geometry/reader values, unsupported format/layout, insufficient or
misaligned stride, mismatched programmed/live bases and invalid source encodings
refuse. Width/height are conservatively bounded to4096; stride is at most32768
and aligned64bytes for linear or512bytes for X tiles.

Range checks cover aperture/GGTT capacity, page alignment, page count and all
endpoints before narrowing. Image plus translation tail must fit inside the
caller-provided, already validated full protected fence. Every byte of that
fence remains unavailable to the candidate, including unused padding well past
the image. Other protected fences/readers still need independent validation;
this single-fence calculation does not establish a whole-device map. A rejection clears output spans; input/output overlap refuses without
modifying either. Success describes observed addresses only, not reusable pages.

## Validation and current boundary

`make test-intel-bdw-primary-range` exercises18,216 geometry/control/stride/
height/padding/capacity/overlap cases, twelve compiled weakening controls, fatal
ASan/UBSan, native compiler2KiB frames and image isolation. A sanitized Air7,2
fixture from source37b54934/releasea38270ffedf462fa carries the original raw-exit
hash, earlier display geometry and original full-GT reader rows. Its offline
replay yields1440×900, stride6144, X tiling, padded image end5,554,176 and
translation-tail end6,078,464bytes, all inside the protected34MiB fence.
The four candidate pages begin at34MiB. The unobserved gap between the tail and
fence end is still rejected as protected. This replay adds no physical acceptance
or same-phase/lifetime proof; the original hardware record remains unchanged.

The frozen workflow includes the fixture JSON bytes, the new classifier target
and this suite in both development and qualification. All42 existing job slots
remain; no previous frozen receipt is reused as qualification of changed code.

## Post-maintenance wire10 capture

The `intel-gt-primary-v57` profile takes two fresh source-size, primary
geometry and reader samples after the wire9 translation comparison and before
restoring the requester. Both samples use the same live boot owner, original
borrow, engine epoch, CPU root and deadlines. Read-only UC aliases bracket
each sample with display-well controls. Ten temporary aliases retire, and
the complete 112 DWORD reads must agree with the original boot and held reset
records. The pure checker then joins that observed display footprint to the
full inherited fence and the prospective single slot. The receiver checks
sample order, aliases, both raw packets and exact source provenance. A refused
sample keeps the enclosing lifetime incomplete.

A native provider still needs documented completion of outstanding and
prefetched accesses, engine pipelines and caches, firmware, and other readers
such as VGA, FBC and PSR. The captured control state and WC/UC observations
leave each of those completion requirements open.

### Native wire10 result

MacBookAir7,2 ran release `2945aece351aa904` from source `be7bad20` and
returned to Ubuntu automatically. Both live packets matched the original
display records. They confirmed the 1440×900 X-tiled primary, a padded image
end of 5,554,176 bytes and a translation-tail end of 6,078,464 bytes inside
the protected 34 MiB fence. The prospective slot starts at the fence end.
All ten temporary display aliases retired after 112 reads. The two
sampling windows took 2.426 ms and 2.424 ms.

The preceding comparison again matched all 129 translations before and
after reset. Preparation took 226.112 ms and the enclosing trial took
462.277 ms under the diagnostic policy. Stricter timing qualification
remains pending. Automatic collection verified the 75,111-byte exit log,
the matching checkpoint, offline USB integrity and Ubuntu recovery. The
run closed as `DIAGNOSED`.

## Auxiliary display readers under the same hold

The wire11 auxiliary profile brackets primary geometry with two snapshots
of the inherited display controls, inside the existing power-well, original
requester and clock checks. It reads VGA control, FBC control and compressed
segment status, and eDP routing, PSR control and PSR state. Each sample uses
four read-only UC aliases and verifies their retirement. The collector
independently checks the raw words, clock brackets, resource counts and blocker
bits. [Intel Broadwell display manual](https://cdrdv2-public.intel.com/690828/intel-gfx-prm-osrc-bdw-vol-11-display.pdf) · [Intel i915 VGA register definitions](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/i915_reg.h) · [Intel i915 FBC implementation](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/display/intel_fbc.c) · [Intel i915 PSR register definitions](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/display/intel_psr_regs.h).

The blocker flags record VGA enabled, FBC enabled, nonzero compressed
segments, PSR enabled, PSR state other than idle, and an unsupported or
changed eDP route. Stable inactive observations require both packets to agree
and every blocker to be clear. Known activity or drift remains a valid
diagnostic result. An inaccessible register, failed alias retirement or
lost parent deadline makes the enclosing diagnostic incomplete.

The snapshot describes the current endpoints. The Intel VGA disable sequence
and PSR idle wait address transitions controlled by their driver. Outstanding
accesses, prefetched requests, write visibility and hardware ownership still
require independent evidence. [Intel Broadwell display manual](https://cdrdv2-public.intel.com/690828/intel-gfx-prm-osrc-bdw-vol-11-display.pdf) · [Intel i915 PSR implementation](https://raw.githubusercontent.com/torvalds/linux/v6.12/drivers/gpu/drm/i915/display/intel_psr.c).
