"""Offline v2 stream restore through the installation plan and native adapter."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import installation_image as install
import profile_bundle as profiles
import profile_stream as stream
import test_installation_profile as fixture
from test_storage_leases import empty_image
from exfat_test_image import ExfatFixture

ROOT = Path(__file__).resolve().parents[1]
DRIVER = ROOT / 'build/tests/profile-image-driver'


class StreamInstallationTests(unittest.TestCase):
    def setUp(self):
        fixture.ProfileInstallationTests.setUp(self)
        self.ident = self.bundle['profile']['id']
        self.payload = b'portable stream document' * 5000
        self.write_bundle(self.payload)
        self.log = self.root / 'driver.log'
        self.fake_driver()

    def write_bundle(self, data, path='Documents/test.txt'):
        metadata = {'format': profiles.FORMAT, 'schema_version': 2,
                    'profile': {'id': self.ident, 'display_name': 'Stream User'},
                    'preferences': {'text_size': 16},
                    'documents': [{'path': path, 'size': len(data),
                                   'sha256': hashlib.sha256(data).hexdigest()}]}
        manifest = json.dumps(metadata, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()
        self.bundle_path.write_bytes(stream.HEADER.pack(stream.MAGIC, len(manifest), 0) + manifest + data)
        return metadata

    def fake_driver(self, fail=None):
        self.driver.write_text('#!/usr/bin/env python3\nimport sys\n'
            + f'with open({str(self.log)!r}, "a") as f: f.write(sys.argv[2] + "\\n")\n'
            + (f'if sys.argv[2] == {fail!r}: sys.exit(1)\n' if fail else ''))
        self.driver.chmod(0o755)

    def prepare(self, **changes):
        options = dict(self.options, profile_expected_id=self.ident)
        options.update(changes)
        return install.prepare(self.target, self.kernel, self.boot, self.exfat, self.init, **options)

    def apply(self):
        with mock.patch.object(install.usb, 'build_image', side_effect=fixture.sparse_builder):
            return install.apply(self.target)

    def assert_unpublished(self):
        self.assertFalse((self.target / 'image.img').exists())
        self.assertFalse((self.target / 'ready.json').exists())

    def test_invalid_stream_and_wrong_id_rejected_before_stage(self):
        good = self.bundle_path.read_bytes()
        for bad in (good[:-1], good + b'x', good[:stream.HEADER.size - 1],
                    good[:-1] + bytes([good[-1] ^ 1])):
            with self.subTest(size=len(bad), suffix=bad[-1:]):
                self.bundle_path.write_bytes(bad)
                with self.assertRaises((ValueError, OSError)):
                    self.prepare()
                self.assertFalse(self.target.exists())
        self.bundle_path.write_bytes(good)
        with self.assertRaises((ValueError, OSError)):
            self.prepare(profile_expected_id='12345678-1234-4abc-8def-123456789abc')
        self.assertFalse(self.target.exists())

    def test_changed_or_replaced_stream_refused_before_apply(self):
        self.prepare()
        original = self.bundle_path.read_bytes()
        self.bundle_path.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
        with self.assertRaisesRegex(ValueError, 'document integrity mismatch'):
            self.apply()
        self.assert_unpublished()
        replacement = self.root / 'replacement'
        replacement.write_bytes(original)
        replacement.replace(self.bundle_path)
        with self.assertRaisesRegex(ValueError, 'bundle changed'):
            self.apply()
        self.assert_unpublished()

    def test_source_change_after_native_provisioning_prevents_publication(self):
        self.prepare()
        original = install.digest(self.exfat)

        def modify(name):
            if name == 'provisioned':
                self.write_bundle(self.payload + b'changed')

        with mock.patch.object(install.usb, 'build_image', side_effect=fixture.sparse_builder):
            with self.assertRaisesRegex(ValueError, 'source changed during build'):
                install.apply(self.target, checkpoint=modify)
        self.assert_unpublished()
        self.assertEqual(install.digest(self.exfat), original)

    def test_stream_operations_and_driver_failures(self):
        original = self.exfat.read_bytes()
        plan = self.prepare()
        self.assertEqual(plan['version'], 2)
        self.assertEqual(plan['profile']['profile_id'], self.ident)
        ready = self.apply()
        calls = self.log.read_text().splitlines()
        self.assertIn('check-stream-base', calls)
        self.assertIn('write-stream', calls)
        self.assertIn('check-stream-final', calls)
        self.assertLess(calls.index('check-stream-base'), calls.index('write-stream'))
        self.assertLess(calls.index('write-stream'), calls.index('check-stream-final'))
        self.assertEqual(ready['profile']['profile_id'], self.ident)
        self.assertEqual(self.exfat.read_bytes(), original)

    def test_driver_refusal_never_publishes(self):
        for operation in ('check-stream-base', 'write-stream', 'check-stream-final'):
            with self.subTest(operation=operation):
                # Each failed apply deliberately leaves its stage unusable.
                self.target = self.root / ('stage-' + operation)
                self.fake_driver(operation)
                before = self.exfat.read_bytes()
                self.prepare()
                with self.assertRaises((ValueError, OSError, subprocess.CalledProcessError)):
                    self.apply()
                self.assert_unpublished()
                self.assertEqual(self.exfat.read_bytes(), before)

    def seed_native(self, size, extra=()):
        with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=self.root) as temp:
            image = Path(temp) / 'image'
            image.write_bytes(empty_image(size).data)
            for operation, path in (('ensure-home', '/fat/home'), ('mkdir', '/fat/bin'),
                                    ('mkdir', '/fat/var'), *extra):
                subprocess.run([str(DRIVER), str(image), operation, path], check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.exfat.write_bytes(image.read_bytes())
        self.options['profile_driver'] = DRIVER

    @unittest.skipUnless(DRIVER.is_file(), 'native driver absent; run make test-installed-root to build it')
    def test_native_large_stream_exact_data(self):
        # Large enough to exercise streaming across many native I/O buffers.
        data = bytes(range(256)) * (16 * 1024 * 1024 // 256)
        metadata = self.write_bundle(data, 'Documents/drafts.profile-import/test.txt')
        self.seed_native(48 * 1024 * 1024)
        original = install.digest(self.exfat)
        self.prepare()
        self.apply()
        fs = ExfatFixture((self.target / 'profile.exfat').read_bytes())
        self.assertEqual(fs.read('home/alice/Documents/drafts.profile-import/test.txt'), data)
        sidecar = fs.read('.srvros/meta').decode('ascii').splitlines()[1:]
        self.assertTrue(sidecar)
        self.assertTrue(all(int(entry.split('\t')[1]) > 2 for entry in sidecar))
        self.assertEqual(profiles.decode_json(fs.read('home/alice/identity.json')), metadata['profile'])
        self.assertEqual(profiles.decode_json(fs.read('home/alice/.config/srvros/portable.json')), metadata['preferences'])
        self.assertEqual(install.digest(self.exfat), original)

    @unittest.skipUnless(DRIVER.is_file(), 'native driver absent; run make test-installed-root to build it')
    def test_native_pending_collision_and_capacity_refuse(self):
        cases = (('pending', 4 * 1024 * 1024, (('mkdir', '/fat/home/.profile-import-pending'),), b'x'),
                 ('journal', 4 * 1024 * 1024, (('mkdir', '/fat/home/alice.profile-import.json'),), b'x'),
                 ('collision', 4 * 1024 * 1024, (('mkdir', '/fat/home/alice'),), b'x'),
                 ('capacity', 4 * 1024 * 1024, (), b'x' * (4 * 1024 * 1024)))
        for name, size, extra, data in cases:
            with self.subTest(name=name):
                self.target = self.root / ('native-' + name)
                self.seed_native(size, extra)
                self.write_bundle(data)
                before = install.digest(self.exfat)
                self.prepare()
                with self.assertRaises((ValueError, OSError, subprocess.CalledProcessError)):
                    self.apply()
                self.assert_unpublished()
                self.assertEqual(install.digest(self.exfat), before)


if __name__ == '__main__':
    unittest.main()
