"""Offline inspection contract tests using disposable images and native drivers."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib

import installation_image as install
import installation_inspection as inspection
from installation_io_faults import build
from exfat_test_image import ExfatFixture
from test_profile_image import bundle
from test_storage_leases import empty_image
import profile_bundle
import profile_image

ROOT = Path(__file__).resolve().parents[1]
INSPECT_DRIVER = ROOT / 'build/tests/installation-inspect-driver'
PROFILE_DRIVER = ROOT / 'build/tests/profile-image-driver'


def snapshot(stage):
    """Capture contents and identity, including unexpected entries, without following links."""
    result = {}
    for path in [stage, *sorted(stage.rglob('*'))]:
        info = path.lstat()
        value = (stat.S_IFMT(info.st_mode), stat.S_IMODE(info.st_mode), info.st_dev,
                 info.st_ino, info.st_nlink, info.st_size)
        if stat.S_ISREG(info.st_mode):
            value += (install.digest(path),)
        elif stat.S_ISLNK(info.st_mode):
            value += (os.readlink(path),)
        result[str(path.relative_to(stage))] = value
    return result


def mutate_profile_file(stage, relative, transform):
    """Patch a same-size file payload in the installed data partition, preserving its baseline."""
    plan = json.loads((stage / 'plan.json').read_bytes())
    spec = plan['layout']['exfat']
    with (stage / 'image.img').open('r+b') as disk:
        disk.seek(spec['offset_bytes'])
        fs = ExfatFixture(disk.read(spec['size_bytes']))
        directory = fs.chain(fs.root)
        for part in relative.split('/'):
            entry = next(entry for name, entry in fs.entries(directory) if name == part)
            first = struct.unpack_from('<I', entry, 52)[0]
            size = struct.unpack_from('<Q', entry, 56)[0]
            contiguous = bool(entry[33] & 2)
            directory = fs.chain(first, size, contiguous)
        changed = transform(directory)
        if len(changed) != size:
            raise ValueError('replacement must have the same size')
        remaining = changed
        cluster = first
        while remaining:
            chunk, remaining = remaining[:fs.cluster], remaining[fs.cluster:]
            disk.seek(spec['offset_bytes'] + fs.offset(cluster))
            disk.write(chunk)
            cluster = cluster + 1 if contiguous else fs.u32(fs.fat + cluster * 4)


def add_profile_file(stage, driver, guest_path, content):
    """Use the real profile driver on a private copy, then update only the data region."""
    spec = json.loads((stage / 'plan.json').read_bytes())['layout']['exfat']
    with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=stage.parent) as temp:
        temp = Path(temp)
        data_image, source = temp / 'image', temp / 'content'
        with (stage / 'image.img').open('rb') as disk:
            disk.seek(spec['offset_bytes'])
            data_image.write_bytes(disk.read(spec['size_bytes']))
        source.write_bytes(content)
        subprocess.run([str(driver), str(data_image), 'write', guest_path, str(source)], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        with (stage / 'image.img').open('r+b') as disk:
            disk.seek(spec['offset_bytes'])
            disk.write(data_image.read_bytes())


class InstalledInspectionTests(unittest.TestCase):
    def setUp(self):
        for driver in (INSPECT_DRIVER, PROFILE_DRIVER):
            if not driver.is_file():
                self.fail(f'{driver} is required; run make test-installed-root to build the native drivers')
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.stage = self.root / 'stage'
        self.kernel, self.boot, self.init, self.exfat = [self.root / n for n in ('kernel', 'boot', 'init', 'exfat')]
        self.kernel.write_bytes(b'\x7fELFkernel')
        self.boot.write_bytes(b'MZefi')
        self.init.write_bytes(b'\x7fELFinit')
        # Seed the namespace targets on an unpublished private image before prepare.
        with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=self.root) as temp:
            image = Path(temp) / 'image'
            image.write_bytes(empty_image(4 * 1024 * 1024).data)
            for operation, path in (('ensure-home', '/fat/home'), ('mkdir', '/fat/bin'),
                                    ('mkdir', '/fat/var')):
                subprocess.run([str(PROFILE_DRIVER), str(image), operation, path], check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.exfat.write_bytes(image.read_bytes())
        self.bundle = self.root / 'bundle.json'
        self.bundle.write_bytes(profile_bundle.encode_json(bundle()))

    def publish(self, profile=True, identity=True):
        opts = {'identity_bound': identity}
        if profile:
            opts.update(profile_bundle=self.bundle, profile_home='/fat/home/alice',
                        profile_alias='alice', profile_uid=1001, profile_gid=1001,
                        profile_driver=PROFILE_DRIVER)
        install.prepare(self.stage, self.kernel, self.boot, self.exfat, self.init, **opts)
        with mock.patch.object(install.usb, 'build_image', side_effect=build):
            install.apply(self.stage)

    def check_inspection(self, valid, cli=False):
        before = snapshot(self.stage)
        if cli:
            proc = subprocess.run([sys.executable, str(ROOT / 'tools/installation_image.py'),
                                   'inspect-installed', str(self.stage), '--driver', str(INSPECT_DRIVER)],
                                  cwd=self.root, capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0 if valid else 1, proc.stderr)
            result = json.loads(proc.stdout)
        else:
            result = inspection.inspect_installed(self.stage, INSPECT_DRIVER)
        self.assertEqual(snapshot(self.stage), before)
        self.assertEqual(result['state'], 'installed-consistent' if valid else 'refused', result)
        self.assertEqual(result['consistent'], valid)
        self.assertTrue(result['read_only'])
        if valid:
            self.assertTrue(result['publication']['historical'])
            self.assertFalse(result['publication']['provisioning_sources_revalidated'])
            self.assertTrue(result['filesystem']['namespace_targets_valid'])
            self.assertEqual(result['filesystem']['writes'], 0)
            self.assertEqual(result['filesystem']['flushes'], 0)
        return result

    def test_profile_pristine_api_and_cli(self):
        self.publish()
        result = self.check_inspection(True)
        self.assertFalse(result['used'])
        self.assertEqual(result['profiles'][0]['profile_id'], bundle()['profile']['id'])
        self.check_inspection(True, cli=True)

    def test_older_v2_profile_without_identity_pristine_and_used(self):
        validate = profile_image.validate_request
        def legacy_request(*args, **kwargs):
            metadata, files, directories = validate(*args, **kwargs)
            del files[metadata['home'] + '/identity.json']
            return metadata, files, directories
        with mock.patch.object(profile_image, 'validate_request', side_effect=legacy_request):
            self.publish()
        pristine = self.check_inspection(True)
        self.assertFalse(pristine['used'])
        self.assertEqual(pristine['profiles'][0]['display_name'], 'alice')
        self.assertFalse(pristine['profiles'][0]['identity_present'])
        self.assertEqual(pristine['profiles'][0]['profile_id'], bundle()['profile']['id'])
        mutate_profile_file(self.stage, 'home/alice/Documents/notes.txt', lambda b: b.upper())
        used = self.check_inspection(True)
        self.assertTrue(used['used'])
        self.assertEqual(used['profiles'][0]['display_name'], 'alice')
        self.assertFalse(used['profiles'][0]['identity_present'])

    def test_portable_preferences_guest_size_boundary(self):
        validate = profile_image.validate_request
        for size in (512, 513):
            with self.subTest(size=size):
                if self.stage.exists():
                    import shutil
                    shutil.rmtree(self.stage)
                def padded_request(*args, **kwargs):
                    metadata, files, directories = validate(*args, **kwargs)
                    path = metadata['home'] + '/' + profile_bundle.PREF_PATH
                    canonical = files[path]
                    self.assertLess(len(canonical), size)
                    files[path] = canonical + b' ' * (size - len(canonical))
                    return metadata, files, directories
                with mock.patch.object(profile_image, 'validate_request', side_effect=padded_request):
                    self.publish()
                self.check_inspection(size == 512)

    def test_profile_used_with_missing_original_sources(self):
        self.publish()
        ready = (self.stage / 'ready.json').read_bytes()
        baseline = install.digest(self.stage / 'profile.exfat')
        mutate_profile_file(self.stage, 'home/alice/Documents/notes.txt', lambda b: b.upper())
        self.assertEqual(install.inspect(self.stage)['state'], 'refused')
        with self.assertRaises((ValueError, OSError)): install.reconcile(self.stage)
        for path in (self.kernel, self.boot, self.init, self.exfat, self.bundle): path.unlink()
        result = self.check_inspection(True)
        self.assertTrue(result['used'])
        self.assertEqual(result['publication']['data_baseline']['kind'], 'retained-profile-exfat')
        self.assertEqual(install.digest(self.stage / 'profile.exfat'), baseline)
        self.assertEqual((self.stage / 'ready.json').read_bytes(), ready)
        self.check_inspection(True, cli=True)

    def test_legacy_pristine_and_used_then_missing_baseline(self):
        self.publish(profile=False, identity=False)
        self.assertFalse(self.check_inspection(True)['used'])
        spec = json.loads((self.stage / 'plan.json').read_bytes())['layout']['exfat']
        with (self.stage / 'image.img').open('r+b') as f:
            f.seek(spec['offset_bytes'] + spec['size_bytes'] - 1); f.write(b'Z')
        result = self.check_inspection(True)
        self.assertTrue(result['used'])
        self.assertEqual(result['publication']['data_baseline']['kind'], 'original-exfat-source')
        self.exfat.unlink()
        self.assertIn('baseline', self.check_inspection(False)['reason'])
        self.check_inspection(False, cli=True)

    def test_used_profile_requires_unchanged_baseline(self):
        self.publish()
        mutate_profile_file(self.stage, 'home/alice/Documents/notes.txt', lambda b: b.upper())
        (self.stage / 'profile.exfat').unlink()
        self.assertIn('baseline', self.check_inspection(False)['reason'])

    def test_pending_and_scratch_files_refused(self):
        for name in ('.profile-import-pending', '.profile-copy-temp-x', '.desktop-profile.tmp-x'):
            with self.subTest(name=name):
                if self.stage.exists():
                    import shutil
                    shutil.rmtree(self.stage)
                self.publish()
                add_profile_file(self.stage, PROFILE_DRIVER, '/fat/home/alice/' + name, b'pending')
                self.check_inspection(False)

    def test_metadata_identity_and_local_binding_mismatch(self):
        for path, old, new in (('home/alice/profile.id', b'12345678', b'22345678'),
                               ('home/alice/profile-local.json', b'alice', b'blice'),
                               ('home/alice/identity.json', b'12345678', b'22345678'),
                               ('home/alice/.config/srvros/portable.json', b'16', b'99')):
            with self.subTest(path=path):
                if self.stage.exists():
                    import shutil
                    shutil.rmtree(self.stage)
                self.publish()
                def change(data):
                    self.assertIn(old, data)
                    return data.replace(old, new, 1)
                mutate_profile_file(self.stage, path, change)
                self.check_inspection(False)

    def test_remembered_selection_pristine_and_used(self):
        prefix = b'srvros-desktop-profile-v1\n12345678-1234-4abc-8def-123456789abc\n/fat/home/alice\n'
        content = prefix + f'{zlib.crc32(prefix):08x}\n'.encode()
        # A selection may already exist at publication time. Provisioning adds its profile.
        with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=self.root) as temp:
            image, source = Path(temp) / 'image', Path(temp) / 'selection'
            image.write_bytes(self.exfat.read_bytes()); source.write_bytes(content)
            subprocess.run([str(PROFILE_DRIVER), str(image), 'write', '/fat/home/.desktop-profile',
                            str(source)], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.exfat.write_bytes(image.read_bytes())
        self.publish()
        expected = {'profile_id': bundle()['profile']['id'], 'home': '/fat/home/alice'}
        pristine = self.check_inspection(True)
        self.assertFalse(pristine['used'])
        self.assertEqual(pristine['selection'], expected)
        mutate_profile_file(self.stage, 'home/alice/Documents/notes.txt', lambda b: b.upper())
        used = self.check_inspection(True)
        self.assertTrue(used['used'])
        self.assertEqual(used['selection'], expected)

    def test_native_driver_refuses_missing_namespace_targets(self):
        raw = self.root / 'empty.exfat'
        raw.write_bytes(empty_image(4 * 1024 * 1024).data)
        before = (raw.stat(), install.digest(raw))
        fd = os.open(raw, os.O_RDONLY)
        try:
            result = subprocess.run([str(INSPECT_DRIVER), '--fd', str(fd), '0', str(raw.stat().st_size)],
                                    pass_fds=(fd,), capture_output=True, text=True)
        finally:
            os.close(fd)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('namespace-targets', result.stderr)
        after = (raw.stat(), install.digest(raw))
        self.assertEqual((before[0].st_dev, before[0].st_ino, before[0].st_mode,
                          before[0].st_nlink, before[0].st_size, before[1]),
                         (after[0].st_dev, after[0].st_ino, after[0].st_mode,
                          after[0].st_nlink, after[0].st_size, after[1]))

    def test_selection_checksum_and_stale_selection(self):
        for checksum in (False, True):
            with self.subTest(checksum=checksum):
                if self.stage.exists():
                    import shutil
                    shutil.rmtree(self.stage)
                self.publish()
                prefix = b'srvros-desktop-profile-v1\n12345678-1234-4abc-8def-123456789abc\n/fat/home/bob\n'
                value = f'{zlib.crc32(prefix):08x}\n'.encode() if checksum else b'00000000\n'
                add_profile_file(self.stage, PROFILE_DRIVER, '/fat/home/.desktop-profile', prefix + value)
                self.check_inspection(False)

    def test_protected_padding_changes_refused(self):
        self.publish()
        spec = json.loads((self.stage / 'plan.json').read_bytes())['layout']
        # MBR bootstrap bytes and unused ESP bytes are outside the mutable data region.
        for offset in (100, spec['esp']['offset_bytes'] + spec['esp']['size_bytes'] - 512):
            with self.subTest(offset=offset):
                with (self.stage / 'image.img').open('r+b') as f:
                    f.seek(offset); old = f.read(1); f.seek(offset); f.write(bytes([old[0] ^ 1]))
                self.check_inspection(False)
                with (self.stage / 'image.img').open('r+b') as f:
                    f.seek(offset); f.write(old)

    def test_unexpected_extra_symlink_and_hardlink_refused(self):
        self.publish()
        extra = self.stage / 'extra'
        extra.write_bytes(b'x'); self.check_inspection(False); extra.unlink()
        ready = self.stage / 'ready.json'; backup = self.root / 'ready'
        ready.rename(backup); ready.symlink_to(backup); self.check_inspection(False); ready.unlink()
        backup.rename(ready)
        os.link(ready, self.root / 'hardlink'); self.check_inspection(False)

    def test_locked_image_and_plan_refused(self):
        self.publish()
        for path in (self.stage / 'image.img', self.stage / 'plan.json'):
            with self.subTest(path=path.name), path.open('rb') as f:
                fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
                self.check_inspection(False)

    def test_invalid_and_oversized_records_refused(self):
        self.publish()
        path = self.stage / 'publication.json'; original = path.read_bytes()
        for data in (b'{"version":1,"version":1}', b' ' * (1024 * 1024 + 1)):
            with self.subTest(size=len(data)):
                path.write_bytes(data); self.check_inspection(False)
        path.write_bytes(original)

    def test_native_driver_rejects_writable_fd_and_invalid_range(self):
        self.publish()
        spec = json.loads((self.stage / 'plan.json').read_bytes())['layout']['exfat']
        before = snapshot(self.stage)
        for flags, offset, length in ((os.O_RDWR, spec['offset_bytes'], spec['size_bytes']),
                                     (os.O_RDONLY, -1, spec['size_bytes']),
                                     (os.O_RDONLY, spec['offset_bytes'], spec['size_bytes'] + 1)):
            with self.subTest(flags=flags, offset=offset, length=length):
                fd = os.open(self.stage / 'image.img', flags)
                try:
                    result = subprocess.run([str(INSPECT_DRIVER), '--fd', str(fd), str(offset), str(length)],
                                            pass_fds=(fd,), capture_output=True)
                    self.assertNotEqual(result.returncode, 0)
                finally: os.close(fd)
        self.assertEqual(snapshot(self.stage), before)

    def test_allocation_leak_refused(self):
        self.publish()
        spec = json.loads((self.stage / 'plan.json').read_bytes())['layout']['exfat']
        with (self.stage / 'image.img').open('r+b') as f:
            f.seek(spec['offset_bytes'])
            fs = ExfatFixture(f.read(spec['size_bytes']))
            # The last cluster is unallocated in this small fixture.
            bit = fs.count - 1
            offset = spec['offset_bytes'] + fs.offset(3) + bit // 8
            f.seek(offset); old = f.read(1)
            self.assertFalse(old[0] & (1 << (bit % 8)))
            f.seek(offset); f.write(bytes([old[0] | (1 << (bit % 8))]))
        self.check_inspection(False)

    def test_corrupt_volume_refused(self):
        self.publish()
        spec = json.loads((self.stage / 'plan.json').read_bytes())['layout']['exfat']
        with (self.stage / 'image.img').open('r+b') as f:
            f.seek(spec['offset_bytes'] + 3); f.write(b'BROKEN!!')
        self.check_inspection(False)


if __name__ == '__main__':
    unittest.main()
