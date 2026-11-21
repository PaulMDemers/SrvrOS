from pathlib import Path
import subprocess,unittest
EXE=Path(__file__).resolve().parents[1]/'build/tests/intel-hda-registry'
class RegistryTests(unittest.TestCase):
    def test_original_registration_nested_pin_and_reentry(self):
        for mode in ('normal','nested','reentry','capacity','drop-list','drop-count','cycle','late','zero','backward'):
            with self.subTest(mode=mode):
                p=subprocess.run([str(EXE),mode],text=True,capture_output=True,timeout=10)
                self.assertEqual(p.returncode,0,(mode,p.stdout,p.stderr))
if __name__=='__main__':unittest.main()
