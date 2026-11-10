"""Parser-only models plus a separately labeled full-parent composition.

Modeled typed rows below do not establish packet type, actual HTTP execution,
process scheduling, USB, keys or physical acceptance. Legacy proofs stay strict.
"""
import os,re,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import wifi_http5_log as h
import wifi_stack5_log as legacy
import hardware_desktop as desktop

RELEASE='0123456789abcdef';VOLUME='12345678'
def emit(prefix,**d):return prefix+': '+' '.join(k+'='+str(v) for k,v in d.items())
def finish(rows):
 text='\n'.join(rows)+'\n'
 return text+f'workspace-log: snapshot=exit.log monotonic_us=2000000 total_before={len(text.encode())} tail_limit=4194304\n'
def boot_rows():
 cmd=' '.join(sorted(h.BOOT_FIXED|{'srvros.desktop-release='+RELEASE,'srvros.workspace-volume='+VOLUME}))
 return ['cmdline: '+cmd,h.BOOT,'laptop-capture: boot options='+cmd]
def modeled(requests=12,rx=20,wraps=None,pending_rx=True):
 tx=requests+5;posted=[tx,rx+int(pending_rx)];wraps=list(wraps) if wraps is not None else [(tx-1)//63,(posted[1]-1)//63]
 rings=[0x300000,0x400000];slot=1;control=lambda d:(slot<<24)|((8+d)<<16)|(32<<10)|1
 previous=[[0,0,0,0,0,0,0,0],[3,33554486,0x100001,0,4096,0,0,0]]
 prefix=boot_rows()+[h.LOG_PARKED,h.PREPARE,'wifi-firmware-control: modeled=1',
  emit('wifi-cal5-stop',ok=1,posts=8,state=3,dequeue=0x100081,ring=0x100000,retained=1),
  emit('wifi-join5-plan',tx=0x500000,rx=0x600000,old=0x100001,slot=slot,max_rx=48),
  emit('wifi-handoff5',old_ring=0x100000,new_ring=0x200000),emit('wifi-tx5',ring=0x700000)]
 for d in (0,1):prefix.append(emit('wifi-join5-context',phase=8,dci=8+d,**{'w'+str(i):v for i,v in enumerate(previous[d])}))
 prefix+=['wifi-join5: modeled=1',h.PROGRESS,'wifi-channel-vco5: modeled=1']
 rows=[emit('wifi-stream5-plan',tx=rings[0],rx=rings[1],epoch=1,slot=slot,pages=4,positions=63,max_tx=80,max_rx=252,max_fences=4)]
 td=[];positions={}
 for n in range(max(tx,rx)):
  for d,count in ((0,tx),(1,rx)):
   if n>=count:continue
   i=len(td);positions[d,n+1]=i;td.append((d,n+1))
   rows.append(f'wifi-stream5-td: {i} {d} 0x{rings[d]+16*(n%63):016x} {(1<<24) if not d else ((13<<24)|3700)} {control(d)}')
 selected={5,rx}
 for boundary in range(63,63*wraps[1]+1,63):
  if boundary>=5 and boundary<=rx:selected.add(boundary)
  if 5<=boundary+1<=rx:selected.add(boundary+1)
 for i,(d,token) in enumerate(td):
  if not d and 5<=token<tx:rows.append(f'wifi-stream5-data: {i} {i+1} {token} {(token-1)%4} {1^(((token-1)//63)&1)} {token-4} 136')
  if d and token in selected:rows.append(f'wifi-stream5-rx: {i} {i+1} {token} {(token-1)%4} {1^(((token-1)//63)&1)} 100 9')
 fence=[]
 for d in (0,1):
  for w in range(wraps[d]):
   for k in (0,1):
    i=len(fence);base=0x900020+16*i;cycle=1^(w&1);new=cycle^1
    fence.append(emit('wifi-stream5-fence',i=i,p=base,t=15+k,d=8+d,q=(rings[d]|new) if k else 0,e=base,s=1<<24,c=(slot<<24)|(33<<10)|1,sp=0 if k else rings[d]+1008,ss=0 if k else 27<<24,sc=0 if k else control(d),sq=0 if k else (rings[d]+1008)|cycle,used=1))
 rows+=fence
 for i,(kind,d) in enumerate(((12,8),(16,9),(15,9),(15,8),(16,9),(12,8))):
  offset=i if i<2 else i+len(fence);q=rings[1]|1 if i==1 else previous[1][2] if i==4 else 0
  rows.append(emit('wifi-stream5-sync',i=i,p=0x900000+16*offset,e=0x900000+16*offset,t=kind,d=d,q=q,slot=slot,cc=1,delta=1))
 configured=[[1,33554454,rings[0]|1,0,1580,0,0,0],previous[1].copy()];configured[1][2]=rings[1]|1
 stopped=[x.copy() for x in configured];front=[]
 for d in (0,1):
  pending=d==1 and pending_rx;n=posted[d]-63*wraps[d]-int(pending);point=rings[d]+16*n;cycle=1^(wraps[d]&1);front.append((point,point|cycle));stopped[d][0]=3;stopped[d][2]=point|cycle
 last=stopped[0].copy();last[0]=0
 for phase,context in ((0,previous),(2,configured),(5,stopped),(9,[last,previous[1]])):
  for d in (0,1):rows.append(emit('wifi-stream5-context',p=phase,d=d,**{'w'+str(i):v for i,v in enumerate(context[d])}))
 for d in (0,1):rows.append(emit('wifi-stream5-stop',d=d,p=front[d][0],s=((26<<24)|4096) if d and pending_rx else 27<<24,c=control(d),q=front[d][1]))
 rows+=[emit('wifi-stream5-state',failed=0,phase=9,first_phase=0,pending_tx=0,pending_rx=0,round=0,command_pending=0,fault_event=0,p=0,s=0,c=0),
  emit('wifi-stream5-keys',auth=1,assoc=1,m1=1,m2=1,m3=1,m4=1,activated=1,erased=1,leave=1,unsafe=0,installs=1,pn=requests),
  emit('wifi-stack5-auth',attempted=1,auth_sent=1,assoc_sent=1,sends=5,polls=4,ignored=0,status=0,aid=7,present=1,peer=1,ok=1,invalid=0,rejected=0,clocks=2,elapsed_us=6,rollback=0),
  emit('wifi-stream5-probe',**{k:int(k=='private_erased') for k in 'complete unsafe setup setup_sends dhcp dns echo sent matched elapsed_us private_erased timed_out exhausted clock_fault polls ignored total_polls'.split()}),
  emit('wifi-stack5',complete=1,parent=1,restored=1,retired=1,empty=1,tx=tx,rx=posted[1],tx_wraps=wraps[0],rx_wraps=wraps[1],data_tx=requests,data_rx=rx-4,tx_after=max(tx-64,0),rx_after=max(rx-63,0) if wraps[1] else 0,commands=6,fences=sum(wraps),rx_receipts=len(selected),dropped=0,borrowed=rx,released=rx,faults=0,services=2000,elapsed_us=1100000,enqueued=requests,submitted=requests,completed=requests,received=rx-4,provider_faults=0,pending=0,retained=1,backend_retired=0),
  emit('wifi-stack5-final',complete=1,detached=1,provider_retired=1,retained=1,backend_retired=0),
  emit('wifi-http5-bridge',**{k:1 for k in h.BRIDGE_KEYS.split()})]
 w={k:0 for k in h.WORKLOAD_KEYS.split()}
 for k in 'version complete prepared worker_allocated attempted dhcp dns route handoff go child_observed child_reaped child_retired tcp_empty provider_drained worker_finished quiescent refs_scrubbed body_eof'.split():w[k]=1
 w.update(phase=10,request_bytes=4159,body_bytes=3072,bootstrap_polls=4,worker_polls=100,worker_turns=2,supervisor_turns=3,elapsed_us=100000)
 rows.append(emit('wifi-http5-workload',**w))
 return prefix+rows+['wifi-firmware-capture: completed=yes; RF and connectivity NOT_TESTED','laptop-capture: completed=yes']
class HTTPTests(unittest.TestCase):
 def check_model(self,rows):
  text=finish(rows)
  with patch.object(h,'parent',return_value={'modeled_parent':True}) as parent,patch.object(h,'join_cleanup',return_value={}) as join,patch.object(h,'calibration_cleanup',return_value={}):
   result=h.check(text,RELEASE,volume=VOLUME);parent.assert_called_once_with(text,stream=True);join.assert_called_once_with(text,require_cleanup=True);return result
 def mutate(self,rows,prefix,key,value):
  result=rows.copy();at=next(i for i,s in enumerate(result) if s.startswith(prefix));result[at]=re.sub(r'\b'+key+r'=(?:0x[0-9a-f]+|\d+)',key+'='+str(value),result[at]);return result
 def test_variable_counts_and_honest_fences(self):
  for args in [(8,20,None,True),(12,70,None,True),(65,20,None,True),(12,130,None,True),(65,200,None,True),(58,20,(0,0),True),(58,20,(1,0),True),(12,63,(0,0),False),(12,63,(0,1),False),(12,126,(0,1),False),(12,126,(0,2),False)]:
   with self.subTest(args=args):
    r=self.check_model(modeled(*args));self.assertTrue(r['http5_complete']);self.assertEqual(r['http5_request_bytes'],4159)
    if args[2]==(1,0) or (args[2]==(0,1) and args[1]==63) or args[2]==(0,2):
     self.assertLess(r['http5_observed_tx_data_crossings']+r['http5_observed_selected_rx_data_crossings'],r['http5_tx_fences']+r['http5_rx_fences'])
 def test_bad_fence_count_or_frontier(self):
  for args in [(12,20,(1,0),True),(12,20,(0,1),True),(12,200,(1,4),True)]:
   with self.assertRaises((ValueError,KeyError)):self.check_model(modeled(*args))
  rows=modeled(12,70)
  for prefix,key,value in [('wifi-stream5-stop: d=1 ','p',0x400000),('wifi-stream5-fence:','used',0),('wifi-stack5:','fences',0),('wifi-stack5:','rx_wraps',0),('wifi-stack5:','borrowed',69),('wifi-stack5:','released',69)]:
   with self.subTest(prefix=prefix,key=key),self.assertRaises(ValueError):self.check_model(self.mutate(rows,prefix,key,value))
 def test_http_workload_refusals(self):
  rows=modeled()
  bad={k:0 for k in 'complete prepared worker_allocated attempted dhcp dns route handoff go child_observed child_reaped child_retired tcp_empty provider_drained worker_finished quiescent refs_scrubbed body_eof'.split()}
  bad.update(version=2,phase=9,reason=1,child_status=23,test_override=1,request_bytes=4158,body_bytes=3071,retained=1,retries=2,bootstrap_polls=0,worker_polls=150000,worker_turns=0,supervisor_turns=150001,elapsed_us=15000000)
  for k,v in bad.items():
   with self.subTest(key=k),self.assertRaises(ValueError):self.check_model(self.mutate(rows,'wifi-http5-workload:',k,v))
  for k,v in [('bootstrap_polls',100),('worker_turns',97),('worker_polls',2001),('elapsed_us',1100001),('supervisor_turns',0),('retries',1)]:
   with self.subTest(key=k,value=v),self.assertRaises(ValueError):self.check_model(self.mutate(rows,'wifi-http5-workload:',k,v))
 def test_schema_order_canonical_unknown_and_missing_rows(self):
  rows=modeled();owned=[i for i,s in enumerate(rows) if s.startswith(('wifi-stream5','wifi-stack5','wifi-http5'))]
  for i in owned:
   for changed in (rows[:i]+rows[i+1:],rows[:i]+[rows[i]]+rows[i:],rows[:i]+[rows[i]+' extra=1']+rows[i+1:]):
    with self.subTest(row=i),self.assertRaises((ValueError,StopIteration)):self.check_model(changed)
  for prefix in ('wifi-http5-bridge:','wifi-http5-workload:'):
   at=next(i for i,s in enumerate(rows) if s.startswith(prefix));a=rows[at].split(' ');a[1],a[2]=a[2],a[1];changed=rows.copy();changed[at]=' '.join(a)
   with self.assertRaises(ValueError):self.check_model(changed)
   changed=rows.copy();changed[at]=changed[at].replace('version=1','version=01')
   with self.assertRaises(ValueError):self.check_model(changed)
  for extra in ['wifi-http5-unknown: complete=1','wifi-stream5-unknown: complete=1','wifi-stack5-pings: 1 2','http-network-vm: complete=1']:
   with self.assertRaises((ValueError,StopIteration)):self.check_model(rows+[extra])
 def test_boot_exact_native_profile(self):
  rows=modeled();self.assertEqual(h.boot_identity(finish(rows),RELEASE,VOLUME),RELEASE)
  for token in h.BOOT_FIXED:
   bad=[s.replace(token,'') if s.startswith(('cmdline: ','laptop-capture: boot options=')) else s for s in rows]
   with self.subTest(token=token),self.assertRaises(ValueError):self.check_model(bad)
  for token in ('srvros.http-network-vm=1','srvros.http-child-preflight=1','srvros.wifi-stream5-dhcp=1','srvros.wifi-http5=1','srvros.unrecognized=1'):
   bad=[s+' '+token if s.startswith(('cmdline: ','laptop-capture: boot options=')) else s for s in rows]
   with self.assertRaises(ValueError):self.check_model(bad)
  for release,volume in [('f'*16,VOLUME),(RELEASE,'87654321')]:
   with self.assertRaises(ValueError):h.boot_identity(finish(rows),release,volume)
  with self.assertRaises(ValueError):legacy.boot_identity(finish(rows),RELEASE)
 def test_raw_typed_identity_and_retirement(self):
  rows=modeled(65,130)
  for prefix,key,value in [('wifi-stream5-plan:','max_tx',81),('wifi-stream5-plan:','epoch',2),('wifi-stream5-plan:','tx',0x500000),('wifi-stream5-keys:','installs',2),('wifi-stream5-keys:','pn',64),('wifi-stream5-state:','pending_rx',1),('wifi-stack5:','data_tx',64),('wifi-stack5-final:','retained',0),('wifi-stack5-final:','backend_retired',1)]:
   with self.subTest(prefix=prefix,key=key),self.assertRaises(ValueError):self.check_model(self.mutate(rows,prefix,key,value))
  for prefix,column,value in [('wifi-stream5-data:',5,'0'),('wifi-stream5-data:',6,'2'),('wifi-stream5-data:',3,'1'),('wifi-stream5-rx:',6,'8'),('wifi-stream5-rx:',2,'9999'),('wifi-stream5-td:',3,'0x0000000000500000')]:
   changed=rows.copy();at=next(i for i,s in enumerate(changed) if s.startswith(prefix));fields=changed[at].split();fields[column]=value;changed[at]=' '.join(fields)
   with self.subTest(prefix=prefix,column=column),self.assertRaises(ValueError):self.check_model(changed)
 def test_saved_prefix_exact_and_bounded(self):
  text=finish(modeled())
  for bad in [text[:text.rindex('workspace-log:')],text+text[text.rindex('workspace-log:'):],text+'x',text.replace('tail_limit=4194304','tail_limit=2097152'),text.replace('monotonic_us=2000000','monotonic_us=02000000'),text.replace('total_before=','total_before=0'),text.replace('total_before=','total_before=1'),('x'*4194304)+text]:
   with self.assertRaises(ValueError):h.saved_prefix(bad)
 def test_full_composition_and_legacy_separation(self):
  path=os.environ.get('WIFI_HTTP5_COMPOSITION');old=os.environ.get('WIFI_HTTP5_LEGACY')
  if not path or not old:self.skipTest('explicit syntheticcomposition+oldreference paths required')
  text=Path(path).read_text();result=h.check(text,RELEASE,volume=VOLUME);self.assertTrue(result['http5_complete'])
  with self.assertRaises(ValueError):legacy.check(text,RELEASE)
  prior=Path(old).read_text();r=legacy.check(prior);self.assertEqual(r['stack5_exact_pings'],64);self.assertFalse(r['stack5_tcp'] or r['stack5_http'])
  with self.assertRaises(ValueError):h.check(prior)
class HTTPTransportTests(unittest.TestCase):
 def test_explicit_http_capture_identity_and_legacy_bounds(self):
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder);mount=root/'usb';work=mount/'srvros-work';name='session-0000000000000001-00';logs=work/'Logs'/name;logs.mkdir(parents=True);(work/'.workspace').write_text('SRVROS-WORKSPACE-V1 '+VOLUME+'\n')
   headers='\n'.join(boot_rows())+'\n';payload=headers.encode()+b'x'*(desktop.RADIO_LOG_BYTES+1-len(headers))
   (logs/'exit.log').write_bytes(payload)
   dst,report=desktop.collect(mount,VOLUME,root/'out',radio_capture=(name,RELEASE),cal5_cleanup_capture=True,http5_capture=True)
   self.assertEqual((dst/name/'exit.log').read_bytes(),payload);self.assertEqual(report['hardware_acceptance'],'manual-pending')
   for kwargs in [dict(http5_capture=True),dict(http5_capture=True,radio_capture=(name,RELEASE)),dict(http5_capture=1,radio_capture=(name,RELEASE),cal5_cleanup_capture=True),dict(http5_capture=True,radio_capture=('session-0000000000000002-00',RELEASE),cal5_cleanup_capture=True)]:
    with self.assertRaises(ValueError):desktop.collect(mount,VOLUME,root/'bad',**kwargs)
   for bad in [payload.replace(b'srvros.wifi-http5=1',b'srvros.wifi-http5=0'),payload.replace(b'srvros.workspace-volume=12345678',b'srvros.workspace-volume=87654321'),payload.replace(b'srvros.wifi-cal5-cleanup=1',b'srvros.wifi-cal5-cleanup=0'),payload.replace(b'laptop-capture: boot options=',b'not-boot: '),payload+b'x'*desktop.CAL5_LOG_BYTES]:
    (logs/'exit.log').write_bytes(bad)
    with self.assertRaises(ValueError):desktop.collect(mount,VOLUME,root/'bad',radio_capture=(name,RELEASE),cal5_cleanup_capture=True,http5_capture=True)
   (logs/'exit.log').unlink();(logs/'boot.log').write_text(headers)
   _,report=desktop.collect(mount,VOLUME,root/'boot-only',radio_capture=(name,RELEASE),cal5_cleanup_capture=True,http5_capture=True);self.assertFalse(report['sessions'][0]['exit_snapshot_present'])
if __name__=='__main__':unittest.main()
