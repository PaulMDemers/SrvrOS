#!/usr/bin/env python3
"""Real-driver A/B: bitmap spans must preserve disk bytes and flush ordering."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
from test_storage_leases import empty_image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--driver',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--output',type=Path,default=ROOT/'build/review/exfat-bitmap-io')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
f=empty_image(128*1024*1024,cluster_shift=8)
f.add_directory_files('archive',{f'Preserved-{i:04}.txt':b'' for i in range(1200)})
image=out/'measure.exfat';image.write_bytes(f.data)
subprocess.run([str(a.driver.resolve()),str(image),'seed'],check=True,capture_output=True)
seed=image.read_bytes();result={}
for op in ('create','overwrite','rename','delete','replace'):
 result[op]={}
 for name,driver in [('full',a.baseline),('span',a.driver)]:
  image.write_bytes(seed)
  q=subprocess.run([str(driver.resolve()),str(image),op],check=True,capture_output=True,text=True)
  (out/f'{op}-{name}.log').write_text(q.stdout+q.stderr)
  count,flush=map(int,re.search(r'complete writes=(\d+) flushes=(\d+)',q.stdout).groups())
  n=int(re.search(r'driver: write_bytes=(\d+)',q.stdout)[1])
  result[op][name]=dict(writes=count,flushes=flush,write_bytes=n,sha256=hashlib.sha256(image.read_bytes()).hexdigest())
 base,new=result[op]['full'],result[op]['span']
 assert base['sha256']==new['sha256'],result[op]
 assert (base['writes'],base['flushes'])==(new['writes'],new['flushes']),result[op]
 assert new['write_bytes']<base['write_bytes'],result[op]
 result[op]['reduction_percent']=round((1-new['write_bytes']/base['write_bytes'])*100,2)
(out/'write-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
