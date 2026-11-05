#!/usr/bin/env python3
import builtins
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
from unittest import mock
import tempfile
import tracemalloc
import unittest
import uuid

import profile_bundle as v1
import profile_stream as s


class StreamTests(unittest.TestCase):
    def setUp(self):
        test_dir = Path(__file__).resolve().parent.parent / '.local/profile-stream/host-tests'
        test_dir.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=test_dir)
        self.base = Path(self.tmp.name)
        self.store = self.base / 'store'
        v1.init_store(self.store)
        self.profile = v1.create_profile(self.store, 'Test', 'alice', 100, 100)
        self.ident = self.profile['profile_id']
        self.home = Path(self.profile['home'])

    def tearDown(self):
        self.tmp.cleanup()

    def doc(self, name, size):
        path = self.home / 'Documents' / name
        with path.open('wb') as out:
            block = b'x' * s.BUFFER
            for _ in range(size // len(block)):
                out.write(block)
            out.write(block[:size % len(block)])
        return 'Documents/' + name

    def raw(self, obj, payload=b'', reserved=0):
        manifest = json.dumps(obj, separators=(',', ':')).encode()
        return s.HEADER.pack(s.MAGIC, len(manifest), reserved) + manifest + payload

    def valid(self):
        return {'format': v1.FORMAT, 'schema_version': 2, 'profile': {'id': str(uuid.uuid4()), 'display_name': 'New'}, 'preferences': {}, 'documents': []}

    def reject(self, data):
        path = self.base / 'bad'
        path.write_bytes(data)
        with self.assertRaises((v1.ProfileError, OSError)):
            with s.open_bundle(path):
                pass

    def test_roundtrip_and_legacy(self):
        doc = self.doc('a', 100000)
        path = self.base / 'v2'
        s.export_bundle(self.store, self.ident, [doc], path)
        with s.open_bundle(path) as bundle:
            self.assertEqual(bundle.offsets[doc], path.stat().st_size - 100000)
            self.assertEqual(bundle.sha256, hashlib.sha256(path.read_bytes()).hexdigest())
        other = self.base / 'other'; v1.init_store(other)
        result = s.import_bundle(other, path, 'bob', 200, 201)
        self.assertEqual((Path(result['home']) / doc).read_bytes(), (self.home / doc).read_bytes())
        legacy = self.base / 'legacy'
        v1.publish_bundle(legacy, v1.export_bundle(self.store, self.ident, [doc]))
        with s.open_bundle(legacy) as bundle:
            self.assertTrue(bundle.legacy)
        third = self.base / 'third'; v1.init_store(third)
        s.import_bundle(third, legacy, 'carol', 300, 300)

    def test_caps_and_memory(self):
        docs = [self.doc(str(i), s.MAX_FILE) for i in range(4)]
        path = self.base / 'large'
        tracemalloc.start()
        s.export_bundle(self.store, self.ident, docs, path)
        with s.open_bundle(path) as source:
            self.assertEqual(sum(e['size'] for e in source.metadata['documents']), s.MAX_TOTAL)
        other = self.base / 'large-import'; v1.init_store(other)
        s.import_bundle(other, path, 'large', 999, 999)
        _, peak = tracemalloc.get_traced_memory(); tracemalloc.stop()
        self.assertLess(peak, 4 * 1024 * 1024)
        self.doc('extra', 1)
        with self.assertRaises(v1.ProfileError):
            s.export_bundle(self.store, self.ident, docs + ['Documents/extra'], self.base / 'over')
        self.assertFalse((self.base / 'over').exists())
        self.doc('huge', s.MAX_FILE + 1)
        with self.assertRaises(v1.ProfileError):
            s.export_bundle(self.store, self.ident, ['Documents/huge'], self.base / 'overfile')

    def test_invalid_framing_and_metadata(self):
        obj = self.valid(); data = self.raw(obj)
        for invalid in (data[:-1], data + b'x', self.raw(obj, reserved=1), s.HEADER.pack(s.MAGIC, 65537, 0), data.replace(b'"schema_version":2', b'"schema_version":3'), data.replace(b'"schema_version":2', b'"schema_version":2,"schema_version":2')):
            self.reject(invalid)
        for paths in (['../bad'], ['Documents/a', 'Documents/A'], ['Documents/a', 'Documents/a/b']):
            obj = self.valid(); obj['documents'] = [{'path': p, 'size': 0, 'sha256': hashlib.sha256(b'').hexdigest()} for p in paths]
            self.reject(self.raw(obj))
        obj = self.valid(); obj['documents'] = [{'path': 'Documents/a', 'size': 1, 'sha256': '0' * 64}]
        self.reject(self.raw(obj, b'x'))
        obj['documents'] *= 65; self.reject(self.raw(obj))

    def test_links_existing_and_recovery(self):
        doc = self.doc('a', 10); path = self.base / 'bundle'
        s.export_bundle(self.store, self.ident, [doc], path)
        with self.assertRaises(FileExistsError):
            s.export_bundle(self.store, self.ident, [doc], path)
        link = self.base / 'link'; link.symlink_to(path)
        with self.assertRaises(OSError):
            with s.open_bundle(link): pass
        hard = self.base / 'hard'; os.link(path, hard)
        with self.assertRaises(v1.ProfileError):
            with s.open_bundle(path): pass
        hard.unlink()
        other = self.base / 'other'; v1.init_store(other)
        def fail(point):
            if point == 'before-publish': raise RuntimeError('interruption')
        with self.assertRaises(RuntimeError):
            s.import_bundle(other, path, 'bob', 200, 200, fail)
        self.assertEqual(v1.recover(other)['discarded_unpublished_imports'], 1)
        s.import_bundle(other, path, 'bob', 200, 200)

    def test_short_io_and_fsync_failure(self):
        doc = self.doc('short', 100000)
        output = self.base / 'short.bundle'
        original_open = s._open
        class ShortReader:
            def __init__(self, inner): self.inner = inner
            def read(self, count): return self.inner.read(min(count, 3))
            def __getattr__(self, name): return getattr(self.inner, name)
            def __enter__(self): return self
            def __exit__(self, *args): self.inner.close()
        def short_open(path):
            stream, stamp = original_open(path)
            return ShortReader(stream), stamp
        class ShortWriter:
            def __init__(self): self.data = bytearray()
            def write(self, data):
                n = min(3, len(data)); self.data.extend(data[:n]); return n
        writer = ShortWriter(); s._write_all(writer, b'abcdefgh')
        self.assertEqual(writer.data, b'abcdefgh')
        with self.assertRaises(v1.ProfileError):
            s._write_all(type('Bad', (), {'write': lambda self, data: 0})(), b'x')
        with mock.patch.object(s, '_open', short_open):
            s.export_bundle(self.store, self.ident, [doc], output)
            with s.open_bundle(output) as source:
                self.assertEqual(source.metadata['documents'][0]['size'], 100000)
        original_builtin_open = builtins.open
        class ShortFile:
            def __init__(self, inner): self.inner = inner
            def write(self, data): return self.inner.write(data[:min(len(data), 7)])
            def __getattr__(self, name): return getattr(self.inner, name)
            def __enter__(self): return self
            def __exit__(self, *args): return self.inner.__exit__(*args)
        def short_write_open(path, mode='r', *args, **kwargs):
            result = original_builtin_open(path, mode, *args, **kwargs)
            return ShortFile(result) if mode == 'xb' and Path(path) == short_output else result
        short_output = self.base / 'short-writes.bundle'
        with mock.patch('builtins.open', short_write_open):
            s.export_bundle(self.store, self.ident, [doc], short_output)
        with s.open_bundle(short_output) as source:
            self.assertEqual(source.metadata['documents'][0]['size'], 100000)
        failed = self.base / 'fsync-failed'
        with mock.patch.object(s.os, 'fsync', side_effect=OSError('injected fsync failure')):
            with self.assertRaises(OSError):
                s.export_bundle(self.store, self.ident, [doc], failed)
        self.assertTrue(failed.exists())
        with self.assertRaises(FileExistsError):
            s.export_bundle(self.store, self.ident, [doc], failed)
        other = self.base / 'fsync-import'; v1.init_store(other)
        real_fsync = os.fsync
        calls = 0
        def fail_document_fsync(fd):
            nonlocal calls
            calls += 1
            if calls == 4: raise OSError('injected document fsync failure')
            return real_fsync(fd)
        with mock.patch.object(s.os, 'fsync', side_effect=fail_document_fsync):
            with self.assertRaises(OSError):
                s.import_bundle(other, output, 'bob', 200, 200)
        self.assertFalse((other / 'profiles' / self.ident).exists())
        self.assertEqual(v1.recover(other)['discarded_unpublished_imports'], 1)

    def test_path_replacement_fails_closed(self):
        doc = self.doc('a', 100); path = self.base / 'bundle'
        s.export_bundle(self.store, self.ident, [doc], path)
        other = self.base / 'other'; v1.init_store(other)
        def replace(point):
            if point == 'before-publish':
                path.rename(self.base / 'original')
                path.write_bytes(b'replacement')
        with self.assertRaises(v1.ProfileError):
            s.import_bundle(other, path, 'bob', 200, 200, replace)
        self.assertFalse((other / 'profiles' / self.ident).exists())
        self.assertEqual(v1.recover(other)['discarded_unpublished_imports'], 1)

    def test_native_guest_interoperability(self):
        root = Path(__file__).resolve().parent.parent
        override = os.environ.get('SRVROS_PROFILE_GUEST_TEST_BINARY')
        if override:
            guest = Path(override).resolve()
            self.assertTrue(guest.is_file(), 'configured native guest executable missing')
        else:
            candidates = (root / 'build/tests/guest-profile-host', root / '.local/profile-stream/guest-tests/profile')
            guest = next ( (path for path in candidates if path.is_file()), None )
            if guest is None: self.skipTest('native guest profile executable not built')
        doc = self.doc('large', 1048577); host_bundle = self.base / 'host.bundle'
        s.export_bundle(self.store, self.ident, [doc], host_bundle)
        def guest_run(*args):
            result = subprocess.run([str(guest), *map(str, args)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode(errors='replace'))
            return result.stdout
        inspected = json.loads(guest_run('inspect', host_bundle))
        self.assertEqual(inspected['documents'], [doc])
        guest_temp = tempfile.TemporaryDirectory(prefix='profile-interop-', dir='/private/tmp' if Path('/private/tmp').is_dir() else None)
        self.addCleanup(guest_temp.cleanup)
        home = Path(guest_temp.name).resolve() / 'guestuser'
        guest_run('import', host_bundle, home, 'guestuser', 1001, 1001)
        self.assertEqual((home / doc).read_bytes(), (self.home / doc).read_bytes())
        review = json.loads(guest_run('preview-export-auto', home, doc))
        self.assertEqual(review['bundle_version'], 2)
        guest_bundle = self.base / 'guest.bundle'
        guest_run('export-auto-reviewed', home, guest_bundle, review['sha256'], doc)
        destination = self.base / 'guest-import'; v1.init_store(destination)
        imported = s.import_bundle(destination, guest_bundle, 'hostuser', 2001, 2001)
        self.assertEqual((Path(imported['home']) / doc).read_bytes(), (self.home / doc).read_bytes())

    def test_changed_source_cannot_publish(self):
        doc = self.doc('a', 100); path = self.base / 'bundle'
        s.export_bundle(self.store, self.ident, [doc], path)
        other = self.base / 'other'; v1.init_store(other)
        def mutate(point):
            if point == 'before-publish':
                with path.open('r+b') as stream:
                    stream.seek(-1, os.SEEK_END); stream.write(b'y')
        with self.assertRaises(v1.ProfileError):
            s.import_bundle(other, path, 'bob', 200, 200, mutate)
        self.assertFalse((other / 'profiles' / self.ident).exists())


if __name__ == '__main__':
    unittest.main()
