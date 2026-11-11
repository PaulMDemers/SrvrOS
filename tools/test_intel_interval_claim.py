"""Original-source interval claim and descendant drain against real cores."""
from pathlib import Path
import os,re,subprocess,unittest
EXE=Path(os.environ.get('INTEL_CLAIM_EXE',Path(__file__).resolve().parents[1]/'build/tests/intel-interval-claim'))
def run(mode,point=1):
    p=subprocess.run([str(EXE),mode,str(point)],text=True,capture_output=True,timeout=10)
    if p.returncode:raise AssertionError((mode,point,p.stdout,p.stderr))
    return p.stdout
class ClaimTests(unittest.TestCase):
    def test_registered_borrower_and_failure_prefixes(self):
        baseline=run('borrow')
        run('borrow-lost-between')
        run('borrow-parent-between')
        clocks,guards=map(int,re.search(r'borrow_clocks=(\d+) borrow_guards=(\d+)',baseline).groups())
        for modes,count in [(('borrow-zero','borrow-backward','borrow-deadline'),clocks),
                            (('borrow-guard','borrow-drop','borrow-count','borrow-register','borrow-rebind',
                              'borrow-parent','borrow-epoch','borrow-sequence','borrow-object','borrow-inactive'),guards)]:
            for mode in modes:
                for point in range(1,count+1):
                    with self.subTest(mode=mode,point=point):run(mode,point)
    def test_actual_lifecycle_and_every_callback_fault(self):
        baseline=run('normal')
        clocks,guards,loads=map(int,re.search(r'clocks=(\d+) guards=(\d+) loads=(\d+)',baseline).groups())
        self.assertEqual(loads,12)
        for mode in ('input-mutation','reentry','leave-open','bad-input','frozen'):run(mode)
        for modes,count in [
            (('zero','backward','prep-deadline','owner-deadline'),clocks),
            (('guard','guard-epoch','guard-owner','guard-detach','guard-rebind','guard-snapshot'),guards),
            (('read','drift','read-epoch','read-detach'),loads)]:
            for mode in modes:
                for point in range(1,count+1):
                    with self.subTest(mode=mode,point=point):run(mode,point)
if __name__=='__main__':unittest.main()
