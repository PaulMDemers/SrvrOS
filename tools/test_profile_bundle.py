"""Host metadata and local-store tests for v2 portable profile streams."""
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

import profile_bundle as p
import profile_stream as s

REPO = Path(__file__).resolve().parents[1]
REVIEW = REPO / '.local/profile-stream/host-tests'
DOC = 'Documents/folder/binary.dat'
DATA = b'\x00\xff\x80data\n'


def metadata():
    return {'format': p.FORMAT, 'schema_version': 2,
            'profile': {'id': str(uuid.uuid4()), 'display_name': 'Readable Name'},
            'preferences': {'text_size': 16, 'wallpaper': 1}}


def seed(root, value, alias='alice', uid=1001, gid=1001, extra=None):
    who = value['profile']
    def populate(home):
        path = home / DOC
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(DATA)
        for name, content in (extra or {}).items():
            target = home / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
    return p.install_profile(root, who, value['preferences'], p.binding(alias, uid, gid), populate)


def make_raw(value, payload=b'', *, indent=None, sort_keys=False):
    encoded = json.dumps(value, sort_keys=sort_keys, indent=indent,
                         separators=None if indent else (',', ':'), ensure_ascii=True).encode()
    return s.HEADER.pack(s.MAGIC, len(encoded), 0) + encoded + payload


def snapshot(root):
    result = {}
    for path in sorted(root.rglob('*')):
        st = path.lstat()
        row = [st.st_dev, st.st_ino, st.st_mode, st.st_nlink, st.st_size,
               st.st_mtime_ns, st.st_ctime_ns]
        if path.is_file() and not path.is_symlink():
            row.append(hashlib.sha256(path.read_bytes()).hexdigest())
        result[str(path.relative_to(root))] = row
    return result


