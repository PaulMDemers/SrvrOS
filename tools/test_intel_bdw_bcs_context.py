"""Externally decoded private CPU image and fail-closed codec controls."""
import resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
import os,subprocess,tempfile,unittest
from pathlib import Path
from fixture_compile import compile_fixture,recipe
ROOT=Path(__file__).resolve().parents[1]
TARGET='build/tests/intel-bdw-bcs-context'
CORE='kernel/diagnostics/intel_bdw_bcs_context.c'
class ContextImage(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  if os.environ.get('SRVROS_FIXTURE_BATCH')!='1':compile_fixture(TARGET)
 def test_image_decoder_all_word_corruption_and_invalid_geometry(self):
  p=subprocess.run([TARGET],cwd=ROOT,capture_output=True,text=True,check=True)
  self.assertIn('PASS cases=4463;',p.stdout);print(p.stdout.strip(),flush=True)
 def test_fatal_sanitizers(self):
  cc,flags,sources=recipe(TARGET)
  with tempfile.TemporaryDirectory(prefix='bcs-context-san-') as d:
   binary=Path(d)/'san'
   compile_fixture(TARGET,output=binary,declaration=(cc,[*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'],sources))
   subprocess.run([str(binary)],cwd=ROOT,check=True,capture_output=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),timeout=90)
 def test_compiled_layout_length_address_mode_and_extent_controls(self):
  original=(ROOT/CORE).read_text()
  changes={
   'wrong-lri-count':('0x11001011','0x1100100f'),
   'wrong-batch-high':('0x22168','0x22144'),
   'wrong-engine':('0x22244','0x20244'),
   'wrong-state-page':('2048','1024'),
   'wrong-address-mode':('c->context_gpu|0x0d','c->context_gpu|0x1d'),
   'ppgtt-without-root':('c->context_gpu|0x0d','c->context_gpu|0x10d'),
   'unknown-coherency':('c->context_gpu|0x0d','c->context_gpu|0x2d'),
   'partial-write':('i<BDW_BCS_IMAGE_WORDS;i++){\n  uint32_t value','i<BDW_BCS_IMAGE_WORDS-1;i++){\n  uint32_t value'),
   'partial-verify':('i<BDW_BCS_IMAGE_WORDS;i++){\n  uint32_t actual','i<BDW_BCS_IMAGE_WORDS-1;i++){\n  uint32_t actual'),
   'without-full-fence':('!separate(p[i],n[i],c->fence_begin,c->fence_end-c->fence_begin)','false'),
   'without-extent':('p[i]+n[i]>c->aperture_bytes','false'),
  }
  with tempfile.TemporaryDirectory(prefix='bcs-context-controls-') as d:
   for name,(old,new) in changes.items():
    self.assertIn(old,original);source=Path(d)/(name+'.c');source.write_text(original.replace(old,new));binary=Path(d)/name
    compile_fixture(TARGET,output=binary,declaration=recipe(TARGET),replacements=[CORE+'='+str(source)])
    p=subprocess.run([str(binary)],cwd=ROOT,capture_output=True,timeout=60)
    self.assertNotEqual(p.returncode,0,name)
  print('Compiled context-image controls',len(changes),flush=True)
