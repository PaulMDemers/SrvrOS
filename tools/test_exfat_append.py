#!/usr/bin/env python3
"""Aligned append: independent contents, old-prefix containment and fault cuts."""
import argparse,json,subprocess,re,struct,hashlib,os
from pathlib import Path
from test_storage_leases import empty_image
from exfat_test_image import ExfatFixture,check_clean_image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--driver',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,default=ROOT/'build/review/exfat-append');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);image=out/'work.exfat';result={};trace=False;extra_env={}
def run(driver,mode,*args,allowed=(0,)):
 q=subprocess.run([str(driver.resolve()),str(image),mode,*map(str,args)],capture_output=True,text=True,timeout=60,env={**os.environ,**extra_env,**({'SRVROS_TRACE_WRITES':'1'} if trace else {})})
 assert q.returncode in allowed,q.stdout+q.stderr
 return q
for shift in (3,8):
 image.write_bytes(empty_image(16*1024*1024,cluster_shift=shift).data)
 run(a.driver,'append-seed');seed=image.read_bytes();f=ExfatFixture(seed);old=f.read('keep.txt')
 entry=dict(f.entries(f.chain(f.root)))['keep.txt'];first=struct.unpack_from('<I',entry,52)[0];start=f.offset(first)
 rows={}
 for name,driver in [('cow',a.baseline),('append',a.driver)]:
  image.write_bytes(seed);q=run(driver,'append-repeat');data=image.read_bytes();actual=ExfatFixture(data).read('keep.txt')
  expected=bytes((i*17+37)&255 for i in range(len(old)+f.cluster*3+512));assert actual==expected
  assert ExfatFixture(data).read('anchor.txt')==b'unchanged';check_clean_image(image)
  rows[name]=dict(write_bytes=int(re.search(r'write_bytes=(\d+)',q.stdout)[1]),sha256=hashlib.sha256(actual).hexdigest())
  if name=='append':assert data[start:start+len(old)]==seed[start:start+len(old)]
 assert rows['append']['write_bytes']<rows['cow']['write_bytes'] and rows['append']['sha256']==rows['cow']['sha256'];result[f'cluster-{f.cluster}']=rows
 # A failed prefix proof must refuse without attempting replacement writes.
 image.write_bytes(seed);extra_env={'SRVROS_FAIL_READ_OFFSET':str(start)}
 refused=run(a.driver,'append-step');extra_env={}
 assert 'operation=FAIL' in refused.stdout and image.read_bytes()==seed;check_clean_image(image)
 # Changed aligned prefixes must still receive a full replacement.
 image.write_bytes(seed);run(a.driver,'append-changed');changed=ExfatFixture(image.read_bytes()).read('keep.txt')
 assert changed[0]==(old[0]^1) and changed[1:len(old)]==old[1:];check_clean_image(image)
 root_entry=dict(ExfatFixture(image.read_bytes()).entries(ExfatFixture(image.read_bytes()).chain(f.root)))['keep.txt']
 assert struct.unpack_from('<I',root_entry,52)[0]!=first
 # An old EOF inside a device sector cannot safely append through sector RMW.
 entry_at=seed.find(entry,f.offset(f.root),f.offset(f.root)+f.cluster);assert entry_at>=0
 for old_length,device_block in [(len(old)-1,512),(len(old)-512,4096)]:
  partial=ExfatFixture.checked_entry('keep.txt',first,old[:old_length],contiguous=True)
  candidate=bytearray(seed);candidate[entry_at:entry_at+len(partial)]=partial;image.write_bytes(candidate)
  extra_env={'SRVROS_TEST_BLOCK_BYTES':str(device_block)};run(a.driver,'append-step');extra_env={}
  final=ExfatFixture(image.read_bytes());after=dict(final.entries(final.chain(final.root)))['keep.txt']
  assert struct.unpack_from('<I',after,52)[0]!=first and final.read('keep.txt').startswith(old);check_clean_image(image)
 # A published alias must retain the old extent and bytes while the source grows.
 alias=ExfatFixture.checked_entry('alias.txt',first,old,contiguous=True)
 candidate=bytearray(seed);root=f.chain(f.root);end=0
 while root[end]:end+=32
 candidate[f.offset(f.root)+end:f.offset(f.root)+end+len(alias)]=alias
 image.write_bytes(candidate);run(a.driver,'append-step')
 final=ExfatFixture(image.read_bytes());assert final.read('alias.txt')==old
 assert final.read('keep.txt').startswith(old);check_clean_image(image)
 original_seed=seed
 for layout in ('contiguous','fat'):
  candidate=bytearray(original_seed);old_extents=[(start,len(old))]
  if layout=='fat':
   entry_at=candidate.find(entry,f.offset(f.root),f.offset(f.root)+f.cluster);assert entry_at>=0
   linked=ExfatFixture.checked_entry('keep.txt',first,old,contiguous=False)
   candidate[entry_at:entry_at+len(linked)]=linked
   root_bytes=f.chain(f.root);bitmap_entry=next(root_bytes[n:n+32] for n in range(0,len(root_bytes),32) if root_bytes[n]==0x81)
   bitmap=f.offset(struct.unpack_from('<I',bitmap_entry,20)[0]);spare=f.count+1
   assert not candidate[bitmap+(spare-2)//8] & (1<<((spare-2)%8))
   candidate[f.offset(spare):f.offset(spare)+f.cluster]=old[f.cluster:]
   candidate[bitmap+(spare-2)//8]|=1<<((spare-2)%8)
   candidate[bitmap+(first-1)//8]&=~(1<<((first-1)%8))
   struct.pack_into('<I',candidate,f.fat+first*4,spare)
   struct.pack_into('<I',candidate,f.fat+spare*4,0xffffffff)
   old_extents=[(start,f.cluster),(f.offset(spare),f.cluster)]
  seed=bytes(candidate)
  published_entry=dict(ExfatFixture(seed).entries(ExfatFixture(seed).chain(f.root)))['keep.txt']
  # Faults leave every old payload byte and unrelated file intact. Repair is an
  # offline checker, never a kernel recovery policy; inspect both before/after.
  image.write_bytes(seed);trace=True;q=run(a.driver,'append-step');trace=False;writes,flushes=map(int,re.search(r'operation=PASS writes=(\d+) flushes=(\d+)',q.stdout).groups())
  cuts=[];missing=[]
  entry_at=seed.find(published_entry,f.offset(f.root),f.offset(f.root)+f.cluster);assert entry_at>=0
  publication={int(m[1]) for m in re.finditer(r'write-trace: call=(\d+) offset=(\d+) bytes=(\d+)',q.stdout) if int(m[2])==entry_at}
  for kind,count in [('write-fail',writes),('write-cut',writes),('tear',writes),('flush-fail',flushes),('flush-cut',flushes)]:
   for n in range(1,count+1):
    (out/'current-cut.json').write_text(json.dumps(dict(cluster=f.cluster,kind=kind,n=n))+'\n')
    image.write_bytes(seed);args=[0]*5;index={'write-fail':0,'write-cut':1,'flush-fail':2,'flush-cut':3,'tear':4}[kind];args[index]=n
    q=run(a.driver,'append-step',*args,allowed=(0,77,78,79));raw=image.read_bytes()
    assert all(raw[off:off+size]==seed[off:off+size] for off,size in old_extents),(kind,n,'old payload changed')
    assert ExfatFixture(raw).read('anchor.txt')==b'unchanged'
    subprocess.run(['/usr/sbin/fsck.exfat','-y',str(image)],capture_output=True,text=True)
    f2=ExfatFixture(image.read_bytes())
    try:assert f2.read('keep.txt').startswith(old),(kind,n,'lost committed prefix')
    except StopIteration:missing.append([kind,n])
    assert all(image.read_bytes()[off:off+size]==seed[off:off+size] for off,size in old_extents),(kind,n,'repair changed committed payload sectors')
    assert f2.read('anchor.txt')==b'unchanged';check_clean_image(image);cuts.append([kind,n])
  result[f'cuts-{f.cluster}-{layout}']=dict(count=len(cuts),lost_name_after_offline_repair=missing,committed_payload_sectors_preserved=True)
  # The existing stream/checksum publication is not crash-atomic. Establish the
  # same limitation on COW rather than claiming append adds atomic directory I/O.
  if missing:
   assert all(kind=='tear' and n in publication for kind,n in missing),missing
   entry_at=seed.find(published_entry,f.offset(f.root),f.offset(f.root)+f.cluster);assert entry_at>=0
   image.write_bytes(seed);trace=True;base=run(a.baseline,'append-step');trace=False
   boundary=next(int(m[1]) for m in re.finditer(r'write-trace: call=(\d+) offset=(\d+) bytes=(\d+)',base.stdout) if int(m[2])==entry_at)
   image.write_bytes(seed);run(a.baseline,'append-step',0,0,0,0,boundary,allowed=(79,))
   subprocess.run(['/usr/sbin/fsck.exfat','-y',str(image)],capture_output=True,text=True)
   try:ExfatFixture(image.read_bytes()).read('keep.txt');raise AssertionError('COW publication did not reproduce lost name')
   except StopIteration:pass
   result[f'cuts-{f.cluster}-{layout}']['cow_torn_stream_same_limitation']=True
  print('append',f.cluster,'PASS',rows,'faults',len(cuts),flush=True)
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
