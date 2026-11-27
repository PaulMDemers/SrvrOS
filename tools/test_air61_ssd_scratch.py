import tempfile
import unittest
from pathlib import Path
import uuid

import air61_ssd_scratch as scratch
from air61_ssd_probe import SECTOR, SECTORS, verify_gpt
from test_air61_ssd_probe import reset_trace

VOLUME = "6ada85bb"


class ScratchTest(unittest.TestCase):
    def setUp(self):
        self.primary, self.backup = scratch.gpt(*(uuid.UUID(int=n) for n in (1, 2, 3)))
        self.fixture = scratch.sha(self.primary) + scratch.sha(self.backup)
        self.write_hash = scratch.sha(scratch.scratch_pages(self.fixture, written=True))
        self.plan = dict(fixture=self.fixture, workspace_volume=VOLUME, written_pages_sha256=self.write_hash)
        self.profile = "air61-scratch: profile " + scratch.profile(VOLUME, self.fixture)
        self.trace = reset_trace("unused", VOLUME)[1:]

    def receipt(self, phase):
        writes, flushes = (8, 1) if phase == "write" else (0, 0)
        return "\n".join([
            self.profile, f"air61-scratch: phase={phase}",
            f"air61-scratch: pages-sha256={self.write_hash} lba=600008 sectors=8 writes={writes} flushes={flushes}",
            *self.trace, "air61-scratch: result=PASS internal_mounted=no",
        ])

    def test_fixture_layout_and_guard_pages(self):
        self.assertEqual(verify_gpt(self.primary, self.backup), 2)
        self.assertLess(scratch.ESP_LBA + scratch.ESP_BYTES // SECTOR, scratch.SCRATCH_LBA)
        other_primary, _ = scratch.gpt(*(uuid.UUID(int=n) for n in (4, 2, 3)))
        self.assertNotEqual(self.primary, other_primary)
        initial = scratch.scratch_pages(self.fixture)
        written = scratch.scratch_pages(self.fixture, written=True)
        page = scratch.PAGE
        self.assertEqual(initial[:page], written[:page])
        self.assertEqual(initial[2 * page:], written[2 * page:])
        self.assertNotEqual(initial[page:2 * page], written[page:2 * page])
        self.assertEqual(len(written), 3 * page)

    def test_two_receipts_are_bound_to_one_fixture_and_retirement(self):
        armed = self.profile + "\nair61-scratch: phase=write"
        scratch.compare_log(armed, self.plan, "armed")
        for phase in ("write", "verify"):
            self.assertEqual(scratch.compare_log(self.receipt(phase), self.plan, phase)["status"], "ready")
        for bad in (
            self.receipt("write"),
            self.receipt("verify").replace("writes=0", "writes=1"),
            self.receipt("verify").replace("flushes=0", "flushes=1"),
            self.receipt("verify").replace("lba=600008", "lba=600000"),
            self.receipt("verify").replace(self.write_hash, "0" * 64),
            self.receipt("verify").replace(self.fixture, "0" * 128),
            self.receipt("verify").replace(self.trace[-1], self.trace[-1].replace(
                "cmd=0x0000000000000000", "cmd=0x0000000000000010")),
        ):
            with self.assertRaises(ValueError):
                scratch.compare_log(bad, self.plan, "verify")
        with self.assertRaisesRegex(ValueError, "Invalid attempt"):
            scratch.compare_log(self.receipt("write"), self.plan, "armed")

    def test_reference_rejects_changes_and_non_regular_targets(self):
        with tempfile.TemporaryDirectory() as path:
            image = Path(path) / scratch.IMAGE
            regions = [(0, self.primary), (scratch.ESP_LBA, bytes(scratch.ESP_BYTES)),
                       (scratch.SCRATCH_LBA, scratch.scratch_pages(self.fixture)), (SECTORS - 33, self.backup)]
            with image.open("xb") as stream:
                stream.truncate(SECTORS * SECTOR)
                for lba, content in regions:
                    if lba != scratch.ESP_LBA:
                        stream.seek(lba * SECTOR)
                        stream.write(content)
            self.plan.update(sectors=SECTORS, regions=[
                dict(lba=lba, sectors=len(content) // SECTOR, sha256=scratch.sha(content))
                for lba, content in regions])
            scratch.verify_image(image, self.plan)
            with image.open("r+b") as stream:
                stream.seek(scratch.SCRATCH_LBA * SECTOR)
                stream.write(b"bad")
            with self.assertRaisesRegex(ValueError, "Fixture bytes changed"):
                scratch.verify_image(image, self.plan)
            alias = Path(path) / "alias"
            alias.symlink_to(image)
            with self.assertRaisesRegex(ValueError, "regular fixture"):
                scratch.verify_image(alias, self.plan)
