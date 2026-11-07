import unittest
from unittest.mock import patch
from wifi_channel_vco5_log import prior,check,PROGRESS,HOOK,DELAYS,summary,child_receipt,legacy_receipt,trim_plan
from test_wifi_vco5_log import fixture as parent_fixture

def fixture(clear=4):
 rows=[parent_fixture().replace('srvros.wifi-vco5=1','srvros.wifi-vco5=1 srvros.wifi-channel-vco5=1'),PROGRESS]
 for old,row in enumerate(trim_plan(12)):
  if old==HOOK:
   for op in range(77):
    q,n=divmod(op,7)
    if 4<=q<=9 and not n:rows.append(f'wifi-channel-vco5-delay: sample={q-4} us={DELAYS[q-4]} ok=1')
    rows.append(child_receipt(op,observed=0xb0 if q<clear else 0x30))
  rows.append(legacy_receipt(old,row))
 rows.append(summary(12,clear-2));return '\n'.join(rows)+'\n'
class ProofTests(unittest.TestCase):
 def test_complete(self):self.assertEqual(check(fixture())['operations'],15618)
 def test_child_fields_and_timings(self):
  good=fixture();parent=prior(good)
  with patch('wifi_channel_vco5_log.prior',return_value=parent.copy()):
   for q in range(3,10):self.assertEqual(check(fixture(q))['channel_vco5_first_clear'],q-2)
   for q in (2,3,4,5,6,7,8,9):
    op=q*7+3;old=child_receipt(op,observed=0xb0 if q<4 else 0x30)
    for replacement in ('',old+'\n'+old,old.replace('cc=13','cc=1'),old.replace('value=0x','value=0x1',1)):
     with self.assertRaises(ValueError):check(good.replace(old,replacement,1))
   with self.assertRaises(ValueError):check(fixture(10))
 def test_inverse_and_boundaries(self):
  good=fixture();parent=prior(good)
  with patch('wifi_channel_vco5_log.prior',return_value=parent.copy()):
   for a,b in [('restored=1','restored=0'),('child_valid=1','child_valid=0'),('child_operations=77','child_operations=76'),('first_clear=2','first_clear=1'),('us=10000 ','us=10001 '),('phase=15 local=0','phase=15 local=1')]:
    before,after=good.split(PROGRESS,1)
    with self.assertRaises(ValueError):check(before+PROGRESS+after.replace(a,b))
   for i in (0,20,HOOK-1,HOOK,HOOK+1,3746,3766):
    row=legacy_receipt(i,trim_plan(12)[i])
    with self.assertRaises(ValueError):check(good.replace(row+'\n','',1))
   with self.assertRaises(ValueError):check(good+'wifi-channel-vco5-extra: ok=1\n')
 def test_parent(self):
  with self.assertRaises(ValueError):check(fixture().replace('wifi-vco5: status=0','wifi-vco5: status=1'))
