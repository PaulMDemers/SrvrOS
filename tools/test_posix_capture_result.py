import unittest
from posix_capture_result import CASES, capture_result

class CaptureTests(unittest.TestCase):
    def fixture(self):
        return '\n'.join(['desktop-idle: completed=yes screenshot=yes'] + [
            f'posix-capture: case={name} exited=1 status=0 retired=1 elapsed_us=1000'
            for name in CASES] + ['posix-capture: complete=yes tests=5 expected=5', 'laptop-capture: completed=yes'])

    def test_complete(self):
        self.assertEqual(len(capture_result(self.fixture())['cases']), 5)

    def test_missing_duplicate_failure_or_retained_never_pass(self):
        text=self.fixture()
        for old,new in [('exited=1','exited=0'), ('status=0','status=1'), ('retired=1','retired=0'),
                        ('elapsed_us=1000','elapsed_us=0'), ('elapsed_us=1000',f'elapsed_us={2**64}'),
                        ('tests=5','tests=4'), ('complete=yes','complete=NO'),
                        ('desktop-idle: completed=yes screenshot=yes',''), ('laptop-capture: completed=yes','')]:
            with self.subTest(old=old,new=new): self.assertIsNone(capture_result(text.replace(old,new,1)))
        lines=text.splitlines()
        self.assertIsNone(capture_result('\n'.join(lines[:1]+lines[2:])))
        self.assertIsNone(capture_result(text+'\n'+lines[1]))
        self.assertIsNone(capture_result(text+'\n'+lines[-2]))
        lines[1],lines[2]=lines[2],lines[1]
        self.assertIsNone(capture_result('\n'.join(lines)))

if __name__ == '__main__': unittest.main()
