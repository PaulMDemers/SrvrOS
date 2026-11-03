"""A passing boot receipt must be attributable to the tested source tree."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import installed_disk_smoke as smoke


class BuildProvenanceTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.current = {'.': {'head': 'a' * 40, 'dirty': False,
                             'tracked_diff_sha256': smoke.digest(b''), 'untracked': {}},
                        'userspace/apps': {'head': 'b' * 40, 'dirty': False}}
        self.record = {'schema': 1, 'source': 'a' * 40, 'worktree_dirty': False,
                       'tracked_diff_sha256': smoke.digest(b''),
                       'repositories': copy.deepcopy(self.current), 'hashes': {}}
        for name in ('build/kernel/srvros.elf', 'build/initramfs.tar', 'build/srvros.exfat'):
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(name.encode())
            self.record['hashes'][name] = smoke.digest(path.read_bytes())
        mock = patch.object(smoke.source_provenance, 'capture', return_value=self.current)
        mock.start()
        self.addCleanup(mock.stop)

    def write_record(self):
        (self.root / 'build/macos-build.json').write_text(json.dumps(self.record))

    def test_missing_or_malformed_record_refuses(self):
        with self.assertRaises(RuntimeError):
            smoke.verified_build(self.root)
        (self.root / 'build/macos-build.json').write_text('{}')
        with self.assertRaises(RuntimeError):
            smoke.verified_build(self.root)

    def test_exact_source_and_artifacts_pass(self):
        self.write_record()
        self.assertEqual(smoke.verified_build(self.root), self.record)

    def test_changed_artifact_refuses(self):
        self.write_record()
        (self.root / 'build/srvros.exfat').write_bytes(b'stale or changed')
        with self.assertRaises(RuntimeError):
            smoke.verified_build(self.root)

    def test_new_head_with_old_build_refuses(self):
        self.write_record()
        self.current['.']['head'] = 'c' * 40
        with self.assertRaises(RuntimeError):
            smoke.verified_build(self.root)

    def test_changed_nested_source_refuses(self):
        self.write_record()
        self.current['userspace/apps']['dirty'] = True
        with self.assertRaises(RuntimeError):
            smoke.verified_build(self.root)

    def test_untracked_source_change_refuses(self):
        self.write_record()
        self.current['.']['untracked']['new.c'] = {'sha256': smoke.digest(b'new')}
        with self.assertRaises(RuntimeError):
            smoke.verified_build(self.root)


if __name__ == '__main__':
    unittest.main()
