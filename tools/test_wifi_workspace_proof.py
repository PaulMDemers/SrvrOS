#!/usr/bin/env python3
import json
from pathlib import Path
import tempfile
import unittest
import wifi_workspace_proof as proof

SESSION = 'session-0123456789abcdef-00'
NEW = 'srvros-work/Logs/' + SESSION


class Preservation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='wifi-workspace-proof-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / 'usb'
        self.evidence = self.base / 'private'
        self.root.mkdir()
        (self.root / 'srvros-work/Logs').mkdir(parents=True)
        (self.root / 'Home').mkdir()
        (self.root / 'Home/anchor.txt').write_bytes(b'unchanged contents\n')
        (self.root / '.srvros').mkdir()
        self.records = {}
        self.refresh()

    def refresh(self, reverse=False):
        paths = {'/work': True}
        for p in self.root.rglob('*'):
            name = p.relative_to(self.root).as_posix()
            if name == '.srvros' or name.startswith('.srvros/'):
                continue
            paths['/work/' + name] = p.is_dir()
        for name, directory in paths.items():
            if name not in self.records:
                self.records[name] = [len(self.records) + 1, 0o40755 if directory else 0o100644,
                                      2 if directory else 1, 0, 0, 100, 100, 100]
        keys = sorted(paths, reverse=reverse)
        raw = 'SRVROSMETA1\n' + ''.join(k + '\t' + '\t'.join(map(str, self.records[k])) + '\n' for k in keys)
        (self.root / proof.META).write_text(raw)

    def capture(self):
        return proof.capture(self.root, self.evidence)

    def new_session(self):
        (self.root / NEW).mkdir()
        (self.root / NEW / 'exit.log').write_bytes(b'result\n')
        self.refresh()

    def verify(self, **kw):
        return proof.verify(self.root, self.evidence, SESSION, **kw)

    def test_success_saved_raw_private_reordering(self):
        before = (self.root / proof.META).read_bytes()
        self.capture()
        self.assertEqual((self.evidence / 'metadata-before.bin').read_bytes(), before)
        self.assertEqual(self.evidence.stat().st_mode & 0o777, 0o700)
        self.assertEqual((self.evidence / 'metadata-before.bin').stat().st_mode & 0o777, 0o600)
        self.new_session()
        self.refresh(reverse=True)
        result = self.verify()
        self.assertTrue(result['metadata_exact_current_tree'])
        self.assertEqual(result['authorized_atime_changes'], 0)

    def test_explicit_bounded_read_atime(self):
        self.capture()
        self.new_session()
        self.records['/work/Home/anchor.txt'][5] = 150
        self.refresh()
        with self.assertRaisesRegex(ValueError, 'unjustified'):
            self.verify()
        self.assertEqual(self.verify(read_paths=['Home/anchor.txt'], time_window=(140, 160))['authorized_atime_changes'], 1)
        for window in ((151, 200), (101, 149), (200, 100), None):
            with self.subTest(window=window), self.assertRaises(ValueError):
                self.verify(read_paths=['Home/anchor.txt'], time_window=window)

    def test_prior_metadata_fields_refused(self):
        self.capture()
        self.new_session()
        original = self.records['/work/Home/anchor.txt'][:]
        for field, value in ((0, 99), (1, 0o100600), (2, 2), (3, 1), (4, 1), (6, 150), (7, 150)):
            with self.subTest(field=field):
                self.records['/work/Home/anchor.txt'] = original[:]
                self.records['/work/Home/anchor.txt'][field] = value
                self.refresh()
                with self.assertRaises(ValueError):
                    self.verify(read_paths=['Home/anchor.txt'], time_window=(100, 200))

    def test_directory_atime_not_justified(self):
        self.capture()
        self.new_session()
        self.records['/work/srvros-work/Logs'][5] = 150
        self.refresh()
        with self.assertRaises(ValueError):
            self.verify(read_paths=['srvros-work/Logs'], time_window=(100, 200))

    def test_changed_missing_ordinary_bytes(self):
        self.capture()
        self.new_session()
        p = self.root / 'Home/anchor.txt'
        p.write_bytes(b'changed contents\n')
        with self.assertRaisesRegex(ValueError, 'ordinary file'):
            self.verify()
        p.unlink()
        self.refresh()
        with self.assertRaisesRegex(ValueError, 'ordinary file'):
            self.verify()

    def test_unexpected_new_path(self):
        self.capture()
        self.new_session()
        (self.root / 'Home/extra').write_bytes(b'unexpected')
        self.refresh()
        with self.assertRaisesRegex(ValueError, 'session paths'):
            self.verify()

    def test_existing_session_and_no_new_session_refused(self):
        self.capture()
        with self.assertRaises(ValueError):
            self.verify()
        self.evidence.rename(self.base / 'unused')
        self.new_session()
        self.capture()
        with self.assertRaisesRegex(ValueError, 'already existed'):
            self.verify()

    def test_metadata_shape_mutations(self):
        self.capture()
        self.new_session()
        p = self.root / proof.META
        raw = p.read_text()
        lines = raw.splitlines()
        for bad in (raw[:-1], raw + lines[1] + '\n', raw.replace('SRVROSMETA1', 'BAD'),
                    '\n'.join(lines[:-1]) + '\n', raw.replace('/work/Home\t', '/work/missing\t'),
                    raw.replace('\t100\t100\t100', '\t-1\t100\t100'),
                    raw.replace('\t100\t100\t100', '\t' + str(1 << 64) + '\t100\t100'),
                    raw + '\n'):
            with self.subTest(bad=bad[:20]):
                p.write_text(bad)
                with self.assertRaises(ValueError):
                    self.verify()
        p.write_text(raw)

    def test_duplicate_inode(self):
        self.capture()
        self.new_session()
        self.records['/work/Home/anchor.txt'][0] = self.records['/work/Home'][0]
        self.refresh()
        with self.assertRaisesRegex(ValueError, 'inode'):
            self.verify()

    def test_tampered_baseline_hash(self):
        self.capture()
        self.new_session()
        (self.evidence / 'metadata-before.bin').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            self.verify()

    def test_public_baseline_refused(self):
        self.capture()
        self.new_session()
        (self.evidence / 'metadata-before.bin').chmod(0o644)
        with self.assertRaisesRegex(ValueError, 'private'):
            self.verify()

    def test_links_and_internal_pending_refused(self):
        for name in ('Home/link', '.srvros/meta.tmp'):
            p = self.root / name
            if name.endswith('link'):
                p.symlink_to('anchor.txt')
            else:
                p.write_bytes(b'pending')
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.capture()
            p.unlink()

    def test_in_workspace_baseline_refused(self):
        with self.assertRaisesRegex(ValueError, 'outside'):
            proof.capture(self.root, self.root / 'evidence')

    def test_invalid_session_and_read_paths(self):
        self.capture()
        self.new_session()
        for s in ('../bad', SESSION + '/extra', 'session-x', ''):
            with self.subTest(session=s), self.assertRaises(ValueError):
                proof.verify(self.root, self.evidence, s)
        for p in ('../Home/anchor.txt', '/Home/anchor.txt', proof.META, 'Home/missing'):
            with self.subTest(path=p), self.assertRaises(ValueError):
                self.verify(read_paths=[p], time_window=(100, 200))

    def test_capture_refuses_stale_sidecar(self):
        (self.root / 'Home/new-file').write_bytes(b'not represented')
        with self.assertRaisesRegex(ValueError, 'incomplete metadata tree'):
            self.capture()


if __name__ == '__main__':
    unittest.main()
