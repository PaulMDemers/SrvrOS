import os,subprocess,unittest
from wifi_handshake5_log import observe

BINARY=os.environ.get('WIFI_HANDSHAKE5_BINARY','build/tests/xhci-http-reconnect5-host')
class HandshakeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.logs={i:subprocess.check_output([BINARY,'--trace-case',str(i)],text=True) for i in (0,1,6,37,56,57,58,59)}
    def test_real_native_paths(self):
        for case in (0,1,6,56,57,58,59):
            r=observe(self.logs[case]);self.assertTrue(r['present']);self.assertFalse(r['native_accepted']);self.assertEqual(len(r['epochs']),2)
        self.assertFalse(observe(self.logs[37])['present'])
    def test_second_M3_distinguished(self):
        for case,result,kind,reject in ((56,2,0,0),(57,9,1,7),(58,9,2,11),(59,1,2,1)):
            with self.subTest(case=case):
                r=observe(self.logs[case])['epochs'][1]['responses'][3]
                self.assertEqual((r['result'],r['kind'],r['reject']),(result,kind,reject))
    def mutate(self,prefix,key,value,case=59):
        import re
        text=self.logs[case];line=next(s for s in text.splitlines() if s.startswith(prefix+': '))
        bad=re.sub(r'(?<![a-z_])'+key+r'=[0-9]+',key+'='+str(value),line)
        self.assertNotEqual(line,bad)
        return text.replace(line,bad,1)
    def test_strict_schema_and_bounds(self):
        for prefix,key,value in [('wifi-handshake5','version',2),('wifi-handshake5','epoch',2),('wifi-handshake5','responses',5),('wifi-handshake5','observation_only',0),('wifi-handshake5-response','stage',1),('wifi-handshake5-response','result',0),('wifi-handshake5-response','polls',20001),('wifi-handshake5-response','present',999),('wifi-handshake5-response','ignored',999),('wifi-handshake5-response','kind',4),('wifi-handshake5-response','reject',14),('wifi-handshake5-response','last_us',2**64),('wifi-stream5-keys','m3',0)]:
            with self.subTest(key=key,value=value):
                with self.assertRaises(ValueError):observe(self.mutate(prefix,key,value))
    def test_missing_duplicate_unknown(self):
        text=self.logs[59];line=next(s for s in text.splitlines() if s.startswith('wifi-handshake5-response: '))+'\n'
        for bad in (text.replace(line,'',1),text.replace(line,line+line,1),text+'wifi-handshake5-unknown: x=1\n',text.replace(' stage=0 result=1',' result=1 stage=0',1)):
            with self.assertRaises(ValueError):observe(bad)
    def test_refusal_not_completion(self):
        for case in (56,57,58):
            text=self.logs[case];r=observe(text)['epochs'][1]
            self.assertEqual(r['key_observation']['activated'],0)
            self.assertIn('wifi-http-reconnect5-final: version=1 complete=0 connections=1',text)
            self.assertFalse(observe(text)['native_accepted'])
    def test_absent_and_historical(self):
        self.assertEqual(observe('legacy log\n'),dict(present=False,native_accepted=False))
if __name__=='__main__':unittest.main()
