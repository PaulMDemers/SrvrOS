import json
from pathlib import Path
import tempfile
import unittest
from capture_linux_hardware import capture_device_details

class DeviceDetailsTest(unittest.TestCase):
    def test_read_only_allowlist_and_descriptors(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);sys=root/'sys';proc=root/'proc';out=root/'out';out.mkdir()
            def put(path,data):
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_bytes(data if isinstance(data,bytes) else data.encode())
            usb=sys/'bus/usb/devices/1-2'
            for name,value in {'idVendor':'0525','idProduct':'a4a2','serial':'NOT-CAPTURED','descriptors':b'\x12\x01\x00\x02'}.items():put(usb/name,value)
            interface=usb/'1-2:1.0';put(interface/'bInterfaceClass','02');put(interface/'modalias','usb:fixture')
            sensor=sys/'class/hwmon/hwmon0';put(sensor/'name','coretemp');put(sensor/'temp1_input','64000');put(sensor/'pwm1','DO-NOT-READ')
            legacy=sys/'class/hwmon/hwmon1/device';put(legacy/'name','applesmc');put(legacy/'fan1_input','6501');put(legacy/'fan1_manual','DO-NOT-READ')
            put(proc/'asound/card0/codec#0','Codec: Cirrus Logic CS4208')
            put(proc/'cpuinfo','vendor_id : GenuineIntel\nmodel : 61\nserial : NOT-CAPTURED\n\nvendor_id : second\n')
            errors=[];capture_device_details(out,errors,sys,proc);self.assertEqual(errors,[])
            result=json.loads((out/'device-details.json').read_text())
            self.assertEqual(result['usb'][0]['properties'],{'idVendor':'0525','idProduct':'a4a2'})
            self.assertEqual(result['usb'][0]['interfaces'][0]['bInterfaceClass'],'02')
            self.assertEqual((out/result['usb'][0]['descriptors']).read_bytes(),b'\x12\x01\x00\x02')
            self.assertEqual(result['sensors'][0]['values'],{'temp1_input':'64000'})
            self.assertEqual(result['sensors'][1],{'name':'applesmc','values':{'fan1_input':'6501'}})
            self.assertEqual(result['boot_cpu'],{'vendor_id':'GenuineIntel','model':'61'})
            self.assertEqual((usb/'serial').read_text(),'NOT-CAPTURED')
            self.assertNotIn('NOT-CAPTURED',json.dumps(result));self.assertNotIn('DO-NOT-READ',json.dumps(result))
    def test_missing_properties_are_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);out=root/'out';out.mkdir();errors=[]
            capture_device_details(out,errors,root/'sys',root/'proc')
            self.assertTrue(any('cpuinfo' in x for x in errors))
            self.assertEqual(json.loads((out/'device-details.json').read_text())['usb'],[])

if __name__=='__main__':unittest.main()
