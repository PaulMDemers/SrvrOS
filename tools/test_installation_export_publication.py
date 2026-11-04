"""Actual host publication transitions, ownership refusals and I/O failures."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import installation_export_publication as pub
import profile_bundle as p
from test_profile_image import bundle


class Stop(BaseException):
    pass


def snapshot(root):
    result = {}
    for path in sorted(root.rglob('*')):
        st = path.lstat()
        row = [st.st_dev, st.st_ino, st.st_mode, st.st_uid, st.st_nlink, st.st_size,
               st.st_mtime_ns, st.st_ctime_ns]
        if stat.S_ISREG(st.st_mode):
            row.append(hashlib.sha256(path.read_bytes()).hexdigest())
        elif stat.S_ISLNK(st.st_mode):
            row.append(os.readlink(path))
        result[str(path.relative_to(root))] = row
    return result


class ExportPublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.source = self.root / 'source'; self.source.mkdir()
        self.observation = {'stage': str(self.source), 'plan_sha256': '1' * 64,
                            'image_sha256': '2' * 64, 'home': '/fat/home/alice'}
        self.value = bundle()
        self.data = p.encode_json(self.value)

    def paths(self):
        root = self.root / str(len(list(self.root.iterdir())))
        root.mkdir(); return root, root / 'attempt', root / 'output.json'

    def stop(self, checkpoint):
        root, attempt, output = self.paths()
        def interrupted(name):
            if name == checkpoint:
                raise Stop()
        with self.assertRaises(Stop):
            pub.publish(attempt, output, self.value, self.observation, interrupted)
        return root, attempt, output

    def inspected(self, root, attempt):
        before = snapshot(root)
        with mock.patch.object(pub.os, 'fsync', side_effect=AssertionError('inspection flushed')):
            result = pub.inspect(attempt)
        self.assertEqual(snapshot(root), before)
        self.assertTrue(result['read_only'])
        return result

    def refused(self, root, attempt):
        before = snapshot(root)
        self.assertEqual(self.inspected(root, attempt)['state'], 'refused')
        self.assertEqual(pub.reconcile(attempt)['state'], 'refused')
        self.assertEqual(snapshot(root), before)

    def test_every_interruption_and_idempotent_reconciliation(self):
        for checkpoint in pub.CHECKPOINTS:
            with self.subTest(checkpoint=checkpoint):
                root, attempt, output = self.stop(checkpoint)
                result = self.inspected(root, attempt)
                if checkpoint in ('recorded', 'partial-write'):
                    self.assertEqual(result['state'], 'incomplete')
                    before = snapshot(root)
                    self.assertEqual(pub.reconcile(attempt)['state'], 'refused')
                    self.assertEqual(snapshot(root), before)
                    self.assertFalse(output.exists())
                else:
                    self.assertTrue(result['can_reconcile'], result)
                    inode = (output if output.exists() else attempt / 'bundle.partial').stat().st_ino
                    self.assertEqual(pub.reconcile(attempt)['state'], 'complete')
                    self.assertEqual(output.read_bytes(), self.data)
                    self.assertEqual(output.stat().st_ino, inode)
                    self.assertEqual(output.stat().st_nlink, 1)
                    before = snapshot(root)
                    self.assertEqual(pub.reconcile(attempt)['state'], 'complete')
                    self.assertEqual(snapshot(root), before)

    def test_reconcile_itself_can_restart_at_every_finish_boundary(self):
        for checkpoint in pub.FINISH_CHECKPOINTS:
            with self.subTest(checkpoint=checkpoint):
                root, attempt, output = self.stop('written')
                def interrupted(name):
                    if name == checkpoint:
                        raise Stop()
                with self.assertRaises(Stop):
                    pub.reconcile(attempt, interrupted)
                self.assertTrue(self.inspected(root, attempt)['can_reconcile'])
                self.assertEqual(pub.reconcile(attempt)['state'], 'complete')
                self.assertEqual(output.read_bytes(), self.data)

    def test_source_is_only_a_historical_observation(self):
        root, attempt, output = self.stop('written')
        self.source.rmdir(); self.source.symlink_to(self.root / 'vanished')
        self.assertEqual(self.inspected(root, attempt)['state'], 'prepared')
        self.assertEqual(pub.reconcile(attempt)['state'], 'complete')
        self.assertEqual(output.read_bytes(), self.data)

    def test_foreign_existing_paths_and_source_overlap_preserved(self):
        root, attempt, output = self.paths(); output.write_bytes(b'foreign')
        before = snapshot(root)
        self.assertEqual(pub.publish(attempt, output, self.value, self.observation)['state'], 'refused')
        self.assertEqual(snapshot(root), before)
        self.assertFalse(attempt.exists())
        output.unlink(); attempt.mkdir()
        before = snapshot(root)
        self.assertEqual(pub.publish(attempt, output, self.value, self.observation)['state'], 'refused')
        self.assertEqual(snapshot(root), before)
        result = pub.publish(self.source / 'attempt', output, self.value, self.observation)
        self.assertEqual(result['state'], 'refused'); self.assertFalse((self.source / 'attempt').exists())

    def test_torn_record_checksum_ack_and_unknown_entries_refuse(self):
        for name, data in (('intent.json', b'{'), ('intent.sha256', b'0' * 64 + b'\n'),
                           ('complete.json', b'{'), ('unknown', b'preserve')):
            with self.subTest(name=name):
                root, attempt, _ = self.stop('directory-synced')
                (attempt / name).write_bytes(data)
                self.refused(root, attempt)

    def test_changed_replaced_and_extra_linked_bundle_refuse(self):
        for kind in ('contents', 'inode', 'extra-link', 'symlink', 'fifo'):
            with self.subTest(kind=kind):
                root, attempt, output = self.stop('unlinked')
                if kind == 'contents':
                    output.write_bytes(self.data.upper())
                elif kind == 'inode':
                    copy = root / 'copy'; copy.write_bytes(self.data); copy.replace(output)
                elif kind == 'extra-link':
                    os.link(output, root / 'extra')
                elif kind == 'symlink':
                    output.unlink(); output.symlink_to(root / 'foreign')
                else:
                    output.unlink(); os.mkfifo(output)
                self.refused(root, attempt)

    def test_missing_or_changed_parent_and_private_directory_refuse(self):
        root, attempt, output = self.stop('written')
        (attempt / 'bundle.partial').unlink(); self.refused(root, attempt)
        root, attempt, output = self.stop('written')
        attempt.chmod(0o755); self.refused(root, attempt)
        root, attempt, output = self.paths()
        destination = root / 'destination'; destination.mkdir(); output = destination / 'output.json'
        def interrupted(name):
            if name == 'written': raise Stop()
        with self.assertRaises(Stop): pub.publish(attempt, output, self.value, self.observation, interrupted)
        destination.rename(root / 'moved'); destination.mkdir()
        self.refused(root, attempt)

    def test_busy_or_replaced_lock_refuses_without_mutation(self):
        root, attempt, _ = self.stop('written')
        with (attempt / '.lock').open('rb') as f:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
            self.refused(root, attempt)
        original = attempt / '.lock'; old = original.stat().st_ino
        replacement = root / 'replacement'; replacement.write_bytes(b''); replacement.replace(original)
        self.assertNotEqual(old, original.stat().st_ino); self.refused(root, attempt)

    def test_valid_checksum_does_not_override_directory_or_bundle_identity(self):
        for key in ('directory_identity', 'lock_identity', 'bundle_identity', 'parent_identity'):
            with self.subTest(key=key):
                root, attempt, _ = self.stop('written')
                record = json.loads((attempt / 'intent.json').read_bytes()); record[key]['inode'] += 1
                raw = p.encode_json(record); (attempt / 'intent.json').write_bytes(raw)
                (attempt / 'intent.sha256').write_text(hashlib.sha256(raw).hexdigest() + '\n')
                self.refused(root, attempt)

    def test_write_short_write_and_file_sync_failures_preserve_recoverable_state(self):
        real_write, real_sync = os.write, os.fsync
        for kind in ('write', 'short-write', 'file-sync'):
            with self.subTest(kind=kind):
                root, attempt, output = self.paths(); armed = False; triggered = []
                def checkpoint(name):
                    nonlocal armed
                    if name == 'recorded': armed = True
                def candidate(fd):
                    return armed and pub.identity(os.fstat(fd)) == pub.identity((attempt / 'bundle.partial').stat())
                def fail_write(fd, data):
                    if kind != 'file-sync' and candidate(fd):
                        triggered.append(True)
                        if kind == 'short-write': return real_write(fd, data[:1])
                        raise OSError('injected payload write')
                    return real_write(fd, data)
                def fail_sync(fd):
                    if kind == 'file-sync' and candidate(fd) and os.fstat(fd).st_size == len(self.data):
                        triggered.append(True); raise OSError('injected payload sync')
                    return real_sync(fd)
                with mock.patch.object(pub.os, 'write', side_effect=fail_write), mock.patch.object(pub.os, 'fsync', side_effect=fail_sync):
                    result = pub.publish(attempt, output, self.value, self.observation, checkpoint)
                self.assertTrue(triggered); self.assertEqual(result['state'], 'refused'); self.assertFalse(output.exists())
                state = self.inspected(root, attempt)
                self.assertEqual(state['state'], 'prepared' if kind == 'file-sync' else 'incomplete')
                if kind == 'file-sync': self.assertEqual(pub.reconcile(attempt)['state'], 'complete')

    def test_link_unlink_directory_sync_and_close_failures_reconcile(self):
        real_unlink, real_sync, real_close = os.unlink, os.fsync, os.close
        for kind in ('link', 'unlink', 'directory-sync', 'close'):
            with self.subTest(kind=kind):
                root, attempt, output = self.paths(); armed = False; triggered = []
                def checkpoint(name):
                    nonlocal armed
                    if name == 'unlinked': armed = True
                def unlink(path, *args, **kwargs):
                    if kind == 'unlink' and path == 'bundle.partial':
                        triggered.append(True); raise OSError('injected unlink')
                    return real_unlink(path, *args, **kwargs)
                def sync(fd):
                    if kind == 'directory-sync' and armed and pub.identity(os.fstat(fd)) == pub.identity(root.stat()):
                        triggered.append(True); raise OSError('injected directory sync')
                    return real_sync(fd)
                def close(fd):
                    if kind == 'close' and armed and not triggered and pub.identity(os.fstat(fd)) == pub.identity(root.stat()):
                        triggered.append(True); real_close(fd); raise OSError('injected close')
                    return real_close(fd)
                def link(*args, **kwargs):
                    triggered.append(True); raise OSError('injected link')
                with mock.patch.object(pub.os, 'unlink', side_effect=unlink), mock.patch.object(pub.os, 'fsync', side_effect=sync), mock.patch.object(pub.os, 'close', side_effect=close):
                    if kind == 'link':
                        with mock.patch.object(pub.os, 'link', side_effect=link):
                            result = pub.publish(attempt, output, self.value, self.observation, checkpoint)
                    else: result = pub.publish(attempt, output, self.value, self.observation, checkpoint)
                self.assertTrue(triggered); self.assertEqual(result['state'], 'refused')
                self.assertTrue(self.inspected(root, attempt)['can_reconcile'])
                self.assertEqual(pub.reconcile(attempt)['state'], 'complete')
                self.assertEqual(output.read_bytes(), self.data)

    def test_torn_ack_and_record_are_preserved_refusals(self):
        for key in ('intent.json', 'complete.json'):
            with self.subTest(key=key):
                root, attempt, output = self.paths(); real_write = os.write; triggered = []
                def write(fd, data):
                    target = attempt / key
                    if target.exists() and pub.identity(os.fstat(fd)) == pub.identity(target.stat()):
                        triggered.append(True); return real_write(fd, data[:1])
                    return real_write(fd, data)
                with mock.patch.object(pub.os, 'write', side_effect=write):
                    result = pub.publish(attempt, output, self.value, self.observation)
                self.assertTrue(triggered); self.assertEqual(result['state'], 'refused')
                self.refused(root, attempt)

    def test_future_bundle_version_refuses_before_creation(self):
        root, attempt, output = self.paths(); value = dict(self.value, schema_version=2)
        before = snapshot(root)
        self.assertEqual(pub.publish(attempt, output, value, self.observation)['state'], 'refused')
        self.assertEqual(snapshot(root), before)

    def test_changed_bytes_or_directory_alias_before_link_are_not_published(self):
        for kind in ('contents', 'directory-alias'):
            root, attempt, output = self.paths()
            def changed(name):
                if name != 'file-synced': return
                if kind == 'contents':
                    (attempt / 'bundle.partial').write_bytes(self.data.upper())
                else:
                    attempt.rename(root / 'moved'); attempt.symlink_to(root / 'moved')
            result = pub.publish(attempt, output, self.value, self.observation, changed)
            self.assertEqual(result['state'], 'refused'); self.assertFalse(output.exists())

    def test_boolean_record_preference_is_not_integer_metadata(self):
        root, attempt, _ = self.stop('written')
        record = json.loads((attempt / 'intent.json').read_bytes())
        record['selection']['preferences']['clock_flags'] = True
        raw = p.encode_json(record); (attempt / 'intent.json').write_bytes(raw)
        (attempt / 'intent.sha256').write_text(hashlib.sha256(raw).hexdigest() + '\n')
        self.refused(root, attempt)

    def test_malformed_attempt_type_is_a_bounded_refusal(self):
        for value in (None, True, 42, []):
            root, attempt, _ = self.stop('written')
            record = json.loads((attempt / 'intent.json').read_bytes()); record['attempt'] = value
            raw = p.encode_json(record); (attempt / 'intent.json').write_bytes(raw)
            (attempt / 'intent.sha256').write_text(hashlib.sha256(raw).hexdigest() + '\n')
            self.refused(root, attempt)

    def test_cli_inspect_and_reconcile_in_fresh_process(self):
        root, attempt, output = self.stop('linked')
        tool = Path(__file__).resolve().with_name('installation_image.py')
        for action, expected in (('inspect-export', 'visible-uncertain'), ('reconcile-export', 'complete'), ('reconcile-export', 'complete')):
            before = snapshot(root)
            result = subprocess.run([sys.executable, str(tool), action, str(attempt)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(json.loads(result.stdout)['state'], expected)
            if action == 'inspect-export': self.assertEqual(snapshot(root), before)
        self.assertEqual(output.read_bytes(), self.data)


if __name__ == '__main__':
    unittest.main()
