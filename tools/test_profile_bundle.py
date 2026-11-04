#!/usr/bin/env python3
"""Offline fixture tests for portable profile bundles; no guest or security claim."""
import base64
import copy
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import uuid
from unittest import mock

import profile_bundle as p

REPO = Path(__file__).resolve().parents[1]
REVIEW = REPO / 'build/review/profiles'


def entry(path, data=b'hello'):
    return {'path': path, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'data_base64': base64.b64encode(data).decode()}


def bundle():
    return {'format': p.FORMAT, 'schema_version': p.VERSION,
            'profile': {'id': str(uuid.uuid4()), 'display_name': 'Readable Name'},
            'preferences': {'text_size': 16, 'wallpaper': 1},
            'documents': [entry('Documents/folder/binary.dat', b'\x00\xff\x80data\n')]}


class ProfileBundleTests(unittest.TestCase):
    def setUp(self):
        REVIEW.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=REVIEW)
        self.base = Path(self.temp.name)
        self.a, self.b = self.base / 'install-a', self.base / 'install-b'
        p.init_store(self.a)
        p.init_store(self.b)
        self.addCleanup(self.temp.cleanup)

    def test_round_trip_and_environment(self):
        initial = bundle()
        src = p.import_bundle(self.a, initial, 'alice', 1001, 1001)
        exported = p.export_bundle(self.a, initial['profile']['id'], ['Documents/folder/binary.dat'])
        dst = p.import_bundle(self.b, exported, 'local_b', 2050, 2050)
        self.assertNotEqual(src['home'], dst['home'])
        self.assertEqual((src['uid'], dst['uid']), (1001, 2050))
        self.assertEqual(p.export_bundle(self.b, initial['profile']['id'], ['Documents/folder/binary.dat']), exported)
        self.assertEqual(exported, initial)
        serialized = json.dumps(exported)
        for local_value in (src['home'], dst['home'], 'alice', 'local_b'):
            self.assertNotIn(local_value, serialized)
        for local_key in ('uid', 'gid', 'alias'):
            self.assertNotIn('\"' + local_key + '\"', serialized)
        env = p.environment(self.b, initial['profile']['id'])
        self.assertEqual(env['HOME'], dst['home'])
        self.assertEqual(env['SRVROS_PROFILE_ID'], initial['profile']['id'])
        self.assertEqual(env['XDG_DATA_HOME'], dst['home'] + '/.local/share')
        self.assertEqual(exported['profile']['display_name'], 'Readable Name')

    def test_c_provider_matches_both_installations(self):
        binary = REPO / 'build/tests/profile-paths-host'
        if not binary.is_file():
            self.skipTest('build/tests/profile-paths-host is not built')
        x = bundle()
        for root, alias, uid in ((self.a, 'alice', 1001), (self.b, 'other', 2050)):
            p.import_bundle(root, x, alias, uid, uid)
            env = p.environment(root, x['profile']['id'])
            result = subprocess.run([str(binary), env['HOME']], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.splitlines(), [env['HOME'], env['XDG_CONFIG_HOME'],
                                                          env['XDG_DATA_HOME'], env['XDG_CACHE_HOME']])

    def test_busy_and_invalid_import_leave_store_unchanged(self):
        x = bundle()
        with open(self.a / '.lock', 'rb') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with self.assertRaises(p.ProfileError):
                p.import_bundle(self.a, x, 'alice', 1001, 1001)
        bad = copy.deepcopy(x)
        bad['documents'][0]['sha256'] = '0' * 64
        with self.assertRaises(p.ProfileError):
            p.import_bundle(self.a, bad, 'alice', 1001, 1001)
        self.assertEqual(list((self.a / 'staging').iterdir()), [])
        self.assertEqual(list((self.a / 'profiles').iterdir()), [])

    def test_init_rejects_symlinks_without_mutation(self):
        target = self.base / 'target'
        target.mkdir()
        link = self.base / 'link'
        link.symlink_to(target, target_is_directory=True)
        for root in (link, link / 'nested' / 'store'):
            with self.subTest(root=root), self.assertRaises((p.ProfileError, OSError)):
                p.init_store(root)
            self.assertEqual(list(target.iterdir()), [])

    def test_resource_limits_and_json_depth(self):
        for raw in (b'9' * 5000, b'[' * 2000 + b']' * 2000, b' ' * (p.MAX_BUNDLE + 1)):
            with self.subTest(length=len(raw)), self.assertRaises(p.ProfileError):
                p.decode_json(raw)
        x = bundle()
        x['documents'] = [entry('Documents/f' + str(i), b'') for i in range(p.MAX_FILES + 1)]
        with self.assertRaises(p.ProfileError): p.validate_bundle(x)
        x['documents'] = [entry('Documents/too-big', b'x' * (p.MAX_FILE + 1))]
        with self.assertRaises(p.ProfileError): p.validate_bundle(x)
        chunk = b'x' * p.MAX_FILE
        document = entry('Documents/f0', chunk)
        x['documents'] = [dict(document, path='Documents/f' + str(i))
                          for i in range(p.MAX_TOTAL // p.MAX_FILE + 1)]
        with self.assertRaises(p.ProfileError): p.validate_bundle(x)

    def test_json_and_schema(self):
        for raw in (b'{', b'{"a":1,"a":2}', b'\xff', b'{"x":NaN}'):
            with self.subTest(raw=raw), self.assertRaises(p.ProfileError): p.decode_json(raw)
        variations = []
        x = bundle(); x['extra'] = 1; variations.append(x)
        x = bundle(); x['schema_version'] = True; variations.append(x)
        x = bundle(); x['schema_version'] = 2; variations.append(x)
        x = bundle(); x['profile']['uid'] = 1001; variations.append(x)
        x = bundle(); del x['profile']['display_name']; variations.append(x)
        x = bundle(); x['profile']['id'] = 'bad'; variations.append(x)
        x = bundle(); x['preferences']['display_width'] = 800; variations.append(x)
        x = bundle(); x['preferences']['password'] = 'not-exportable'; variations.append(x)
        x = bundle(); x['preferences']['text_size'] = True; variations.append(x)
        for x in variations:
            with self.subTest(x=x), self.assertRaises(p.ProfileError): p.validate_bundle(x)

    def test_document_paths_integrity_and_collisions(self):
        for path in ('../Documents/a', '/Documents/a', 'Documents/../a', 'Documents/./a',
                     'Documents/.hidden', 'Documents/a\\b', 'Documents//a', 'Documents/a/'):
            x = bundle(); x['documents'] = [entry(path)]
            with self.subTest(path=path), self.assertRaises(p.ProfileError): p.validate_bundle(x)
        for key, value in (('data_base64', '!'), ('sha256', '0' * 64), ('size', 99)):
            x = bundle(); x['documents'][0][key] = value
            with self.subTest(key=key), self.assertRaises(p.ProfileError): p.validate_bundle(x)
        for paths in (('Documents/a', 'Documents/a'), ('Documents/a', 'Documents/a/b'),
                      ('Documents/A', 'Documents/a'), ('Documents/Folder/a', 'Documents/folder/b')):
            x = bundle(); x['documents'] = [entry(q) for q in paths]
            with self.subTest(paths=paths), self.assertRaises(p.ProfileError): p.validate_bundle(x)

    def test_conflicts_do_not_mutate_store(self):
        existing = bundle(); p.import_bundle(self.a, existing, 'alice', 1001, 1001)
        before = sorted(str(q.relative_to(self.a)) for q in self.a.rglob('*'))
        for candidate, alias, uid in ((existing, 'other', 2000), (bundle(), 'alice', 2000),
                                      (bundle(), 'other', 1001)):
            with self.subTest(alias=alias, uid=uid), self.assertRaises(p.ProfileError):
                p.import_bundle(self.a, candidate, alias, uid, 2000)
            self.assertEqual(before, sorted(str(q.relative_to(self.a)) for q in self.a.rglob('*')))
            self.assertEqual(p.export_bundle(self.a, existing['profile']['id'], ['Documents/folder/binary.dat']), existing)

    def test_source_file_types_and_explicit_omissions(self):
        x = bundle(); src = p.import_bundle(self.a, x, 'alice', 1001, 1001)
        home = Path(src['home']); docs = home / 'Documents'
        for rel in ('.cache/secret', 'session', 'credentials', 'display.conf', 'Documents/unselected'):
            target = home / rel; target.parent.mkdir(parents=True, exist_ok=True); target.write_bytes(b'SECRET')
        exported = p.export_bundle(self.a, x['profile']['id'], ['Documents/folder/binary.dat'])
        self.assertEqual([item['path'] for item in exported['documents']], ['Documents/folder/binary.dat'])
        self.assertEqual(exported['preferences'], x['preferences'])
        (docs / 'symlink').symlink_to(docs / 'folder/binary.dat')
        (docs / 'linked_dir').symlink_to(docs / 'folder', target_is_directory=True)
        os.link(docs / 'folder/binary.dat', docs / 'hardlink')
        os.mkfifo(docs / 'fifo')
        for path in ('Documents/symlink', 'Documents/linked_dir/binary.dat', 'Documents/hardlink', 'Documents/fifo'):
            with self.subTest(path=path), self.assertRaises((p.ProfileError, OSError)):
                p.export_bundle(self.a, x['profile']['id'], [path])

    def test_legacy_projection(self):
        x = bundle(); src = p.import_bundle(self.a, x, 'alice', 1001, 1001)
        contents = ("srvros-desktop-v7\ntext_size=20\nclock_flags=3\nwallpaper=2\n"
                    "theme_accent=1\nglass_level=2\ntheme_style=1\n"
                    "utc_offset_minutes=330\npointer_speed=4\npointer_acceleration=1\n"
                    "disable_while_typing=1\ntap_to_click=1\ntwo_finger_tap=1\n"
                    "display_scale=2\ndisplay_width=1280\ndisplay_height=800\n")
        desktop = Path(src['home']) / 'desktop.conf'
        desktop.write_text(contents)
        projected = p.export_bundle(self.a, x['profile']['id'], [], legacy_desktop=True)
        expected = {'text_size': 20, 'clock_flags': 3, 'wallpaper': 2,
                    'theme_accent': 1, 'glass_level': 2, 'theme_style': 1}
        self.assertEqual(projected['preferences'], expected)
        self.assertEqual(desktop.read_text(), contents)
        target = self.base / 'legacy.json'
        result = subprocess.run([sys.executable, str(REPO / 'tools/profile_bundle.py'), 'export',
                                 str(self.a), x['profile']['id'], str(target), '--legacy-desktop'],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(target.read_text()), projected)
        dst = p.import_bundle(self.b, projected, 'other', 2050, 2050)
        self.assertEqual(json.loads((Path(dst['home']) / p.PREF_PATH).read_text()), expected)
        self.assertEqual(desktop.read_text(), contents)
        (Path(src['home']) / 'desktop.conf').write_text(contents + 'extra=1\n')
        with self.assertRaises(p.ProfileError): p.export_bundle(self.a, x['profile']['id'], [], legacy_desktop=True)

    def test_checkpoint_recovery_and_retry(self):
        for fail_at in ('stage', 'metadata', 'document:Documents/folder/binary.dat', 'before-publish', 'after-publish'):
            with self.subTest(fail_at=fail_at):
                root = self.base / ('checkpoint-' + fail_at.replace('/', '_').replace(':', '_'))
                p.init_store(root); x = bundle()
                def checkpoint(name):
                    if name == fail_at: raise RuntimeError(name)
                with self.assertRaises(RuntimeError): p.import_bundle(root, x, 'alice', 1001, 1001, checkpoint)
                if fail_at == 'after-publish':
                    self.assertEqual(p.recover(root)['discarded_unpublished_imports'], 0)
                    with self.assertRaises(p.ProfileError): p.import_bundle(root, x, 'alice', 1001, 1001)
                    self.assertEqual(p.export_bundle(root, x['profile']['id'], ['Documents/folder/binary.dat']), x)
                else:
                    with self.assertRaises(p.ProfileError): p.import_bundle(root, x, 'alice', 1001, 1001)
                    self.assertEqual(p.recover(root)['discarded_unpublished_imports'], 1)
                    p.import_bundle(root, x, 'alice', 1001, 1001)
                    self.assertEqual(p.export_bundle(root, x['profile']['id'], ['Documents/folder/binary.dat']), x)

    def test_write_failure_recovery(self):
        x = bundle()
        x['documents'].append(entry('Documents/second', b'second'))
        original = p.write_new
        def failing(path, data):
            if Path(path).name == 'second':
                raise OSError('injected write failure')
            return original(path, data)
        with mock.patch.object(p, 'write_new', side_effect=failing):
            with self.assertRaises(OSError): p.import_bundle(self.a, x, 'alice', 1001, 1001)
        self.assertEqual(list((self.a / 'profiles').iterdir()), [])
        self.assertEqual(p.recover(self.a)['discarded_unpublished_imports'], 1)
        p.import_bundle(self.a, x, 'alice', 1001, 1001)
        self.assertEqual(p.export_bundle(self.a, x['profile']['id'],
                         ['Documents/folder/binary.dat', 'Documents/second']), x)

    def test_publish_no_overwrite_and_cli_inspect_failure(self):
        target = self.base / 'bundle.json'; target.write_bytes(b'existing')
        with self.assertRaises(p.ProfileError): p.publish_bundle(target, bundle())
        self.assertEqual(target.read_bytes(), b'existing')
        target.write_bytes(b'{broken')
        result = subprocess.run([sys.executable, str(REPO / 'tools/profile_bundle.py'), 'inspect', str(target)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('profile:', result.stderr)


if __name__ == '__main__': unittest.main()
