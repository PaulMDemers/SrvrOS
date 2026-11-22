import unittest
from unittest.mock import patch
from wifi_vco5_log import prior,check,receipt,summary,PROGRESS,DELAYS
from test_wifi_dac5_log import fixture as parent_fixture

def fixture(clear=6):
 rows=[parent_fixture().replace('srvros.wifi-dac5=1','srvros.wifi-dac5=1 srvros.wifi-vco5=1'),PROGRESS]
 for op in range(105):
  q,n=divmod(op,7)
  if 6<=q<=11 and not n:rows.append(f'wifi-vco5-delay: sample={q-6} us={DELAYS[q-6]} ok=1')
  rows.append(receipt(op,observed=0xb0 if q<clear else 0x30))
 rows.append(summary(clear-4));return '\n'.join(rows)+'\n'
class ProofTests(unittest.TestCase):
 def test_complete(self):
  good=fixture();parent=prior(good)
  with patch('wifi_vco5_log.prior',return_value=parent):
   for q in range(5,12):self.assertEqual(check(fixture(q))['vco5_first_clear'],q-4)
  self.assertEqual(check(good)['operations'],11774)
 def test_every_record(self):
  good=fixture();parent=prior(good)
  with patch('wifi_vco5_log.prior',return_value=parent):
   for row in good.splitlines():
    if not row.startswith('wifi-vco5'):continue
    for replacement in ('',row+'\n'+row+'\n'):
     with self.assertRaises(ValueError):check(good.replace(row+'\n',replacement,1))
   with self.assertRaises(ValueError):check(good+'wifi-vco5-extra: ok=1\n')
 def test_fields(self):
  good=fixture();parent=prior(good);before,after=good.split(PROGRESS,1)
  for a,b in [('write=1','write=0'),('words_restored=1','words_restored=0'),('triggered=1','triggered=0'),('op=35','op=34'),('cc=13','cc=1'),('residue=1008','residue=0'),('us=10000 ','us=9999 '),('first_clear=2','first_clear=1'),('0x0000000080000004','0x0000000080000049'),('value=0x0000000040000050','value=0x00000000ffffffff')]:
   with patch('wifi_vco5_log.prior',return_value=parent):
    with self.assertRaises(ValueError):check(before+PROGRESS+after.replace(a,b))
  with patch('wifi_vco5_log.prior',return_value=parent):
   for q in range(6,12):
    with self.assertRaises(ValueError):check(good.replace(receipt(q*7+3,observed=0x30),receipt(q*7+3,observed=0x31)))
   with self.assertRaises(ValueError):check(good.replace(receipt(80,observed=0x30),receipt(80,observed=0xb0)))
   with self.assertRaises(ValueError):check(fixture(12))
 def test_parent_not_optional(self):
  with self.assertRaises(ValueError):check(fixture().replace('wifi-dac5: status=0','wifi-dac5: status=1'))
