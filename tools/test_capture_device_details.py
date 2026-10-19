import json
from pathlib import Path
import tempfile
import unittest
from capture_linux_hardware import capture_device_details, capture_machine_topology

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

    def test_topology_is_not_tied_to_air_product_id(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);sys=root/'sys';out=root/'out';out.mkdir()
            def put(path,value):
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_bytes(value if isinstance(value,bytes) else value.encode())
            for index,product in enumerate(('0291','0262','0249')):
                node=sys/f'bus/hid/devices/0003:05AC:{product}.000{index}'
                put(node/'uevent',f'HID_ID=0003:000005AC:0000{product}\nMODALIAS=hid:fixture\nHID_UNIQ=PRIVATE\nHID_PHYS=PRIVATE\n')
                put(node/'report_descriptor',bytes((index,1,2)))
            put(sys/'bus/hid/devices/other/uevent','HID_ID=0003:00001234:00000001\n')
            put(sys/'bus/hid/devices/bad/uevent','HID_ID=not-an-id\n')
            board=sys/'class/dmi/id'
            put(board/'board_name','Mac-TEST');put(board/'board_serial','PRIVATE')
            card=sys/'class/drm/card0';put(card/'device/vendor','0x8086');put(card/'device/boot_vga','1')
            connector=sys/'class/drm/card0-eDP-1';put(connector/'status','connected');put(connector/'modes','2880x1800\n1440x900\n');put(connector/'edid',b'EDID-fixture')
            pci=sys/'bus/pci/devices/0000:04:00.0';put(pci/'class','0x010802');put(pci/'device','0x1234')
            disk=sys/'class/block/nvme0n1';put(disk/'size','1000');put(disk/'queue/rotational','0');put(disk/'device/serial','PRIVATE')
            part=sys/'class/block/nvme0n1p1';put(part/'partition','1');put(part/'start','2048')
            put(sys/'class/block/loop0/size','123')
            errors=[];capture_machine_topology(out,errors,sys);self.assertEqual(errors,[])
            result=json.loads((out/'machine-topology.json').read_text())
            self.assertEqual(len(result['apple_hid']),3)
            for item in result['apple_hid']:
                self.assertTrue((out/item['report_descriptor']).is_file())
            self.assertNotIn('PRIVATE',json.dumps(result))
            self.assertEqual(result['board'],{'board_name':'Mac-TEST'})
            self.assertEqual(result['graphics'][0]['properties']['boot_vga'],'1')
            self.assertEqual(result['graphics'][1]['properties']['modes'],'2880x1800\n1440x900')
            self.assertEqual((out/result['graphics'][1]['edid']).read_bytes(),b'EDID-fixture')
            self.assertEqual(result['pci'][0]['properties']['class'],'0x010802')
            self.assertEqual([node['name'] for node in result['block']],['nvme0n1','nvme0n1p1'])
            self.assertEqual(result['block'][1]['properties']['start'],'2048')

    def test_missing_hid_report_is_partial_not_absent_device(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);out=root/'out';out.mkdir();node=root/'sys/bus/hid/devices/apple';node.mkdir(parents=True)
            (node/'uevent').write_text('HID_ID=0003:000005AC:00000291\n')
            errors=[];capture_machine_topology(out,errors,root/'sys')
            result=json.loads((out/'machine-topology.json').read_text())
            self.assertEqual(len(result['apple_hid']),1)
            self.assertIsNone(result['apple_hid'][0]['report_descriptor'])
            self.assertTrue(any('report_descriptor' in error for error in errors))

if __name__=='__main__':unittest.main()
