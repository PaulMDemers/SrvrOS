import copy
import hashlib
import json
from pathlib import Path
import tempfile
import struct
import subprocess
import unittest
from unittest.mock import patch
import uuid

import profile_bundle as p
import profile_image as image
import profile_stream as stream
import profile_stream_fixture as fixture

DATA = b'portable data'


def bundle():
    return {'format': p.FORMAT, 'schema_version': 2,
            'profile': {'id': '12345678-1234-4abc-8def-123456789abc', 'display_name': 'A'},
            'preferences': {'text_size': 16},
            'documents': [{'path': 'Documents/notes.txt', 'size': len(DATA),
                           'sha256': hashlib.sha256(DATA).hexdigest()}]}


def raw_stream(metadata, payload=b''):
    encoded = json.dumps(metadata, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()
    return stream.HEADER.pack(stream.MAGIC, len(encoded), 0) + encoded + payload


class ImageTests(unittest.TestCase):
    def setUp(self):
        base = Path(__file__).resolve().parents[1] / 'build' / 'review' / 'profile-image'
        base.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=base)
        self.root = Path(self.tmp.name)
        self.source = self.root / 'source.img'; self.source.write_bytes(b'S' * 1024)
        self.target = self.root / 'target.img'
        self.bundle_path = self.root / 'bundle.srvrprofile'
        self.value = bundle()
        fixture.write_stream(self.bundle_path, self.value['profile'], self.value['preferences'],
                             [('Documents/notes.txt', DATA)])
        self.calls = []
    def tearDown(self): self.tmp.cleanup()
    def run_driver(self, args, check):
        self.assertTrue(check)
        self.calls.append(tuple(args[2:4]))
        if len(args) == 5:
            self.assertTrue(Path(args[4]).is_file())
    def provision(self, **kw):
        args = dict(source_exfat=self.source, target_exfat=self.target, home='/fat/home/alice',
                    alias='alice', uid=1000, gid=1000, driver='/fake/driver')
        args.update(kw)
        bundle_path = args.pop('bundle_path', self.bundle_path)
        with stream.open_bundle(bundle_path) as source:
            return image.provision(bundle=source, **args)
    def test_publish_and_source_preserved(self):
        with patch('profile_image.subprocess.run', side_effect=self.run_driver):
            result = self.provision()
        self.assertEqual(self.source.read_bytes(), b'S' * 1024)
        self.assertEqual(self.target.read_bytes(), b'S' * 1024)
        self.assertEqual(result['home'], '/fat/home/alice')
        self.assertIn(('write', '/fat/home/alice/profile.id'), self.calls)
        self.assertIn(('write-stream', '/fat/home/alice/Documents/notes.txt'), self.calls)
        self.assertEqual(self.calls[-1], ('check-stream-final', '/fat/home'))
        self.assertEqual(list(self.root.glob('.profile-image-*')), [])
    def test_invalid_before_copy(self):
        for changes in ({'home': '/fat/home/../bad'}, {'home': '/fat/home/' + 'a' * 86},
                        {'alias': 'bad alias'}, {'uid': True}, {'machine_preferences': b'bad'}):
            with self.subTest(changes=changes), patch('profile_image.subprocess.run') as run:
                with self.assertRaises(p.ProfileError): self.provision(**changes)
                run.assert_not_called()
                self.assertFalse(self.target.exists())
        bad = copy.deepcopy(self.value); bad['documents'][0]['sha256'] = '0' * 64
        self.bundle_path.write_bytes(raw_stream(bad, DATA))
        with self.assertRaises(p.ProfileError): self.provision()
        self.assertEqual(self.source.read_bytes(), b'S' * 1024)
    def test_expanded_path_bound(self):
        bad = copy.deepcopy(self.value)
        bad['documents'][0]['path'] = 'Documents/' + '/'.join(['a' * 60] * 3)
        self.bundle_path.write_bytes(raw_stream(bad, DATA))
        with self.assertRaises(p.ProfileError): self.provision()
        self.assertFalse(self.target.exists())
    def test_driver_failure_removes_stage(self):
        with patch('profile_image.subprocess.run', side_effect=RuntimeError('driver failed')):
            with self.assertRaises(RuntimeError): self.provision()
        self.assertFalse(self.target.exists())
        self.assertEqual(self.source.read_bytes(), b'S' * 1024)
        self.assertEqual(list(self.root.glob('.profile-image-*')), [])
    def test_existing_target_never_overwritten(self):
        self.target.write_bytes(b'existing')
        with self.assertRaises(p.ProfileError): self.provision()
        self.assertEqual(self.target.read_bytes(), b'existing')
    def test_nonregular_source_refused(self):
        with self.assertRaises((p.ProfileError, OSError)): self.provision(source_exfat=self.root)
        self.assertFalse(self.target.exists())


class DriverTests(unittest.TestCase):
    setUp = ImageTests.setUp
    tearDown = ImageTests.tearDown
    provision = ImageTests.provision
    def test_guest_namespace_metadata(self):
        from test_storage_leases import empty_image
        from exfat_test_image import ExfatFixture
        driver = Path(__file__).resolve().parents[1] / 'build/tests/profile-image-driver'
        # The native adapter only opens private .profile-image-*/image paths.
        # Seed the runtime directories there before passing this image as source.
        with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=self.root) as private:
            seeded = Path(private) / 'image'
            seeded.write_bytes(empty_image(32 * 1024 * 1024).data)
            for operation, path in (('ensure-home', '/fat/home'), ('mkdir', '/fat/bin'),
                                    ('mkdir', '/fat/var')):
                subprocess.run([str(driver), str(seeded), operation, path], check=True)
            self.source.write_bytes(seeded.read_bytes())
        self.provision(driver=driver)
        fs = ExfatFixture(self.target.read_bytes())
        metadata = fs.read('.srvros/meta').splitlines()
        self.assertEqual(metadata[0], b'SRVROSMETA1')
        self.assertTrue(all(line.split(b'\t')[0] == b'/fat' or line.startswith(b'/fat/') for line in metadata[1:]))
        self.assertEqual(fs.read('home/alice/Documents/notes.txt'), DATA)
        self.assertEqual(p.decode_json(fs.read('home/alice/identity.json')), self.value['profile'])
        self.assertEqual(fs.read('home/alice/.config/srvros/portable.json'), p.encode_json(self.value['preferences']))
        entry = dict(fs.entries(fs.read('.srvros')))['meta']
        offset = fs.offset(struct.unpack_from('<I', entry, 52)[0]) + len(b'SRVROSMETA1\n')
        damaged = bytearray(self.target.read_bytes()); damaged[offset:offset + 4] = b'/bad'
        self.source.write_bytes(damaged)
        refused = self.root / 'refused.img'
        with self.assertRaises(subprocess.CalledProcessError):
            self.provision(driver=driver, target_exfat=refused, home='/fat/home/bob')
        self.assertEqual(self.source.read_bytes(), damaged)
        self.assertFalse(refused.exists())
        self.assertEqual(list(self.root.glob('.profile-image-*')), [])


if __name__ == '__main__': unittest.main()
