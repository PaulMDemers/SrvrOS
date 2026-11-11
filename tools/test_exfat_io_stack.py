#!/usr/bin/env python3
"""Per-pass completed I/O through the actual filesystem and block-cache code.

Synthetic 4/128KiB-cluster images, 512-byte device blocks and the existing16KiB
command cap. Source clocks/scheduler are host models; no native speed claim.
"""
import argparse,hashlib,json,os,platform,re,shlex,subprocess
from pathlib import Path
from exfat_test_image import check_clean_image
from test_storage_leases import empty_image
from test_populated_save import populated_seed,logical_files
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build/review/exfat-io-stack')
p.add_argument('--pass-count',type=int,choices=[2,3],default=3)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sources=['tools/test_exfat_io_stack.c','kernel/src/fs/exfat.c','kernel/src/vfs.c','kernel/src/autotest_storage.c']
deps=sources+['kernel/src/fs/exfat_stream.inc','kernel/src/block.c','kernel/src/block_claim.c','tools/exfat_host_driver.c','tools/test_exfat_io_stack.py']
source_hash={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in deps}
variants=[('before',['-DSRVROS_EXFAT_BITMAP_CLUSTER_READS','-DSRVROS_EXFAT_FAT_WINDOW_OFF','-DSRVROS_EXFAT_PREFIX_CACHE_ON']),('bitmap',['-DSRVROS_EXFAT_FAT_WINDOW_OFF','-DSRVROS_EXFAT_PREFIX_CACHE_ON']),('current',[])][:a.pass_count]
record=dict(status='RUNNING',scope=__doc__,commands=[],cases=[]);drivers={}
for name,flags in variants:
 cmd=shlex.split(os.environ.get('CC','cc'))
 if platform.system()=='Darwin':cmd+=['-arch','x86_64']
 cmd+=['-DSRVROS_EXFAT_HOST_TEST','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I',str(ROOT/'shared/include'),'-I',str(ROOT/'kernel/include'),*flags,*[str(ROOT/n) for n in sources],'-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections','-o',str(out/name)]
 subprocess.run(cmd,check=True);drivers[name]=out/name;record['commands'].append(cmd)
f=empty_image(32*1024*1024,cluster_shift=3);f.add_directory_files('work',{'anchor.txt':b'unchanged','other.txt':b''})
for label,seed,block in [('4k',bytes(f.data),512),('populated-128k',populated_seed(),512),('populated-128k-4k-device',populated_seed(),4096)]:
 for mode in ['growth','replace','metadata']:
  rows={}
  for name,driver in drivers.items():
   image=out/'work.exfat';image.write_bytes(seed)
   q=subprocess.run([str(driver),str(image),mode],capture_output=True,text=True,timeout=180,env={**os.environ,'SRVROS_TEST_BLOCK_BYTES':str(block)})
   (out/f'{label}-{mode}-{name}.log').write_text(q.stdout+q.stderr)
   assert q.returncode==0,(label,mode,name,q.stderr)
   summary,=[x for x in q.stdout.splitlines() if x.startswith('io-stack:')]
   fields={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',summary)}
   raw=image.read_bytes();check_clean_image(image)
   rows[name]=dict(io=fields,sha256=hashlib.sha256(raw).hexdigest(),files=logical_files(raw))
  ref=rows['before'];prev=ref
  for name in list(drivers)[1:]:
   cur=rows[name]
   assert cur['sha256']==ref['sha256'] and cur['files']==ref['files'],(label,mode,name,'media differs')
   for key in ['writes','write_bytes','flushes']:assert cur['io'][key]==ref['io'][key],(label,mode,name,key,ref['io'],cur['io'])
   if name=='bitmap':
    assert cur['io']['reads']<=prev['io']['reads'] and cur['io']['read_bytes']<=prev['io']['read_bytes']
    assert cur['io']['heap_peak_delta']<=prev['io']['heap_peak_delta']
   if name=='current':assert cur['io']['reads']<=prev['io']['reads'] and cur['io']['read_bytes']<=prev['io']['read_bytes']
   prev=cur
  record['cases'].append(dict(geometry=label,mode=mode,variants=rows))
  print('io-stack:',label,mode,{n:v['io'] for n,v in rows.items()},flush=True)
assert source_hash=={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in deps}
assert sum(x['variants']['bitmap']['io']['read_bytes'] for x in record['cases'])<sum(x['variants']['before']['io']['read_bytes'] for x in record['cases'])
if a.pass_count==3:
 assert sum(x['variants']['current']['io']['reads'] for x in record['cases'])<sum(x['variants']['bitmap']['io']['reads'] for x in record['cases'])
record.update(status='PASS' if a.pass_count==3 else 'PASS_BITMAP_ONLY',source_sha256=source_hash)
(out/'result.json').write_text(json.dumps(record,indent=2)+'\n')
