#!/usr/bin/env python3
"""Real-filesystem guest CLI contract tests (pass the native profile executable).

The executable uses SRVROS_PROFILE_HOST_TEST solely to allow temporary HOME roots
and host fsync instead of guest srv_sync. The exact parser/codec/transaction code
is exercised; no mocked filesystem and no guest account/security claims.
"""
import base64
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import uuid

BINARY = str(Path(sys.argv.pop(1)).resolve()) if len(sys.argv) > 1 else None
sys.path.insert(0, str(Path(__file__).resolve().parent))
import profile_bundle as normative


class GuestProfile(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='guest-profile-', dir='/tmp')
        self.root = Path(self.temp.name).resolve()
        self.a = self.root / 'a'
        self.b = self.root / 'b'
        self.a.mkdir()
        self.b.mkdir()
        self.bundle = self.root / 'transfer.json'
        self.data = {'Documents/note.txt': b'portable\x00bytes\xff\n',
                     'Documents/Nested/binary.bin': bytes(range(256))}
        self.value = self.make_bundle(self.data)
        self.save()

    def tearDown(self):
        self.temp.cleanup()

    @staticmethod
    def make_bundle(data):
        return {'format': normative.FORMAT, 'schema_version': 1,
                'profile': {'id': str(uuid.uuid4()), 'display_name': 'Guest Profile'},
                'preferences': {'text_size': 20, 'clock_flags': 3, 'wallpaper': 2,
                                'theme_accent': 1, 'glass_level': 0, 'theme_style': 2},
                'documents': [{'path': p, 'size': len(d), 'sha256': hashlib.sha256(d).hexdigest(),
                               'data_base64': base64.b64encode(d).decode()} for p, d in data.items()]}

    def save(self, value=None):
        self.bundle.write_text(json.dumps(self.value if value is None else value))

    def run_cli(self, *args, code=0, stop=None):
        env = dict(os.environ)
        env.pop('SRVROS_PROFILE_STOP_AFTER', None)
        if stop:
            env['SRVROS_PROFILE_STOP_AFTER'] = stop
        result = subprocess.run([BINARY, *map(str, args)], env=env, capture_output=True)
        self.assertEqual(result.returncode, code, result.stderr.decode(errors='replace'))
        return result

    def import_home(self, home=None, **kwargs):
        home = home or self.a / 'alice'
        self.run_cli('import', self.bundle, home, 'alice', 1001, 100, **kwargs)
        return home

    def verify(self, home):
        self.assertEqual((home / 'profile.id').read_text(),
                         'srvros-profile-v1\n' + self.value['profile']['id'] + '\n')
        self.assertEqual(json.loads((home / 'identity.json').read_text()), self.value['profile'])
        self.assertEqual(json.loads((home / '.config/srvros/portable.json').read_text()), self.value['preferences'])
        for path, data in self.data.items():
            self.assertEqual((home / path).read_bytes(), data)
        self.assertFalse((home / '.profile-import-pending').exists())
        self.assertFalse(Path(str(home) + '.profile-import.json').exists())
        self.assertTrue((home / '.cache').is_dir())
        self.assertTrue((home / '.local/share').is_dir())

    @staticmethod
    def snapshot(root):
        return {str(p.relative_to(root)): p.read_bytes() if p.is_file() else None
                for p in root.rglob('*')}

    def test_roundtrip_two_bindings(self):
        first = self.import_home()
        self.verify(first)
        output = self.root / 'export.json'
        self.run_cli('export', first, output, *self.data)
        exported = json.loads(output.read_text())
        self.assertEqual(exported, self.value)
        normative.validate_bundle(exported)
        second = self.b / 'bob'
        self.run_cli('import', output, second, 'bob', 2002, 200)
        self.verify(second)
        self.assertEqual(json.loads((second / 'profile-local.json').read_text()),
                         {'alias': 'bob', 'uid': 2002, 'gid': 200})
        self.assertEqual(json.loads(self.run_cli('inspect', output).stdout)['bytes'], sum(map(len, self.data.values())))
        self.run_cli('export', first, output, *self.data, code=1)
        self.assertEqual(json.loads(output.read_text()), self.value)

    def test_source_without_display_metadata(self):
        home = self.import_home()
        (home / 'identity.json').unlink()
        output = self.root / 'export.json'
        self.run_cli('export', home, output, 'Documents/note.txt')
        self.assertEqual(json.loads(output.read_text())['profile']['display_name'], 'Portable profile')

    def test_hash_and_base64_boundaries(self):
        sizes = [0, 1, 2, 3, 55, 56, 63, 64, 65, 1024, 1024 * 1024]
        self.data = {f'Documents/b{n}.bin': bytes(i % 251 for i in range(n)) for n in sizes}
        self.value = self.make_bundle(self.data)
        self.save()
        home = self.import_home()
        self.verify(home)
        output = self.root / 'export.json'
        self.run_cli('export', home, output, *self.data)
        self.assertEqual(json.loads(output.read_text()), self.value)
        normative.validate_bundle(json.loads(output.read_text()))

    def test_durable_checkpoints_and_partial_scratch(self):
        for phase in ['journal', 'directories', 'document', 'identity', 'commit-ready']:
            with self.subTest(phase=phase):
                parent = self.root / phase
                parent.mkdir()
                home = parent / 'probe'
                self.import_home(home, stop=phase, code=86)
                if home.exists():
                    self.run_cli('activate', home, code=1)
                    # Simulate an interrupted exclusive scratch content write.
                    (home / '.profile-copy-temp').write_bytes(b'partial write\x00')
                self.run_cli('recover', home)
                self.verify(home)

    def test_after_publication_reconciliation(self):
        home = self.import_home(stop='commit-ready', code=86)
        (home / '.profile-import-pending').unlink()
        before = self.snapshot(home)
        self.run_cli('recover', home)
        self.assertEqual(self.snapshot(home), before)
        self.verify(home)

    def test_unknown_and_mismatched_files_refuse_without_mutation(self):
        for kind in ['unknown', 'changed', 'link', 'unknown-dir', 'case-dir']:
            with self.subTest(kind=kind):
                parent = self.root / kind
                parent.mkdir()
                home = self.import_home(parent / 'probe', stop='document', code=86)
                if kind == 'unknown':
                    (home / 'unowned.txt').write_bytes(b'keep me')
                elif kind == 'changed':
                    (home / 'Documents/note.txt').write_bytes(b'keep changed data')
                elif kind == 'link':
                    (home / 'evil').symlink_to(self.bundle)
                elif kind == 'unknown-dir':
                    (home / 'unowned').mkdir()
                else:
                    (home / 'Documents/Unknown').mkdir()
                before = self.snapshot(home)
                self.run_cli('recover', home, code=1)
                self.assertEqual(self.snapshot(home), before)

    def test_changed_bundle_and_malformed_journal_refuse(self):
        home = self.import_home(stop='directories', code=86)
        before = self.snapshot(home)
        original = self.bundle.read_bytes()
        self.bundle.write_bytes(original + b' ')
        self.run_cli('recover', home, code=1)
        self.assertEqual(self.snapshot(home), before)
        self.bundle.write_bytes(original)
        journal = Path(str(home) + '.profile-import.json')
        original_journal = journal.read_bytes()
        journal.write_bytes(original_journal[:35])
        self.run_cli('recover', home, code=1)
        self.assertEqual(self.snapshot(home), before)
        journal.write_bytes(original_journal)
        self.run_cli('recover', home)
        self.verify(home)

    def test_partial_initial_journal_preserves_unowned_data(self):
        home = self.a / 'alice'
        journal = Path(str(home) + '.profile-import.json')
        journal.write_bytes(b'{"format":')
        self.run_cli('recover', home, code=1)
        self.assertEqual(journal.read_bytes(), b'{"format":')
        self.assertFalse(home.exists())

    def test_binding_and_identity_collisions(self):
        first = self.import_home()
        before = self.snapshot(first)
        second = self.a / 'bob'
        self.run_cli('import', self.bundle, second, 'bob', 2002, 100, code=1)
        self.value['profile']['id'] = str(uuid.uuid4())
        self.save()
        self.run_cli('import', self.bundle, second, 'alice', 2002, 100, code=1)
        self.run_cli('import', self.bundle, second, 'bob', 1001, 100, code=1)
        self.run_cli('import', self.bundle, self.a / 'ALICE', 'bob', 2002, 100, code=1)
        self.assertFalse(second.exists())
        self.assertEqual(self.snapshot(first), before)
        self.run_cli('import', self.bundle, second, 'bob', 2002, 100)
        self.assertTrue(second.exists())  # shared local GID is permitted

    def test_pending_reserves_identity_and_binding(self):
        home = self.import_home(stop='journal', code=86)
        other = self.a / 'bob'
        self.run_cli('import', self.bundle, other, 'bob', 2002, 100, code=1)
        self.assertFalse(other.exists())
        self.run_cli('recover', home)

    def test_invalid_inputs_never_mutate_destination(self):
        bad = []
        def variant(change):
            value = copy.deepcopy(self.value)
            change(value)
            bad.append(json.dumps(value).encode())
        variant(lambda v: v.update(schema_version=2))
        variant(lambda v: v.update(extra=1))
        variant(lambda v: v['profile'].update(id=str(uuid.uuid1())))
        variant(lambda v: v['preferences'].update(text_size=True))
        variant(lambda v: v['preferences'].update(theme_style=1.0))
        variant(lambda v: v['preferences'].update(password=0))
        variant(lambda v: v['documents'][0].update(sha256='0'*64))
        variant(lambda v: v['documents'][0].update(size=1024*1024+1))
        variant(lambda v: v['documents'][0].update(data_base64='@@@='))
        variant(lambda v: v['documents'][0].update(path='Documents/../escape'))
        variant(lambda v: v['documents'][0].update(path='Documents/.secret'))
        variant(lambda v: v['documents'][0].update(path='Documents/file.'))
        variant(lambda v: v['documents'][0].update(path='Documents/bad\\name'))
        variant(lambda v: v['documents'].append(copy.deepcopy(v['documents'][0])))
        variant(lambda v: v['documents'][1].update(path='Documents/NOTE.txt'))
        variant(lambda v: v['documents'][1].update(path='Documents/note.txt/child'))
        variant(lambda v: v['documents'][0].update(path='Documents/nested/second'))
        variant(lambda v: v['documents'].extend([copy.deepcopy(v['documents'][0])]*64))
        valid = json.dumps(self.value).encode()
        bad += [valid[:-1], valid + b' trailing', valid.replace(b'"schema_version": 1', b'"schema_version": 01'),
                valid.replace(b'"schema_version": 1', b'"schema_version": 1, "schema_version": 1'),
                valid.replace(b'"schema_version": 1', b'"schema_version": 111111111111111111111'),
                valid.replace(b'Guest Profile', b'Guest\\u0000Profile'),
                valid.replace(b'Guest Profile', b'Guest\\ud800'),
                valid.replace(b'Guest Profile', b'Guest\xff'), b'['*9+b']'*9, json.dumps({str(n): n for n in range(10000)}).encode(), b'x'*(12*1024*1024+1)]
        for index, raw in enumerate(bad):
            with self.subTest(index=index):
                self.bundle.write_bytes(raw)
                self.run_cli('inspect', self.bundle, code=1)
                self.import_home(code=1)
                self.assertEqual(list(self.a.iterdir()), [])

    def test_noncanonical_base64(self):
        for text in ['Zh==', 'Zg=A', 'Zg==\n', '=g==', 'Zg===']:
            v = self.make_bundle({'Documents/one': b'f'})
            v['documents'][0]['data_base64'] = text
            self.save(v)
            self.run_cli('inspect', self.bundle, code=1)

    def test_aggregate_and_expanded_path_limits_before_mutation(self):
        self.value = self.make_bundle({f'Documents/f{i}': b'x' * (1024 * 1024) for i in range(8)})
        extra = self.make_bundle({'Documents/extra': b'x'})['documents'][0]
        self.value['documents'].append(extra)
        self.save()
        self.import_home(code=1)
        self.assertEqual(list(self.a.iterdir()), [])
        self.value = self.make_bundle({'Documents/' + 'a' * 64 + '/' + 'b' * 64: b'x'})
        self.save()
        self.import_home(code=1)
        self.assertEqual(list(self.a.iterdir()), [])

    def test_maximum_document_count_and_empty_preferences(self):
        self.data = {f'Documents/f{i}': bytes([i]) for i in range(64)}
        self.value = self.make_bundle(self.data)
        self.value['preferences'] = {}
        self.save()
        home = self.import_home()
        self.verify(home)
        output = self.root / 'export.json'
        self.run_cli('export', home, output, *self.data)
        self.assertEqual(json.loads(output.read_text()), self.value)

    def test_source_links_and_existing_home_refused(self):
        home = self.a / 'alice'
        home.mkdir()
        (home / 'precious').write_bytes(b'keep')
        self.import_home(code=1)
        self.assertEqual((home / 'precious').read_bytes(), b'keep')
        link = self.root / 'linked.json'
        link.symlink_to(self.bundle)
        self.run_cli('import', link, self.b / 'bob', 'bob', 2002, 100, code=1)
        hardlink = self.root / 'hard.json'
        os.link(self.bundle, hardlink)
        self.run_cli('inspect', hardlink, code=1)


if __name__ == '__main__':
    if not BINARY:
        raise SystemExit('usage: test_guest_profile.py /absolute/path/to/native-profile')
    unittest.main()
