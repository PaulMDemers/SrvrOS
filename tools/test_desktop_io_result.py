import binascii
import unittest
from desktop_io_result import validate, BYTES

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

if __name__=='__main__': unittest.main()
