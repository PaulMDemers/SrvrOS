import os
from pathlib import Path
import subprocess
import unittest
import wifi_mcu_loss_trace as trace


def fixture():
    s=dict.fromkeys(trace.FIELDS[trace.SUMMARY],0)
    s.update(version=1,sequence=3817,published=1,original_reason=4,original_flags=29,
             late_attempted=1,late_reason=2,elapsed_us=1500000,final_flags=29,original_result=-1)
    b=dict.fromkeys(trace.FIELDS[trace.BUFFER],0)
    b.update(version=1,sequence=3817,valid=1,stable=1)
    c=dict(b,phase=1)
    return s,b,c


def emit(data):
    lines=[]
    for p,r in zip([trace.SUMMARY,trace.BUFFER,trace.BUFFER],data):
        lines.append(p+': '+' '.join(k+'='+(f'0x{r[k]:016x}' if k in trace.HEX else str(r[k])) for k in trace.FIELDS[p]))
    return '\n'.join(lines)+'\n'


class TraceTests(unittest.TestCase):
    def test_no_acceptance_and_legacy(self):
        self.assertEqual(trace.observe('legacy')['groups'],[])
        with self.assertRaises(ValueError):trace.observe('legacy',required=True)
        r=trace.observe(emit(fixture()),required=True)
        self.assertEqual(len(r['groups']),1)
        self.assertFalse(any(v for k,v in r.items() if k!='groups'))

    def test_late_completion_stays_failed(self):
        s,b,c=fixture();s.update(late_completed=1,late_reason=1,elapsed_us=650000,consumed=1,consumed_exact=1,dispatched=1,dispatched_exact=1,claimed=1,final_flags=30)
        c.update(nonzero=10,reply_match=1,reply_seq=1,reply_length=8)
        self.assertEqual(trace.observe(emit((s,b,c)))['groups'][0]['summary']['original_result'],-1)
        s['original_result']=16
        with self.assertRaises(ValueError):trace.observe(emit((s,b,c)))

    def test_truncation_order_binding_and_unknown(self):
        text=emit(fixture())
        for bad in [text.rsplit('\n',2)[0]+'\n',text.replace('sequence=3817','sequence=3818',1),
                    text+'wifi-mcu-loss-unknown: version=1\n',text.replace(' phase=0',' phase=1',1),
                    text.replace('version=1','version=2',1),text.replace('late_attempted=1','late_attempted=2')]:
            with self.subTest(bad=bad[:50]),self.assertRaises(ValueError):trace.observe(bad)

    def test_no_claim_without_dispatch_and_no_invalid_buffer(self):
        s,b,c=fixture();s['claimed']=1
        with self.assertRaises(ValueError):trace.observe(emit((s,b,c)))
        s['claimed']=0;c.update(valid=0,nonzero=1)
        with self.assertRaises(ValueError):trace.observe(emit((s,b,c)))

    def test_actual_native_formatter(self):
        binary=Path(os.environ.get('WIFI_MCU_LOSS_BINARY','build/tests/xhci-cal5-mcu-host')).resolve()
        self.assertTrue(binary.is_file(),'build actual native MCU fixture first')
        for fault,target in [(0,0),(1,2),(2,2),(3,2),(7,2),(11,2),(12,9)]:
            with self.subTest(fault=fault):
                run=subprocess.run([str(binary),'--trace-cal5-mcu',str(fault),str(target)],capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stderr)
                result=trace.observe(run.stdout,required=True)
                self.assertEqual(len(result['groups']),5 if not fault else 4 if fault==12 else 1)


if __name__=='__main__':unittest.main()
