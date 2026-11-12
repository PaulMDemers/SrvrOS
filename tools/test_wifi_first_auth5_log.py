import subprocess,unittest
from pathlib import Path
from unittest.mock import patch
from wifi_first_auth5_log import observe,TOKEN
import wifi_join5_log as join
from test_wifi_join5_log import Tests as Historical
BINARY=str(Path('build/tests/xhci-join5-host').resolve())
def emitted(mode):return TOKEN+' srvros.wifi-cal5-cleanup=1\n'+subprocess.check_output([BINARY,mode],text=True)
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.logs={m:emitted(m) for m in ('--radio-trace','--radio-timeout-trace','--radio-unsupported-trace')}
 def test_success_and_full_native_child(self):
  log=self.logs['--radio-trace'];r=observe(log)
  self.assertEqual(r['completed_control_reads'],4);self.assertFalse(r['native_accepted'])
  t=Historical();t.trace=log.splitlines();rows=t.rows()
  with patch.object(join,'parent',return_value={'tx5_complete':True}):
   self.assertTrue(join.check('\n'.join(rows))['first_auth5']['present'])
 def test_failed_auth_observations_never_accept(self):
  for mode in ('--radio-timeout-trace','--radio-unsupported-trace'):
   r=observe(self.logs[mode]);self.assertTrue(r['present']);self.assertFalse(r['native_accepted'])
  self.assertTrue(any(f['raw_fc']==176 and f['view']==2 for f in observe(self.logs['--radio-unsupported-trace'])['frames']))
 def test_missing_duplicate_or_unknown_receipts_refuse(self):
  log=self.logs['--radio-trace'];rows=[s for s in log.splitlines() if s.startswith(('wifi-join5-radio','wifi-join5-framing'))]
  for row in rows:
   for bad in (log.replace(row+'\n','',1),log.replace(row+'\n',row+'\n'+row+'\n',1),log.replace(row,row+' unknown=1',1)):
    with self.subTest(row=row),self.assertRaises(ValueError):observe(bad)
  for bad in (log.replace(TOKEN,''),log+'wifi-join5-radio-unknown: x=1\n',log.replace('reads=1','reads=18',1),log.replace('raw_fc=','raw_fc=0',1)):
   with self.assertRaises(ValueError):observe(bad)
 def test_absent_is_not_fabricated_success(self):self.assertEqual(observe(TOKEN+'\n'),dict(present=False,native_accepted=False))
if __name__=='__main__':unittest.main()
