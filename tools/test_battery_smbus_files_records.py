import re,unittest
from battery_native_smbus_records import check_files
from battery_native_query_smoke import verify_capture_proof
from test_battery_native_smbus_records import PROOF
from test_battery_desktop_records import FILES

def fixture():
    rows=FILES.replace(b'epoch=2',b'epoch=1').splitlines(keepends=True)
    prepared=b''.join(r for r in rows if r.startswith(b'workspace-image: prepared '))
    revoked=b''.join(r for r in rows if r.startswith(b'workspace-image: revoked '))
    work=b''.join(r for r in rows if not r.startswith((b'workspace-image:',b'battery-service: retired ')))
    core=PROOF.replace(b'snapshot generation=3',b'snapshot generation=24').replace(b'generation=3 state=4',b'generation=24 state=4').replace(b'attempts=3',b'attempts=24').replace(b'battery-smbus-capture:',b'battery-smbus-files-capture:')
    core=re.sub(rb'battery-smbus: event=(\d+) generation=(\d+) status=(\d+) alarm=(\d+) address=(\d+) data=(\d+) acknowledged=(\d+) refresh=(\d+); aml=0 serialized=1',rb'smbus-e:\1,\2,\3,\4,\5,\6,\7,\8',core)
    core=re.sub(rb'battery-smbus: invalidate generation=(\d+) cached=(\d+) valid=(\d+) fresh=0',rb'smbus-i:\1,\2,\3',core)
    core=core.replace(b'smbus-e:1',prepared+b'battery-smbus-files: begin limit=512 storage=image; aml=0 serialized=1\n'+b'smbus-e:1',1)
    core=core.replace(b'battery-native-smbus: snapshot',work+b'battery-native-smbus: snapshot',1)
    return core.replace(b'battery-native-smbus: outcome=',revoked+b'battery-native-smbus: outcome=',1)
FILES_PROOF=fixture()
class Proof(unittest.TestCase):
    def test_full_proof(self):
        result=check_files(FILES_PROOF,vm=True)
        self.assertEqual((result['events'],result['file_cycles'],result['generated']),(3,5,60))
    def test_saved_proof_survives_only_uart_interleaving(self):
        interleaved=FILES_PROOF.replace(b'smbus-e:2,',b'smbus-desktop log\ne:2,')
        self.assertEqual(verify_capture_proof(FILES_PROOF,interleaved,check_files,False)['events'],3)
        with self.assertRaises(AssertionError):verify_capture_proof(FILES_PROOF,interleaved,check_files)
        for saved in (interleaved,FILES_PROOF[:-1],FILES_PROOF.replace(b'completed=yes',b'completed=NO')):
            with self.assertRaises(AssertionError):verify_capture_proof(saved,FILES_PROOF,check_files,False)

    def test_compact_and_retirement_corruption(self):
        for old,new in ((b'smbus-e:1,0',b'smbus-e:1,100'),(b'smbus-i:2,2,1',b'smbus-i:2,2,0'),
                        (b'limit=512',b'limit=1024'),(b'hex=4141',b'hex=4142'),
                        (b'smbus-e:3',b'smbus-e:2'),(b'fresh=1',b'fresh=0'),(b'clean=yes',b'clean=NO')):
            with self.subTest(old=old):
                self.assertIn(old,FILES_PROOF)
                with self.assertRaises(AssertionError):check_files(FILES_PROOF.replace(old,new),vm=True)
    def test_missing_duplicate_torn(self):
        for prefix in (b'smbus-e:',b'smbus-i:',b'battery-smbus-files:',b'workspace-image: revoked',b'battery-native-smbus: snapshot'):
            row=next(r for r in FILES_PROOF.splitlines(keepends=True) if r.startswith(prefix))
            with self.assertRaises(AssertionError):check_files(FILES_PROOF.replace(row,b'',1),vm=True)
            with self.assertRaises(AssertionError):check_files(FILES_PROOF+row,vm=True)
        with self.assertRaises(AssertionError):check_files(FILES_PROOF[:-1],vm=True)
    def test_revoke_before_runtime_is_not_clean(self):
        row=b'workspace-image: revoked clean=yes\n'
        with self.assertRaises(AssertionError):check_files(row+FILES_PROOF.replace(row,b''),vm=True)
if __name__=='__main__':unittest.main()
