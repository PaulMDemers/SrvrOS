"""Bounded PCI requester inhibition; no GPU mapping/completion authority."""
import base64,re,struct
from intel_joint import fields,authority,joint_recipe
FIELDS='v state reason phase samples reads attempts writes polls n0 n1 n2 origin owner_origin begin last end clock_at clock_prev drain_begin restored returned complete borrow_active borrow_fault borrow_epoch borrow_sequence issued pci_reads admit_reads admit0 admit1 uncertain claim_borrows claim_returns borrow_object borrow_claim obligations assurances pte_writes managed_pages hardware_ownership'.split()
BOOL='restored returned complete borrow_active borrow_fault uncertain borrow_object borrow_claim'.split()
def _prefix(w):
 n=len(w)
 return not any(x==0xffffffff for x in w) and (not n or w[0]==0x16268086) and (n<2 or w[1]&(1<<20) and w[1]&2) and (n<3 or w[2]==0x03000009) and (n<4 or w[3]==0x011b106b) and (n<5 or w[4]&255==0x90) and (n<6 or w[5]&65535==0xd005) and (n<7 or w[6]&65535==0xa401 and w[6]>>16&7==2) and (n<8 or w[7]==0x03060013) and (n<9 or not w[8]&~256)
def gpu_hold_summary(line):
 if line.startswith('intel-gpu-hold-summary: v=2 '):
  m=re.fullmatch(r'intel-gpu-hold-summary: v=2 b=([A-Za-z0-9+/=]+)',line)
  if not m:raise ValueError('GPU hold packed summary encoding')
  try:raw=base64.b64decode(m[1],validate=True)
  except ValueError:raise ValueError('GPU hold packed summary base64')
  if len(raw)!=8*len(FIELDS) or base64.b64encode(raw).decode()!=m[1]:raise ValueError('GPU hold packed summary size/canonical')
  vs=struct.unpack('<'+'Q'*len(FIELDS),raw)
  if vs[0]!=1:raise ValueError('GPU hold packed schema')
  line='intel-gpu-hold-summary: '+' '.join(f'{k}={v}' for k,v in zip(FIELDS,vs))
 s=fields(line,'intel-gpu-hold-summary',FIELDS);authority(s);return s
def gpu_hold_capture(text):
 lines=[l for l in text.splitlines() if l.startswith('intel-gpu-hold-')]
 if not lines:return None
 if len(lines)!=2 or not lines[0].startswith('intel-gpu-hold-data: v=1 b=') or not lines[1].startswith('intel-gpu-hold-summary: '):raise ValueError('GPU hold receipt coverage/order')
 s=gpu_hold_summary(lines[1])
 from intel_gt_wake import wake_result
 wake=wake_result(text)
 if lines[1].startswith('intel-gpu-hold-summary: v=2 ')!=bool(wake and wake['summary']['v']==27):raise ValueError('GPU hold summary image binding')
 if any(s[k]>1 for k in BOOL) or s['state']>6 or s['reason']&~255 or s['phase']>2 or s['polls']>32 or s['attempts']>2 or s['writes']>s['attempts'] or s['issued']!=s['writes'] or s['admit_reads']>s['attempts']:raise ValueError('GPU hold schema/counts')
 encoded=lines[0].split('b=',1)[1]
 try:raw=base64.b64decode(encoded,validate=True)
 except Exception as e:raise ValueError('GPU hold data encoding') from e
 if len(raw)!=700 or base64.b64encode(raw).decode()!=encoded:raise ValueError('GPU hold canonical data')
 values=struct.unpack_from('<27I6Q',raw)
 phases=[dict(words=list(values[n*9:n*9+9]),begin=values[27+n*2],end=values[28+n*2],count=s['n'+str(n)]) for n in range(3)]
 polls=[dict(zip(('command','pending','at','count'),struct.unpack_from('<IIQB',raw,156+n*17))) for n in range(32)]
 if any(p['count']>9 for p in phases) or any(p['count']>2 for p in polls):raise ValueError('GPU hold prefix count')
 if s['samples']!=sum(p['count']==9 for p in phases) or s['reads']!=sum(p['count'] for p in phases)+sum(p['count'] for p in polls) or s['pci_reads']!=s['reads']+s['admit_reads']:raise ValueError('GPU hold raw read accounting')
 for p in phases:
  n=p['count'];w=p['words']
  if any(w[n:]):raise ValueError('GPU hold data past prefix')
  for i in range(1,n+1):
   if not _prefix(w[:i]) and (i!=n or not s['reason']&24):raise ValueError('GPU hold invalid capability/read prefix')
  if p['end'] and (not p['begin'] or not p['begin']<=p['end']<=s['last']):raise ValueError('GPU hold sample clocks')
 for n,p in enumerate(polls):
  if n>=s['polls'] and any(p.values()) or any(p[k] for k in ('command','pending')[p['count']:]):raise ValueError('GPU hold poll data past prefix')
  if p['at'] and (not s['drain_begin'] or not s['drain_begin']<=p['at']<=s['last']):raise ValueError('GPU hold poll clock')
 if s['end']!=s['last'] or s['begin'] and not s['origin']<=s['begin']<=s['last']:raise ValueError('GPU hold original lifetime')
 if s['state']==6 and (not s['reason'] or not s['uncertain'] or not s['borrow_active'] or not s['borrow_fault']):raise ValueError('GPU hold lost fault pin')
 if s['clock_at'] or s['clock_prev']:
  if not s['reason']&4 or not s['clock_prev']:raise ValueError('GPU hold unexplained clock')
 if any(s['admit'+str(n)] for n in range(s['admit_reads'],2)):raise ValueError('GPU hold command data past admission')
 if s['returned'] and (s['borrow_active'] or s['claim_borrows']!=s['claim_returns'] or not s['restored']):raise ValueError('GPU hold premature return')
 if s['complete']:
  if s['state']!=5 or s['phase']!=2 or s['borrow_sequence']!=s['claim_borrows'] or s['reason'] or s['uncertain'] or s['borrow_active'] or s['borrow_fault'] or not all(s[k] for k in ('restored','returned','borrow_object','borrow_claim','borrow_epoch','borrow_sequence')) or s['claim_borrows']!=1 or s['claim_returns']!=1:raise ValueError('GPU hold false completion')
  if s['admit_reads']!=2 or s['admit0']!=phases[0]['words'][1] or s['admit1']!=phases[1]['words'][1]:raise ValueError('GPU hold fresh pre-store command')
  if s['attempts']!=2 or s['writes']!=2 or s['samples']!=3 or any(p['count']!=9 for p in phases):raise ValueError('GPU hold incomplete phases')
  for n,p in enumerate(phases):
   w=p['words']
   if not _prefix(w) or w[1]&65535!=(3 if n==1 else 7):raise ValueError('GPU hold phase command')
   if n and any((w[i]&(~4 if i==1 else 0xffffffff))!=(phases[0]['words'][i]&(~4 if i==1 else 0xffffffff)) for i in range(8)):raise ValueError('GPU hold changed configuration')
   if not s['begin']<=p['begin']<=p['end']<=s['end']<=s['origin']+100000 or n and phases[n-1]['end']>p['begin']:raise ValueError('GPU hold phase ordering/deadline')
  if s['end']>s['owner_origin']+300000 or phases[2]['words'][8]:raise ValueError('GPU hold return without completion')
  if s['drain_begin']!=phases[1]['end'] or bool(phases[1]['words'][8])!=bool(s['polls']):raise ValueError('GPU hold drain binding')
  previous=s['drain_begin']
  for n,p in enumerate(polls[:s['polls']]):
   if p['count']!=2 or p['command']!=phases[1]['words'][1] or p['pending']!=(0 if n==s['polls']-1 else 256) or not previous<=p['at']<=s['drain_begin']+10000 or p['at']>phases[2]['begin']:raise ValueError('GPU hold pending completion')
   previous=p['at']
 return dict(summary=s,phases=phases,polls=polls[:s['polls']],scope_complete=bool(s['complete']))
