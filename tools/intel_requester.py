"""Strict RO PCI AF requester receipts; no DMA/cache/ownership authority."""
import re
from intel_engine_census import _fields
FIELDS='v attempts samples profiles reads begin end epoch blocked borrowed returned retained stable bus_master pending flr reason clock_at clock_prev complete obligations config_writes reset_writes pte_writes managed_pages hardware_ownership'.split()
BOOL='blocked borrowed returned retained stable complete'.split()
def requester_summary(line):
 s=_fields(line,'intel-requester-summary:',FIELDS)
 if s['v']!=1 or any(v>=1<<64 for v in s.values()) or any(s[k]>1 for k in BOOL) or s['epoch']>=1<<32 or not 0<=s['profiles']<=s['samples']<=s['attempts']<=2 or s['reads']>18 or max(s['bus_master'],s['pending'])>3 or s['flr'] or s['reason']>255:raise ValueError('requester schema/range')
 if s['obligations']!=255 or any(s[k] for k in ('config_writes','reset_writes','pte_writes','managed_pages','hardware_ownership')):raise ValueError('requester unsupported authority')
 if s['returned']+s['retained']!=s['borrowed'] or s['blocked']>s['borrowed'] or s['attempts'] and not s['blocked'] or s['retained'] and not s['reason']&128:raise ValueError('requester borrow lifecycle')
 if (s['clock_at'] or s['clock_prev']) and not s['reason']&2:raise ValueError('requester unexplained clock')
 return s

def _profile(w):
 return len(w)==9 and all(v!=0xffffffff for v in w) and w[0]==0x16268086 and w[2]==0x03000009 and w[3]==0x011b106b and bool(w[1]&(1<<20)) and bool(w[1]&2) and w[4]&255==0x90 and w[5]&65535==0xd005 and w[6]&65535==0xa401 and (w[6]>>16)&7==2 and w[7]==0x03060013 and not w[8]&~0x100

def requester_result(text):
 rows=[];sums=[];order=[]
 for line in text.splitlines():
  if line.startswith('intel-requester-row:'):
   m=re.fullmatch(r'intel-requester-row: v=1 s=([01]) n=([0-9]) d=(-|[0-9]{1,10}(?:,[0-9]{1,10}){0,8})',line)
   if not m:raise ValueError('requester row encoding')
   w=[] if m[3]=='-' else list(map(int,m[3].split(',')))
   if len(w)!=int(m[2]) or any(v>0xffffffff for v in w) or (m[3]!='-' and ','.join(map(str,w))!=m[3]):raise ValueError('requester raw width/prefix')
   rows.append(dict(sample=int(m[1]),words=w));order.append('r')
  elif line.startswith('intel-requester-summary:'):sums.append(requester_summary(line));order.append('s')
  elif line.startswith('intel-requester-'):raise ValueError('unknown requester receipt')
 if not sums:
  if rows:raise ValueError('requester missing summary')
  return None
 if len(sums)!=1:raise ValueError('requester duplicate summary')
 s=sums[0]
 if order!=['r']*s['attempts']+['s'] or [r['sample'] for r in rows]!=list(range(s['attempts'])) or sum(len(r['words']) for r in rows)!=s['reads'] or sum(len(r['words'])==9 for r in rows)!=s['samples']:raise ValueError('requester coverage/order')
 if any(0xffffffff in r['words'] for r in rows) and not s['reason']&16:raise ValueError('requester unreported floating read')
 valid=rows[:s['profiles']]
 if any(not _profile(r['words']) for r in valid):raise ValueError('requester accepted profile')
 if s['bus_master']!=sum(((r['words'][1]>>2)&1)<<n for n,r in enumerate(valid)) or s['pending']!=sum(((r['words'][8]>>8)&1)<<n for n,r in enumerate(valid)):raise ValueError('requester derived masks')
 if s['profiles']<2 and s['stable'] or s['profiles']==2 and bool(s['stable'])!=(rows[0]['words']==rows[1]['words']):raise ValueError('requester raw stability')
 from intel_gt_wake import wake_result
 from intel_takeover import takeover_result
 wake=wake_result(text);take=takeover_result(text)
 if not wake or wake['summary']['v']!=15 or not take:raise ValueError('requester wrong image/prerequisites')
 t=take['summary']
 if s['borrowed'] and (not t['complete'] or not t['returned'] or t['retained'] or s['epoch']!=t['epoch'] or not t['end_us']<s['begin']<=t['end_us']+100000):raise ValueError('requester preflight epoch/time binding')
 complete=bool(not s['reason'] and s['attempts']==s['samples']==s['profiles']==2 and s['reads']==18 and s['borrowed'] and s['blocked'] and s['returned'] and not s['retained'] and 0<s['begin']<s['end']<=s['begin']+100000)
 if s['complete']!=int(complete):raise ValueError('requester completion declaration')
 if complete and not t['end_us']<s['begin']<s['end']<wake['summary']['end_us']:raise ValueError('requester owner ordering')
 if not text.index('intel-takeover-summary:')<text.index('intel-requester-summary:')<text.index('intel-clock:')<text.index('intel-wake-summary:'):raise ValueError('requester prerequisite/report order')
 return dict(summary=s,rows=rows,complete=bool(complete and wake['lease_complete']),native_complete=False,observed_pending_mask=s['pending'],observed_bus_master_mask=s['bus_master'],dma_drained=False,cache_ownership=False,hardware_ownership=False,managed_range=False,physical_tearfree=False)

def native_requester_result(text):
 r=requester_result(text)
 if not r or not r['complete']:return r
 from intel_gt_wake import native_wake_result
 wake=native_wake_result(text)
 if not wake or not wake['native_wake_complete']:raise ValueError('requester native wake coverage')
 r['native_complete']=True;return r
