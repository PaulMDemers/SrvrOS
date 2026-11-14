import os,subprocess,unittest
from wifi_auth_radio5_log import decode


def fixture():
    return [
        'wifi-stream5-radio: version=1 phase=0 attempted=1 complete=1 reads=1 empty=1 saturated=0 observation_only=1',
        'wifi-stream5-radio-entry: phase=0 i=0 ext=0x0000000000000000 fifo=0x0000000000000000',
        'wifi-stream5-radio: version=1 phase=1 attempted=1 complete=1 reads=2 empty=1 saturated=0 observation_only=1',
        'wifi-stream5-radio-entry: phase=1 i=0 ext=0x0000000000008002 fifo=0x000000002000ffa1',
        'wifi-stream5-radio-entry: phase=1 i=1 ext=0x0000000000008002 fifo=0x0000000000000000']


class Tests(unittest.TestCase):
    def test_raw_status_is_observation_only(self):
        phases,n=decode(fixture()+['following owner row'])
        self.assertEqual(n,5)
        self.assertEqual(phases[1]['entries'][0]['fifo'],0x2000ffa1)
        self.assertNotIn('accepted',phases[1])

    def test_no_missing_duplicate_or_reordered_snapshot(self):
        rows=fixture()
        for i in range(len(rows)):
            for changed in (rows[:i]+rows[i+1:],rows[:i]+[rows[i]]+rows[i:]):
                with self.subTest(i=i):
                    try:unused,n=decode(changed)
                    except ValueError:continue
                    # The composable decoder returns its consumed prefix;
                    # the enclosing owner must reject any extra owned row.
                    self.assertLess(n,len(changed))
                    self.assertTrue(changed[n].startswith('wifi-stream5-radio-entry:'))
        with self.assertRaises(ValueError):decode(rows[2:]+rows[:2])

    def test_strict_schema_bounds_and_pop_order(self):
        rows=fixture()
        for i,old,new in ((0,'version=1','version=2'),(0,'reads=1','reads=18'),(0,'complete=1','complete=0'),
                          (0,'observation_only=1','observation_only=0'),(2,'empty=1','empty=0'),
                          (2,'reads=2','reads=01'),(3,'i=0','i=1'),(3,'2000ffa1','00000000'),
                          (4,'00000000','2000ffa1'),(3,'0x000000002000ffa1','0x000000012000ffa1')):
            changed=rows.copy();changed[i]=changed[i].replace(old,new)
            with self.subTest(i=i,old=old,new=new),self.assertRaises(ValueError):decode(changed)

    def test_actual_native_statuses_never_accept_missing_auth(self):
        binary=os.environ.get('WIFI_AUTH_RADIO5_BINARY')
        if not binary:self.skipTest('actual native C binary required')
        for case in (6,7,8,9,11):
            raw=subprocess.check_output([binary,'--trace-case',str(case)],text=True)
            rows=[s for s in raw.splitlines() if s.startswith(('wifi-stream5-radio:','wifi-stream5-radio-entry:'))]
            first,n=decode(rows);second,used=decode(rows[n:]);self.assertEqual(n+used,len(rows))
            self.assertTrue(all(p['summary']['complete']==1 for p in first+second))
            if case==9:self.assertEqual((second[0]['summary']['reads'],second[0]['summary']['saturated']),(17,1))
            else:self.assertEqual(second[0]['summary']['empty'],1)
            values=[e['fifo'] for e in second[1]['entries'] if e['fifo']&1]
            self.assertEqual(values,[] if case in (6,11) else [0x2000ff81 if case==8 else 0x2000ffa1])
            self.assertIn('generation=1 complete=1',raw)
            self.assertNotIn('generation=2 complete=1',raw)
            self.assertIn('complete=0 connections=1 parent_restored=0 provider_retired=0',raw)
            from wifi_http_reconnect5_log import check_owner
            with self.assertRaises(ValueError):check_owner(raw)


if __name__=='__main__':unittest.main()
