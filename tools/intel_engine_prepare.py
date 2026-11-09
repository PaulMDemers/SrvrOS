"""Bounded all-engine prepare/hold/cancel; no reset or GPU-memory authority."""
import re
from intel_engine_census import _fields
from intel_compact import compact_row
FIELDS='v begin end epoch attempts writes reads polls snapshots sample_attempts acks ready cancelled blocked admitted held preserved uncertain returned borrowed retained reason stage index clock_at clock_prev plane_samples plane_reads complete obligations reset_writes pte_writes managed_pages hardware_ownership'.split()
BOOL='blocked admitted held preserved uncertain returned borrowed retained complete'.split()
def prepare_summary(line):
 s=_fields(line,'intel-prepare-summary:',FIELDS)
 if any(x>=1<<64 for x in s.values()) or s['v']!=1 or any(s[k]>1 for k in BOOL) or s['epoch']>=1<<32 or s['attempts']>10 or s['writes']>s['attempts'] or s['acks']!=s['writes'] or s['attempts']-s['writes']>1 or s['reads']>2594 or s['polls']>2560 or s['snapshots']>s['sample_attempts'] or s['sample_attempts']>4 or s['ready']>31 or s['cancelled']>31 or s['reason']>4095 or s['stage']>6 or s['index']>4 or s['plane_samples']>2 or s['plane_reads']>54:raise ValueError('prepare schema/range')
 if s['obligations']!=255 or any(s[k] for k in ('reset_writes','pte_writes','managed_pages','hardware_ownership')):raise ValueError('prepare unsupported authority')
 if s['returned']+s['retained']!=s['borrowed'] or s['blocked']>s['borrowed'] or s['uncertain']>s['retained'] or s['attempts'] and not s['admitted'] or s['admitted'] and not s['blocked']:raise ValueError('prepare borrow/issue lifecycle')
 if s['retained'] and not s['reason']&512 or s['writes'] and not s['preserved'] and not s['retained']:raise ValueError('prepare unprotected write')
 if (s['clock_at'] or s['clock_prev']) and not s['reason']&(2|128):raise ValueError('prepare unexplained clock sample')
 return s

def _words(encoded,n):
 w=[] if encoded=='-' else list(map(int,encoded.split(',')))
 if len(w)!=n or any(x>0xffffffff for x in w) or encoded!=('-' if not w else ','.join(map(str,w))):raise ValueError('prepare raw width/progress')
 return w

