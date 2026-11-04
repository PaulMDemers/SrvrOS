"""Fault attachment must stay on a distinct, fixture-owned work image."""
import argparse
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import installed_disk_smoke as proof

class NeverLaunch(Exception): pass

class FaultScopeTests(unittest.TestCase):
    def check(self,kind):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);out=root/'owned';out.mkdir()
            system=out/'system.img';work=out/'work.img';cfg=out/'work.conf';vars=root/'vars.fd'
            for path in (system,work,cfg,vars):path.write_bytes(b'fixture')
            args=argparse.Namespace(ovmf_vars=str(vars),ovmf='firmware',qemu='not-launched')
            number=1;dirty=True
            if kind=='same':work=system
            if kind=='outside-work':work=root/'outside.img'
            if kind=='outside-system':system=root/'outside.img'
            if kind=='outside-config':cfg=root/'outside.conf'
            if kind=='ordinary':dirty=False
            if kind=='second':number=2
            with patch.object(proof,'choose_port',return_value=34567),patch.object(proof.subprocess,'Popen',side_effect=NeverLaunch) as launch:
                expected=NeverLaunch if kind=='valid' else RuntimeError
                with self.assertRaises(expected):
                    proof.boot(system,out,number,args,other_disk=work,dirty_salvage=dirty,work_fault_config=cfg)
                if kind=='valid':
                    command=launch.call_args.args[0]
                    drives=[command[i+1] for i,v in enumerate(command) if v=='-drive']
                    injected=[d for d in drives if 'blkdebug:' in d]
                    self.assertEqual(len(injected),1)
                    self.assertIn(str(work),injected[0]);self.assertNotIn(str(system),injected[0])
                    self.assertIn('werror=report',injected[0])
                else:launch.assert_not_called()
    def test_distinct_owned_target(self):self.check('valid')
    def test_same_disk_refused(self):self.check('same')
    def test_external_work_refused(self):self.check('outside-work')
    def test_external_system_refused(self):self.check('outside-system')
    def test_external_config_refused(self):self.check('outside-config')
    def test_normal_boot_refused(self):self.check('ordinary')
    def test_second_boot_refused(self):self.check('second')

if __name__=='__main__':unittest.main()
