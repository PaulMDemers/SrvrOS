#!/usr/bin/env python3
import hashlib
from importlib.machinery import SourceFileLoader
from importlib.util import module_from_spec, spec_from_loader
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

CLI = Path(__file__).with_name("trial")
loader = SourceFileLoader("trial", str(CLI))
trial = module_from_spec(spec_from_loader(loader.name, loader))
loader.exec_module(trial)

RELEASE = "1234567890abcdef"
SESSION = "session-0123456789abcdef-00"
OTHER = "session-0123456789abcdef-01"
VOLUME = "ABCD-0123"


def write_json(path, obj):
    path.write_text(json.dumps(obj))


def framed(name, body, limit=65536):
    footer = f"workspace-log: snapshot={name} monotonic_us=100 total_before={len(body)} tail_limit={limit}\n"
    return (body + footer.encode())[-limit:]


class TrialTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.logs = self.root / "return-evidence/sessions"
        self.logs.mkdir(parents=True)
        self.run = dict(model="Fixture", source="unit-source", release=RELEASE, workspace_uuid=VOLUME,
                        prior_boot_id="old-boot", expected_receipts=["fixture:", "exit.log"])
        self.baseline = dict(uuid=VOLUME, sessions=[], files={})
        write_json(self.root / "run.json", self.run)
        write_json(self.root / "workspace-before.json", self.baseline)

    def session(self, name=SESSION, release=RELEASE):
        folder = self.logs / name
        folder.mkdir()
        (folder / "boot.log").write_bytes(f"cmdline: srvros.desktop-release={release}\n".encode())
        (folder / "exit.log").write_bytes(framed("exit.log", b"fixture: complete\n"))
        return folder

    def report(self):
        return trial.collect(self.root, True)

    def test_archive_cli_and_save(self):
        folder = self.session()
        result = subprocess.run([CLI, "collect", self.root, "--archive"], capture_output=True, check=True)
        report = json.loads(result.stdout)
        self.assertEqual(report["exit_snapshot"], "verified")
        self.assertTrue(all(report["observed_receipts"].values()))
        with patch.object(trial.boot, "ROOT", self.root / "host"):
            saved = trial.save(*self.report()[:2])
            self.assertEqual(saved.stat().st_mode & 0o777, 0o700)
            self.assertEqual((saved / "exit.log").read_bytes(), (folder / "exit.log").read_bytes())
            self.assertEqual(json.loads((saved / "return.json").read_text()), report)
            self.assertNotEqual(saved, trial.save(*self.report()[:2]))

    def test_partial_keeps_periodic_snapshot(self):
        folder = self.session()
        (folder / "exit.log").rename(folder / "session-a.log")
        report, data, complete = self.report()
        self.assertFalse(complete)
        self.assertEqual(report["exit_snapshot"], "missing")
        self.assertIn("session-a.log", data)
        self.assertFalse(report["observed_receipts"]["exit.log"])

    def test_ambiguous_and_unknown_sessions(self):
        self.session()
        folder = self.session(OTHER)
        self.assertEqual(self.report()[0]["session_match"]["status"], "ambiguous")
        (folder / "boot.log").unlink()
        self.assertEqual(self.report()[0]["session_match"]["unidentified"], [OTHER])
        self.assertEqual(self.report()[1], {})

    def test_preexisting_session_and_echoed_identity(self):
        self.session()
        self.baseline["sessions"] = [SESSION]
        write_json(self.root / "workspace-before.json", self.baseline)
        self.assertEqual(self.report()[0]["session_match"]["status"], "no_matching_session")
        folder = self.session(OTHER, "0000000000000001")
        with (folder / "boot.log").open("a") as out:
            out.write(f"srv> echo srvros.desktop-release={RELEASE}\n")
        self.assertEqual(self.report()[0]["session_match"]["other_releases"], [OTHER])
        self.assertFalse(self.report()[2])

    def test_rejects_wrong_uuid_result_and_links(self):
        folder = self.session()
        self.baseline["uuid"] = "1111-2222"
        write_json(self.root / "workspace-before.json", self.baseline)
        with self.assertRaisesRegex(ValueError, "UUID mismatch"):
            self.report()
        self.baseline["uuid"] = VOLUME
        write_json(self.root / "workspace-before.json", self.baseline)
        path = self.root / "return-evidence/result.json"
        write_json(path, dict(session=OTHER, release=RELEASE, source=self.run["source"]))
        with self.assertRaisesRegex(ValueError, "result identity mismatch"):
            self.report()
        path.unlink()
        (folder / "session-a.log").symlink_to(folder / "boot.log")
        with self.assertRaisesRegex(ValueError, "unsafe file"):
            self.report()

    def test_damaged_exit_and_missing_receipt(self):
        folder = self.session()
        path = folder / "exit.log"
        path.write_bytes(path.read_bytes()[2:])
        self.assertEqual(self.report()[0]["exit_snapshot"], "incomplete")
        path.write_bytes(framed("exit.log", b"fixture\n"))
        report, _, complete = self.report()
        self.assertFalse(complete)
        self.assertFalse(report["observed_receipts"]["fixture:"])

    def test_wrapped_snapshot_and_replug_continuation(self):
        self.assertEqual(trial.exit_snapshot({"exit.log": framed("exit.log", b"line\n"*100, 128)}), "verified")
        prefix = framed("session-a.log", b"test\n", trial.desktop.CAL5_LOG_BYTES)
        body = (f"workspace-log: durable snapshot=session-a.log monotonic_us=100\n"
                f"workspace-log: Wi-Fi replug continuation session-a.log bytes={len(prefix)}\n").encode()
        raw = framed("exit.log", prefix + body, trial.desktop.CAL5_LOG_BYTES)[len(prefix):]
        data = {"exit.log": raw, "session-a.log": prefix}
        self.assertEqual(trial.exit_snapshot(data), "verified_continuation")
        data["session-a.log"] = prefix[1:]
        self.assertEqual(trial.exit_snapshot(data), "incomplete")

    def test_recovery_requires_both_installed_and_archived_payloads(self):
        previous = "1122334455667788"
        archive = self.root / "recovery"
        self.run.update(recovery_release=previous, recovery_archive=str(archive))
        write_json(self.root / "state.json", dict(current=RELEASE, previous=previous))
        manifests = [{"release": previous}, {"release": previous}, {"release": previous}, {"release": RELEASE}]
        with patch.object(trial.boot, "BASE", self.root), patch.object(trial.boot, "verify", side_effect=manifests), \
             patch.object(Path, "read_text", return_value=""), patch.object(trial.host, "check_ubuntu_default"):
            self.assertEqual(trial.recovery_checks(self.run)["installed_recovery"], "verified")
            self.assertIn("differs", trial.recovery_checks(self.run)["installed_recovery"])
            write_json(self.root / "state.json", dict(current=previous, previous=RELEASE))
            self.assertIn("pair differs", trial.recovery_checks(self.run)["installed_recovery"])

    def test_live_volume_and_old_boot(self):
        self.session()
        work = self.root / "srvros-work"
        work.mkdir()
        self.logs.rename(work / "Logs")
        (work / ".workspace").write_bytes(b"SRVROS-WORKSPACE-V1 abcd0123\n")
        (work / "kept").write_bytes(b"proof")
        entry = {"sha256": hashlib.sha256(b"proof").hexdigest(), "size": 5}
        self.baseline["files"] = dict(kept=entry, changed=entry, missing=entry)
        (work / "changed").write_bytes(b"other")
        write_json(self.root / "workspace-before.json", self.baseline)
        with patch.object(trial.host, "volume_mount", return_value=self.root) as resolve, \
             patch.object(trial, "recovery_checks", return_value={"ubuntu_default": "verified"}), \
             patch.object(Path, "read_text", side_effect=["Fixture", "old-boot", "Fixture", "new-boot"]):
            with self.assertRaisesRegex(ValueError, "pre-trial"):
                trial.collect(self.root, False)
            report, _, complete = trial.collect(self.root, False)
            self.assertTrue(complete)
            resolve.assert_called_once_with("abcd0123")
            self.assertEqual(report["workspace_changes"], {"prior_files": 3, "changed": ["changed"], "missing": ["missing"]})


if __name__ == "__main__":
    unittest.main()
