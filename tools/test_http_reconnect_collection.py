"""Explicit selected-session transport profile; no native acceptance inferred."""
import tempfile,unittest
from pathlib import Path
import hardware_desktop as d
import wifi_http_reconnect5_log as c
class Tests(unittest.TestCase):
 def test_explicit_identity_and_mutual_exclusion(self):
  for kwargs in ({},{'cal5_cleanup_capture':True},{'radio_capture':('session-0000000000000001-00','0123456789abcdef')},{'radio_capture':('session-0000000000000001-00','0123456789abcdef'),'cal5_cleanup_capture':True,'http5_capture':True},{'radio_capture':('session-0000000000000001-00','0123456789abcdef'),'cal5_cleanup_capture':True,'reconnect5_capture':True}):
   with self.subTest(kwargs=kwargs),self.assertRaises(ValueError):d.collect(Path('/not-a-workspace'),'12345678',Path('/never-created'),http_reconnect5_capture=True,**kwargs)
 def test_exact_snapshots_and_wrong_identity_refuse_before_output(self):
  session='session-0000000000000001-00';release='0123456789abcdef';volume='12345678'
  tokens=' '.join(sorted(c.BOOT_FIXED|{'srvros.desktop-release='+release,'srvros.workspace-volume='+volume}))
  content='cmdline: '+tokens+'\n'+c.BOOT+'\nlaptop-capture: boot options='+tokens+'\n'
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);workspace=root/'srvros-work';logs=workspace/'Logs'/session;logs.mkdir(parents=True)
   (workspace/'.workspace').write_text('SRVROS-WORKSPACE-V1 '+volume+'\n');(logs/'boot.log').write_text(content)
   kwargs=dict(radio_capture=(session,release),cal5_cleanup_capture=True,http_reconnect5_capture=True)
   with self.assertRaises(ValueError):d.collect(root,volume,root/'out',**kwargs)
   self.assertFalse((root/'out').exists())
   (logs/'exit.log').write_text(content.replace('srvros.wifi-reconnect5-http=1','srvros.wifi-http5=1'))
   with self.assertRaises(ValueError):d.collect(root,volume,root/'out',**kwargs)
   self.assertFalse((root/'out').exists())
   (logs/'exit.log').write_text(content)
   result=d.collect(root,volume,root/'out',**kwargs)
   self.assertTrue((root/'out').exists());self.assertIsNotNone(result)
