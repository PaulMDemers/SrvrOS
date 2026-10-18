import binascii
import unittest
from desktop_io_result import validate, validate_timing, timing_gate, BYTES

class DesktopIOTests(unittest.TestCase):
    run_id='0123456789abcdef'
    def record(self,kind,fields):
        data=f'format=srvros-desktop-{kind}-v1\nrun={self.run_id}\n'.encode()
        data+=''.join(f'{k}={v}\n' for k,v in fields.items()).encode()
        return data+f'crc32={binascii.crc32(data)&0xffffffff}\n'.encode()
    def test_durable_payload_and_coverage(self):
        worker=self.record('io',dict(state=3,rounds=16,bytes=16*BYTES))
        activity=dict(state=3,rounds=16,keys=8,motions=1,wheels=1,focus_changes=2,cancels=1)
        ui=self.record('ui',activity)
        payload=bytes((i*17+16*29+ord(self.run_id[i%16]))&255 for i in range(BYTES))
        self.assertEqual(validate(self.run_id,worker,ui,payload)['status'],'EXERCISED')
        for key in ('keys','motions','wheels','focus_changes','cancels'):
            self.assertEqual(validate(self.run_id,worker,self.record('ui',dict(activity,**{key:0})),payload)['status'],'INCOMPLETE')
        with self.assertRaises(ValueError): validate(self.run_id,worker,ui,payload[:-1]+b'\0')
        with self.assertRaises(ValueError): validate(self.run_id,worker[:-1],ui,payload)
        with self.assertRaises(ValueError): validate(self.run_id,worker,self.record('ui',dict(activity,state=4)),payload)

    def test_timing_evidence(self):
        fields=dict(clock=1,active_count=51,active_p95_us=50000,active_p99_us=100000,active_max_us=150000,
                    render_count=25,render_p95_us=8000,render_p99_us=10000,render_max_us=10000)
        self.assertEqual(validate_timing(self.run_id,self.record('timing',fields)),fields)
        target=dict(fields,active_count=100,active_max_us=250000)
        self.assertTrue(timing_gate(target))
        for miss in (dict(active_count=99),dict(active_p95_us=50001),dict(active_p99_us=100001),dict(active_max_us=250001),dict(render_p95_us=16668)):
            self.assertFalse(timing_gate(dict(target,**miss)))
        for invalid in (dict(clock=0),dict(active_count=0),dict(active_p95_us=200000),dict(render_max_us=1)):
            with self.assertRaises(ValueError): validate_timing(self.run_id,self.record('timing',dict(fields,**invalid)))

if __name__=='__main__': unittest.main()
