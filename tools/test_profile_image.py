#!/usr/bin/env python3
import copy
import hashlib
import base64
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import profile_bundle as p
import profile_image as image


def bundle():
    data=b'portable data'
    return {'format':p.FORMAT,'schema_version':p.VERSION,
            'profile':{'id':'12345678-1234-4abc-8def-123456789abc','display_name':'A'},
            'preferences':{'text_size':16},
            'documents':[{'path':'Documents/notes.txt','size':len(data),
                          'sha256':hashlib.sha256(data).hexdigest(),
                          'data_base64':base64.b64encode(data).decode()}]}


class ImageTests(unittest.TestCase):
    def setUp(self):
        base=Path(__file__).resolve().parents[1] / "build" / "review" / "profile-image"
        base.mkdir(parents=True, exist_ok=True)
        self.tmp=tempfile.TemporaryDirectory(dir=base)
        self.root=Path(self.tmp.name)
        self.source=self.root/'source.img'; self.source.write_bytes(b'S'*1024)
        self.target=self.root/'target.img'
        self.calls=[]
    def tearDown(self): self.tmp.cleanup()
    def run_driver(self,args,check):
        self.assertTrue(check)
        self.calls.append(tuple(args[2:4]))
        if len(args)==5:
            self.assertTrue(Path(args[4]).is_file())
    def provision(self, **kw):
        args=dict(source_exfat=self.source,target_exfat=self.target,bundle=bundle(),home='/fat/home/alice',
                  alias='alice',uid=1000,gid=1000,driver='/fake/driver')
        args.update(kw)
        return image.provision(**args)
    def test_publish_and_source_preserved(self):
        with patch('profile_image.subprocess.run',side_effect=self.run_driver):
            result=self.provision()
        self.assertEqual(self.source.read_bytes(),b'S'*1024)
        self.assertEqual(self.target.read_bytes(),b'S'*1024)
        self.assertEqual(result['home'],'/fat/home/alice')
        self.assertIn(('write','/fat/home/alice/profile.id'),self.calls)
        self.assertIn(('write','/fat/home/alice/Documents/notes.txt'),self.calls)
        self.assertEqual(list(self.root.glob('.profile-image-*')),[])
    def test_invalid_before_copy(self):
        for changes in ({'home':'/fat/home/../bad'}, {'home':'/fat/home/'+'a'*86},
                        {'alias':'bad alias'}, {'uid':True}, {'machine_preferences':b'bad'}):
            with self.subTest(changes=changes), patch('profile_image.subprocess.run') as run:
                with self.assertRaises(p.ProfileError): self.provision(**changes)
                run.assert_not_called()
                self.assertFalse(self.target.exists())
        bad=copy.deepcopy(bundle());bad['documents'][0]['sha256']='bad'
        with self.assertRaises(p.ProfileError): self.provision(bundle=bad)
        self.assertEqual(self.source.read_bytes(),b'S'*1024)
    def test_expanded_path_bound(self):
        bad=copy.deepcopy(bundle())
        bad['documents'][0]['path']='Documents/' + '/'.join(['a'*60]*3)
        with self.assertRaises(p.ProfileError): self.provision(bundle=bad)
        self.assertFalse(self.target.exists())
    def test_driver_failure_removes_stage(self):
        with patch('profile_image.subprocess.run',side_effect=RuntimeError('driver failed')):
            with self.assertRaises(RuntimeError): self.provision()
        self.assertFalse(self.target.exists())
        self.assertEqual(self.source.read_bytes(),b'S'*1024)
        self.assertEqual(list(self.root.glob('.profile-image-*')),[])
    def test_existing_target_never_overwritten(self):
        self.target.write_bytes(b'existing')
        with self.assertRaises(p.ProfileError): self.provision()
        self.assertEqual(self.target.read_bytes(),b'existing')
    def test_nonregular_source_refused(self):
        with self.assertRaises((p.ProfileError,OSError)): self.provision(source_exfat=self.root)
        self.assertFalse(self.target.exists())

if __name__=='__main__': unittest.main()
