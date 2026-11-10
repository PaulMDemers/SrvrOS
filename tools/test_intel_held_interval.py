"""Actual GT/prepare/reset and same-borrow one-page observer fault boundaries."""
from pathlib import Path
import re
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[1]
EXE=ROOT/'build/tests/intel-held-interval'
def run(mode,point=1):
    p=subprocess.run([str(EXE),mode,str(point)],capture_output=True,text=True,timeout=5)
    if p.returncode:raise AssertionError(p.stdout+p.stderr)
    return p.stdout
class IntervalTests(unittest.TestCase):
    def test_actual_core(self):
        baseline=run('normal')
        clocks,guards=map(int,re.search(r'clocks=(\d+) guards=(\d+)',baseline).groups())
        for mode in ('reentry','input-mutation','high-original','bad-input'):run(mode)
        for modes,count in [(('zero','backward','deadline','clock-epoch'),clocks),
                            (('guard','guard-epoch','guard-borrow','guard-detach','guard-stage'),guards),
                            (('read','drift','drift-guard','read-epoch'),6)]:
            for mode in modes:
                for point in range(1,count+1):
                    with self.subTest(mode=mode,point=point):run(mode,point)
if __name__=='__main__':unittest.main()
