"""Arithmetic/coverage from documented register values, never a GPU grant."""
import json,os,shlex,struct,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from fixture_compile import recipe,compile_fixture
if os.name!='nt':
 import resource
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
ROOT=Path(__file__).resolve().parents[1]
CORE='kernel/diagnostics/intel_bdw_primary_range.c'
TARGET='build/tests/intel-bdw-primary-range'
FIXTURE=ROOT/'tools/fixtures/intel-bdw-primary-air72.json'

def packet(data):
 values=[]
 for sample in data['samples']:
  values.extend(sample['source'])
  values.extend(v for row in sample['pipe'] for v in row)
  values.extend(v for row in sample['reader'] for v in row)
 return struct.pack('<I4Q3I138I',0x50524931,*[data[k] for k in ('aperture_bytes','ggtt_bytes','fence_begin','fence_end','first','pages')],len(data['samples']),*values)

def raw(binary,data):
 p=subprocess.run([str(binary),'--raw'],input=packet(data),capture_output=True,timeout=30,check=True)
 return json.loads(p.stdout)

class Primary(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':compile_fixture(TARGET)

 def test_geometry_control_padding_capacity_and_whole_fence_matrix(self):
  p=subprocess.run([str(ROOT/TARGET)],capture_output=True,text=True,timeout=30,check=True)
  self.assertIn('PASS cases=',p.stdout);print(p.stdout.strip(),flush=True)

 def test_native_raw_replay_has_no_live_ownership_meaning(self):
  data=json.loads(FIXTURE.read_text());r=raw(ROOT/TARGET,data)
  self.assertTrue(r['classified']);self.assertEqual(r['reason'],0)
  self.assertEqual((r['width'],r['height'],r['stride'],r['tiled']),(1440,900,6144,1))
  self.assertEqual((r['begin'],r['end'],r['pte_end']),(0,5554176,6078464))
  self.assertEqual((r['candidate_begin'],r['candidate_end']),(0x2200000,0x2204000))
  for n in range(2):
   bad=json.loads(FIXTURE.read_text());bad['samples'][n]['reader'][0][2]=4096
   self.assertFalse(raw(ROOT/TARGET,bad)['classified'])
  # Pixels and the translation tail fit far before the complete fence ends.
  # That remaining gap is still protected; the classifier may not allocate it.
  data['first']=1484
  self.assertFalse(raw(ROOT/TARGET,data)['classified'])

 def test_bad_packets_do_not_become_classifications(self):
  data=packet(json.loads(FIXTURE.read_text()))
  for bad in (data[:-1],data+b'x',b'XXXX'+data[4:]):
   p=subprocess.run([str(ROOT/TARGET),'--raw'],input=bad,capture_output=True,timeout=30)
   self.assertNotEqual(p.returncode,0)

 def test_fatal_sanitizers(self):
  cc,flags,sources=recipe(TARGET)
  with tempfile.TemporaryDirectory(prefix='bdw-primary-san-') as directory:
   binary=Path(directory)/'fixture'
   compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
   with patch.dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
    subprocess.run([str(binary)],capture_output=True,check=True,timeout=30)
    self.assertTrue(raw(binary,json.loads(FIXTURE.read_text()))['classified'])

 def test_compiled_controls_cover_footprint_and_readers(self):
  original=(ROOT/CORE).read_text()
  changes={
   'lost-translation-tail':('128*4096ull','0ull'),
   'lost-surface-alignment':('r[1]&0x3ffff','r[1]&4095'),
   'unprotected-padding':('candidate<in->fence_end &&','false &&'),
   'missing-programmed-live':('r[1]!=r[2]','false'),
   'missing-potential-eye-join':('r[3]!=r[1] || r[4]!=r[1]','false'),
   'missing-reader-stability':('s->reader[p][i]!=old->reader[p][i]','false'),
   'missing-capture-stability':('s->pipe[p][i]!=old->pipe[p][i]','false'),
   'missing-shared-control':('r[0]!=c[2]','false'),
   'ignored-sprite':('r[5] || r[8]','r[5]'),
   'ignored-cursor':('|| r[5] || r[8]','|| r[8]'),
   'unpadded-tile-rows':('tiled?((uint64_t)height+7)&~7ull:height','height'),
   'unbounded-stride':('stride>32768','false'),
  }
  with tempfile.TemporaryDirectory(prefix='bdw-primary-controls-') as directory:
   for name,(old,new) in changes.items():
    with self.subTest(control=name):
     self.assertEqual(original.count(old),1)
     source=Path(directory)/(name+'.c');source.write_text(original.replace(old,new))
     binary=Path(directory)/name
     compile_fixture(TARGET,output=binary,declaration=recipe(TARGET),replacements=[CORE+'='+str(source)])
     p=subprocess.run([str(binary)],capture_output=True,timeout=30)
     self.assertNotEqual(p.returncode,0,'Undetected weakening '+name)

 def test_native_frame_and_no_image_caller(self):
  plans=subprocess.check_output(['make','-Bn','build/kernel/srvros.elf','build/intel-gt-arena/srvros.elf'],cwd=ROOT,text=True,timeout=120)
  self.assertNotIn(CORE,plans)
  plan=subprocess.check_output(['make','-Bn','build/intel-command-memory/owner.o'],cwd=ROOT,text=True,timeout=120)
  command=next(shlex.split(l) for l in plan.splitlines() if '-c kernel/diagnostics/intel_command_memory.c ' in l)
  with tempfile.TemporaryDirectory(prefix='bdw-primary-frame-') as directory:
   args=[a for a in command if a not in ('-MMD','-MP')]
   args[args.index('-c')+1]=CORE;args[args.index('-o')+1]=str(Path(directory)/'range.o')
   subprocess.run(args,cwd=ROOT,check=True,capture_output=True,timeout=180)

if __name__=='__main__':unittest.main()
