"""Refusal preflight must not accidentally start a missing/incomplete workload."""
import subprocess
import tempfile
import unittest
from pathlib import Path
import desktop_session_workload as workload


class RefusalTests(unittest.TestCase):
    def test_missing_and_completed_workload(self):
        run, trial = '0123456789abcdef', 'fedcba9876543210'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            inputs = workload.files(run) | workload.refusal_files(run, trial)
            for name, data in inputs.items():
                (root / name).write_bytes(data)
            script = (root / f'refusal-{trial}.sh').read_text().replace('/work/srvros-work', directory)
            result = subprocess.run(['sh', '-c', script], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            folder = root / f'session-{run}'
            self.assertFalse(folder.exists())
            folder.mkdir()
            for name in ('payload.bin', 'rounds.txt', 'complete.txt'):
                (folder / name).write_bytes((root / f'refusal-{trial}-expected-{name}').read_bytes())
            for name in (f'session-{run}.sh', f'refusal-{trial}-expected-script.sh'):
                path = root / name
                path.write_text(path.read_text().replace('/work/srvros-work', directory))
            result = subprocess.run(['sh', '-c', script], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            # Restore only host path substitutions before using the strict collector.
            for name, data in inputs.items():
                (root / name).write_bytes(data)
            read = lambda name: (root / name).read_bytes()
            self.assertEqual(workload.verify_refusal(read, run, trial)['exit_status'], 1)
            (root / f'refusal-{trial}-status.txt').write_bytes(b'0\n')
            with self.assertRaises(ValueError):
                workload.verify_refusal(read, run, trial)


if __name__ == '__main__':
    unittest.main()
