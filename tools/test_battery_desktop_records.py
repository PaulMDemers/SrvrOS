import unittest
from battery_desktop_smoke import check_capture, check_text_capture, check_load_capture, check_files_capture, verify_app_metadata
from test_sci_observe_records import BASE as SCI
from hardware_desktop import battery_desktop_result, battery_text_result, battery_load_result, battery_app_files_result, needs_attention

BASE = SCI + b'''battery-desktop: started pid=4 epoch=2 generation=1 start_us=1000 storage=ram
desktop-coexist: completed=yes start_us=2000 end_us=60002000 loops=6000 max_gap_us=12000 generated=60 presented=60 frames=61 storage=ram
deskclient: exited frames=61 input=Ab
deskd: exited frames=65 fb_bytes=1000000
battery-service: retired epoch=2 attempts=24 rejected=0 queries=100 notifications=100 attached=1 detached=1 heap=0 errors=0 denied=0
battery-desktop: PASS outcome=complete epoch=2 first_gen=1 last_gen=24 start_us=1000 end_us=60500000 reads=6100 busy_reads=600 process_status=0; retired=yes
'''

class CoexistRecords(unittest.TestCase):
    def test_file_metadata_exact_paths_and_identity(self):
        paths=['/battery-test','/battery-test/fixture','/battery-test/fixture/.battery-test',
               '/battery-test/fixture/anchor.txt']+[f'/battery-test/fixture/text-{i}.txt' for i in range(1,6)]
        rows=[f'{p}\t{512+i}\t{16877 if i<2 else 33188}\t{2 if i<2 else 1}\t0\t0\t1\t2\t3\n' for i,p in enumerate(paths)]
        data=('SRVROSMETA1\n'+''.join(rows)).encode()
        verify_app_metadata(data)
        for bad in (data+rows[-1].encode(),data.replace(rows[-1].encode(),b''),
                    data.replace(b'text-5.txt',b'text-draft.txt'),data.replace(b'\t520\t',b'\t519\t'),
                    data.replace(b'33188',b'33279'),data.replace(b'\t1\t2\t3',b'\t1\t2\t-3')):
            with self.assertRaises(AssertionError):verify_app_metadata(bad)

    def test_complete(self):
        self.assertEqual(check_capture(BASE)['presented'],60)

    def test_missing_duplicate_or_failure(self):
        for prefix in (b'battery-desktop: started',b'battery-desktop: PASS',b'desktop-coexist: completed',b'battery-service: retired'):
            line=next(l for l in BASE.splitlines(keepends=True) if l.startswith(prefix))
            with self.subTest(prefix=prefix):
                with self.assertRaises(AssertionError):check_capture(BASE.replace(line,b''))
        for prefix in (b'battery-desktop: started',b'battery-desktop: PASS',b'desktop-coexist: completed'):
            line=next(l for l in BASE.splitlines(keepends=True) if l.startswith(prefix))
            with self.assertRaises(AssertionError):check_capture(BASE+line)
        for failure in (b'deskclient: FAIL',b'deskd: FAIL',b'battery-desktop: FAIL',b'battery: HALTED'):
            with self.assertRaises(AssertionError):check_capture(BASE+failure+b'\n')

    def test_collector_requires_recipe_and_return(self):
        prefix=b'laptop-capture: boot options=srvros.capture=battery-desktop\n'
        suffix=b'battery-desktop-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
        self.assertIsNotNone(battery_desktop_result((prefix+BASE+suffix).decode()))
        for data in (BASE,prefix+BASE,BASE+suffix,prefix+BASE+suffix.replace(b'clean=yes',b'clean=NO')):
            self.assertIsNone(battery_desktop_result(data.decode()))
        for line in (prefix+BASE+suffix).decode().splitlines():self.assertFalse(needs_attention(line),line)
        self.assertTrue(needs_attention('battery-desktop: FAIL outcome=desktop deadline'))
        self.assertTrue(needs_attention('desktop-coexist: completed=NO'))

    def test_no_false_overlap_or_progress(self):
        changes=((b'first_gen=1',b'first_gen=2'),(b'last_gen=24',b'last_gen=1'),
                 (b'end_us=60500000',b'end_us=60001000'),(b'end_us=60002000',b'end_us=50002000'),
                 (b'start_us=2000',b'start_us=1'),(b'busy_reads=600',b'busy_reads=0'),
                 (b'presented=60',b'presented=59'),(b'frames=61',b'frames=1'),
                 (b'process_status=0',b'process_status=1'),(b'detached=1',b'detached=0'),
                 (b'heap=0',b'heap=1'),(b'epoch=2 first_gen',b'epoch=3 first_gen'))
        for old,new in changes:
            with self.subTest(old=old,new=new):
                with self.assertRaises(AssertionError):check_capture(BASE.replace(old,new))

