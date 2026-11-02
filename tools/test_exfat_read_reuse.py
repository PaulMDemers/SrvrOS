#!/usr/bin/env python3
"""Bounded metadata-heavy A/B fixture; timings are not native acceptance.

1200 empty file nodes and 128 KiB clusters exercise sidecar/directory overhead.
Read counters include mounting; the operation inputs and outputs are identical.
"""
import sys,subprocess,json,re,hashlib,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from test_storage_leases import empty_image
parser=argparse.ArgumentParser(description='Compare real exFAT mutation I/O with transaction cache enabled/disabled; require identical disk bytes and write/flush counts.')
parser.add_argument('--driver',type=Path,required=True)
parser.add_argument('--baseline',type=Path,required=True)
parser.add_argument('--output',type=Path,default=ROOT/'build/review/exfat-read-reuse')
args=parser.parse_args();r=args.output.resolve();r.mkdir(parents=True,exist_ok=True);p=r/'measure.exfat';f=empty_image(128*1024*1024,cluster_shift=8)
f.add_directory_files('archive',{f'Preserved-{i:04}.txt':b'' for i in range(1200)})
p.write_bytes(f.data)
subprocess.run([str(args.driver.resolve()),str(p),'seed'],check=True,capture_output=True)
seed=p.read_bytes();result={}
for op in ('create','rename','delete'):
 result[op]={}
 for name,driver in [('uncached',str(args.baseline.resolve())),('cached',str(args.driver.resolve()))]:
  p.write_bytes(seed);q=subprocess.run([driver,str(p),op],check=True,capture_output=True,text=True)
  reads,bytes_=map(int,re.search(r'driver: reads=(\d+) read_bytes=(\d+)',q.stdout).groups());writes,flushes=map(int,re.search(r'complete writes=(\d+) flushes=(\d+)',q.stdout).groups())
  result[op][name]=dict(reads=reads,read_bytes=bytes_,writes=writes,flushes=flushes,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
 assert result[op]['cached']['read_bytes']<result[op]['uncached']['read_bytes'],result[op]
 assert result[op]['cached']['sha256']==result[op]['uncached']['sha256'],result[op]
 assert (result[op]['cached']['writes'],result[op]['cached']['flushes'])==(result[op]['uncached']['writes'],result[op]['uncached']['flushes'])
(r/'read-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
