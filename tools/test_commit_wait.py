#!/usr/bin/env python3
"""Actual C observer -> production formatter -> strict owner/status parser."""
import argparse
from pathlib import Path
import subprocess
import re
from commit_wait import commit_wait, FIELDS

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',type=Path,required=True)
a=p.parse_args()
q=subprocess.run([str(a.binary.resolve())],check=True,capture_output=True)
def at(stamp):
 return b'\n'.join(x for x in q.stdout.splitlines() if x.startswith(b'capture-commit-') and f' snapshot_us={stamp} '.encode() in x)+b'\n'
blob=at(1200)
assert blob and len(blob)<3500 and max(map(len,blob.splitlines()))<1024
source=(Path(__file__).resolve().parents[1]/'kernel/src/workspace_log.c').read_text()
sizes={f.split(':')[0]:len(f.replace('\\n','\n').replace('%u',str(2**64-1)).replace('%s','X'*31)) for f in re.findall(r'console_printf\("(capture-commit-[^"\n]+)',source)}
assert len(sizes)==4 and max(sizes.values())<1024 and sum(sizes.values())<4096
writes=dict(snapshot_us=1200);queued=dict(snapshot_us=1200,workers=[dict(pid=11,calls=1,max_us=90)])
obs=commit_wait([blob,blob],writes,queued)
assert obs['status']=='VALID' and obs['owner']['thread']==55 and obs['owner']['owner_pid']==0
assert obs['owner']['name']=='workspace-log' and obs['wait']['calls']==0 and obs['wait']['target']==999
assert obs['wait']['owned_us']==70 and obs['uncovered_wait_us']==20
assert obs['csw']['tag']==77 and obs['csw_wait_overlap_us']==40 and obs['csw']['command_bytes']==16384
partial=commit_wait([at(41)],dict(snapshot_us=41),dict(snapshot_us=41,workers=[dict(pid=11,calls=1,max_us=30)]))
assert partial['status']=='VALID' and partial['owner']['released']==0 and partial['owner']['end_us']==0
assert partial['csw_wait_overlap_us']==20 and partial['owner']['csw_us']==22
no_status=commit_wait([at(51)],dict(snapshot_us=51),dict(snapshot_us=51,workers=[dict(pid=11,calls=1,max_us=40)]))
assert no_status['status']=='VALID_OWNER_NO_STATUS' and no_status['wait']['owner_calls']==2
content=commit_wait([at(1300)],dict(snapshot_us=1300),dict(queued,snapshot_us=1300))
assert content['status']=='VALID_NO_OWNER' and content['wait']['identity']==101 and content['wait']['overlap_us']==70
assert content['io']['payload_bytes']==16384
assert commit_wait([at(61)],dict(snapshot_us=61),dict(snapshot_us=61,workers=[dict(pid=11,calls=1,max_us=40),dict(pid=12,calls=1,max_us=30)]))['status']=='VALID_NO_OWNER'
assert commit_wait([at(121)],dict(snapshot_us=121),dict(snapshot_us=121,workers=[dict(pid=11,calls=1,max_us=40),dict(pid=12,calls=1,max_us=30),dict(pid=13,calls=1,max_us=55)]))['wait']['open_at_end']==1
try:commit_wait([at(62)],dict(snapshot_us=62),dict(snapshot_us=62,workers=[dict(pid=11,calls=1,max_us=35)]))
except ValueError:pass
else:raise AssertionError('Actual failed C content was accepted')
def reject(b):
 try:commit_wait([b],writes,queued)
 except ValueError:return
 raise AssertionError('Malformed companion accepted: '+repr(b[:100]))
count=0
for line in blob.splitlines(keepends=True):
 reject(blob.replace(line,b''));count+=1
 reject(blob.replace(line,line.rstrip()+b' extra=1\n'));count+=1
 reject(blob+line.replace(b'final=1',b'final=0'));count+=1
for kind,field,value in [('wait','invalid',1),('wait','active',1),('wait','end_us',1099),('wait','pid',12),('wait','waits',2),('wait','target',0),('wait','base',262145),('wait','live',196608),('wait','request',65537),('wait','borrowed',2),('wait','owned_us',91),('owner','thread',0),('owner','start_us',1011),('owner','end_us',1081),('owner','released',0),('owner','overlap_start',1011),('owner','csws',0),('owner','status_overlap_us',41),('owner','write512',1),('status','command_bytes',32768),('status','observed',1062),('status','empty_checks',2),('status','service_gap',41),('status','actual',12),('status','tag',2**32)]:
 lines=blob.decode().splitlines()
 for i,line in enumerate(lines):
  if line.startswith('capture-commit-'+kind+':'):
   fields=dict(x.split('=') for x in line.split(': ',1)[1].split());fields[field]=str(value)
   lines[i]=line.split(': ',1)[0]+': '+' '.join(k+'='+v for k,v in fields.items());break
 reject(('\n'.join(lines)+'\n').encode());count+=1
reject(blob.replace(b'version=2',b'version=1'));count+=1
reject(blob.replace(b'snapshot_us=1200',b'snapshot_us=1201'));count+=1
reject(blob.replace(b'waits=1 ',b'waits=18446744073709551616 '));count+=1
# Explicit absent observations retain no invented owner, content or status.
rows={k:dict(v) for k,v in [('wait',obs['wait']),('owner',obs['owner'])]}
for kind in rows:
 for k in FIELDS[kind].split():rows[kind][k]='none' if k=='name' else 0
rows['wait']['enabled']=1
empty=b'\n'.join(('capture-commit-'+kind+': '+' '.join(f'{k}={v}' for k,v in row.items())).encode() for kind,row in rows.items())+b'\n'
assert commit_wait([empty],writes,dict(snapshot_us=1200,workers=[dict(pid=11,calls=0,max_us=0)]))['status']=='VALID_UNEXERCISED'
print('commit-wait-wire: PASS actual C/production formatter, kernel owner without SYS_SYNC, partial owner/no late release, completed content,',count,'negative variants; report',len(blob),'bytes')
