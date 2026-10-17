import binascii
import contextlib
import subprocess
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import hardware_boot as boot
import hardware_test as test




class HostTests(unittest.TestCase):
    def inventory(self, transport='usb', duplicate=False, mounted=True):
        item = {'name': '/dev/example1', 'type': 'part', 'fstype': 'exfat',
                'uuid': '37D8-C5D0', 'mountpoints': ['/media/test'] if mounted else [None]}
        return {'blockdevices': [{'tran': transport, 'children': [item, item] if duplicate else [item]}]}

    def test_volume_selection(self):
        self.assertEqual(test.volume_id('37D8-C5D0'), '37d8c5d0')
        self.assertEqual(test.volume_mount('37d8c5d0', self.inventory()), Path('/media/test'))
        for inventory in (self.inventory('sata'), self.inventory(duplicate=True), self.inventory(mounted=False)):
            with self.assertRaises(RuntimeError): test.volume_mount('37d8c5d0', inventory)
        with self.assertRaises(ValueError): test.volume_id('../../a')

    def test_default_return_preflight(self):
        defaults = 'GRUB_DEFAULT=0\nGRUB_SAVEDEFAULT=false\n'
        config = 'set default="${next_entry}"\nset default="0"\nmenuentry \'Ubuntu\' {\n}\n'
        test.check_ubuntu_default(defaults, config)
        for d, c in ((defaults.replace('=0', '=saved'), config),
                     (defaults.replace('false', 'true'), config),
                     (defaults, config.replace('Ubuntu', 'SrvrOS')),
                     (defaults, config.replace('menuentry', 'submenu')),
                     (defaults, config.replace('"0"', '"1"'))):
            with self.assertRaises(RuntimeError): test.check_ubuntu_default(d, c)

    def test_launch_orders_install_selection_and_reboot_and_cleans_up_failure(self):
        run = '0123456789abcdef'
        release = 'abcdef0123456789'
        metadata = {'run': run, 'release': release, 'volume': '37d8c5d0',
                    'boot_manifest': {'sha256': {'srvros.elf': 'test'}}}
        manifest = {'config_version': f'ram-usb-v5-autotest-37d8c5d0-{run}',
                    'sha256': metadata['boot_manifest']['sha256']}
        with tempfile.TemporaryDirectory() as tmp:
            mount = Path(tmp)
            folder = mount/'srvros-tests'; folder.mkdir()
            (folder/f'{run}.req').write_text(f'SRVROS-AUTOTEST-V1 {run}\n')
            for failure in (None, 'install', 'reboot', 'default'):
                calls = []
                def execute(command, **kwargs):
                    calls.append(command)
                    if (failure == 'install' and 'install' in command) or (failure == 'reboot' and 'systemctl' in command):
                        raise subprocess.CalledProcessError(1, command)
                    return subprocess.CompletedProcess(command, 0)
                with contextlib.ExitStack() as stack:
                    stack.enter_context(patch.object(test, 'load_run', return_value=metadata))
                    stack.enter_context(patch.object(boot, 'verify', return_value=manifest))
                    stack.enter_context(patch.object(test, 'volume_mount', return_value=mount))
                    paths = stack.enter_context(patch.object(test, 'Path'))
                    paths.return_value.read_text.return_value = 'GRUB_DEFAULT=0\n'
                    stack.enter_context(patch.object(test.os, 'sync'))
                    stack.enter_context(patch.object(test.subprocess, 'run', side_effect=execute))
                    default = '1' if failure == 'default' else '0'
                    stack.enter_context(patch.object(test.subprocess, 'check_output',
                        return_value=f'set default="{default}"\nmenuentry \'Ubuntu\' {{\n}}\n'))
                    if failure:
                        with self.assertRaises((RuntimeError, subprocess.CalledProcessError)):
                            test.launch(run)
                    else:
                        test.launch(run)
                selected = [i for i, c in enumerate(calls) if 'grub-reboot' in c]
                if failure in ('install', 'default'):
                    self.assertFalse(selected)
                else:
                    install = next(i for i, c in enumerate(calls) if 'install' in c)
                    reboot = next(i for i, c in enumerate(calls) if 'systemctl' in c)
                    self.assertLess(install, selected[0]); self.assertLess(selected[0], reboot)
                    self.assertIn(release, calls[install])
                    self.assertEqual(any('grub-editenv' in c for c in calls), failure == 'reboot')

    def report(self, status='PASS'):
        metadata = {'run': '0123456789abcdef', 'release': 'abcdef0123456789', 'volume': '37d8c5d0'}
        fields = dict(format='srvros-autotest-v1', **metadata)
        fields.update({key: status for key in ('boot_ready', 'timer', 'memory', 'fpu_traps',
                       'render', 'usb_keyboard_enumeration', 'usb_write_flush_read')})
        fields.update(physical_keypress='NOT_TESTED', status=status)
        data = ''.join(f'{key}={value}\n' for key, value in fields.items()).encode()
        done = f"COMPLETE {metadata['run']} {binascii.crc32(data):08x}\n".encode()
        return metadata, data, done

    def test_complete_report_and_failure(self):
        for status in ('PASS', 'FAIL'):
            metadata, data, done = self.report(status)
            self.assertEqual(test.validate_result(metadata, data, done)['status'], status)

    def test_corrupt_stale_incomplete_results(self):
        metadata, data, done = self.report()
        for d, c in ((data+b'x', done), (data,b''), (data,done.replace(b'0123456789abcdef',b'1123456789abcdef'))):
            with self.assertRaises(ValueError): test.validate_result(metadata,d,c)
        wrong = dict(metadata, release='1111111111111111')
        with self.assertRaises(ValueError): test.validate_result(wrong,data,done)
        bad = data.replace(b'timer=PASS',b'timer=FAIL')
        done = f"COMPLETE {metadata['run']} {binascii.crc32(bad):08x}\n".encode()
        with self.assertRaises(ValueError): test.validate_result(metadata,bad,done)

    def test_listed_but_unreadable_result(self):
        metadata, data, done = self.report()
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)/'srvros-tests'; folder.mkdir()
            report = folder/(metadata['run']+'.txt'); report.write_bytes(data)
            marker = folder/(metadata['run']+'.done'); marker.write_bytes(done)
            original = Path.is_file
            with patch.object(test, 'load_run', return_value=metadata), \
                 patch.object(test, 'volume_mount', return_value=Path(tmp)), \
                 patch.object(Path, 'is_file', lambda p: False if p == report else original(p)):
                with self.assertRaisesRegex(RuntimeError, 'listed but Ubuntu cannot open'):
                    test.collect(metadata['run'])

    def test_collect_to_run_directory(self):
        metadata, data, done = self.report()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); results = root/'runs'; mount = root/'usb'
            run = results/metadata['run']; run.mkdir(parents=True)
            (run/'manifest.json').write_text(json.dumps(metadata))
            folder = mount/'srvros-tests'; folder.mkdir(parents=True)
            (folder/(metadata['run']+'.txt')).write_bytes(data)
            with patch.object(test, 'RUNS', results), patch.object(test, 'volume_mount', return_value=mount):
                with self.assertRaises(RuntimeError): test.collect(metadata['run'])
                (folder/(metadata['run']+'.done')).write_bytes(done)
                with self.assertRaises(OSError): test.collect(metadata['run'])
                (folder/(metadata['run']+'.probe')).write_bytes(test.PROBE_BYTES)
                self.assertTrue(test.collect(metadata['run']))
                self.assertEqual((run/'report.txt').read_bytes(), data)
                self.assertEqual(json.loads((run/'result.json').read_text())['status'], 'PASS')


if __name__ == '__main__': unittest.main()
