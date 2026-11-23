#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import trial_qualify as qualifier


class QualificationTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.operations = []
        self.release = "1234567890abcdef"
        self.manifest = {"git_head": "built-source", "release": self.release}
        self.fail = None

    def worker(self, operation, source, stage, run, volume, qemu, release=None):
        self.operations.append((operation, stage, release))
        if operation == self.fail:
            raise RuntimeError("simulated failure")
        if operation == "stage":
            folder = stage / self.release
            folder.mkdir(parents=True)
            qualifier.write_json(folder / "manifest.json", self.manifest)
            self.digest = hashlib.sha256((folder / "manifest.json").read_bytes()).hexdigest()
            qualifier.write_json(run / "stage.json", dict(release=self.release, manifest=self.manifest,
                                                          manifest_sha256=self.digest))
            return
        result = {"release": release, "manifest_sha256": self.digest, "status": "PASS"}
        if operation == "present":
            result.update(retina_desktop=True, retina_fractional=True, workspace_table="gpt",
                          saved=True, unmounted=True, automatic_return=True)
            if self.fail == "swapped":
                result["release"] = "0000000000000000"
        else:
            result.update(retina_missing_workspace=True, returned=True)
            if self.fail == "changed_manifest":
                result["manifest_sha256"] = "0" * 64
            if self.fail == "tampered_stage":
                (stage / release / "manifest.json").write_bytes(b"changed")
            if self.fail == "wrong_case":
                result["retina_missing_workspace"] = False
        path = run / operation / qualifier.RESULT
        path.parent.mkdir(parents=True)
        qualifier.write_json(path, result)

    def qualify(self):
        with patch.object(qualifier, "run_step", self.worker):
            path, passed = qualifier.qualify(self.root, "53525452", self.root / "results",
                                             self.root / "stages", "qemu")
        self.assertTrue(path.is_file())
        return path, passed, json.loads(path.read_bytes())

    def test_one_stage_two_pinned_cases_isolated_on_repeat(self):
        path, passed, report = self.qualify()
        self.assertTrue(passed)
        stage = Path(report["stage"])
        self.assertEqual(self.operations, [("stage", stage, None), ("present", stage, self.release),
                                           ("missing", stage, self.release)])
        self.assertEqual(report["manifest"], self.manifest)
        self.assertEqual(report["cases"]["missing"]["manifest_sha256"], self.digest)
        second, _, retry = self.qualify()
        self.assertNotEqual(second, path)
        self.assertNotEqual(retry["stage"], report["stage"])

    def test_cases_are_independent_and_evidence_is_bound(self):
        for failure in ("present", "swapped", "changed_manifest", "tampered_stage", "wrong_case"):
            with self.subTest(failure=failure):
                self.fail = failure
                self.operations.clear()
                _, passed, report = self.qualify()
                self.assertFalse(passed)
                self.assertEqual(report["status"], "failed")
                self.assertEqual([operation for operation, _, _ in self.operations], ["stage", "present", "missing"])
                if failure == "present":
                    self.assertEqual(report["cases"]["missing"]["status"], "PASS")

    def test_stage_failure_writes_receipt_and_skips_vms(self):
        self.fail = "stage"
        _, passed, report = self.qualify()
        self.assertFalse(passed)
        self.assertEqual(len(self.operations), 1)
        self.assertIn("simulated failure", report["error"])


if __name__ == "__main__":
    unittest.main()
