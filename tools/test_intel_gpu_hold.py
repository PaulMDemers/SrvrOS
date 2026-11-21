"""Original-source GPU requester hold, including every clock/read/store prefix."""
import os,re,subprocess,unittest
from pathlib import Path
EXE=Path(os.environ.get('INTEL_GPU_HOLD_EXE',Path(__file__).resolve().parents[1]/'build/tests/intel-gpu-hold'))
def run(mode,point=1,pending=False):
 p=subprocess.run([str(EXE),mode,str(point),*(["pending"] if pending else [])],text=True,capture_output=True,timeout=15)
 if p.returncode:raise AssertionError((mode,point,p.stdout,p.stderr))
 return {k:int(v) for k,v in re.findall(r'(clocks|guards|reads|attempts|stores|polls)=(\d+)',p.stdout)}
class HoldTests(unittest.TestCase):
 def test_lifetime_and_every_callback_prefix(self):
  n=run('normal');pending=run('pending');run('timeout')
  self.assertEqual((n['attempts'],n['stores'],n['reads']),(2,2,27))
  self.assertEqual(pending['polls'],3)
  for is_pending,baseline in [(False,n),(True,pending)]:
   for modes,count in [(('zero','backward','cross-backstep','forward-then-backward','deadline','source'),baseline['clocks']),
                       (('guard','drop'),baseline['guards']),
                       (('read','drift'),baseline['reads']), (('none','uncertain'),2)]:
    for mode in modes:
     for point in range(1,count+1):
      with self.subTest(mode=mode,point=point,pending=is_pending):run(mode,point,is_pending)
if __name__=='__main__':unittest.main()
