"""Compare real drivers and independently inspect durable sidecar metadata."""
import argparse,os,re,json,subprocess,struct
from pathlib import Path
from test_storage_leases import empty_image
from exfat_test_image import ExfatFixture,check_clean_image
p=argparse.ArgumentParser();p.add_argument('--driver',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);a=p.parse_args()
out=Path('build/review/exfat-sidecar-equality');out.mkdir(parents=True,exist_ok=True);image=out/'work.exfat';results=[]
def run(driver,mode,*args,env=None):
 q=subprocess.run([str(driver.resolve()),str(image),mode,*map(str,args)],env={**os.environ,**(env or {})},capture_output=True,text=True)
 assert q.returncode in (0,77,78,79),q.stdout+q.stderr
 return q
for shift in (3,8):
 image.write_bytes(empty_image(16*1024*1024,cluster_shift=shift).data);run(a.driver,'sidecar-seed');seed=image.read_bytes();f=ExfatFixture(seed);rows=[]
 for name,driver in [('rewrite',a.baseline),('equal',a.driver)]:
  image.write_bytes(seed);q=run(driver,'sidecar-same');assert 'operation=PASS' in q.stdout
  raw=image.read_bytes();check_clean_image(image);current=ExfatFixture(raw)
  assert current.read('.srvros/meta')==f.read('.srvros/meta') and current.read('keep.txt')==b'kept'
  assert 'mode=384' in run(driver,'sidecar-verify').stdout
  m=re.search(r'writes=(\d+) flushes=(\d+) write_bytes=(\d+)',q.stdout);row=dict(kind=name,writes=int(m[1]),flushes=int(m[2]),write_bytes=int(m[3]));rows.append(row)
  if name=='equal':assert raw==seed,'No-op modified media'
 assert rows[1]['writes']<rows[0]['writes'] and rows[1]['write_bytes']<rows[0]['write_bytes'] and rows[1]['flushes']>=3
 changed_results=[]
 for driver in (a.baseline,a.driver):
  image.write_bytes(seed);q=run(driver,'sidecar-changed',env={'SRVROS_TRACE_WRITES':'1'});assert 'operation=PASS' in q.stdout;check_clean_image(image)
  changed_results.append((image.read_bytes(),[x for x in q.stdout.splitlines() if x.startswith(('write-trace:','flush-trace:'))]))
  assert 'mode=416' in run(driver,'sidecar-verify').stdout
 assert changed_results[0]==changed_results[1], 'Changed-sidecar media/write/barrier sequence changed'
 assert ExfatFixture(changed_results[1][0]).read('.srvros/meta')!=f.read('.srvros/meta')
 image.write_bytes(seed);q=run(a.driver,'sidecar-pending');assert 'operation=PASS' in q.stdout
 final=ExfatFixture(image.read_bytes());assert final.read('.srvros/meta')==f.read('.srvros/meta')
 directory2=struct.unpack_from('<I',dict(final.entries(final.chain(final.root)))['.srvros'],52)[0]
 assert 'meta.tmp' not in dict(final.entries(final.chain(directory2)));check_clean_image(image)
 image.write_bytes(seed);q=run(a.driver,'sidecar-same',env={'SRVROS_SIDECAR_CLIENT':'1'});assert int(re.search(r'writes=(\d+)',q.stdout)[1])==rows[0]['writes']
 # Read failure is injected after mount, so it tests proof refusal, not import.
 # Use independent directory traversal to locate the first sidecar data extent.
 root=dict(f.entries(f.chain(f.root)));directory=struct.unpack_from('<I',root['.srvros'],52)[0]
 meta=dict(f.entries(f.chain(directory)))['meta'];cluster=struct.unpack_from('<I',meta,52)[0]
 image.write_bytes(seed);q=run(a.driver,'sidecar-same',env={'SRVROS_SIDECAR_FAIL_OFFSET':str(f.offset(cluster))});assert 'operation=FAIL' in q.stdout and image.read_bytes()==seed;check_clean_image(image)
 cuts=[]
 for kind,count,index in [('write-fail',rows[1]['writes'],0),('write-cut',rows[1]['writes'],1),('flush-fail',rows[1]['flushes'],2),('flush-cut',rows[1]['flushes'],3),('tear',rows[1]['writes'],4)]:
  for n in range(1,count+1):
   image.write_bytes(seed);args=[0]*5;args[index]=n;q=run(a.driver,'sidecar-same',*args)
   assert q.returncode or 'operation=FAIL' in q.stdout,(kind,n,q.stdout)
   raw=ExfatFixture(image.read_bytes());assert raw.read('.srvros/meta')==f.read('.srvros/meta') and raw.read('keep.txt')==b'kept' and raw.read('anchor.txt')==b'unchanged'
   subprocess.run(['/usr/sbin/fsck.exfat','-y',str(image)],capture_output=True,check=True);check_clean_image(image)
   raw=ExfatFixture(image.read_bytes());assert raw.read('.srvros/meta')==f.read('.srvros/meta') and raw.read('keep.txt')==b'kept'
   cuts.append([kind,n])
 results.append(dict(cluster=f.cluster,cuts=cuts,pending_temp_fallback=True,records=rows,changed_metadata_persisted=True,read_failure_refused=True,live_client_fallback=True));print(results[-1],flush=True)
(out/'result.json').write_text(json.dumps(dict(status='PASS',cases=results),indent=2)+'\n')
