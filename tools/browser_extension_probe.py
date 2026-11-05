#!/usr/bin/env python3
"""Build matched C++ linkage and algorithm probes; no guest execution."""
import hashlib,json,os,signal,subprocess,sys,tempfile,time
from pathlib import Path
from browser_evidence import verify_runtime
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/browser-runtime';OUT.mkdir(parents=True,exist_ok=True)
run=Path(tempfile.mkdtemp(prefix='extensions-',dir=OUT));receipt={'result':'failed','scope':'compile/link only; no guest acceptance','commands':[]}
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
env=os.environ.copy()
for key in ('CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','LIBS','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','SDKROOT','ZIG_LIB_DIR','ZIG_LOCAL_CACHE_DIR','ZIG_GLOBAL_CACHE_DIR'):env.pop(key,None)
env['ZIG_LOCAL_CACHE_DIR']=str(run/'zig-cache');env['ZIG_GLOBAL_CACHE_DIR']=str(run/'zig-global-cache')
def command(name,argv):
 argv=list(map(str,argv));t=time.monotonic()
 with (run/(name+'.log')).open('w') as f:
  f.write(json.dumps(argv)+'\n');f.flush();p=subprocess.Popen(argv,cwd=ROOT,env=env,stdout=f,stderr=subprocess.STDOUT,start_new_session=True)
  try:status=p.wait(timeout=60)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();status=None
 receipt['commands'].append({'name':name,'argv':argv,'status':status,'seconds':round(time.monotonic()-t,3)})
 if status!=0:raise RuntimeError(name+' failed; see '+str(run/(name+'.log')))
try:
 runtime=verify_runtime();fs_path=ROOT/'build/browser-filesystem/receipt.json';math_path=ROOT/'build/browser-libm/receipt.json'
 for path,required in [(fs_path,('build/browser-filesystem/libcxx-filesystem.a','ports/srvros/libcxx/algorithm_guest.cc')),(math_path,('build/browser-libm/libsrvros-jsc-math.a','build/browser-libm/libmprobe.elf'))]:
  data=json.loads(path.read_text());hashes=data['sha256']
  if data.get('result')!='success' or data.get('runtime',{}).get('sdk_headers_sha256')!=runtime['sdk_headers_sha256']:raise ValueError('mismatched dependency '+str(path))
  # The algorithm guest was added after the filesystem build; it is hashed below.
  for name in required:
   if name=='ports/srvros/libcxx/algorithm_guest.cc':continue
   if name not in hashes:raise ValueError('missing dependency '+name)
  for name,digest in hashes.items():
   p=Path(name)
   if p.is_absolute() or '..' in p.parts or not (ROOT/p).is_file() or sha(ROOT/p)!=digest:raise ValueError('stale dependency '+name)
 cxx=ROOT/'ports/srvros/webkit/cxx';fs=ROOT/'build/browser-filesystem/libcxx-filesystem.a';sdk=ROOT/'build/sysroot/srvros'
 linkage=OUT/'cpp-linkage.elf';algorithm=OUT/'algorithmprobe.elf'
 command('linkage',[cxx,'-D_LIBCPP_HAS_FILESYSTEM=1',ROOT/'tools/test_browser_cpp_linkage.cc','-o',linkage])
 command('algorithm',[cxx,'-D_LIBCPP_HAS_FILESYSTEM=1',ROOT/'ports/srvros/libcxx/algorithm_guest.cc','-Wl,--start-group',fs,OUT/'libcxx-threaded.a',OUT/'libcxxabi-rtti.a',sdk/'lib/libsrvros.a','-Wl,--end-group','-o',algorithm])
 paths=[Path(__file__).resolve(),ROOT/'tools/test_browser_cpp_linkage.cc',ROOT/'ports/srvros/libcxx/algorithm_guest.cc',cxx,ROOT/'ports/srvros/webkit/compiler.py',sdk/'lib/libsrvros.a',sdk/'include/semaphore.h',sdk/'include/regex.h',OUT/'receipt.json',fs_path,math_path,fs,OUT/'libcxx-threaded.a',OUT/'libcxxabi-rtti.a',ROOT/'build/browser-libm/libmprobe.elf',ROOT/'build/browser-libm/libsrvros-jsc-math.a',linkage,algorithm]
 receipt.update(result='success',sdk_headers_sha256=runtime['sdk_headers_sha256'],runtime=runtime,sha256={str(p.relative_to(ROOT)):sha(p) for p in paths})
except (OSError,KeyError,ValueError,RuntimeError) as e:receipt['error']=str(e)
finally:
 data=json.dumps(receipt,indent=2)+'\n';(run/'receipt.json').write_text(data);(OUT/'extension-receipt.json').write_text(data);print('Extension receipt:',OUT/'extension-receipt.json','result:',receipt['result'])
sys.exit(receipt['result']!='success')
