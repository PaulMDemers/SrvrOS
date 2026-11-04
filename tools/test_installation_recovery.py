import fcntl
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import uuid

import installation_image as image
import profile_bundle as profiles
from test_installation_profile import sparse_builder
from test_installation_image import exfat_fixture


class Interrupted(Exception):
    pass


def stop_at(wanted):
    def stop(name):
        if name == wanted:
            raise Interrupted(name)
    return stop


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.target = self.root / 'stage'
        self.kernel, self.init, self.boot = (self.root / n for n in ('kernel', 'init', 'boot'))
        self.kernel.write_bytes(b'\x7fELFkernel')
        self.init.write_bytes(b'\x7fELFinit')
        self.boot.write_bytes(b'MZefi')
        self.exfat = self.root / 'source.exfat'
        exfat_fixture(self.exfat)
        self.driver = self.root / 'driver'
        self.driver.write_text('#!/usr/bin/env python3\nimport sys\nwith open(sys.argv[1], "r+b") as f:\n f.seek(-1,2); f.write(b"X")\n')
        self.driver.chmod(0o755)
        self.bundle = self.root / 'bundle.json'
        self.bundle.write_bytes(profiles.encode_json({'format': profiles.FORMAT, 'schema_version': 1,
            'profile': {'id': str(uuid.uuid4()), 'display_name': 'A'}, 'preferences': {}, 'documents': []}))

    def prepare(self, with_profile=False):
        extra = dict(profile_bundle=self.bundle, profile_home='/fat/home/alice', profile_alias='alice',
                     profile_uid=1001, profile_gid=1001, profile_driver=self.driver) if with_profile else {}
        return image.prepare(self.target, self.kernel, self.boot, self.exfat, self.init,
                             identity_bound=True, **extra)

    def stage(self, where, profile=False):
        self.prepare(profile)
        with mock.patch.object(image.usb, 'build_image', side_effect=sparse_builder):
            with self.assertRaises(Interrupted):
                image.apply(self.target, checkpoint=stop_at(where))

    def snapshot(self):
        return {p.name: (p.lstat().st_ino, p.lstat().st_size, p.lstat().st_nlink,
                         os.readlink(p) if p.is_symlink() else image.digest(p))
                for p in self.target.iterdir()}

    def test_checkpoint_matrix_v1_v2(self):
        states = {'provisioning-started': 'incomplete', 'provisioned': 'incomplete',
                  'assembled': 'incomplete', 'verified': 'verified', 'image-linked': 'image-linked',
                  'image-published': 'image-published', 'ready-written': 'ready-written',
                  'ready-linked': 'ready-linked', 'ready-published': 'ready'}
        # Separate test instances are unnecessary: each iteration removes only
        # this test's temporary staging directory.
        import shutil
        for profile in (False, True):
            for point, expected in states.items():
                with self.subTest(profile=profile, point=point):
                    self.stage(point, profile)
                    result = image.inspect(self.target)
                    self.assertEqual(result['state'], expected, result)
                    self.assertEqual(result['can_reconcile'], expected not in ('incomplete',))
                    if expected == 'incomplete':
                        before = self.snapshot()
                        with self.assertRaises(ValueError):
                            image.reconcile(self.target)
                        self.assertEqual(self.snapshot(), before)
                    else:
                        ready = image.reconcile(self.target)
                        self.assertEqual(image.inspect(self.target)['state'], 'ready')
                        self.assertEqual(image.reconcile(self.target), ready)
                    shutil.rmtree(self.target)

    def test_interrupted_reconcile_is_restartable(self):
        self.stage('verified', True)
        for point in ('image-linked', 'image-published', 'ready-written', 'ready-linked', 'ready-published'):
            with self.assertRaises(Interrupted):
                image.reconcile(self.target, checkpoint=stop_at(point))
            self.assertTrue(image.inspect(self.target)['can_reconcile'])
        self.assertEqual(image.inspect(self.target)['state'], 'ready')
        image.reconcile(self.target)

    def test_refuse_corrupt_journal_unknown_symlink_and_external_hardlink(self):
        self.stage('verified')
        journal = self.target / 'publication.json'
        saved = journal.read_bytes()
        journal.write_bytes(b'{')
        before = self.snapshot()
        self.assertEqual(image.inspect(self.target)['state'], 'refused')
        with self.assertRaises(ValueError):
            image.reconcile(self.target)
        self.assertEqual(self.snapshot(), before)
        journal.write_bytes(saved)
        unknown = self.target / 'unknown'; unknown.write_text('preserve')
        before = self.snapshot()
        self.assertEqual(image.inspect(self.target)['state'], 'refused')
        self.assertEqual(self.snapshot(), before)
        unknown.unlink()
        os.link(self.target / 'image.partial', self.root / 'outside')
        self.assertEqual(image.inspect(self.target)['state'], 'refused')
        (self.root / 'outside').unlink()
        (self.target / 'rogue').symlink_to(self.target / 'image.partial')
        self.assertEqual(image.inspect(self.target)['state'], 'refused')

    def test_refuse_changed_source_image_and_ready(self):
        self.stage('ready-published', True)
        source = self.driver.read_bytes(); self.driver.write_bytes(source + b'# changed')
        before = self.snapshot()
        self.assertFalse(image.inspect(self.target)['can_reconcile'])
        with self.assertRaises(ValueError): image.reconcile(self.target)
        self.assertEqual(self.snapshot(), before)
        self.driver.write_bytes(source)
        ready = self.target / 'ready.json'; contents = ready.read_bytes()
        ready.write_text('{}')
        self.assertFalse(image.inspect(self.target)['can_reconcile'])
        ready.write_bytes(contents)
        disk = self.target / 'image.img'
        with open(disk, 'r+b') as f:
            f.seek(-1, 2); f.write(b'X')
        self.assertFalse(image.inspect(self.target)['can_reconcile'])

    def test_early_untrusted_aliases_and_nonprivate_stage_refused(self):
        self.stage('provisioned')
        candidate = self.target / 'image.partial'
        candidate.symlink_to(self.exfat)
        before = self.snapshot()
        self.assertEqual(image.inspect(self.target)['state'], 'refused')
        with self.assertRaises(ValueError):
            image.reconcile(self.target)
        self.assertEqual(self.snapshot(), before)
        candidate.unlink()
        os.link(self.exfat, candidate)
        before = self.snapshot()
        self.assertEqual(image.inspect(self.target)['state'], 'refused')
        with self.assertRaises(ValueError):
            image.reconcile(self.target)
        self.assertEqual(self.snapshot(), before)
        candidate.unlink()
        self.target.chmod(0o755)
        before = self.snapshot()
        self.assertEqual(image.inspect(self.target)['state'], 'refused')
        with self.assertRaises(ValueError):
            image.reconcile(self.target)
        self.assertEqual(self.snapshot(), before)
        self.target.chmod(0o700)

    def test_equivalent_but_edited_ready_marker_is_refused_and_preserved(self):
        self.stage('ready-published', True)
        path = self.target / 'ready.json'
        expected = json.loads(path.read_text())
        expected['profile']['uid'] = float(expected['profile']['uid'])
        edited = (json.dumps(expected, separators=(',', ':')) + '\n').encode()
        path.write_bytes(edited)
        before = self.snapshot()
        self.assertEqual(image.inspect(self.target)['state'], 'refused')
        with self.assertRaises(ValueError):
            image.reconcile(self.target)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(path.read_bytes(), edited)

    def test_malformed_journal_candidate_refused_without_changes(self):
        self.stage('verified')
        path = self.target / 'publication.json'
        journal = json.loads(path.read_text())
        journal['candidate'] = None
        path.write_text(json.dumps(journal))
        before = self.snapshot()
        result = image.inspect(self.target)
        self.assertEqual(result['state'], 'refused')
        with self.assertRaises(ValueError):
            image.reconcile(self.target)
        self.assertEqual(self.snapshot(), before)

    def test_plan_lock_refuses_concurrent_reconcile(self):
        self.stage('verified')
        with open(self.target / 'plan.json', 'rb') as plan:
            fcntl.flock(plan, fcntl.LOCK_EX | fcntl.LOCK_NB)
            before = self.snapshot()
            with self.assertRaises(BlockingIOError):
                image.reconcile(self.target)
            self.assertEqual(self.snapshot(), before)


if __name__ == '__main__':
    unittest.main()
