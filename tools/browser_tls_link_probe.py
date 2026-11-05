#!/usr/bin/env python3
"""Check GOT placement and PT_TLS coverage without rebuilding the SDK."""
import hashlib,json,os,shutil,signal,struct,subprocess,sys,tempfile,time
from browser_evidence import verify_runtime
from browser_jsc_elf import inspect_target_elf
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]; BASE=ROOT/'build/browser-tls-got';BASE.mkdir(parents=True,exist_ok=True)
run=Path(tempfile.mkdtemp(prefix='proof-',dir=BASE)); sdk=ROOT/'build/sysroot/srvros';zig=ROOT/'build/tooling/zig/zig';source=ROOT/'tools/test_browser_tls_got.c';script=ROOT/'userspace/app.ld'
receipt={'result':'failed','scope':'static ELF/linker regression; no guest execution','commands':[],'cases':[]}
env=os.environ.copy()
for key in ('CC','CXX','CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','LIBS','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','SDKROOT','ZIG_LIB_DIR','ZIG_LOCAL_CACHE_DIR','ZIG_GLOBAL_CACHE_DIR'):
 env.pop(key,None)
env['ZIG_LOCAL_CACHE_DIR']=str(run/'zig-cache');env['ZIG_GLOBAL_CACHE_DIR']=str(run/'zig-global-cache')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def cmd(name,args):
 if shutil.disk_usage(BASE).free<2*1024**3:raise RuntimeError('2 GiB reserve reached')
 args=list(map(str,args));start=time.monotonic()
 with (run/(name+'.log')).open('w') as log:
  log.write(json.dumps(args)+'\n');log.flush();p=subprocess.Popen(args,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
  try:code=p.wait(timeout=60)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait();code=None
 receipt['commands'].append({'name':name,'argv':args,'status':code,'seconds':round(time.monotonic()-start,3)})
 if code!=0:raise RuntimeError(name+' failed')
def inspect(path):
 b=path.read_bytes()
 if b[:6]!=b'\x7fELF\x02\x01':raise ValueError('expected little-endian ELF64')
 e=struct.unpack_from('<16sHHIQQQIHHHHHH',b);sections=[struct.unpack_from('<IIQQQQIIQQ',b,e[6]+i*e[11]) for i in range(e[12])]
 st=sections[e[13]];names=b[st[4]:st[4]+st[5]]
 def name(s):return names[s[0]:].split(b'\0',1)[0].decode()
 sec={name(s):s for s in sections}; tls=[struct.unpack_from('<IIQQQQQQ',b,e[5]+i*e[9]) for i in range(e[10]) if struct.unpack_from('<I',b,e[5]+i*e[9])[0]==7]
 if len(tls)!=1:raise ValueError('expected one PT_TLS')
 p=tls[0];start=p[3];memend=start+p[6];fileend=start+p[5]; parts=[]
 for s in sections:
  if s[2]&0x400 and s[5]:
   parts.append({'name':name(s),'address':s[3],'size':s[5],'type':s[1]})
 covered=all(s['address']>=start and s['address']+s['size']<=memend and (s['type']==8 or s['address']+s['size']<=fileend) for s in parts)
 got=sec.get('.got');gotsize=got[5] if got else 0
 return {'tls_vaddr':start,'tls_filesz':p[5],'tls_memsz':p[6],'tls_align':p[7],'sections':parts,'got_size':gotsize,'covered':covered,'within_kernel_tls_limit':p[6]<=4096,'got_before_tls':bool(got and got[3]+got[5]<=start)}
try:
 receipt['source_observation_only']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True,timeout=15).strip()
 receipt['dirty_observation_only']=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True,timeout=15)
 receipt['ports_head']=subprocess.check_output(['git','-C','ports','rev-parse','HEAD'],cwd=ROOT,text=True,timeout=15).strip()
 receipt['runtime_provenance']=verify_runtime()
 receipt['sdk_headers_sha256']=receipt['runtime_provenance']['sdk_headers_sha256']
 if sha(script)!=sha(sdk/'lib/app.ld'):raise ValueError('source app.ld and SDK app.ld differ')
 for p in (zig,sdk/'lib/crt0.o',sdk/'lib/libsrvros.a',source,script):
  if not p.is_file():raise ValueError('missing '+str(p))
 text=script.read_text();block='''    /* Keep LLD's GOT out of the TLS program header, including when .tdata
       is empty and only zero-initialized TLS is present. */
    .got : ALIGN(0x8)
    {
        *(.got .got.* .igot .igot.*)
    } :data

'''
 if text.count(block)!=1:raise ValueError('expected explicit GOT block not found')
 legacy=run/'legacy-app.ld';legacy.write_text(text.replace(block,''))
 for mode in ('zero','initialized'):
  obj=run/(mode+'.o');flags=['-DINITIALIZED_TLS'] if mode=='initialized' else []
  cmd('compile-'+mode,[zig,'cc','-target','x86_64-freestanding-none','-ffreestanding','-fno-sanitize=undefined','-fno-stack-protector','-mno-red-zone','-I',sdk/'include',*flags,'-c',source,'-o',obj])
  for label,ld in [('legacy',legacy),('fixed',script)]:
   elf=run/(mode+'-'+label+'.elf')
   cmd('link-'+mode+'-'+label,[zig,'ld.lld','-m','elf_x86_64','-nostdlib','-static','--no-relax','-z','max-page-size=0x1000','--gc-sections','-T',ld,sdk/'lib/crt0.o',obj,sdk/'lib/libsrvros.a','-o',elf])
   result=inspect(elf);result.update(mode=mode,layout=label,elf=str(elf.relative_to(ROOT)),sha256=sha(elf));receipt['cases'].append(result)
   try:
    result['elf_validation']=inspect_target_elf(elf)
    result['elf_validation_passed']=True
   except ValueError as exc:
    result['elf_validation_passed']=False;result['elf_validation_error']=str(exc)
   if label=='legacy' and result['elf_validation_passed']:raise ValueError('legacy unexpectedly passed independent ELF validator')
   if label=='fixed' and not result['elf_validation_passed']:raise ValueError('fixed failed independent ELF validator')
   if result['got_size']==0:raise ValueError('fixture failed to produce a GOT entry')
   if label=='legacy' and result['covered']:raise ValueError('legacy negative control unexpectedly covers TLS')
   if label=='fixed' and not (result['covered'] and result['got_before_tls'] and result['within_kernel_tls_limit']):raise ValueError('fixed TLS coverage or GOT placement failed')
 receipt['sha256']={str(p.relative_to(ROOT)):sha(p) for p in (Path(__file__).resolve(),ROOT/'tools/browser_jsc_elf.py',ROOT/'tools/browser_evidence.py',source,script,zig,sdk/'lib/app.ld',sdk/'lib/crt0.o',sdk/'lib/libsrvros.a',ROOT/'build/browser-runtime/receipt.json')}
 receipt['result']='passed'
except (OSError,RuntimeError,ValueError,KeyError,struct.error,subprocess.SubprocessError) as e:receipt['error']=str(e)
finally:
 (run/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print('TLS linker receipt:',run/'receipt.json','result:',receipt['result'])
sys.exit(receipt['result']!='passed')
