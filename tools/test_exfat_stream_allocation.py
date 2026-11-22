#!/usr/bin/env python3
"""Actual stream allocation: stale FAT, required terminals and NoFatChain work."""
import argparse, hashlib, json, os, platform, re, shlex, struct, subprocess
from pathlib import Path
from exfat_test_image import ExfatFixture, check_clean_image
from test_storage_leases import empty_image
from test_populated_save import logical_files
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build/review/exfat-stream-allocation')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
sources=['tools/exfat_stream_driver.c','kernel/src/fs/exfat.c','kernel/src/vfs.c','kernel/src/autotest_storage.c']
deps=sources+['kernel/src/fs/exfat_stream.inc',__file__]
hashes={n:hashlib.sha256(Path(n).read_bytes()).hexdigest() for n in deps}
drivers={};record=dict(status='RUNNING',cases=[],commands=[])
for name,flags in [('fat',['-DSRVROS_EXFAT_STREAM_CONTIGUOUS_OFF']),('candidate',[])]:
 cmd=shlex.split(os.environ.get('CC','cc'))
 if platform.system()=='Darwin':cmd+=['-arch','x86_64']
 cmd+=['-DSRVROS_EXFAT_HOST_TEST','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I',str(ROOT/'shared/include'),'-I',str(ROOT/'kernel/include'),*flags,*[str(ROOT/n) for n in sources],'-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections','-o',str(out/name)]
 subprocess.run(cmd,check=True);drivers[name]=out/name;record['commands'].append(cmd)
for shift in [3,7]:
 f=empty_image(32*1024*1024,cluster_shift=shift)
 # Ignore invalid free FAT only while NoFatChain; real chains must overwrite it.
 bits=f.offset(3)
 for c in range(2,f.count+2):
  if not f.data[bits+(c-2)//8] & (1<<((c-2)%8)):
   struct.pack_into('<I',f.data,f.fat+c*4,0xdeadbeef)
 seed=bytes(f.data)
 for size,mode in [(1,'ok'),(65537,'ok'),(131096,'ok'),(1048576,'ok'),(1048576,'fragment-write')]:
  rows={}
  for name,driver in drivers.items():
   image=out/'work.exfat';image.write_bytes(seed)
   q=subprocess.run([str(driver),str(image),str(size),mode],capture_output=True,text=True,timeout=120)
   (out/f'{shift}-{size}-{name}.log').write_text(q.stdout+q.stderr)
   assert q.returncode==0,(shift,size,name,q.stderr)
   raw=image.read_bytes();check_clean_image(image);current=ExfatFixture(raw)
   entry=dict(current.entries(current.chain(current.root)))['stream.bin']
   m=re.search(r' writes=(\d+) flushes=(\d+) PASS',q.stdout);assert m
   rows[name]=dict(writes=int(m[1]),flushes=int(m[2]),nofat=bool(entry[33]&2),files=logical_files(raw))
   assert current.read('stream.bin')==bytes((i*29+(i>>13)+17)&255 for i in range(size))
   if name=='fat':assert not rows[name]['nofat']
   if size==1 and name=='candidate':assert rows[name]['nofat']
   if mode=='fragment-write':assert not rows[name]['nofat']
  assert rows['fat']['files']==rows['candidate']['files']
  assert rows['fat']['flushes']==rows['candidate']['flushes']
  assert rows['candidate']['writes']<=rows['fat']['writes']
  if size==1:assert rows['candidate']['writes']<rows['fat']['writes']
  record['cases'].append(dict(cluster=f.cluster,size=size,mode=mode,results=rows))
  print('stream-allocation:',f.cluster,size,mode,'PASS writes',rows['fat']['writes'],'->',rows['candidate']['writes'],'flushes',rows['candidate']['flushes'],flush=True)
assert hashes=={n:hashlib.sha256(Path(n).read_bytes()).hexdigest() for n in deps}
record.update(status='PASS',source_sha256=hashes)
(out/'result.json').write_text(json.dumps(record,indent=2)+'\n')
