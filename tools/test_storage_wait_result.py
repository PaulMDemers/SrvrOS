import unittest
from storage_wait_result import ORDER,capture_result,PAYLOAD

class CaptureTests(unittest.TestCase):
    def fixture(self):
        lines=['storage-wait: begin version=1 path=/work/srvros-work/Logs/session-0123456789abcdef-00/wait-fixture.bin bytes=32768 rounds=6 policies=3 original_us=4000']
        for r,policies in enumerate(ORDER):
            for p,policy in enumerate(policies):
                lines.append(f'storage-wait: sample round={r} position={p} poll_us={policy} bytes=32768 elapsed_us=10000 sampled_ticks=1 idle_ticks=0 cbw_calls=2 cbw_us=100 data_calls=8 data_us=200 csw_calls=2 csw_us=300 failures=0 ok=1')
        lines.append('storage-wait: complete=yes samples=18 restored=yes elapsed_us=180001')
        return '\n'.join(lines)
    def test_complete(self):
        result=capture_result(self.fixture());self.assertIsNotNone(result)
        self.assertEqual([p['count'] for p in result['policies']],[6,6,6])
    def test_refuse_incomplete_or_invalid(self):
        text=self.fixture()
        for bad in (text.replace('complete=yes','complete=NO'),text.replace('restored=yes','restored=NO'),text+'\n'+text,text.replace('round=5','round=4'),text.replace('idle_ticks=0','idle_ticks=2'),text.replace('csw_us=300','csw_us=999999'),text.replace('failures=0','failures=1'),text.replace('bytes=32768','bytes=8'),text.replace('elapsed_us=10000','elapsed_us=0'),text.replace('original_us=4000','original_us=0'),'\n'.join(text.splitlines()[:-2])):
            self.assertIsNone(capture_result(bad))
    def test_offline_fixture_required(self):
        import tempfile
        from pathlib import Path
        import hardware_desktop
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);mount=root/'usb';work=mount/'srvros-work'
            folder=work/'Logs/session-0123456789abcdef-00';folder.mkdir(parents=True)
            (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            boot='laptop-capture: boot options=srvros.desktop-release=0123456789abcdef srvros.capture=storage-wait\n'
            (folder/'boot.log').write_text(boot)
            (folder/'exit.log').write_text(boot+self.fixture())
            fixture=folder/'wait-fixture.bin'
            for data,valid in ((None,False),(b'wrong',False),(PAYLOAD,True)):
                if data is not None:fixture.write_bytes(data)
                _,report=hardware_desktop.collect(mount,'37d8c5d0',root/'out')
                item=report['sessions'][0]
                self.assertEqual(item['storage_wait'] is not None,valid)
                self.assertEqual(bool(item['attention']),not valid)
            fixture.unlink();fixture.symlink_to(root/'target');(root/'target').write_bytes(PAYLOAD)
            _,report=hardware_desktop.collect(mount,'37d8c5d0',root/'out')
            self.assertIsNone(report['sessions'][0]['storage_wait'])
if __name__=='__main__':unittest.main()