def post_vector(line):
 """Decode exact held-image v3 raw words into the established v2 schema."""
 m=re.fullmatch(r'intel-prepare-post-vector: v=3 s=([01]) n=(8|16|24|32|40|46) h=([0-9a-f]+)',line)
 if not m or len(m[3])!=int(m[2])*8:raise ValueError('prepare post vector encoding/count')
 sample,n=int(m[1]),int(m[2]);w=[int(m[3][i:i+8],16) for i in range(0,len(m[3]),8)]
 lines=[]
 for e in range(min(n//8,5)):
  lines.append('intel-completion-row: v=2 s='+str(sample)+' e='+str(e)+' d='+','.join(map(str,w[e*8:e*8+8])))
 if n==46:lines.append('intel-completion-context: v=2 s='+str(sample)+' d='+','.join(map(str,w[40:46])))
 return sample,lines

def prepare_result(text):
 rows=[];acks=[];planes=[];audio=[];sums=[];postlines=[];order=[];actions=[];vectors=[];legacy_post=False
 for line in text.splitlines():
  if line.startswith('intel-prepare-action:'):
   from intel_held_requester import action_summary
   actions.append(action_summary(line))
  elif line.startswith('intel-prepare-row:'):
   m=re.fullmatch(r'intel-prepare-row: v=1 s=([0-3]) n=([0-6]) d=(-|[0-9,]+)',line)
   if not m:raise ValueError('prepare row encoding')
   row=dict(sample=int(m[1]),words=_words(m[3],int(m[2])));rows.append(row);order.append(('r',row['sample']))
  elif line.startswith('intel-prepare-ack:'):
   a=compact_row(line,'intel-prepare-ack:',[('n','n')],['first','last','polls'])
   if a['v']!=2 or a['n']>=10 or max(a['first'],a['last'])>0xffffffff or a['polls']>256:raise ValueError('prepare acknowledgement width')
   acks.append(a);order.append(('a',a['n']))
  elif line.startswith('intel-prepare-post-vector:'):
   sample,normalized=post_vector(line)
   if sample!=len(vectors):raise ValueError('prepare post vector sample order')
   vectors.append(sample);postlines.extend(normalized);order.extend([('p',)]*len(normalized))
  elif line.startswith('intel-prepare-post'):
   if not line.startswith('intel-prepare-post-summary:'):legacy_post=True
   postlines.append(line.replace('intel-prepare-post','intel-completion',1));order.append(('p',))
  elif line.startswith('intel-prepare-plane:'):
   m=re.fullmatch(r'intel-prepare-plane: v=1 s=([01]) p=([0-2]) d=([0-9,]+)',line)
   if not m:raise ValueError('prepare plane encoding')
   planes.append(dict(sample=int(m[1]),pipe=int(m[2]),words=_words(m[3],9)));order.append(('plane',int(m[1]),int(m[2])))
  elif line.startswith('intel-prepare-audio:'):
   m=re.fullmatch(r'intel-prepare-audio: v=1 s=([01]) d=([0-9,]+)',line)
   if not m:raise ValueError('prepare audio encoding')
   audio.append(dict(sample=int(m[1]),words=_words(m[2],10)));order.append(('audio',int(m[1])))
  elif line.startswith('intel-prepare-summary:'):
   sums.append(prepare_summary(line));order.append(('s',))
  elif line.startswith('intel-prepare-'):raise ValueError('unknown prepare receipt')
 if not sums:
  if order:raise ValueError('prepare missing summary')
  return None
 if len(sums)!=1:raise ValueError('prepare duplicate summary')
 s=sums[0];expected=[('r',i) for i in range(s['sample_attempts'])]+[('a',i) for i in range(s['acks'])]+[('p',)]*len(postlines)
 for n in range(s['plane_samples']):expected+=[('plane',n,p) for p in range(3)]+[('audio',n)]
 if order!=expected+[('s',)] or sum(len(r['words'])==6 for r in rows)!=s['snapshots'] or sum(len(r['words']) for r in rows)+s['acks']+s['polls']!=s['reads'] or sum(a['polls'] for a in acks)!=s['polls']:raise ValueError('prepare coverage/order')
 from intel_gt_wake import wake_result
 from intel_completion_audit import completion_result
 from intel_takeover import takeover_result
 wake=wake_result(text)
 if not wake or wake['summary']['v'] not in (14,16):raise ValueError('prepare wrong image')
 if wake['summary']['v']==14 and (actions or vectors or s['reason']>2047):raise ValueError('prepare unexpected action')
 if wake['summary']['v']==16:
  if legacy_post:raise ValueError('prepare held image requires compact post vectors')
  if len(actions)!=1:raise ValueError('prepare action coverage')
  a=actions[0]
  if a['completed']>s['held'] or a['attempted'] and (s['ready']!=31 or not s['begin']<=a['begin'] or not s['borrowed']):raise ValueError('prepare action binding')
  if s['complete'] and not (a['attempted'] and a['completed'] and s['begin']<=a['begin']<=a['end']<s['end']):raise ValueError('prepare action completion')
 pre=completion_result(text);take=takeover_result(text)
 if not pre or not take:raise ValueError('prepare missing prerequisites')
 clocks='\n'.join(l for l in text.splitlines() if l.startswith(('intel-clock:','intel-wake-summary:')))
 post=completion_result('\n'.join(postlines)+'\n'+clocks)
 if not post:raise ValueError('prepare missing preservation census')
 if s['borrowed'] and (not take['summary']['complete'] or not take['summary']['quiescent'] or s['epoch'] or not take['summary']['end_us']<s['begin']<=take['summary']['end_us']+100000):raise ValueError('prepare admission binding')
 valid=lambda v:v!=0xffffffff and not v&0xfffc
 ready=cancelled=0
 for a in acks:
  if valid(a['last']) and a['last']&3==(3 if a['n']<5 else 0):
   if a['n']<5:ready|=1<<a['n']
   else:cancelled|=1<<(9-a['n'])
 # A completed ACK can be followed by a failing post-guard before its bit is
 # accepted. Never infer a positive mask after that failed guard.
 if s['ready']&~ready or s['cancelled']&~cancelled:raise ValueError('prepare derived acknowledgement masks')
 if rows and len(rows[0]['words'])==6 and s['admitted'] and rows[0]['words']!=take['rows'][1]['words']:raise ValueError('prepare entry binding')
 if s['held']:
  if len(rows)<3 or len(rows[1]['words'])!=6 or rows[1]['words']!=rows[2]['words'] or rows[1]['words'][5] or any(not valid(v) or v&3!=3 for v in rows[1]['words'][:5]):raise ValueError('prepare held controls')
 if s['preserved']:
  if not post['summary']['complete'] or pre['rows']!=post['rows'] or pre['contexts']!=post['contexts'] or s['plane_samples']!=2 or s['plane_reads']!=54 or planes[:3]!=[dict(r,sample=0) for r in planes[3:]] or audio[0]['words']!=audio[1]['words']:raise ValueError('prepare raw preservation')
  if len(rows)!=4 or len(rows[3]['words'])!=6 or any(rows[3]['words'][i]&0xffff!=rows[0]['words'][i]&0xffff for i in range(6)):raise ValueError('prepare final controls')
  for a in audio:
   if a['words'][0]!=0x160c8086 or a['words'][1]!=0x040300 or a['words'][9]!=1 or any(v==0xffffffff for v in a['words']):raise ValueError('prepare HDA identity/state')
 complete=bool(not s['reason'] and s['admitted'] and s['held'] and s['preserved'] and s['returned'] and not s['retained'] and not s['uncertain'] and s['attempts']==s['writes']==s['acks']==10 and s['snapshots']==s['sample_attempts']==4 and s['ready']==ready==s['cancelled']==cancelled==31 and 0<s['begin']<s['end']<=s['begin']+100000)
 if s['complete']!=int(complete):raise ValueError('prepare completion declaration')
 if complete and not take['summary']['end_us']<s['begin']<post['summary']['begin_us']<post['summary']['end_us']<s['end']<wake['summary']['end_us']:raise ValueError('prepare preservation ordering')
 if not text.index('intel-takeover-summary:')<text.index('intel-prepare-summary:')<text.index('intel-clock:')<text.index('intel-wake-summary:'):raise ValueError('prepare parent receipt order')
 return dict(summary=s,rows=rows,acks=acks,planes=planes,audio=audio,post=post,core_complete=complete,complete=bool(complete and wake['lease_complete']),native_complete=False,reset_admitted=False,hardware_ownership=False,managed_range=False,physical_tearfree=False)

def native_prepare_result(text):
 r=prepare_result(text)
 if not r or not r['complete']:return r
 from intel_gt_wake import native_wake_result
 w=native_wake_result(text)
 if not w or not w['native_wake_complete']:raise ValueError('prepare native prerequisites')
 r['native_complete']=True;return r