TEXT = BASE.replace(b'deskclient: exited frames=61 input=Ab',b'desktools: exited role=text failure=0')
TEXT += b''.join(f'text-coexist: checkpoint serial={i*12} length=2 caret=2 anchor=2 clipboard_busy=0 hex=4141\n'.encode() for i in range(1,6))
TEXT += b'text-coexist: final serial=60 length=2 caret=2 anchor=2 clipboard_busy=0 hex=4141\n'
TEXT += b'text-coexist: file_worker=disabled checkpoints=5\n'
TEXT += b'desktop-text: generated=60 presented=60 keyboard_sent=0 keyboard_presented=0 storage=ram\n'

class TextCoexistRecords(unittest.TestCase):
    def test_script_and_injected_input(self):
        self.assertEqual(check_text_capture(TEXT)['script_checkpoints'],5)
        injected=TEXT.replace(b'final serial=60 length=2 caret=2 anchor=2 clipboard_busy=0 hex=4141',b'final serial=1046 length=4 caret=4 anchor=4 clipboard_busy=0 hex=5a625a62').replace(b'keyboard_sent=0 keyboard_presented=0',b'keyboard_sent=46 keyboard_presented=46')
        self.assertEqual(check_text_capture(injected,True)['keyboard_presented'],46)
        with self.assertRaises(AssertionError):check_text_capture(injected)
        with self.assertRaises(AssertionError):check_text_capture(TEXT,True)
        # The accepted v40 study collector must not silently accept a new mode.
        with self.assertRaises(AssertionError):check_capture(TEXT)

    def test_native_collector_requires_exact_recipe_return_and_text_proof(self):
        prefix=b'laptop-capture: boot options=srvros.capture=battery-text srvros.capture-failure=reboot\n'
        suffix=b'battery-text-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
        valid=prefix+TEXT+suffix
        self.assertEqual(battery_text_result(valid.decode())['script_checkpoints'],5)
        self.assertIsNone(battery_desktop_result(valid.decode()))
        for data in (TEXT,prefix+TEXT,TEXT+suffix,valid.rstrip(b'\n'),valid+suffix,
                     valid+prefix,valid.replace(b'clean=yes',b'clean=NO'),
                     valid.replace(b'srvros.capture=battery-text ',b'srvros.capture=battery-text-other '),
                     valid.replace(b'hex=4141',b'hex=4142'),prefix+BASE+suffix):
            with self.subTest(data=data[-100:]):self.assertIsNone(battery_text_result(data.decode()))
        for line in valid.decode().splitlines():self.assertFalse(needs_attention(line),line)
        self.assertTrue(needs_attention('battery-text-capture: returned=yes clean=NO'))

    def test_reject_incomplete_or_wrong_editing(self):
        changes=((b'serial=12 ',b'serial=13 '),(b'hex=4141',b'hex=4162'),
                 (b'caret=2',b'caret=1'),(b'anchor=2',b'anchor=1'),
                 (b'clipboard_busy=0',b'clipboard_busy=1'),(b'length=2',b'length=1'),
                 (b'file_worker=disabled',b'file_worker=UNEXPECTED'),
                 (b'keyboard_presented=0',b'keyboard_presented=1'),
                 (b'failure=0',b'failure=1'))
        for old,new in changes:
            with self.subTest(old=old):
                with self.assertRaises(AssertionError):check_text_capture(TEXT.replace(old,new))
        for prefix in (b'text-coexist: checkpoint',b'text-coexist: final',b'desktop-text: generated',b'text-coexist: file_worker'):
            line=next(l for l in TEXT.splitlines(keepends=True) if l.startswith(prefix))
            with self.assertRaises(AssertionError):check_text_capture(TEXT.replace(line,b''))
            with self.assertRaises(AssertionError):check_text_capture(TEXT+line)
        with self.assertRaises(AssertionError):check_text_capture(TEXT+b'desktools: file-result role=text\n')
        with self.assertRaises(AssertionError):check_text_capture(TEXT.rstrip(b'\n'))

LOAD = b'workspace-image: prepared path=/work/srvros-work/battery-fs.img raw_offset=2097152 bytes=16777216; unmounted=yes aliases=revoked\n'+TEXT
LOAD += b'workspace-image: revoked clean=yes\n'
LOAD += b'battery-filesystem: PASS cycles=10 commands=1000 busy_cycles=2 progress_bursts=10 bursts=180 max_burst_us=60000 elapsed_us=60598500 steps=180 max_step_us=50000 slowest_step=1 profile=1; retired=yes revoked=yes\n'
LOAD += b'battery-filesystem-profile: cycle=1 step=1 elapsed_us=50000 io_us=0\n'
LOAD += b''.join(f'battery-filesystem-phase: phase={i} reads=0 read_bytes=0 read_us=0 read_max_us=0 writes=0 write_bytes=0 write_us=0 write_max_us=0 flushes=0 flush_us=0 flush_max_us=0\n'.encode() for i in range(4))
LOAD += b'battery-text-load: PASS epoch=2 start_us=1000 end_us=60600000 first_step_us=1500 last_step_us=60600000; retired=yes revoked=yes\n'