class ProfileBundleTests(unittest.TestCase):
    def setUp(self):
        REVIEW.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=REVIEW)
        self.base = Path(self.temp.name)
        self.a, self.b = self.base / 'install-a', self.base / 'install-b'
        p.init_store(self.a); p.init_store(self.b)
        self.value = metadata()
        self.source = seed(self.a, self.value)
        self.addCleanup(self.temp.cleanup)

    def export(self, root, target, docs=(DOC,)):
        return s.export_bundle(root, self.value['profile']['id'], list(docs), target)

    def test_round_trip_is_stream_v2_and_keeps_destination_local_binding(self):
        source_file = self.base / 'source.srvrprofile'
        self.export(self.a, source_file)
        with s.open_bundle(source_file, require_canonical=True) as opened:
            self.assertEqual(opened.metadata['schema_version'], 2)
            self.assertEqual(opened.metadata['profile'], self.value['profile'])
            self.assertEqual(opened.metadata['preferences'], self.value['preferences'])
            self.assertEqual(opened.metadata['documents'], [
                {'path': DOC, 'size': len(DATA), 'sha256': hashlib.sha256(DATA).hexdigest()}])
            self.assertEqual(opened.sha256, hashlib.sha256(source_file.read_bytes()).hexdigest())
        result = s.import_bundle(self.b, source_file, 'local_b', 2050, 2051)
        self.assertNotEqual(self.source['home'], result['home'])
        self.assertEqual((Path(result['home']) / DOC).read_bytes(), DATA)
        env = p.environment(self.b, self.value['profile']['id'])
        self.assertEqual(env['HOME'], result['home'])
        self.assertEqual(env['SRVROS_PROFILE_ID'], self.value['profile']['id'])
        self.assertEqual(env['XDG_DATA_HOME'], result['home'] + '/.local/share')
        binding = p.local_binding(Path(result['home']).parent)
        self.assertEqual(binding, {'alias': 'local_b', 'uid': 2050, 'gid': 2051})

    def test_cli_inspect_reports_v2_and_selected_metadata_only(self):
        target = self.base / 'selected.srvrprofile'
        self.export(self.a, target)
        proc = subprocess.run([sys.executable, str(REPO / 'tools/profile_stream.py'),
                               'inspect', str(target)], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        result = json.loads(proc.stdout)
        self.assertEqual(result['bundle_version'], 2)
        self.assertEqual(result['documents'], [DOC])
        self.assertNotIn('data_base64', proc.stdout)
        self.assertNotIn(self.source['home'], proc.stdout)

    def test_c_provider_matches_both_stores(self):
        binary = REPO / 'build/tests/profile-paths-host'
        if not binary.is_file():
            self.skipTest('build/tests/profile-paths-host is not built')
        target = self.base / 'stream.srvrprofile'
        self.export(self.a, target)
        s.import_bundle(self.b, target, 'other', 2050, 2050)
        for root, identifier in ((self.a, self.value['profile']['id']),
                                 (self.b, self.value['profile']['id'])):
            env = p.environment(root, identifier)
            result = subprocess.run([str(binary), env['HOME']], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.splitlines(), [env['HOME'], env['XDG_CONFIG_HOME'],
                env['XDG_DATA_HOME'], env['XDG_CACHE_HOME']])

    def test_busy_and_invalid_stream_import_leave_store_unchanged(self):
        target = self.base / 'stream.srvrprofile'; self.export(self.a, target)
        before = snapshot(self.b)
        with (self.b / '.lock').open('rb') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            with self.assertRaises(p.ProfileError):
                s.import_bundle(self.b, target, 'bob', 1002, 1002)
        self.assertEqual(snapshot(self.b), before)
        for bad in (self.base / 'legacy.json', self.base / 'truncated'):
            bad.write_bytes(p.encode_json({'format': p.FORMAT, 'schema_version': 1,
                'profile': self.value['profile'], 'preferences': self.value['preferences'],
                'documents': [{'path': DOC, 'size': len(DATA),
                    'sha256': hashlib.sha256(DATA).hexdigest(), 'data_base64': 'AA=='}]}))
            if bad.name == 'truncated': bad.write_bytes(target.read_bytes()[:-1])
            with self.subTest(path=bad.name), self.assertRaises((p.ProfileError, OSError)):
                s.import_bundle(self.b, bad, 'bob', 1002, 1002)
            self.assertEqual(snapshot(self.b), before)

    def test_init_rejects_symlinks_without_mutation(self):
        target = self.base / 'target'; target.mkdir()
        link = self.base / 'link'; link.symlink_to(target, target_is_directory=True)
        for root in (link, link / 'nested' / 'store'):
            with self.subTest(root=root), self.assertRaises((p.ProfileError, OSError)):
                p.init_store(root)
            self.assertEqual(list(target.iterdir()), [])

    def test_json_and_schema_limits(self):
        for raw in (b'9' * 5000, b'[' * 2000 + b']' * 2000,
                    b' ' * (p.MAX_JSON + 1), b'{', b'{"a":1,"a":2}',
                    b'\xff', b'{"x":NaN}'):
            with self.subTest(length=len(raw)), self.assertRaises(p.ProfileError): p.decode_json(raw)
        manifest = {'format': p.FORMAT, 'schema_version': 2,
                    'profile': self.value['profile'], 'preferences': {}, 'documents': []}
        variations = []
        bad = copy.deepcopy(manifest); bad['schema_version'] = 1; variations.append(bad)
        bad = copy.deepcopy(manifest); bad['schema_version'] = True; variations.append(bad)
        bad = copy.deepcopy(manifest); bad['extra'] = 1; variations.append(bad)
        bad = copy.deepcopy(manifest); bad['preferences'] = {'password': 'not-exportable'}; variations.append(bad)
        bad = copy.deepcopy(manifest); bad['preferences'] = {'text_size': True}; variations.append(bad)
        bad = copy.deepcopy(manifest); bad['profile']['uid'] = 1001; variations.append(bad)
        for value in variations:
            with self.subTest(value=value), self.assertRaises(p.ProfileError): s.validate_manifest(value)

    def test_v1_json_and_bad_framing_are_rejected(self):
        doc = {'path': DOC, 'size': 0, 'sha256': hashlib.sha256(b'').hexdigest()}
        valid = {'format': p.FORMAT, 'schema_version': 2, 'profile': self.value['profile'],
                 'preferences': {}, 'documents': []}
        cases = [
            p.encode_json({'format': p.FORMAT, 'schema_version': 1, 'profile': self.value['profile'],
                           'preferences': {}, 'documents': []}),
            make_raw(valid)[:-1], make_raw(valid) + b'x',
            s.HEADER.pack(s.MAGIC, 65537, 0) + b'x' * 65537,
            s.HEADER.pack(s.MAGIC, 1, 1) + b'{}',
            make_raw(dict(valid, documents=[dict(doc, data_base64='')]))]
        for index, raw in enumerate(cases):
            path = self.base / ('invalid-' + str(index)); path.write_bytes(raw)
            with self.subTest(index=index), self.assertRaises((p.ProfileError, OSError)):
                with s.open_bundle(path): pass

    def test_noncanonical_v2_manifest_remains_readable(self):
        value = {'format': p.FORMAT, 'schema_version': 2, 'profile': self.value['profile'],
                 'preferences': {}, 'documents': []}
        path = self.base / 'noncanonical.srvrprofile'
        path.write_bytes(make_raw(value, indent=2, sort_keys=False))
        with s.open_bundle(path) as opened:
            self.assertEqual(opened.metadata, value)
        with self.assertRaises(p.ProfileError):
            with s.open_bundle(path, require_canonical=True): pass

    def test_document_paths_and_case_collisions(self):
        for path in ('../Documents/a', '/Documents/a', 'Documents/../a', 'Documents/./a',
                     'Documents/.hidden', 'Documents/a\\b', 'Documents//a', 'Documents/a/',
                     'Documents/a' * 100):
            with self.subTest(path=path), self.assertRaises(p.ProfileError): p.document_path(path)
        for paths in (('Documents/a', 'Documents/a'), ('Documents/a', 'Documents/a/b'),
                      ('Documents/A', 'Documents/a'), ('Documents/Folder/a', 'Documents/folder/b')):
            manifest = {'format': p.FORMAT, 'schema_version': 2,
                        'profile': self.value['profile'], 'preferences': {},
                        'documents': [{'path': q, 'size': 0, 'sha256': hashlib.sha256(b'').hexdigest()}
                                      for q in paths]}
            with self.subTest(paths=paths), self.assertRaises(p.ProfileError): s.validate_manifest(manifest)

    def test_conflicts_and_source_types_do_not_mutate_store(self):
        stream_path = self.base / 'stream.srvrprofile'; self.export(self.a, stream_path)
        before = snapshot(self.a)
        for alias, uid, gid in (('alice', 2000, 2000), ('other', 1001, 2000)):
            with self.subTest(alias=alias, uid=uid), self.assertRaises(p.ProfileError):
                s.import_bundle(self.a, stream_path, alias, uid, gid)
            self.assertEqual(snapshot(self.a), before)
        docs = Path(self.source['home']) / 'Documents'
        (docs / 'symlink').symlink_to(docs / 'folder' / 'binary.dat')
        (docs / 'linked_dir').symlink_to(docs / 'folder', target_is_directory=True)
        os.link(docs / 'folder' / 'binary.dat', docs / 'hardlink')
        os.mkfifo(docs / 'fifo')
        for path in ('Documents/symlink', 'Documents/linked_dir/binary.dat',
                     'Documents/hardlink', 'Documents/fifo'):
            with self.subTest(path=path), self.assertRaises((p.ProfileError, OSError)):
                s.export_bundle(self.a, self.value['profile']['id'], [path], self.base / 'bad')
            self.assertFalse((self.base / 'bad').exists())

    def test_legacy_desktop_projection_still_emits_v2(self):
        contents = ("srvros-desktop-v7\ntext_size=20\nclock_flags=3\nwallpaper=2\n"
                    "theme_accent=1\nglass_level=2\ntheme_style=1\n"
                    "utc_offset_minutes=330\npointer_speed=4\npointer_acceleration=1\n"
                    "disable_while_typing=1\ntap_to_click=1\ntwo_finger_tap=1\n"
                    "display_scale=2\ndisplay_width=1280\ndisplay_height=800\n")
        Path(self.source['home'], 'desktop.conf').write_text(contents)
        target = self.base / 'legacy-settings-projected.srvrprofile'
        s.export_bundle(self.a, self.value['profile']['id'], [], target, legacy_desktop=True)
        with s.open_bundle(target, require_canonical=True) as opened:
            self.assertEqual(opened.metadata['schema_version'], 2)
            self.assertEqual(opened.metadata['preferences'], {'text_size': 20, 'clock_flags': 3,
                'wallpaper': 2, 'theme_accent': 1, 'glass_level': 2, 'theme_style': 1})
        self.assertEqual(Path(self.source['home'], 'desktop.conf').read_text(), contents)

    def test_checkpoint_recovery_and_retry(self):
        stream_path = self.base / 'stream.srvrprofile'; self.export(self.a, stream_path)
        for fail_at in ('stage', 'metadata', 'document:' + DOC, 'before-publish', 'after-publish'):
            with self.subTest(fail_at=fail_at):
                root = self.base / ('checkpoint-' + fail_at.replace('/', '_').replace(':', '_'))
                p.init_store(root)
                def checkpoint(name):
                    if name == fail_at: raise RuntimeError(name)
                with self.assertRaises(RuntimeError): s.import_bundle(root, stream_path, 'alice', 1001, 1001, checkpoint)
                if fail_at == 'after-publish':
                    self.assertEqual(p.recover(root)['discarded_unpublished_imports'], 0)
                    with self.assertRaises(p.ProfileError): s.import_bundle(root, stream_path, 'alice', 1001, 1001)
                else:
                    with self.assertRaises(p.ProfileError): s.import_bundle(root, stream_path, 'alice', 1001, 1001)
                    self.assertEqual(p.recover(root)['discarded_unpublished_imports'], 1)
                    s.import_bundle(root, stream_path, 'alice', 1001, 1001)
                self.assertEqual((Path(p.environment(root, self.value['profile']['id'])['HOME']) / DOC).read_bytes(), DATA)


if __name__ == '__main__': unittest.main()
