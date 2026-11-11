"""Independent images plus ordered logical FAT writes/barriers versus old path."""
import argparse,os,re,json,subprocess,hashlib
from pathlib import Path
from test_storage_leases import empty_image
from exfat_test_image import ExfatFixture,check_clean_image
p=argparse.ArgumentParser();p.add_argument('--driver',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);a=p.parse_args()
out=Path('build/review/exfat-link-batch');out.mkdir(parents=True,exist_ok=True);image=out/'work.exfat';results=[]
def run(driver,mode,trace=False):
 q=subprocess.run([str(driver.resolve()),str(image),mode],env={**os.environ,**({'SRVROS_TRACE_WRITES':'1'} if trace else {})},capture_output=True,text=True,check=True)
 return q.stdout
def canonical(text,f):
 rows=[]
 for line in text.splitlines():
  m=re.fullmatch(r'write-trace: call=\d+ offset=(\d+) bytes=(\d+)',line)
  if m:
   off,n=map(int,m.groups())
   if f.fat<=off and off+n<=f.fat+(f.count+2)*4:
    assert off%4==n%4==0
    rows.extend(('write',x,4) for x in range(off,off+n,4))
   else:rows.append(('write',off,n))
  elif line.startswith('flush-trace:'):rows.append(('flush',))
 return rows
for shift in (3,8):
 image.write_bytes(empty_image(16*1024*1024,cluster_shift=shift).data);run(a.driver,'append-seed');seed=image.read_bytes()
 # anchor follows keep, forcing a real FAT-chain conversion, not ignored FAT.
 f=ExfatFixture(seed);old=f.read('keep.txt');rows=[]
 for name,driver in [('old',a.baseline),('batch',a.driver)]:
  image.write_bytes(seed);text=run(driver,'append-step',True);raw=image.read_bytes();check_clean_image(image)
  current=ExfatFixture(raw);assert current.read('keep.txt')==bytes((i*17+37)&255 for i in range(len(old)+f.cluster*3+512)) and current.read('anchor.txt')==b'unchanged'
  row=dict(kind=name,writes=int(re.search(r'operation=PASS writes=(\d+)',text)[1]),flushes=int(re.search(r'flushes=(\d+)',text)[1]),image_sha256=hashlib.sha256(raw).hexdigest())
  rows.append((row,raw,canonical(text,f)));(out/f'{shift}-{name}.log').write_text(text)
 assert rows[0][1]==rows[1][1], 'Final media bytes differ'
 assert rows[0][2]==rows[1][2], 'Logical FAT writes or barrier ordering changed'
 assert rows[1][0]['writes']<rows[0][0]['writes'] and rows[0][0]['flushes']==rows[1][0]['flushes']
 results.append(dict(cluster=f.cluster,records=[x[0] for x in rows]));print(results[-1],flush=True)
(out/'result.json').write_text(json.dumps(dict(status='PASS',cases=results),indent=2)+'\n')
