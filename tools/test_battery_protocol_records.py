import subprocess,unittest
from battery_protocol_records import first_protocol_record
class ProtocolRecords(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.clean=subprocess.check_output(['build/tests/battery-transfer','--protocol-clean'])
  cls.failure=subprocess.check_output(['build/tests/battery-transfer','--protocol-failure'])
 def test_real_clean(self):
  r=first_protocol_record([self.clean,self.clean]);self.assertEqual(r['status'],'VALID_CLEAN');self.assertEqual(r['captured'],0)
 def test_real_first_failure_survives_later_refusal_and_cleanup(self):
  r=first_protocol_record([self.failure],require_clean=False)
  self.assertEqual(r['status'],'OBSERVED_FAILURE');self.assertEqual(r['context']['poison'],1)
  self.assertEqual(r['bus']['reason'],2);self.assertEqual(r['bus-ec']['phase'],6);self.assertEqual(r['bus-ec']['error'],0)
  self.assertEqual(r['sample']['reason'],1);self.assertEqual(r['bus']['at_us'],2000100)
  with self.assertRaises(ValueError):first_protocol_record([self.failure])
 def test_missing_malformed_conflicting_and_unbound(self):
  bad=[b'',self.clean.replace(b'version=1',b'version=2'),self.clean.replace(b'kind=0',b'kind=1'),
   self.failure.replace(b'current=15',b'current=18446744073709551616',1),
   self.failure.replace(b'result=-1',b'result=-2',1),
   self.failure.replace(b'value_valid=0',b'value_valid=2'),
   b'\n'.join(self.failure.splitlines()[:-3]),
   self.failure.replace(b'epoch=1',b'epoch=2'),
   self.failure.replace(b'snapshot=retired',b'snapshot=fault'),
   self.clean+self.failure.splitlines()[1]+b'\n',
   self.failure+self.failure.splitlines()[1].replace(b'current=15',b'current=16')+b'\n']
  for x in bad:
   with self.subTest(data=x[:100]),self.assertRaises(ValueError):first_protocol_record([x],require_clean=False)
 def test_clock_fault_is_raw_evidence_not_normalized(self):
  # Failure clocks can be reversed: preserving that observation is the point.
  raw=self.failure.replace(b'previous_us=500',b'previous_us=3000000').replace(b'reason=2 phase=4',b'reason=1 phase=4')
  self.assertEqual(first_protocol_record([raw],require_clean=False)['bus']['previous_us'],3000000)
if __name__=='__main__':unittest.main()