def gpu_hold_result(text,*,entry_reset=False):
 r=gpu_hold_capture(text)
 if not r:return None
 from intel_joint import joint_result
 j=joint_result(text)
 if not j or j['wake']['summary']['v']!=(27 if entry_reset else 21):raise ValueError('GPU hold wrong image/source')
 s=r['summary'];c=j['claim']['summary']
 if s['origin'] and (s['origin']!=c['origin'] or s['owner_origin']!=c['owner_origin'] or s['borrow_epoch']!=c['epoch']):raise ValueError('GPU hold replacement source')
 if s['begin'] and (s['begin']<c['begin'] or c['complete'] and s['end']>c['end']):raise ValueError('GPU hold outside original claim')
 if s['borrow_active'] and (not c['pinned'] or j['wake']['summary']['released'] or j['summary']['hda_attempts']>1):raise ValueError('GPU hold early parent/HDA retirement')
 complete=bool(r['scope_complete'] and j['complete'])
 if complete and j['held']['summary']['live_checks']!=62+s['polls']:raise ValueError('GPU hold live source read binding')
 if j['wake']['summary']['complete'] and not complete:raise ValueError('GPU hold false enclosing completion')
 return dict(r,joint=j,complete=complete,native_complete=False,hardware_ownership=False,managed_pages=0,physical_tearfree=False)
def native_gpu_hold_result(text):
 r=gpu_hold_result(text)
 if not r or not r['complete']:return r
 from intel_gt_wake import native_wake_result
 w=native_wake_result(text)
 if not w or not w['native_wake_complete']:raise ValueError('GPU hold native prerequisites')
 r['native_complete']=True
 return r
def verify_gpu_hold_checkpoint(pre,text,native=False):
 from intel_joint import verify_joint_checkpoint
 if 'intel-gpu-hold-' in pre:raise ValueError('GPU hold late checkpoint')
 verify_joint_checkpoint(pre,text,native=native)
 cmd=[l.split('=',1)[1] for l in pre.splitlines() if l.startswith('laptop-capture: boot options=')]
 if len(cmd)!=1:raise ValueError('GPU hold boot provenance')
 joint_recipe(cmd[0],requester_hold=True)
 if not gpu_hold_result(text):raise ValueError('GPU hold missing result')
 return True
