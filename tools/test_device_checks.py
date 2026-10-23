"""Attended checklist preserves evidence, waits for stdin and records observations only."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DeviceChecks(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.home = self.root / 'home'
        self.home.mkdir()
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        examples = self.root / 'share/examples'
        examples.mkdir(parents=True)
        for name in ('device-checks.sh', 'network-trial.sh', 'trackpad-checks.sh'):
            text = (ROOT / 'userspace/examples' / name).read_text()
            (examples / name).write_text(text.replace('/fat', str(self.root)))
        (examples / 'network-check.sh').write_text('echo dhcp-status=0\necho dns-status=0\necho http-status=0\necho HTTP-fixture\n')
        for name, script in {'playwav': 'echo playwav-fixture\nexit 0', 'ifconfig': 'echo role=active-net0 link=up', 'dhcp': 'exit 1', 'sleep': ':', 'sync': ':'}.items():
            f = self.bin / name
            f.write_text('#!/bin/sh\n' + script + '\n')
            f.chmod(0o755)
        self.script = examples / 'device-checks.sh'
        self.env = dict(os.environ, HOME=str(self.home), PATH=str(self.bin)+':'+os.environ['PATH'])

    def run_checks(self, data):
        return subprocess.run(['/bin/sh', str(self.script)], input=data, text=True,
                              capture_output=True, env=self.env, timeout=5)

    def test_trackpad_only_preserves_original_reports_and_refuses_replay(self):
        self.script=self.script.with_name('trackpad-checks.sh')
        original=self.home/'device-checks'
        original.mkdir();(original/'observations.txt').write_text('original audio/network evidence\n')
        r=self.run_checks('press-ok tap-ok\ndrag-ok scroll-ok cancel-ok\n\n')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        report=self.home/'trackpad-checks'
        self.assertEqual((report/'sequence.txt').read_text(),'started\ncompleted\n')
        self.assertIn('secondary-input: press-ok tap-ok',(report/'observations.txt').read_text())
        before={p.name:p.read_bytes() for p in report.iterdir()}
        self.assertNotEqual(self.run_checks('overwrite\n').returncode,0)
        self.assertEqual(before,{p.name:p.read_bytes() for p in report.iterdir()})
        self.assertEqual((original/'observations.txt').read_text(),'original audio/network evidence\n')

    def test_eof_never_completes(self):
        result = self.run_checks('')
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((self.home/'device-checks/sequence.txt').read_text(), 'started\n')
        self.assertFalse((self.home/'device-checks/speakers.txt').exists())

    def test_complete_records_failures_without_claiming_pass_and_refuses_replay(self):
        # Audio ready/observation gates, two network cable gates, four final observations/gates.
        data = '\nspeaker-left-missing\n\nheadphone-ok\n\nstop-missed\n\n\nlink-stuck\npress-ok tap-failed\ndrag-ok cancel-skipped\n\n'
        result = self.run_checks(data)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        report = self.home/'device-checks'
        self.assertEqual((report/'sequence.txt').read_text(), 'started\ncompleted\n')
        observed = (report/'observations.txt').read_text()
        for item in ('speaker-left-missing','headphone-ok','stop-missed','link-stuck','press-ok tap-failed','drag-ok cancel-skipped'):
            self.assertIn(item, observed)
        self.assertIn('disconnected-dhcp-status=1', (report/'network-down.txt').read_text())
        self.assertIn('Completion is NOT a pass', result.stdout)
        self.assertNotIn('To return: press F10', result.stdout)
        before = {p.name:p.read_bytes() for p in report.iterdir()}
        self.assertNotEqual(self.run_checks(data).returncode, 0)
        self.assertEqual(before, {p.name:p.read_bytes() for p in report.iterdir()})

    def test_partial_observations_survive_eof(self):
        self.assertNotEqual(self.run_checks('\nspeakers-ok\n').returncode, 0)
        report = self.home/'device-checks'
        self.assertIn('speakers-ok', (report/'observations.txt').read_text())
        self.assertEqual((report/'sequence.txt').read_text(), 'started\n')

if __name__ == '__main__':
    unittest.main()
