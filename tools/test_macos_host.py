#!/usr/bin/env python3
"""Host-selection, artifact provenance and persistent VM disk regressions."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS / 'macos'))
sys.path.insert(0, str(TOOLS))
import bootstrap_zig
import build_manifest

spec = importlib.util.spec_from_file_location('macos_run', TOOLS / 'macos/run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class HostTools(unittest.TestCase):
    def test_simulated_builds_cannot_relabel_images(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            commands = {
                'uname': 'echo "Darwin arm64"',
                'brew': 'printf "%s\\n" "$TEST_BREW_PREFIX"',
                'make': 'if [ "$1" = --version ]; then echo "GNU Make 4.4.1"; else exit 99; fi',
                'tar': 'echo "tar (GNU tar) 1.35"',
            }
            for name, body in commands.items():
                path = root / name
                path.write_text('#!/bin/sh\n' + body + '\n')
                path.chmod(0o755)
            env = dict(os.environ, PATH=f'{root}:{os.environ["PATH"]}',
                       TEST_BREW_PREFIX=temp, MAKEFLAGS='', MFLAGS='')
            for flags, inherited in ((['-n'], ''), (['-nt'], ''),
                                     (['--dry-run'], ''), (['--touch'], ''),
                                     ([], 'n'), ([], '--question')):
                with self.subTest(flags=flags, inherited=inherited):
                    result = subprocess.run(['sh', str(TOOLS / 'macos/build.sh'), *flags, 'all'],
                                            env=dict(env, MAKEFLAGS=inherited),
                                            text=True, capture_output=True)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertIn('require a real build', result.stderr)

    def test_pinned_compiler_platforms(self):
        for system, arch, expected in (
            ('Darwin', 'arm64', 'aarch64-macos'),
            ('Darwin', 'aarch64', 'aarch64-macos'),
            ('Linux', 'x86_64', 'x86_64-linux'),
            ('Windows', 'AMD64', 'x86_64-windows'),
            ('MSYS_NT-10.0', 'x86_64', 'x86_64-windows'),
        ):
            with self.subTest(system=system, arch=arch):
                name, extension, digest = bootstrap_zig.release_for('0.15.2', system, arch)
                self.assertEqual(name, f'zig-{expected}-0.15.2')
                self.assertEqual(extension, 'zip' if 'windows' in expected else 'tar.xz')
                self.assertEqual(len(bytes.fromhex(digest)), 32)
        for version, system, arch in (('0.15.2', 'Linux', 'arm64'),
                                     ('0.15.2', 'Darwin', 'x86_64'),
                                     ('0.99.0', 'Darwin', 'arm64')):
            with self.assertRaises(ValueError):
                bootstrap_zig.release_for(version, system, arch)

    def test_changed_images_refuse_old_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            repo = Path(temp)
            for name in build_manifest.INPUTS:
                path = repo / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(name.encode())
            with self.assertRaisesRegex(RuntimeError, 'build.sh'):
                runner.verified_build(repo)
            hashes = build_manifest.image_hashes(repo)
            (repo / 'build/macos-build.json').write_text(json.dumps(
                {'schema': 1, 'source': 'recorded-source', 'hashes': hashes}))
            self.assertEqual(runner.verified_build(repo)['source'], 'recorded-source')
            (repo / build_manifest.INPUTS[0]).write_bytes(b'new kernel')
            with self.assertRaisesRegex(RuntimeError, 'hashes differ'):
                runner.verified_build(repo)

    def test_disk_reuse_preserves_user_bytes_and_failed_copy_is_not_reused(self):
        with tempfile.TemporaryDirectory() as temp:
            repo = Path(temp); state = repo / 'state'; state.mkdir()
            (repo / 'build').mkdir()
            (repo / 'build/srvros.exfat').write_bytes(b'template')
            disk = runner.prepare_disk(state, repo, {'kernel': 'first'})
            disk.write_bytes(b'user document')
            self.assertEqual(runner.prepare_disk(state, repo, {'kernel': 'first'}), disk)
            self.assertEqual(disk.read_bytes(), b'user document')
            def fail(source, out):
                out.write(b'partial')
                raise OSError('disk full')
            with patch.object(runner.shutil, 'copyfileobj', side_effect=fail):
                with self.assertRaises(OSError):
                    runner.prepare_disk(state, repo, {'kernel': 'second'})
            self.assertEqual(list(state.iterdir()), [disk])
            second = runner.prepare_disk(state, repo, {'kernel': 'second'})
            self.assertNotEqual(second, disk)
            self.assertEqual(second.read_bytes(), b'template')

    def test_running_vm_and_invalid_pid_are_not_replaced(self):
        with tempfile.TemporaryDirectory() as temp:
            pid = Path(temp) / 'pid'
            for invalid in ('garbage', '0', '-1'):
                pid.write_text(invalid)
                with self.assertRaises(RuntimeError):runner.refuse_running(pid)
            pid.write_text('123')
            with patch.object(runner.os, 'kill', return_value=None) as check:
                with self.assertRaisesRegex(RuntimeError, 'still exists'):runner.refuse_running(pid)
                check.assert_called_once_with(123, 0)
            with patch.object(runner.os, 'kill', side_effect=ProcessLookupError):runner.refuse_running(pid)


if __name__ == '__main__':
    unittest.main()
