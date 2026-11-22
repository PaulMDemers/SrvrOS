"""Each actual-core process owns its terminal exclusion state independently."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
OWNED_PAGE_MODES = ("owned-no-grant", "owned-short-import", "owned-vacant", "owned-populated", "owned-before-loss",
                    "owned-rollback-loss", "owned-invalidation", "owned-plane-moved", "owned-mmio-roundtrip", "owned-mmio-wrong-page",
                    "owned-mmio-no-l3", "owned-mmio-widi-loss", "owned-mmio-port-loss")
MODES = list(OWNED_PAGE_MODES) + """normal missing invalid failed-open clock-open stuck-open deadline-open
post-open-clock source-open changed-first changed-last pool-exhausted poison-open
input-mutation reentry source-live clock-live poison-live regress-live failed-close
clock-close changed-close full-buffers linear-tail partial-write pending-readers reuse
capacity last-aperture-page one-page one-page-air partial-restore lost-mapped-owner other-pin
one-page-before-change one-page-apply-change one-page-restore-snapshot one-page-restore-clock
linear-tail-original-change tiled-tail tiled-tail-partial tiled-tail-no-owner tiled-tail-invalid-original
tiled-tail-high-ram tiled-flip tiled-flip-wrong-pixels
pte-store-late-alias pte-store-restore-late-alias pte-store-deadline pte-store-restore-deadline
pte-store-late-deadline pte-store-restore-late-deadline pte-store-cache pte-store-alias
pte-store-foreign pte-store-restore-cache pte-store-restore-alias pte-store-restore-foreign
pte-store-invalidate pte-store-restore-invalidate
firmware-page-air reservation-clean reservation-removed reservation-promote-clock reservation-promote-provider reservation-fault reservation-stale-original boot-framebuffer one-page-no-boot-fence pte-store-aperture pte-store-restore-aperture pte-store-restore-guard-epoch""".split()

for failure in ("sa-preflight", "sa-no-write", "sa-uncertain", "sa-lost-hold", "sa-expired",
                "pre-store-widi", "sa-store-widi", "sa-post-widi", "sa-post-engine", "sa-post-l3", "sa-post-plane", "sa-read-error", "sa-bad-post", "widi-open", "widi-idle", "slice-mask","plane-off","plane-moved","queued-flip","engine-reset-reason","l3-steering","l3-unposted","l3-off","l3-power-lost","register-alias", "no-block", "lost-block", "media-busy", "engine-go", "gam-busy", "l3-unblock",
                "gam-request", "gam-response"):
    MODES.extend((f"pte-store-{failure}", f"pte-store-restore-{failure}"))
MODES.extend(f"pte-store-{client}-before-request" for client in
             ("rcs", "vcs0", "vcs1", "bcs", "vecs", "widi", "l3"))
MODES.extend(f"pte-store-engine-early-ack-{i}" for i in range(5))
MODES.extend(("pte-store-unfilled-l3", "pte-store-widi-early-ack", "pte-store-changed-policy", "pte-store-widi-reopened","pte-store-air-post-reset","pte-store-stale-l3","pte-store-missing-l3",
              "pte-store-frozen-request","pte-store-uncertain-request","pte-store-restore-new-block", "pte-store-restore-retirement-block"))

MODES.extend(f"unblock-{failure}" for failure in
             ("uncertain-release", "widi-open", "widi-stuck", "still-held", "read-failed", "frozen-release", "engine-stuck", "l3-steering","l3-unposted","l3-off","l3-power-lost", "display-changed"))

for phase in ("pte-store-", "pte-store-restore-", "unblock-"):
    for slice_id in range(2):
        for bank in range(4):
            MODES.extend((f"{phase}l3-narrow-{slice_id}-{bank}", f"{phase}l3-missing-{slice_id}-{bank}"))
MODES.append("unblock-engine-reset-reason")

class TransferTests(unittest.TestCase):
    def test_actual_core(self):
        for mode in MODES:
            with self.subTest(mode=mode):
                run = subprocess.run([str(ROOT/'build/tests/scanout-transfer'), mode],
                                     capture_output=True, text=True, timeout=10)
                self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
                self.assertIn('PASS', run.stdout)

if __name__ == '__main__':
    unittest.main()
