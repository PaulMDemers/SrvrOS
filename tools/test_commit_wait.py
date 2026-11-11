#!/usr/bin/env python3
"""Actual C observer -> production formatter -> strict Python parser."""
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
# Bound every production row at full uint64 width and the entire extension,
# preserving atomic console records and spare room in the existing 64 KiB tail.
source=(Path(__file__).resolve().parents[1]/'kernel/src/workspace_log.c').read_text()
sizes={f.split(':')[0]:len(f.replace('\\n','\n').replace('%u',str(2**64-1))) for f in re.findall(r'console_printf\("(capture-commit-[^"\n]+)',source)}
assert len(sizes)==5 and max(sizes.values())<1024
assert sum(sizes.values())+2*sizes['capture-commit-phase']<4096
writes=dict(snapshot_us=1200)
queued=dict(snapshot_us=1200,workers=[dict(pid=11,calls=1,max_us=90)])
obs=commit_wait([blob,blob],writes,queued)
assert obs['status']=='VALID' and obs['wait']['identity']==101 and obs['wait']['target']==999
assert obs['wait']['parent_pid']==7 and obs['wait']['parent_id']==10
assert obs['wait']['overlap_us']==70 and obs['uncovered_wait_us']==20
assert obs['io']['payload_bytes']==16384 and obs['csw_wait_overlap_us']==36
assert obs['csw']['command_bytes']==16384
assert commit_wait([at(61)],dict(snapshot_us=61),dict(snapshot_us=61,workers=[dict(pid=11,calls=1,max_us=40),dict(pid=12,calls=1,max_us=30)]))['status']=='VALID_NO_USB_COMMAND'
assert commit_wait([at(121)],dict(snapshot_us=121),dict(snapshot_us=121,workers=[dict(pid=11,calls=1,max_us=40),dict(pid=12,calls=1,max_us=30),dict(pid=13,calls=1,max_us=55)]))['status']=='VALID_NO_COMPLETED_OVERLAP'
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
 # All conflicting duplicates must fail, including the singleton header.
 reject(blob+line.replace(b'final=1',b'final=0'));count+=1
for kind,field,value in [('wait','invalid',1),('wait','active',1),('wait','end_us',1099),('wait','pid',12),('wait','waits',2),('wait','overlap_start',1010),('wait','parent_id',1021),('wait','target',0),('wait','base',262145),('wait','live',196608),('wait','request',65537),('wait','borrowed',2),('io','io_us',41),('io','valid',0),('io','usb_active',1),('io','csws',0),('io','payload_bytes',16385),('command','complete',0),('command','csw_start',1021),('command','checkpoint_calls',5),('csw','command_bytes',32768),('csw','observed',1062),('csw','empty_checks',2),('csw','service_gap',37),('phase','calls',2)]:
 lines=blob.decode().splitlines()
 for i,line in enumerate(lines):
  if line.startswith('capture-commit-'+kind+':'):
   fields=dict(x.split('=') for x in line.split(': ',1)[1].split());fields[field]=str(value)
   lines[i]=line.split(': ',1)[0]+': '+' '.join(k+'='+v for k,v in fields.items());break
 reject(('\n'.join(lines)+'\n').encode());count+=1
reject(blob.replace(b'version=1',b'version=2'));count+=1
reject(blob.replace(b'snapshot_us=1200',b'snapshot_us=1201'));count+=1
reject(blob.replace(b'waits=1 ',b'waits=18446744073709551616 '));count+=1
# Explicit unavailable/no-work states retain no invented call or USB data.
header=dict(obs['wait'])
for k in 'calls overlap_us parent_pid parent_id identity slot size content_start content_end ok dirty_after overlap_start overlap_end'.split():header[k]=0
fields=['version','snapshot_us',*FIELDS['wait'].split(),'final']
def wire(h):return ('capture-commit-wait: '+' '.join(f'{k}={h[k]}' for k in fields)+'\n').encode()
assert commit_wait([wire(header)],writes,queued)['status']=='VALID_NO_COMPLETED_OVERLAP'
for k in FIELDS['wait'].split():header[k]=0
header['enabled']=1
assert commit_wait([wire(header)],writes,dict(snapshot_us=1200,workers=[dict(pid=11,calls=0,max_us=0)]))['status']=='VALID_UNEXERCISED'
print('commit-wait-wire: PASS actual C/production format, exact aggregate maximum and completed overlap/USB bounds,',count,'negative variants; compact final-only report',len(blob),'bytes')
