"""Offline installed-profile export contract tests on disposable images."""
import fcntl
import json
import os
import stat
import tempfile
from pathlib import Path
import struct
import subprocess
import sys
import unittest
from unittest import mock

import installation_export as export
import profile_bundle
import profile_image
import test_installation_inspection as fixtures
from test_installation_inspection import (INSPECT_DRIVER, PROFILE_DRIVER, ROOT,
    add_profile_file, mutate_profile_file, snapshot)
from test_profile_image import bundle

EXPORT_DRIVER = ROOT / 'build/tests/installation-export-driver'
PROFILE_ID = bundle()['profile']['id']
HOME = '/fat/home/alice'


class InstalledExportTests(unittest.TestCase):
    # Reuse only fixture setup/publication, not the inspection test methods.
    def setUp(self):
        fixtures.InstalledInspectionTests.setUp(self)
        if not EXPORT_DRIVER.is_file():
            self.fail(f'{EXPORT_DRIVER} is required; build the native export driver')
    publish = fixtures.InstalledInspectionTests.publish

    def call(self, output=None, documents=None, **changes):
        args = dict(stage=self.stage, output=output or self.root / 'export.json',
                    profile_id=PROFILE_ID, home=HOME,
                    documents=[] if documents is None else documents,
                    inspect_driver=INSPECT_DRIVER, export_driver=EXPORT_DRIVER)
        args.update(changes)
        return export.export_installed(**args)

    def check(self, success, output=None, documents=None, **changes):
        output = output or self.root / 'export.json'
        before = snapshot(self.stage)
        result = self.call(output=output, documents=documents, **changes)
        self.assertEqual(snapshot(self.stage), before)
        self.assertEqual(result['state'], 'exported' if success else 'refused', result)
        if success:
            self.assertTrue(result['read_only_source'])
            value = profile_bundle.decode_json(output.read_bytes())
            profile_bundle.validate_bundle(value)
            return value
        self.assertTrue(result.get('reason'), result)
        self.assertFalse(output.exists())
        return result

    def test_pristine_and_used_selected_documents(self):
        self.publish()
        pristine = self.check(True, documents=['Documents/notes.txt'])
        self.assertEqual(pristine, bundle())
        (self.root / 'export.json').unlink()
        mutate_profile_file(self.stage, 'home/alice/Documents/notes.txt', lambda data: data.upper())
        add_profile_file(self.stage, PROFILE_DRIVER, HOME + '/Documents/binary.dat', b'\x00\xff\x80\n')
        add_profile_file(self.stage, PROFILE_DRIVER, HOME + '/Documents/secret.txt', b'not selected')
        for path in (self.kernel, self.boot, self.init, self.exfat, self.bundle):
            path.unlink()
        result = self.check(True, documents=['Documents/notes.txt', 'Documents/binary.dat'])
        self.assertEqual(result['profile'], bundle()['profile'])
        self.assertEqual(result['preferences'], {'text_size': 16})
        payloads = profile_bundle.validate_bundle(result)
        self.assertEqual(payloads, {'Documents/notes.txt': b'PORTABLE DATA',
                                    'Documents/binary.dat': b'\x00\xff\x80\n'})
        serialized = json.dumps(result)
        for value in ('secret.txt', '/fat/home/alice', 'profile-local', 'uid', 'gid', 'alias'):
            self.assertNotIn(value, serialized)

    def test_no_documents_and_legacy_identity(self):
        validate = profile_image.validate_request
        def legacy(*args, **kwargs):
            metadata, files, directories = validate(*args, **kwargs)
            del files[metadata['home'] + '/identity.json']
            return metadata, files, directories
        with mock.patch.object(profile_image, 'validate_request', side_effect=legacy):
            self.publish()
        value = self.check(True)
        self.assertEqual(value['profile'], {'id': PROFILE_ID, 'display_name': 'alice'})
        self.assertEqual(value['documents'], [])

    def test_invalid_selection_refused_without_stage_changes(self):
        self.publish()
        cases = [dict(profile_id='22345678-1234-4abc-8def-123456789abc'),
                 dict(home='/fat/home/bob'),
                 dict(documents=['Documents/missing']),
                 dict(documents=['Documents']),
                 dict(documents=['Documents/../profile.id']),
                 dict(documents=['Documents/Notes.txt']),
                 dict(documents=['Documents/notes.txt', 'Documents/notes.txt']),
                 dict(documents=['Documents/notes.txt', 'Documents/NOTES.txt']),
                 dict(documents=['Documents/a'] * 65)]
        for changes in cases:
            with self.subTest(changes=changes):
                self.check(False, **changes)

    def test_existing_symlink_and_inside_stage_outputs_refused(self):
        self.publish()
        existing = self.root / 'existing'
        existing.write_bytes(b'preserve')
        link = self.root / 'link'
        link.symlink_to(existing)
        before = snapshot(self.stage)
        for output in (existing, link, self.stage / 'new.json', self.root / 'missing' / 'x'):
            with self.subTest(output=output):
                result = self.call(output=output)
                self.assertEqual(result['state'], 'refused', result)
                self.assertTrue(result.get('reason'))
                self.assertEqual(existing.read_bytes(), b'preserve')
                self.assertEqual(snapshot(self.stage), before)
        self.assertTrue(link.is_symlink())
        self.assertFalse((self.stage / 'new.json').exists())

    def test_pending_journal_and_locked_inputs_refused(self):
        self.publish()
        add_profile_file(self.stage, PROFILE_DRIVER, HOME + '/.profile-import-pending', b'pending')
        self.check(False)
        # A fresh fixture keeps each refusal independent.
        import shutil
        shutil.rmtree(self.stage)
        self.publish()
        record = self.stage / 'publication.json'
        original = record.read_bytes()
        record.write_bytes(b'{}')
        self.check(False)
        record.write_bytes(original)
        for path in (self.stage / 'image.img', self.stage / 'plan.json'):
            with self.subTest(path=path), path.open('rb') as stream:
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
                self.check(False)

    def test_native_driver_rejects_writable_fd_bad_range_and_paths(self):
        self.publish()
        spec = json.loads((self.stage / 'plan.json').read_bytes())['layout']['exfat']
        before = snapshot(self.stage)
        cases = [(os.O_RDWR, spec['offset_bytes'], spec['size_bytes'], HOME, []),
                 (os.O_RDONLY, -1, spec['size_bytes'], HOME, []),
                 (os.O_RDONLY, spec['offset_bytes'], spec['size_bytes'] + 1, HOME, []),
                 (os.O_RDONLY, spec['offset_bytes'], spec['size_bytes'], '/fat/home/../alice', []),
                 (os.O_RDONLY, spec['offset_bytes'], spec['size_bytes'], HOME, ['Documents/../profile.id'])]
        for flags, offset, length, home, documents in cases:
            with self.subTest(flags=flags, offset=offset, length=length, home=home, documents=documents):
                fd = os.open(self.stage / 'image.img', flags)
                try:
                    proc = subprocess.run([str(EXPORT_DRIVER), '--fd', str(fd), str(offset), str(length),
                                           home, *documents], pass_fds=(fd,), capture_output=True)
                finally:
                    os.close(fd)
                self.assertNotEqual(proc.returncode, 0, proc.stdout)
        self.assertEqual(snapshot(self.stage), before)

    def test_malformed_driver_frames_are_refused(self):
        self.publish()
        fake = self.root / 'fake-driver'
        magic = b'srvros-installed-export-v1\n'
        cases = (b'bad header', magic, magic + struct.pack('<Q', 513) + b'x' * 513,
                 magic + struct.pack('<Q', 2) + b'{',
                 magic + struct.pack('<Q', 2) + b'{}' + b'extra')
        for payload in cases:
            with self.subTest(length=len(payload)):
                fake.write_text('#!/usr/bin/env python3\nimport sys\nsys.stdout.buffer.write(' + repr(payload) + ')\n')
                fake.chmod(0o755)
                self.check(False, export_driver=fake)

    def test_driver_changed_during_read_refused(self):
        self.publish()
        fake = self.root / 'fake-driver'
        fake.write_text('#!/usr/bin/env python3\nimport sys\nsys.stdout.buffer.write(' +
                        repr(b'srvros-installed-export-v1\n' + struct.pack('<Q', 2) + b'{}') + ')\n')
        fake.chmod(0o755)
        real = export.driver_bytes
        def changed(*args):
            frames = real(*args)
            fake.write_text(fake.read_text() + '# changed\n')
            return frames
        with mock.patch.object(export, 'driver_bytes', side_effect=changed):
            self.check(False, export_driver=fake)

    def test_publication_failures_never_report_success(self):
        self.publish()
        output = self.root / 'export.json'
        before = snapshot(self.stage)
        real_fsync = os.fsync
        def fail_directory(fd):
            import stat
            if stat.S_ISDIR(os.fstat(fd).st_mode):
                raise OSError('directory sync failed')
            return real_fsync(fd)
        for target, effect in (('link', OSError('link failed')),
                               ('fsync', OSError('file sync failed')),
                               ('fsync', fail_directory)):
            with self.subTest(target=target, effect=effect):
                with mock.patch.object(export.os, target, side_effect=effect):
                    result = self.call(output=output)
                self.assertEqual(result['state'], 'refused', result)
                self.assertTrue(result.get('reason'))
                self.assertEqual(snapshot(self.stage), before)
                if output.exists():
                    profile_bundle.validate_bundle(profile_bundle.decode_json(output.read_bytes()))
                    output.unlink()
                self.assertFalse(list(self.root.glob('.profile-export-*')))

    def test_native_selected_file_and_aggregate_limits(self):
        # A larger private filesystem accommodates nine independent 1 MiB files.
        from test_storage_leases import empty_image
        with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=self.root) as temp:
            image = Path(temp) / 'image'
            image.write_bytes(empty_image(32 * 1024 * 1024).data)
            for operation, path in (('ensure-home', '/fat/home'), ('mkdir', '/fat/bin'),
                                    ('mkdir', '/fat/var')):
                subprocess.run([str(PROFILE_DRIVER), str(image), operation, path], check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.exfat.write_bytes(image.read_bytes())
        self.publish()
        data = b'x' * profile_bundle.MAX_FILE
        names = [f'Documents/limit{i}.bin' for i in range(9)]
        for name in names:
            add_profile_file(self.stage, PROFILE_DRIVER, HOME + '/' + name, data)
        first = self.check(True, documents=names[:1])
        self.assertEqual(profile_bundle.validate_bundle(first)[names[0]], data)
        (self.root / 'export.json').unlink()
        eight = self.check(True, documents=names[:8])
        self.assertEqual(sum(len(v) for v in profile_bundle.validate_bundle(eight).values()),
                         profile_bundle.MAX_TOTAL)
        (self.root / 'export.json').unlink()
        result = self.check(False, documents=names)
        self.assertIn('driver', result['reason'].lower())
        # The native driver must reject the aggregate before emitting even a header.
        spec = json.loads((self.stage / 'plan.json').read_bytes())['layout']['exfat']
        fd = os.open(self.stage / 'image.img', os.O_RDONLY)
        try:
            proc = subprocess.run([str(EXPORT_DRIVER), '--fd', str(fd), str(spec['offset_bytes']),
                                   str(spec['size_bytes']), HOME, *names],
                                  pass_fds=(fd,), capture_output=True)
        finally:
            os.close(fd)
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(proc.stdout, b'')

    def test_oversized_native_frame_refused(self):
        self.publish()
        fake = self.root / 'fake-driver'
        # A frame header alone is sufficient to test the advertised per-file cap.
        payload = (b'srvros-installed-export-v1\n' + struct.pack('<Q', 2) + b'{}' +
                   struct.pack('<Q', profile_bundle.MAX_FILE + 1))
        fake.write_text('#!/usr/bin/env python3\nimport sys\nsys.stdout.buffer.write(' + repr(payload) + ')\n')
        fake.chmod(0o755)
        result = self.check(False, documents=['Documents/notes.txt'], export_driver=fake)
        self.assertIn('frame', result['reason'].lower())

    def test_source_changes_after_native_read_are_refused(self):
        self.publish()
        image = self.stage / 'image.img'
        before = snapshot(self.stage)
        real = export.driver_bytes
        old = None
        def changed(*args):
            nonlocal old
            frames = real(*args)
            with image.open('r+b') as stream:
                stream.seek(100)
                old = stream.read(1)
                stream.seek(100)
                stream.write(bytes([old[0] ^ 1]))
            return frames
        try:
            with mock.patch.object(export, 'driver_bytes', side_effect=changed):
                result = self.call(documents=['Documents/notes.txt'])
            self.assertIsNotNone(old, 'native driver was not reached')
            self.assertEqual(result['state'], 'refused', result)
            self.assertIn('image changed', result['reason'].lower())
            self.assertFalse((self.root / 'export.json').exists())
        finally:
            if old is not None:
                with image.open('r+b') as stream:
                    stream.seek(100)
                    stream.write(old)
        self.assertEqual(snapshot(self.stage), before)

    def test_publication_write_and_post_link_failures(self):
        self.publish()
        output = self.root / 'export.json'
        before = snapshot(self.stage)
        real_fdopen, real_unlink, real_close = os.fdopen, os.unlink, os.close

        class FaultStream:
            def __init__(self, stream, short):
                self.stream, self.short = stream, short
            def __enter__(self):
                self.stream.__enter__()
                return self
            def __exit__(self, *args):
                return self.stream.__exit__(*args)
            def write(self, data):
                if self.short:
                    return self.stream.write(data[:1])
                raise OSError('injected write failure')
            def __getattr__(self, name):
                return getattr(self.stream, name)

        for short in (False, True):
            with self.subTest(short=short):
                triggered = []
                def faulty_fdopen(*args, **kwargs):
                    stream = real_fdopen(*args, **kwargs)
                    mode = args[1] if len(args) > 1 else kwargs.get('mode', 'r')
                    if mode == 'wb':
                        triggered.append(True)
                        return FaultStream(stream, short)
                    return stream
                with mock.patch.object(export.os, 'fdopen', side_effect=faulty_fdopen):
                    result = self.call(output=output)
                self.assertTrue(triggered, 'publication stream was not reached')
                self.assertEqual(result['state'], 'refused', result)
                self.assertIn('short' if short else 'injected write', result['reason'].lower())
                self.assertFalse(result.get('visible_uncertain', False))
                self.assertFalse(output.exists())
                self.assertEqual(snapshot(self.stage), before)

        failed = False
        def unlink_once(path, *args, **kwargs):
            nonlocal failed
            if not failed and Path(path).name.startswith('.profile-export-'):
                failed = True
                raise OSError('injected unlink failure')
            return real_unlink(path, *args, **kwargs)
        with mock.patch.object(export.os, 'unlink', side_effect=unlink_once):
            result = self.call(output=output)
        self.assertTrue(failed)
        self.assertEqual(result['state'], 'refused', result)
        self.assertTrue(result.get('visible_uncertain'), result)
        self.assertTrue(output.exists())
        output.unlink()

        closed = False
        def close_directory(fd):
            nonlocal closed
            if not closed and stat.S_ISDIR(os.fstat(fd).st_mode):
                closed = True
                real_close(fd)
                raise OSError('injected directory close failure')
            return real_close(fd)
        with mock.patch.object(export.os, 'close', side_effect=close_directory):
            result = self.call(output=output)
        self.assertTrue(closed)
        self.assertEqual(result['state'], 'refused', result)
        self.assertTrue(result.get('visible_uncertain'), result)
        self.assertTrue(output.exists())
        self.assertEqual(snapshot(self.stage), before)

    def test_cli_export(self):
        self.publish()
        output = self.root / 'export.json'
        before = snapshot(self.stage)
        proc = subprocess.run([sys.executable, str(ROOT / 'tools/installation_image.py'),
            'export-profile', str(self.stage), str(output), '--profile-id', PROFILE_ID,
            '--home', HOME, '--document', 'Documents/notes.txt',
            '--inspect-driver', str(INSPECT_DRIVER), '--export-driver', str(EXPORT_DRIVER)],
            cwd=self.root, capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(json.loads(proc.stdout)['state'], 'exported')
        self.assertEqual(profile_bundle.decode_json(output.read_bytes()), bundle())
        self.assertEqual(snapshot(self.stage), before)


if __name__ == '__main__':
    unittest.main()
