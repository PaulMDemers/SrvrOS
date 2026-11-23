#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from test_trial import CLI, SESSION, VOLUME, framed, trial, write_json
from trial_compare import TRAFFIC


class CompareTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.a = self.make_run("first", "0000000000000001", 18)
        self.b = self.make_run("second", "0000000000000002", 60)

    def make_run(self, name, release, replies):
        folder = self.root / name
        evidence = folder / "return-evidence"
        session = evidence / "sessions" / SESSION
        session.mkdir(parents=True)
        run = dict(model="fixture", release=release, source=f"{name}-commit", workspace_uuid=VOLUME,
                   prior_sessions=[], expected_gateway_replies=60, expected_receipts=[TRAFFIC, "exit.log"])
        write_json(folder / "run.json", run)
        (session / "boot.log").write_text(f"cmdline: srvros.desktop-release={release}\n")
        lines = (f"wifi-desktop-replug5-data: version=1 epoch=1 dhcp=1 gateway_ping=1 dns=1\n"
                 f"{TRAFFIC} version=1 replies={replies} required=60 elapsed_ticks=1000\n"
                 "wifi-desktop-active-remove5-loss: version=1 slot=1 pending_tx=0 pending_rx=1 worker_stopped=1\n"
                 "wifi-desktop-active-remove5-close: version=1 disabled=1 keys_erased=1 provider_retired=1 dma_retained=1\n"
                 f"wifi-desktop-active-remove5-final: version=1 complete={int(replies == 60)} removed=1 sustained={int(replies == 60)} return_safe=1\n")
        (session / "exit.log").write_bytes(framed("exit.log", lines.encode()))
        write_json(evidence / "result.json", dict(release=release, source=run["source"], session=SESSION,
                   gateway_replies=replies, returned_to_ubuntu=True, slot_disabled=True, keys_erased=True,
                   provider_retired=True, workspace_clean=True, dma_retained=True, failure_injected=True,
                   text_save_and_reopen_verified=False))
        write_json(evidence / "comparison.json", dict(release=release, uuid=VOLUME, new_sessions=[SESSION],
                   changed=[], missing=[], fsck_return=0, ubuntu_default_verified=True, recovery_verified=True))
        return folder

    def compare(self, left, right, code=0):
        result = subprocess.run([CLI, "compare", left, right, "--json"], capture_output=True, text=True)
        self.assertEqual(result.returncode, code, result.stderr + result.stdout)
        return json.loads(result.stdout)

    def test_archive_divergence_and_evidence(self):
        d = self.compare(self.a, self.b)
        self.assertEqual(d["first_difference"], "Gateway replies before removal")
        rows = {r["check"]: r for r in d["checks"]}
        traffic = rows[d["first_difference"]]
        self.assertEqual(traffic["stage"], "Traffic")
        self.assertEqual(traffic["a"]["value"], {"observed": 18, "target": 60})
        self.assertFalse(traffic["a"]["passed"])
        self.assertTrue(traffic["b"]["passed"])
        for side, run in (("a", self.a), ("b", self.b)):
            self.assertEqual(Path(traffic[side]["evidence"]), run / "return-evidence/sessions" / SESSION / "exit.log")
        self.assertTrue({"DHCP", "Network worker stopped", "USB slot disabled", "Prior workspace files preserved",
                         "Recovery backup verified"}.issubset(d["shared_passes"]))
        self.assertIn("Safe return after removal", d["shared_passes"])
        self.assertNotIn("Sustained traffic verified", d["shared_passes"])
        self.assertNotIn("Recipe / failure_injected", d["shared_passes"])
        self.assertNotIn("Recipe / text_save_and_reopen_verified", d["shared_passes"])
        self.assertTrue(all(Path(side["evidence"]).exists() for row in d["checks"]
                            for side in (row["a"], row["b"]) if side))
        reverse = self.compare(self.b, self.a)
        self.assertEqual(reverse["first_difference"], d["first_difference"])
        self.assertEqual(self.compare(self.a, self.a)["first_difference"], None)

    def test_unmatched_session_and_missing_observations(self):
        run = json.loads((self.a / "run.json").read_bytes())
        write_json(self.a / "run.json", dict(run, prior_sessions=[SESSION]))
        d = self.compare(self.a, self.b, code=1)
        self.assertFalse(d["complete_log_capture"])
        self.assertEqual(d["first_difference"], "Session match")
        self.assertFalse(d["shared_passes"])
        row = next(row for row in d["checks"] if row["check"] == "Gateway replies before removal")
        self.assertIsNone(row["a"])
        self.assertEqual(row["relation"], "one_sided")

    def test_saved_bundle_and_changed_snapshot(self):
        report, data, _ = trial.collect(self.a, True)
        with patch.object(trial.boot, "ROOT", self.root):
            bundle = trial.save(report, data)
        saved = self.compare(bundle / "return.json", self.b)
        self.assertEqual(saved["first_difference"], self.compare(self.a, self.b)["first_difference"])
        log = bundle / "exit.log"
        log.write_bytes(log.read_bytes().replace(b"replies=18", b"replies=17"))
        self.assertIn("fingerprint", self.compare(bundle, self.b, code=2)["error"])

    def test_contradictory_receipts(self):
        path = self.a / "return-evidence/result.json"
        receipt = json.loads(path.read_bytes())
        for patch_fields in ({"gateway_replies": 19}, {"slot_disabled": False}, {"session": "wrong"}):
            with self.subTest(patch_fields=patch_fields):
                write_json(path, dict(receipt, **patch_fields))
                self.assertEqual(self.compare(self.a, self.b, code=2)["status"], "comparison_error")

    def test_replug_prefix_keeps_its_own_source_link(self):
        folder = self.a / "return-evidence/sessions" / SESSION
        path = folder / "exit.log"
        body = path.read_bytes().split(b"workspace-log: snapshot=")[0]
        first, tail = body.split(b"wifi-desktop-active-remove5-loss:", 1)
        limit = trial.desktop.CAL5_LOG_BYTES
        prefix = framed("session-a.log", first, limit)
        handoff = (f"workspace-log: durable snapshot=session-a.log monotonic_us=100\n"
                   f"workspace-log: Wi-Fi replug continuation session-a.log bytes={len(prefix)}\n").encode()
        (folder / "session-a.log").write_bytes(prefix)
        path.write_bytes(framed("exit.log", prefix + handoff + b"wifi-desktop-active-remove5-loss:" + tail, limit)[len(prefix):])
        d = self.compare(self.a, self.b)
        rows = {r["check"]: r for r in d["checks"]}
        self.assertEqual(d["first_difference"], "Gateway replies before removal")
        self.assertEqual(rows[d["first_difference"]]["a"]["evidence"], str(folder / "session-a.log"))
        self.assertEqual(rows["USB slot disabled"]["a"]["evidence"], str(path))

    def test_duplicate_or_malformed_traffic_line_is_refused(self):
        path = self.a / "return-evidence/sessions" / SESSION / "exit.log"
        body = path.read_bytes().split(b"workspace-log: snapshot=")[0]
        for extra in (f"{TRAFFIC} version=1 replies=18 required=60 elapsed_ticks=1000\n", f"{TRAFFIC}\n"):
            with self.subTest(extra=extra):
                path.write_bytes(framed("exit.log", body + extra.encode()))
                self.assertIn("Duplicate or malformed", self.compare(self.a, self.b, code=2)["error"])

    def test_one_sided_field_is_not_a_measured_difference(self):
        path = self.a / "return-evidence/result.json"
        receipt = json.loads(path.read_bytes())
        write_json(path, dict(receipt, extra_measurement=123))
        d = self.compare(self.a, self.b)
        row = next(r for r in d["checks"] if r["check"] == "Recipe / extra_measurement")
        self.assertEqual(row["relation"], "one_sided")
        self.assertIsNone(row["b"])
        self.assertEqual(d["first_difference"], "Gateway replies before removal")


if __name__ == "__main__":
    unittest.main()
