#!/usr/bin/env python3
"""Build a current-SDK signal boundary probe; optional guest runs explicitly."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
from browser_evidence import verify_runtime

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/browser-runtime'
OUT.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--guest',action='store_true',help='after link, run combined ICU and signal guest checks')
args=parser.parse_args()
run=Path(tempfile.mkdtemp(prefix='signal-',dir=OUT))
receipt={'result':'failed','scope':'matched SDK compile/link only; guest result separate','commands':[],'sha256':{}}
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()
def command(name,argv,timeout=60):
    argv=list(map(str,argv));start=time.monotonic();log_path=run/(name+'.log')
    with log_path.open('w') as log:
        log.write(json.dumps(argv)+'\n');log.flush()
        try:
            proc=subprocess.Popen(argv,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:status=proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid,signal.SIGKILL);proc.wait();status=None;log.write('TIMEOUT\n')
        except OSError as exc:status=None;log.write(str(exc)+'\n')
    receipt['commands'].append({'name':name,'argv':argv,'status':status,'seconds':round(time.monotonic()-start,3),'log':str(log_path.relative_to(ROOT))})
    if status!=0:raise RuntimeError('command failed: '+str(log_path))
env=os.environ.copy()
for key in ('CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','LIBS','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','SDKROOT','MACOSX_DEPLOYMENT_TARGET','ZIG_LIB_DIR','ZIG_LOCAL_CACHE_DIR','ZIG_GLOBAL_CACHE_DIR'):
    env.pop(key,None)
env['ZIG_LOCAL_CACHE_DIR']=str(run/'zig-cache')
env['ZIG_GLOBAL_CACHE_DIR']=str(run/'zig-global-cache')
try:
    runtime=verify_runtime()
    src=ROOT/'tools/test_browser_signal_contract.c'; elf=OUT/'signal-contract.elf';sdk=ROOT/'build/sysroot/srvros'
    command('guest-link',[ROOT/'ports/srvros/webkit/cc','-std=c11','-fno-builtin',src,'-o',elf])
    paths=[Path(__file__).resolve(),src,ROOT/'ports/srvros/webkit/cc',ROOT/'ports/srvros/webkit/compiler.py',ROOT/'build/tooling/zig/zig',sdk/'lib/libsrvros.a',sdk/'lib/crt0.o',sdk/'lib/app.ld',OUT/'receipt.json',elf]
    receipt['sha256']={str(p.relative_to(ROOT)):sha(p) for p in paths}
    receipt['sdk_headers_sha256']=runtime['sdk_headers_sha256'];receipt['runtime_provenance']=runtime
    receipt['guest_marker']='browser-signal-contract: siginfo rejected; handler preserved'
    receipt['result']='success'
except (OSError,KeyError,ValueError,RuntimeError) as exc:
    receipt['error']=str(exc)
finally:
    data=json.dumps(receipt,indent=2)+'\n';(run/'receipt.json').write_text(data);(OUT/'signal-receipt.json').write_text(data)
    print('Signal receipt:',OUT/'signal-receipt.json','result:',receipt['result'])
if receipt['result']!='success':sys.exit(1)
if args.guest:
    sys.exit(subprocess.run([sys.executable,str(ROOT/'tools/browser_runtime_smoke.py'),'--icu','--signal-contract'],cwd=ROOT).returncode)
