"""Independent old/new firmware witnesses; timing is host-only evidence."""
import hashlib,os,resource,subprocess,tempfile,unittest
from pathlib import Path
from fixture_compile import recipe
ROOT=Path(__file__).resolve().parents[1]
TARGET='build/tests/laptop-firmware-digest'
CAPTURE=ROOT/'build/device-fixtures/macbookair7-2-dsdt.bin'
resource.setrlimit(resource.RLIMIT_CORE,(0,0))

class Firmware(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':
            subprocess.run(['make',TARGET,'build/tests/laptop-resources-host'],cwd=ROOT,check=True,capture_output=True)

    def call(self,*args,exe=ROOT/TARGET,env=None):
        p=subprocess.run([str(exe),*map(str,args)],cwd=ROOT,capture_output=True,text=True,timeout=180,env=env)
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
        self.assertIn('firmware-digest: PASS',p.stdout)
        return p.stdout

    def test_independent_reference_unknown_tables_and_existing_resources(self):
        self.assertIn('captured=0',self.call())
        subprocess.run(['build/tests/laptop-resources-host'],cwd=ROOT,check=True)

    def test_captured_table_every_prefix_byte_and_checksum_preserving_drift(self):
        if not CAPTURE.is_file() and os.environ.get('SRVROS_REQUIRE_NATIVE_FIRMWARE')!='1':
            self.skipTest('Local native DSDT fixture is not present')
        before=CAPTURE.read_bytes();self.assertEqual(len(before),35034)
        out=self.call(CAPTURE,'--exhaustive')
        self.assertIn('captured=1 prefixes=35035 byte-mutations=35034 checksum-preserving=494',out)
        self.assertEqual(CAPTURE.read_bytes(),before)
        print(out.strip(),flush=True)
        print('Captured fixture sha256='+hashlib.sha256(before).hexdigest(),flush=True)

    def test_fatal_sanitizers_and_runtime_weakening_controls(self):
        cc,flags,sources=recipe(TARGET)
        text=(ROOT/'kernel/src/drivers/laptop_resources.c').read_text()
        with tempfile.TemporaryDirectory(prefix='firmware-digest-') as directory:
            exe=Path(directory)/'san'
            subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',*sources,'-o',str(exe)],cwd=ROOT,check=True)
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1',UBSAN_OPTIONS='halt_on_error=1')
            self.call(exe=exe,env=env)
            if CAPTURE.is_file():self.call(CAPTURE,exe=exe,env=env)
            controls=[('d.sum+=byte;','(void)byte;'),
                      ('d.hash=(d.hash^byte)*1099511628211ull;','d.hash^=byte;'),
                      ('for(size_t i=0;i<n;i++) {','for(size_t i=0;i<n;i+=2) {')]
            for i,(old,new) in enumerate(controls):
                self.assertEqual(text.count(old),1)
                unit=Path(directory)/f'{i}.c';unit.write_text(text.replace(old,new));exe=Path(directory)/str(i)
                subprocess.run([cc,*flags,'-DSRVROS_LAPTOP_RESOURCES_FILE="'+str(unit)+'"',*sources,'-o',str(exe)],cwd=ROOT,check=True)
                p=subprocess.run([str(exe)],cwd=ROOT,capture_output=True,text=True,timeout=30)
                self.assertNotEqual(p.returncode,0,'Undetected firmware digest weakening '+str(i))
        print('Firmware digest controls PASS 3 (compiled and detected at runtime)',flush=True)

if __name__=='__main__':unittest.main()
