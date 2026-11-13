#!/usr/bin/env python3
"""Replay the actual C scheduler formatter and reject incomplete/crossed context."""
import os,shlex,subprocess,tempfile,unittest
from pathlib import Path
from launch_gate_trace import launch_gate_trace,FIELDS
ROOT=Path(__file__).resolve().parents[1]

class LaunchGateTraceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        folder=ROOT/'build/tests';folder.mkdir(parents=True,exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(prefix='launch-gate-wire-',dir=folder);binary=Path(cls.temp.name)/'wire'
        command=shlex.split(os.environ.get('CC','cc'))+['-DSRVROS_LAUNCH_TRACE_WIRE','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-Ishared/include','-Ikernel/include','-I'+os.environ.get('SRVROS_LIMINE_INCLUDE','build/limine-protocol/include'),'tools/test_io_scheduler.c','-Wl,--gc-sections','-o',str(binary)]
        subprocess.run(command,cwd=ROOT,check=True,capture_output=True)
        cls.raw=subprocess.check_output([str(binary)],cwd=ROOT)
        cls.rows=[x for x in cls.raw.splitlines(keepends=True) if x.startswith(b'launch-gate-')]
        assert len(cls.rows)==3
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def test_actual_formatter_and_overlap(self):
        d=launch_gate_trace([self.raw,self.raw]);self.assertEqual(d['exec']['reason'],'mappings');self.assertEqual(d['exec']['record']['mappings'],5000)
        self.assertEqual(d['fcntl']['command'],'SETFL');self.assertEqual(d['summary']['entries'],1);self.assertEqual(d['summary']['fcntl_calls'],3)
        self.assertNotIn(b'3735928559',b''.join(self.rows))
    def test_missing_rows_duplicate_and_truncated(self):
        for rows in [[],self.rows[1:],self.rows[:2],self.rows[::2],self.rows+[self.rows[0]],[b''.join(self.rows)[:-1]]]:
            with self.subTest(rows=len(rows)),self.assertRaises(ValueError):launch_gate_trace([b''.join(rows)])
    def test_malformed_fields_and_bindings(self):
        changes=[(b'version=1',b'version=2'),(b'final=1',b'final=0'),(b'invalid=0',b'invalid=1'),(b'decisions=1',b'decisions=2'),(b'mappings=5000',b'mappings=1'),(b'elapsed_us=600000',b'elapsed_us=600001'),(b'reason=3',b'reason=11'),(b'atomic=0',b'atomic=6'),(b'request_mask=0',b'request_mask=64'),(b'owner_thread=202',b'owner_thread=101'),(b'snapshot_us=2000000',b'snapshot_us=1'),(b'arg=1',b'arg=18446744073709551616')]
        original=b''.join(self.rows)
        for old,new in changes:
            self.assertIn(old,original)
            with self.subTest(old=old),self.assertRaises(ValueError):launch_gate_trace([original.replace(old,new,1)])
    def test_pointer_redaction_and_unknown_commands(self):
        raw=b''.join(self.rows)
        for command in [1,3,5,6,7,999]:
            bad=raw.replace(b'command=4 scalar=1 arg=1',f'command={command} scalar=0 arg=1'.encode())
            with self.subTest(command=command),self.assertRaises(ValueError):launch_gate_trace([bad])
            good=bad.replace(b'scalar=0 arg=1',b'scalar=0 arg=0');self.assertEqual(launch_gate_trace([good])['fcntl']['record']['arg'],0)
    def test_bounded_wire(self):
        maximum=sum(len('launch-gate-'+kind+': ')+len(' '.join(k+'='+('9'*20) for k in fields))+1 for kind,fields in FIELDS.items())
        self.assertLessEqual(maximum,2048)
        self.assertLessEqual(len(b''.join(self.rows)),maximum)
    def test_no_long_wait_is_explicit(self):
        summary=self.rows[0].replace(b'exec_waits=2',b'exec_waits=0').replace(b'fcntl_waits=1',b'fcntl_waits=0')
        d=launch_gate_trace([summary]);self.assertIsNone(d['exec']);self.assertIsNone(d['fcntl'])
    def test_epoch_and_legacy_cross(self):
        summary=self.rows[0];epoch=summary.split(b'epoch=')[1].split()[0]
        bad=self.rows[1].replace(b'epoch='+epoch,b'epoch=9999');self.assertRaises(ValueError,launch_gate_trace,[summary+bad+self.rows[2]])
        legacy=b'io-gate: version=1 snapshot_us=2000000 pid=1 thread=101 syscall=54 initial_owner_pid=2 initial_owner_thread=202 initial_owner_seq=8 start_us=100000 end_us=700000 elapsed_us=600000\n'
        self.assertRaises(ValueError,launch_gate_trace,[b''.join(self.rows)+legacy])
        launch_gate_trace([b''.join(self.rows)+legacy.replace(b'initial_owner_seq=8',b'initial_owner_seq=7')])
if __name__=='__main__':unittest.main()
