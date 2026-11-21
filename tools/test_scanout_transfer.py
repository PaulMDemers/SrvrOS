"""Each actual-core process owns its terminal exclusion state independently."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODES = """normal missing invalid failed-open clock-open stuck-open deadline-open
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
reservation-clean reservation-removed reservation-promote-clock reservation-promote-provider reservation-fault reservation-stale-original boot-framebuffer one-page-no-boot-fence pte-store-aperture pte-store-restore-aperture pte-store-restore-guard-epoch""".split()

for failure in ("register-alias", "no-block", "lost-block", "media-busy", "engine-go", "gam-busy", "l3-unblock",
                "gam-request", "gam-response"):
    MODES.extend((f"pte-store-{failure}", f"pte-store-restore-{failure}"))
MODES.extend(("pte-store-restore-new-block", "pte-store-restore-retirement-block"))

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
