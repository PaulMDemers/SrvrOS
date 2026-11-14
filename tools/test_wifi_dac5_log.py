import unittest
from unittest.mock import patch
from wifi_dac5_log import prior,check,receipt,PROGRESS,SUMMARY
from test_wifi_trim5_log import fixture as parent_fixture

def fixture():return parent_fixture().replace('srvros.wifi-trim5-channel=1','srvros.wifi-trim5-channel=1 srvros.wifi-dac5=1')+PROGRESS+'\n'+'\n'.join(receipt(op) for op in range(91))+'\n'+SUMMARY+'\n'
class ProofTests(unittest.TestCase):
 def test_complete(self):self.assertEqual(check(fixture())['operations'],11669)
 def test_every_record(self):
  good=fixture();parent=prior(good)
  with patch('wifi_dac5_log.prior',return_value=parent):
   for row in good.splitlines():
    if not row.startswith('wifi-dac5'):continue
    for replacement in ('',row+'\n'+row+'\n'):
     with self.assertRaises(ValueError):check(good.replace(row+'\n',replacement,1))
   with self.assertRaises(ValueError):check(good+'wifi-dac5-extra: ok=1\n')
 def test_fields(self):
  good=fixture();parent=prior(good);before,after=good.split(PROGRESS,1)
  for a,b in [('write=1','write=0'),('words_restored=1','words_restored=0'),('dac_reset=1','dac_reset=0'),('op=35','op=34'),('cc=13','cc=1'),('residue=1008','residue=0'),('0x0000000080000049','0x0000000080000004'),('value=0x0000000040000050','value=0x00000000ffffffff')]:
   with patch('wifi_dac5_log.prior',return_value=parent):
    with self.assertRaises(ValueError):check(before+PROGRESS+after.replace(a,b))
  with self.assertRaises(ValueError):check(good.replace(PROGRESS+'\n','').replace('wifi-trim5-progress:',PROGRESS+'\nwifi-trim5-progress:',1))
 def test_parent_not_optional(self):
  with self.assertRaises(ValueError):check(fixture().replace('wifi-trim5: status=0','wifi-trim5: status=1'))