class LoadCoexistRecords(unittest.TestCase):
    def test_complete_load(self):
        result=check_load_capture(LOAD)
        self.assertEqual(result['filesystem']['cycles'],10)
        self.assertEqual(result['overlap_us'],60000000)
        prefix=b'laptop-capture: boot options=srvros.capture=battery-load\n'
        tail=b'battery-load-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
        self.assertIsNotNone(battery_load_result((prefix+LOAD+tail).decode()))
        for data in (LOAD,prefix+LOAD,LOAD+tail,prefix+LOAD+tail.rstrip(b'\n'),prefix+LOAD+tail+tail,
                     (prefix+LOAD+tail).replace(b'srvros.capture=battery-load',b'srvros.capture=battery-load-other')):
            self.assertIsNone(battery_load_result(data.decode()))
        self.assertTrue(needs_attention('battery-text-load: FAIL outcome=filesystem retirement'))
        self.assertTrue(needs_attention('battery-load-capture: returned=yes clean=NO'))

    def test_reject_sequential_or_incomplete_load(self):
        changes=((b'first_step_us=1500',b'first_step_us=59000000'),
                 (b'last_step_us=60600000',b'last_step_us=1000000'),
                 (b'epoch=2 start_us=1000 end_us=60600000',b'epoch=3 start_us=1000 end_us=60600000'),
                 (b'elapsed_us=60598500',b'elapsed_us=60000000'),
                 (b'steps=180',b'steps=179'),(b'progress_bursts=10',b'progress_bursts=0'),
                 (b'busy_cycles=2',b'busy_cycles=0'),(b'revoked clean=yes',b'revoked clean=NO'),
                 (b'hex=4141',b'hex=4142'))
        for old,new in changes:
            with self.subTest(old=old):
                with self.assertRaises(AssertionError):check_load_capture(LOAD.replace(old,new))
        for prefix in (b'workspace-image: prepared',b'workspace-image: revoked',b'battery-filesystem: PASS',b'battery-text-load: PASS'):
            row=next(r for r in LOAD.splitlines(keepends=True) if r.startswith(prefix))
            with self.assertRaises(AssertionError):check_load_capture(LOAD.replace(row,b''))
            with self.assertRaises(AssertionError):check_load_capture(LOAD+row)
        with self.assertRaises(AssertionError):check_load_capture(LOAD.rstrip(b'\n'))

FILES=TEXT.replace(b'storage=ram',b'storage=image').replace(b'caret=2 anchor=2',b'caret=0 anchor=0').replace(b'file_worker=disabled',b'file_worker=enabled')
FILES+=b'workspace-image: prepared path=/work/srvros-work/battery-fs.img raw_offset=2097152 bytes=16777216; unmounted=yes aliases=revoked\n'
FILES+=b'battery-app-files: PASS descendants=3; retired=yes revoked=yes\ntext-files: cycles=5 pending=0 storage=image\nworkspace-image: revoked clean=yes\n'
FILES+=b'desktop-battery: reads=60 first_gen=1 last_gen=24 cached=yes\n'
FILES+=b''.join(f'desktools: file-result role=text op={op} status=0 bytes={size}\n'.encode() for _ in range(5) for op,size in ((2,0),(8,32),(5,0),(1,2)))

class ApplicationFileRecords(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(check_files_capture(FILES)['worker_operations'],20)
        with self.assertRaises(AssertionError):check_text_capture(FILES)

    def test_native_recipe_and_return(self):
        prefix=b'laptop-capture: boot options=srvros.capture=battery-app-files srvros.capture-failure=reboot\n'
        suffix=b'battery-app-files-capture: returned=yes clean=yes; physical accuracy NOT_TESTED\n'
        good=prefix+FILES+suffix
        self.assertEqual(battery_app_files_result(good.decode())['file_cycles'],5)
        for bad in (FILES,prefix+FILES,FILES+suffix,good+suffix,good+prefix,good.rstrip(b'\n'),good.replace(b'clean=yes',b'clean=NO')):
            self.assertIsNone(battery_app_files_result(bad.decode()))
        for line in good.decode().splitlines():self.assertFalse(needs_attention(line),line)

    def test_incomplete_worker_document_or_lifecycle(self):
        for before,after in ((b'cycles=5',b'cycles=4'),(b'pending=0',b'pending=1'),
            (b'descendants=3',b'descendants=2'),(b'revoked=yes',b'revoked=NO'),
            (b'bytes=2\n',b'bytes=1\n'),(b'status=0',b'status=1'),
            (b'hex=4141',b'hex=4241'),(b'caret=0',b'caret=2'),
            (b'file_worker=enabled',b'file_worker=disabled'),(b'reads=60 first_gen=1 last_gen=24',b'reads=60 first_gen=1 last_gen=1')):
            with self.subTest(before=before):
                with self.assertRaises(AssertionError):check_files_capture(FILES.replace(before,after))
        for prefix in (b'battery-app-files:',b'text-files:',b'desktop-battery:',b'desktools: file-result'):
            line=next(r for r in FILES.splitlines(keepends=True) if r.startswith(prefix))
            with self.assertRaises(AssertionError):check_files_capture(FILES.replace(line,b'',1))
            with self.assertRaises(AssertionError):check_files_capture(FILES+line)

if __name__=='__main__':unittest.main()
