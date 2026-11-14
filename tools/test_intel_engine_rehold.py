"""Actual GT/preparation/reset/rehold/interval core fault boundaries."""
from pathlib import Path
import subprocess,re,unittest
EXE=Path(__file__).resolve().parents[1]/'build/tests/intel-engine-rehold'
def run(mode,point=1):
    p=subprocess.run([str(EXE),mode,str(point)],text=True,capture_output=True,timeout=10)
    if p.returncode:raise AssertionError((mode,point,p.stdout,p.stderr))
    return p.stdout
class ReholdTests(unittest.TestCase):
    def test_actual_original_source_and_failure_boundaries(self):
        baseline=run('normal')
        clocks,guards,loads=map(int,re.search(r'clocks=(\d+) guards=(\d+) loads=(\d+)',baseline).groups())
        for mode in ('wait','reentry','child-live','child-fail','child-epoch','child-detach','child-late','parent-deadline','child-held-corrupt'):run(mode)
        groups=[(('zero','backward','deadline','clock-epoch'),clocks),
                (('guard','guard-epoch','guard-detach','guard-stage','guard-binding','guard-owner'),guards),
                (('read','value','value-guard','read-epoch'),loads),
                (('write-none','write-uncertain','write-epoch','write-time','ack-float','ack-mask','poll-read','poll-time','never-ready'),2),
                (('count',),4)]
        for modes,count in groups:
            for mode in modes:
                for point in range(1,count+1):
                    with self.subTest(mode=mode,point=point):run(mode,point)
if __name__=='__main__':unittest.main()
