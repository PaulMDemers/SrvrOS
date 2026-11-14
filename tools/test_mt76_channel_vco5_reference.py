import hashlib,json,os,shutil,tempfile,unittest
from pathlib import Path
from mt76_channel_vco5_reference import audit
REFERENCE=os.environ.get('SRVROS_MT76_REFERENCE')
@unittest.skipUnless(REFERENCE,'pinned local source reference not supplied')
class ReferenceTests(unittest.TestCase):
 def test_pins_and_projection(self):
  r=audit(Path(REFERENCE));self.assertEqual(r['channel_vco_operations'],77);self.assertFalse(r['hardware_operations'])
 def test_corruption(self):
  original=Path(__file__).with_name('fixtures')
  with tempfile.TemporaryDirectory() as t:
   f=Path(t)/'fixtures';shutil.copytree(original,f)
   p=f/'mt7610-5ghz-channel-vco.bin';b=p.read_bytes();damaged=bytearray(b);damaged[12*6+4]^=1;p.write_bytes(damaged)
   with self.assertRaises(ValueError):audit(Path(REFERENCE),f)
   m=f/'mt7610-5ghz-channel-vco.json';metadata=json.loads(m.read_text());metadata['sha256']=hashlib.sha256(damaged).hexdigest();m.write_text(json.dumps(metadata))
   with self.assertRaises(ValueError):audit(Path(REFERENCE),f)
   p.write_bytes(b);metadata['sha256']=hashlib.sha256(b).hexdigest();metadata['original']=0xb0;m.write_text(json.dumps(metadata))
   with self.assertRaises(ValueError):audit(Path(REFERENCE),f)
