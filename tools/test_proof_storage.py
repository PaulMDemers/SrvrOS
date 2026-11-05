#!/usr/bin/env python3
"""Small host-only safety regressions; never opens a VM or real proof directory."""
import argparse
import importlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import proof_storage as storage


class ProofStorageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        parser = argparse.ArgumentParser()
        storage.add_arguments(parser)
        self.args = parser.parse_args([])
        self.base = self.root / 'build/review/profile-chooser'
        self.base.mkdir(parents=True)
        self.free = patch.object(storage.shutil, 'disk_usage', return_value=argparse.Namespace(free=100 * storage.GIB))
        self.free.start()
        self.addCleanup(self.free.stop)

    def manager(self):
        return storage.ProofStorage(self.root, 'profile-chooser', self.args)

    def completed(self, number, status='PASS'):
        path = self.base / f'proof-{number:02d}'
        path.mkdir()
        image = path / 'disk.img'
        image.write_bytes(b'fixture')
        (path / 'serial.log').write_text('diagnostic evidence')
        (path / 'receipt.json').write_text(json.dumps({'status': status}))
        record = {'version': 1, 'harness': 'profile-chooser', 'state': status,
                  'completed_ns': number, 'images': {'disk.img': storage.identity(image.stat())}}
        storage.save(path / storage.MARKER, record)
        return path

    def prune(self):
        with self.manager():
            # Unsuccessful context must never mark this fixture complete.
            raise InterruptedError('test interruption')

    def sweep(self):
        with self.assertRaises(InterruptedError):
            self.prune()

    def test_low_space_refuses_before_creating_a_run_or_calling_work(self):
        with patch.object(storage.shutil, 'disk_usage', return_value=argparse.Namespace(free=11 * storage.GIB)):
            with self.assertRaisesRegex(storage.StorageLimit, 'need 12.00 GiB'):
                with self.manager():
                    self.fail('admitted low-space work')
        self.assertEqual(list(self.base.glob('proof-*')), [])
        # Failed admission releases its lock, so a later retry can succeed.
        self.sweep()

    def test_space_consumed_by_another_process_stops_next_write(self):
        with self.assertRaisesRegex(storage.StorageLimit, 'GiB free'):
            with self.manager() as run:
                with patch.object(storage.shutil, 'disk_usage', return_value=argparse.Namespace(free=3 * storage.GIB)):
                    run.call(1, self.fail, 'write should not happen')
        marker = json.loads((run.out / storage.MARKER).read_text())
        self.assertEqual(marker['state'], 'INCOMPLETE')

    def test_run_budget_checks_allocation_before_write(self):
        with self.assertRaisesRegex(storage.StorageLimit, 'run budget'):
            with self.manager() as run:
                run.call(9 * storage.GIB, self.fail, 'write should not happen')

    def test_total_budget_counts_protected_legacy_files(self):
        self.args.proof_budget_gib = 8
        (self.base / 'legacy.img').write_bytes(b'protected')
        with self.assertRaisesRegex(storage.StorageLimit, 'proof directory budget'):
            with self.manager():
                self.fail('should refuse')
        self.assertEqual((self.base / 'legacy.img').read_bytes(), b'protected')

    def test_concurrent_invocation_refuses_without_removing_active_run(self):
        with self.assertRaises(InterruptedError):
            with self.manager() as first:
                with self.assertRaisesRegex(storage.StorageLimit, 'another .* active'):
                    with self.manager():
                        self.fail('second proof admitted')
                self.assertTrue(first.out.is_dir())
                raise InterruptedError()

    def test_retention_keeps_two_successes_logs_and_receipts(self):
        runs = [self.completed(i) for i in range(4)]
        self.sweep()
        for i, run in enumerate(runs):
            self.assertEqual((run / 'disk.img').exists(), i >= 2)
            self.assertTrue((run / 'serial.log').exists())
            self.assertTrue((run / 'receipt.json').exists())
        self.assertEqual(json.loads((runs[0] / storage.MARKER).read_text())['removed_images'], ['disk.img'])

    def test_protected_runs_and_recovery_copy_survive(self):
        pinned = self.completed(0); (pinned / '.keep').touch()
        failed = self.completed(1, 'FAIL')
        incomplete = self.completed(2, 'INCOMPLETE')
        legacy = self.completed(3); (legacy / storage.MARKER).unlink()
        active = self.completed(4)
        held = storage.lock(active / '.proof-active.lock')
        self.addCleanup(held.close)
        external = self.root / '.local/installation-evidence/accepted'
        shutil.copytree(self.completed(5), external)
        self.completed(6); self.completed(7)
        self.sweep()
        for path in (pinned, failed, incomplete, legacy, active, external):
            self.assertTrue((path / 'disk.img').exists(), str(path))

    def test_open_image_is_not_pruned(self):
        run = self.completed(0)
        self.completed(1); self.completed(2)
        with (run / 'disk.img').open('rb'):
            self.sweep()
            self.assertTrue((run / 'disk.img').exists())

    def test_unavailable_or_uncertain_lsof_preserves_images(self):
        run = self.completed(0)
        self.completed(1); self.completed(2)
        for outcome in (FileNotFoundError(), subprocess.TimeoutExpired('lsof', 30),
                        argparse.Namespace(returncode=1, stdout='', stderr='cannot stat'),
                        argparse.Namespace(returncode=0, stdout='', stderr='')):
            with self.subTest(outcome=outcome):
                kwargs = {'side_effect': outcome} if isinstance(outcome, Exception) else {'return_value': outcome}
                with patch.object(storage.subprocess, 'run', **kwargs):
                    self.sweep()
                self.assertTrue((run / 'disk.img').exists())

    def test_modified_image_and_hardlink_are_preserved(self):
        changed = self.completed(0)
        (changed / 'disk.img').write_bytes(b'modified for recovery')
        linked = self.completed(1)
        os.link(linked / 'disk.img', self.root / 'recovery.img')
        self.completed(2); self.completed(3)
        self.sweep()
        self.assertTrue((changed / 'disk.img').exists())
        self.assertTrue((linked / 'disk.img').exists())

    def test_symlink_and_traversal_cannot_delete_external_files(self):
        external = self.root / 'recovery.img'; external.write_bytes(b'recovery')
        run = self.completed(0)
        (run / 'link').symlink_to(self.root, target_is_directory=True)
        record = json.loads((run / storage.MARKER).read_text())
        for name in ('link/recovery.img', '../../../../../recovery.img', str(external)):
            record['images'][name] = storage.identity(external.stat())
        storage.save(run / storage.MARKER, record)
        self.completed(1); self.completed(2)
        with patch.object(storage.ProofStorage, '_unused_images', return_value=True):
            self.sweep()
        self.assertEqual(external.read_bytes(), b'recovery')

    def test_linked_base_is_refused(self):
        other = self.root / 'elsewhere'; other.mkdir()
        self.base.rmdir(); self.base.symlink_to(other, target_is_directory=True)
        with self.assertRaisesRegex(storage.StorageLimit, 'symlink'):
            with self.manager():
                self.fail('linked root admitted')
        self.assertEqual(list(other.iterdir()), [])

    def test_success_inventory_and_failed_exit(self):
        with self.manager() as run:
            (run.out / 'disk.img').write_bytes(b'generated')
            (run.out / 'receipt.json').write_text('{"status": "PASS"}')
        record = json.loads((run.out / storage.MARKER).read_text())
        self.assertEqual(record['state'], 'PASS')
        self.assertEqual(set(record['images']), {'disk.img'})
        with self.assertRaises(RuntimeError):
            with self.manager() as failed:
                (failed.out / 'receipt.json').write_text('{"status": "FAIL"}')
                raise RuntimeError('fixture failure')
        self.assertEqual(json.loads((failed.out / storage.MARKER).read_text())['state'], 'INCOMPLETE')

    def test_final_guard_failure_is_not_recorded_as_pass(self):
        with self.assertRaises(storage.StorageLimit):
            with self.manager() as run:
                (run.out / 'receipt.json').write_text('{"status": "PASS"}')
                storage.shutil.disk_usage.return_value.free = 0
        self.assertEqual(json.loads((run.out / 'receipt.json').read_text())['status'], 'FAIL')
        self.assertEqual(json.loads((run.out / storage.MARKER).read_text())['state'], 'INCOMPLETE')

    def test_harness_entry_points_refuse_before_build_or_qemu(self):
        for name in ('profile_chooser_smoke', 'installation_recovery_smoke'):
            if not (Path(__file__).parent / (name + '.py')).exists():
                continue
            with self.subTest(harness=name):
                harness = importlib.import_module(name)
                firmware = self.root / 'firmware.fd'; firmware.write_bytes(b'fixture')
                argv = [name, '--ovmf', str(firmware), '--ovmf-vars', str(firmware)]
                with patch.object(harness, 'ROOT', self.root), patch.object(sys, 'argv', argv), \
                        patch.object(harness, 'run') as work, \
                        patch.object(storage.shutil, 'disk_usage', return_value=argparse.Namespace(free=0)):
                    with self.assertRaises(storage.StorageLimit):
                        harness.main()
                    work.assert_not_called()

    def test_chooser_shared_methods_accept_unmanaged_consumers(self):
        chooser = importlib.import_module('profile_chooser_smoke')
        for module in ('profile_transfer_smoke', 'profile_switch_smoke'):
            self.assertIs(importlib.import_module(module).c.VM, chooser.VM)
        vm = chooser.VM(None, None, self.root, 'test', argparse.Namespace())
        vm.output = bytearray(b'ready')
        chooser.observe(vm, b'ready')
        vm.consumed = 0
        vm.wait(b'ready')
        vm.run = lambda command: None
        vm.qmp = unittest.mock.Mock()
        vm.qmp.call.side_effect = lambda command: {'status': 'paused'} if command == 'query-status' else None
        with patch.object(chooser.d, 'partitions', return_value=(None, (0, 4))), \
             patch.object(chooser.d, 'data_image', return_value=b'data'):
            result = vm.quiesced_data_snapshot('test')
        self.assertEqual(result.read_bytes(), b'data')
        self.assertEqual(vm.qmp.call.call_args_list[-1].args, ('cont',))

    def test_root_symlink_is_rejected(self):
        link = self.root.parent / (self.root.name + '-link')
        link.symlink_to(self.root, target_is_directory=True)
        self.addCleanup(link.unlink)
        with self.assertRaisesRegex(storage.StorageLimit, 'symlink'):
            with storage.ProofStorage(link, 'profile-chooser', self.args):
                self.fail('linked root admitted')

    def test_confined_unlink_rejects_linked_parent_and_stale_identity(self):
        run = self.completed(0)
        image = run / 'disk.img'
        identity = storage.identity(image.stat())
        external = self.root / 'external'; external.mkdir()
        (external / 'disk.img').write_bytes(b'fixture')
        (run / 'redirect').symlink_to(external, target_is_directory=True)
        self.assertFalse(storage.ProofStorage._unlink_confined(run, Path('redirect/disk.img'), identity))
        image.write_bytes(b'changed')
        self.assertFalse(storage.ProofStorage._unlink_confined(run, Path('disk.img'), identity))
        self.assertTrue(image.exists())
        self.assertTrue((external / 'disk.img').exists())

    def test_nonfinite_and_unsafe_limits_rejected(self):
        for value in ('nan', 'inf', '-1', '0'):
            with self.assertRaises(argparse.ArgumentTypeError):
                storage.positive_gib(value)
        with self.assertRaises(argparse.ArgumentTypeError):
            storage.keep_count('1')


if __name__ == '__main__':
    unittest.main()
