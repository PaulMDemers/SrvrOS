#!/usr/bin/env python3
"""Check installer rollback/retention without root or the host EFI filesystem."""
import contextlib
import io
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import hardware_boot as boot


class InstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.stage = self.root / "stage"
        self.esp = self.root / "efi"
        self.base = self.esp / "EFI/srvros"
        self.snippet = self.root / "42_srvros"
        self.cfg = self.root / "grub.cfg"
        self.stage.mkdir()
        self.esp.mkdir()
        self.cfg.write_text("original Ubuntu config\n")
        self.fail_update = False
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        for name, value in {"STAGE": self.stage, "ESP": self.esp, "BASE": self.base,
                            "SNIPPET": self.snippet, "GRUB_CFG": self.cfg}.items():
            self.stack.enter_context(patch.object(boot, name, value))
        self.stack.enter_context(patch.object(boot, "host_uuid", return_value="7000-79A2"))
        self.stack.enter_context(patch.object(boot.os, "geteuid", return_value=0))
        self.stack.enter_context(patch.object(boot.os, "sync"))
        self.stack.enter_context(patch.object(boot.shutil, "which", return_value="/mock/program"))
        self.stack.enter_context(patch.object(boot.subprocess, "run", side_effect=self.run_command))
        self.stack.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def run_command(self, command, **kwargs):
        if command[0] == "update-grub":
            self.cfg.write_text("generated config\n")
            if self.fail_update:
                raise subprocess.CalledProcessError(1, command)
        return subprocess.CompletedProcess(command, 0)

    def payload(self, version, config_version=boot.CONFIG_VERSION):
        hashes = {name: hashlib.sha256((name + version).encode()).hexdigest()
                  for name in boot.FILES if not name.endswith("limine.conf")}
        release = boot.release_id(hashes, config_version)
        directory = self.stage / release
        directory.mkdir()
        for name in boot.FILES:
            path = directory / name
            path.parent.mkdir(exist_ok=True)
            path.write_text(boot.limine_config(release, config_version) if name.endswith("limine.conf") else name + version)
            hashes[name] = boot.digest(path)
        (directory / "manifest.json").write_text(json.dumps({"release": release, "sha256": hashes}))
        (self.stage / "latest").write_text(release)
        return release

    def test_three_updates_keep_current_and_previous(self):
        releases = []
        for version in ("one", "two", "three"):
            releases.append(self.payload(version))
            boot.install()
        state = json.loads((self.base / "state.json").read_text())
        self.assertEqual(state["current"], releases[2])
        self.assertEqual(state["previous"], releases[1])
        self.assertFalse((self.base / releases[0]).exists())
        self.assertTrue((self.base / releases[1]).is_dir())
        self.assertIn("--id srvros-previous", self.snippet.read_text())
        # An identical redeploy must retain the previous release.
        boot.install()
        self.assertEqual(json.loads((self.base / "state.json").read_text()), state)

    def test_failed_update_restores_grub_and_old_selection(self):
        first = self.payload("one")
        boot.install()
        before = {path: path.read_bytes() for path in (self.snippet, self.cfg, self.base / "state.json")}
        self.payload("two")
        self.fail_update = True
        with self.assertRaises(subprocess.CalledProcessError):
            boot.install()
        for path, content in before.items():
            self.assertEqual(path.read_bytes(), content)
        self.assertTrue((self.base / first).is_dir())

    def test_failed_first_install_can_be_retried(self):
        self.payload("one")
        self.fail_update = True
        with self.assertRaises(subprocess.CalledProcessError):
            boot.install()
        self.assertFalse(self.snippet.exists())
        self.assertEqual(self.cfg.read_text(), "original Ubuntu config\n")
        self.fail_update = False
        boot.install()
        self.assertTrue(self.snippet.is_file())

    def test_upgrade_retains_old_configuration_release(self):
        old = self.payload("old", "ram-usb-v3-inputdiag")
        boot.install()
        current = self.payload("new")
        boot.install()
        state = json.loads((self.base / "state.json").read_text())
        self.assertEqual((state["current"], state["previous"]), (current, old))
        self.assertIn("srvros.inputdiag", (self.base / old / "limine.conf").read_text())
        self.assertNotIn("srvros.inputdiag", (self.base / current / "limine.conf").read_text())
        self.assertNotIn("srvros.log=debug", (self.base / current / "limine.conf").read_text())

    def test_all_configuration_versions_verify(self):
        for version in boot.CONFIG_VERSIONS:
            release = self.payload(version, version)
            boot.verify(self.stage / release)
        debug = boot.limine_config("0" * 16, boot.DIAGNOSTIC_VERSION)
        self.assertIn("srvros.inputprobe=1", debug)

    def test_hash_corruption_rejected_before_install(self):
        release = self.payload("one")
        (self.stage / release / "srvros.elf").write_text("corrupt")
        with self.assertRaises(ValueError):
            boot.install()
        self.assertFalse(self.base.exists())
        self.assertFalse(self.snippet.exists())

    def test_foreign_efi_directory_is_preserved(self):
        self.payload("one")
        self.base.mkdir(parents=True)
        foreign = self.base / "important.txt"
        foreign.write_text("keep me")
        with self.assertRaises(RuntimeError):
            boot.install()
        self.assertEqual(foreign.read_text(), "keep me")
        self.assertFalse(self.snippet.exists())

    def test_foreign_grub_snippet_is_preserved(self):
        self.payload("one")
        self.snippet.write_text("custom config\n")
        with self.assertRaises(RuntimeError):
            boot.install()
        self.assertEqual(self.snippet.read_text(), "custom config\n")
        self.assertFalse(self.base.exists())

    def test_autotest_profile_roundtrip_and_exact_install(self):
        version = 'ram-usb-v6-autotest-37d8c5d0-0123456789abcdef'
        release = self.payload('auto', version)
        path = self.stage / release / 'manifest.json'
        manifest = json.loads(path.read_text())
        manifest['config_version'] = version
        path.write_text(json.dumps(manifest))
        self.payload('different latest')
        boot.install(release)
        self.assertEqual(json.loads((self.base / 'state.json').read_text())['current'], release)
        cfg = (self.base / release / 'limine.conf').read_text()
        self.assertIn('srvros.autotest-volume=37d8c5d0', cfg)
        self.assertIn('srvros.autotest-run=0123456789abcdef', cfg)
        self.assertIn('srvros.autotest-release='+release, cfg)
        self.assertNotIn('srvros.usb-storage=rw', cfg)

    def test_workspace_profile_roundtrip_and_install(self):
        version = 'ram-usb-v7-workspace-37d8c5d0'
        release = self.payload('workspace', version)
        path = self.stage / release / 'manifest.json'
        manifest = json.loads(path.read_text())
        manifest['config_version'] = version
        path.write_text(json.dumps(manifest))
        boot.install(release)
        cfg = (self.base / release / 'limine.conf').read_text()
        self.assertIn('srvros.storage=ram srvros.workspace-volume=37d8c5d0', cfg)
        self.assertNotIn('srvros.usb-storage=rw', cfg)
        self.assertNotIn('srvros.autotest', cfg)

    def test_workspace_profile_conflicts_refused(self):
        for options in ({'diagnostic':True}, {'autotest_volume':'37d8c5d0'},
                        {'run_id':'0123456789abcdef'}):
            with self.assertRaises(ValueError): boot.stage(workspace_volume='37d8c5d0', **options)
        with self.assertRaises(ValueError): boot.stage(workspace_volume='bad')

    def test_gesture_profile(self):
        version='ram-usb-v12-trackpad-37d8c5d0-0123456789abcdef'
        release=self.payload('gestures',version)
        cfg=boot.limine_config(release,version)
        self.assertIn('srvros.trackpad=gestures',cfg)
        self.assertNotIn('srvros.trackpad=pointer',cfg)
        with self.assertRaises(ValueError): boot.stage(gestures=True)
    def test_pointer_profile(self):
        version='ram-usb-v11-trackpad-37d8c5d0-0123456789abcdef'
        release=self.payload('pointer',version)
        cfg=boot.limine_config(release,version)
        self.assertIn('srvros.trackpad=pointer',cfg)
        self.assertNotIn('srvros.trackpad=capture',cfg)
        with self.assertRaises(ValueError): boot.stage(pointer=True)

    def test_trackpad_profile_roundtrip_and_conflicts(self):
        version='ram-usb-v10-trackpad-37d8c5d0-0123456789abcdef'
        release=self.payload('trackpad',version)
        path=self.stage/release/'manifest.json'
        manifest=json.loads(path.read_text()); manifest['config_version']=version
        path.write_text(json.dumps(manifest)); boot.install(release)
        cfg=(self.base/release/'limine.conf').read_text()
        self.assertIn('srvros.trackpad=capture',cfg)
        self.assertIn('srvros.workspace-volume=37d8c5d0',cfg)
        self.assertIn('srvros.trackpad-release='+release,cfg)
        self.assertNotIn('srvros.usb-storage=rw',cfg)
        for args in ({'workspace_volume':'bad'},{'diagnostic':True},
                     {'workspace_volume':'37d8c5d0','autotest_volume':'37d8c5d0'},{}):
            with self.assertRaises(ValueError): boot.stage(trackpad_run='0123456789abcdef',**args)

    def test_unrecognized_profile_rejected(self):
        version = 'ram-usb-v5-autotest-37d8c5d0-0123456789abcdeg'
        release = self.payload('bad', version)
        path = self.stage / release / 'manifest.json'
        manifest = json.loads(path.read_text()); manifest['config_version'] = version
        path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError): boot.install(release)


if __name__ == "__main__":
    unittest.main()
