"""Separate v28 original-entry nonzero reset. Receipts grant no live authority."""
import base64,re,struct
from intel_engine_census import _fields
S='v policy eligible blocked epoch_before epoch_after epoch_advanced admission_attempts control_samples bcs_samples bcs0 bcs1 readers controls control_writes control_acks resets reset_writes reset_acks control_reads bcs_reads checks stage index post_mode reason preserved returned retained uncertain complete native_controls native_resets obligations managed_pages pte_writes hardware_ownership'.split()
T='v source_begin source_end source_last entry_end parent_begin admit_begin admit_end begin end last settle_begin settle_end guard_begin guard_end return_begin clock_at clock_prev'.split()
A='v n ack final polls begin end store previous'.split()
B='policy eligible blocked epoch_advanced preserved returned retained uncertain complete'.split()
def nonzero_recipe(cmd):
 from hardware_boot import limine_config
 if not isinstance(cmd,str) or len(cmd)>2048 or any(c.isspace() and c!=' ' for c in cmd):raise ValueError('nonzero recipe encoding')
 ts=cmd.split();vs=[t.split('=',1)[1] for t in ts if re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',t)];rs=[t.split('=',1)[1] for t in ts if re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',t)]
 if len(ts)!=15 or len(set(ts))!=15 or len(cmd)+len(ts)>2048 or len(vs)!=1 or len(rs)!=1:raise ValueError('nonzero exact15 recipe')
 conf=limine_config(rs[0],'ram-usb-v1-intel-nonzero-reset-laptop-probe-'+vs[0]+'-auto-return')
 expected=next(l.split('cmdline:',1)[1].strip().split() for l in conf.splitlines() if 'cmdline:' in l)
 if set(ts)!=set(expected):raise ValueError('nonzero foreign recipe')
 return dict(volume=vs[0],release=rs[0])
def _entry(text):
 from intel_engine_prepare import post_vector
 from intel_completion_audit import entry_census_lines
 from intel_ring_transition import _ring_result
 if any(l.startswith(('intel-ring-','intel-reset-','intel-entry-census:','intel-completion-row:','intel-completion-context:')) for l in text.splitlines()):raise ValueError('nonzero mixed entry image')
 out=[]
 if 'intel-nonzero-census:' in text:out+=entry_census_lines(text,nonzero=True)
 for l in text.splitlines():
  if l.startswith('intel-nonzero-census:'):continue
  if l.startswith('intel-nonzero-entry-post-vector:'):
   _,rows=post_vector(l.replace('intel-nonzero-entry-post-vector:','intel-prepare-post-vector:',1))
   out.extend(row.replace('intel-completion-','intel-ring-post-',1) for row in rows)
  elif l.startswith('intel-nonzero-entry-readers:'):
   m=re.fullmatch(r'intel-nonzero-entry-readers: v=1 n=([01]) b=([A-Za-z0-9+/=]+)',l)
   if not m:raise ValueError('nonzero entry readers encoding')
   raw=base64.b64decode(m[2],validate=True)
   if len(raw)!=148 or base64.b64encode(raw).decode()!=m[2]:raise ValueError('nonzero entry readers size')
   ws=struct.unpack('<37I',raw);n=int(m[1])
   for p in range(3):out.append(f'intel-ring-reader: v=1 n={n} p={p} d='+','.join(map(str,ws[p*9:p*9+9])))
   out.append(f'intel-ring-audio: v=1 n={n} d='+','.join(map(str,ws[27:])))
  else:out.append(l.replace('intel-nonzero-entry-','intel-ring-',1) if l.startswith('intel-nonzero-entry-') else l)
 return _ring_result('\n'.join(out),entry=True,nonzero=True)
def _vector(line,kind,width):
 m=re.fullmatch(re.escape('intel-nonzero-'+kind)+r': v=1 n=([0-9]) count=([0-9]{1,2}) b=([A-Za-z0-9+/=]+|-)',line)
 if not m:raise ValueError('nonzero vector encoding')
 n,count=int(m[1]),int(m[2])
 if count>width:raise ValueError('nonzero vector range')
 if not count:
  if m[3]!='-':raise ValueError('nonzero empty vector')
  return n,[]
 raw=base64.b64decode(m[3],validate=True)
 if len(raw)!=4*count or base64.b64encode(raw).decode()!=m[3]:raise ValueError('nonzero vector canonical/size')
 return n,list(struct.unpack('<'+'I'*count,raw))
def nonzero_result(text, *, source=False):
 if 'intel-nonzero-summary:' not in text:
  if any(l.startswith('intel-nonzero-') for l in text.splitlines()):raise ValueError('nonzero missing summary')
  return None
 from intel_gt_wake import wake_result
 from intel_completion_audit import completion_result
 w=wake_result(text)
 if not w or w['summary']['v']!=(29 if source else 28):raise ValueError('nonzero wrong image')
 boots=re.findall(r'^laptop-capture: boot options=(.*)$',text,re.M)
 if len(boots)!=1:raise ValueError('nonzero recipe coverage')
 if source:
  from intel_nonzero_source import source_recipe
  source_recipe(boots[0])
 else:nonzero_recipe(boots[0])
 e=_entry(text)
 if not e:raise ValueError('nonzero original entry missing')
 lines=[l for l in text.splitlines() if l.startswith('intel-nonzero-') and not l.startswith(('intel-nonzero-entry-','intel-nonzero-census:'))]
 kinds=[('admit',2,6),('control',5,6),('bcs',2,8),('domain',1,4)]
 expected=[];vectors={}
 for k,n,width in kinds:
  vs=[_vector(l,k,width) for l in lines if l.startswith('intel-nonzero-'+k+':')]
  if [a for a,_ in vs]!=list(range(n)):raise ValueError('nonzero vector coverage/order')
  vectors[k]=[v for _,v in vs];expected+=['intel-nonzero-'+k+':']*n
 acks=[_fields(l,'intel-nonzero-ack:',A) for l in lines if l.startswith('intel-nonzero-ack:')]
 pulses=[_fields(l,'intel-nonzero-pulse:',A) for l in lines if l.startswith('intel-nonzero-pulse:')]
 if [a['n'] for a in acks]!=list(range(10)) or [a['n'] for a in pulses]!=[0,1]:raise ValueError('nonzero transition coverage')
 expected+=['intel-nonzero-ack:']*10+['intel-nonzero-pulse:']*2
 for k,n,width in [('post',2,46),('readers',1,37)]:
  vs=[_vector(l,k,width) for l in lines if l.startswith('intel-nonzero-'+k+':')]
  if [a for a,_ in vs]!=list(range(n)):raise ValueError('nonzero post/reader coverage')
  vectors[k]=[v for _,v in vs];expected+=['intel-nonzero-'+k+':']*n
 expected+=['intel-nonzero-post-summary:','intel-nonzero-time:','intel-nonzero-summary:']
 if [l.split(' ',1)[0] for l in lines]!=expected:raise ValueError('nonzero record order/unknown')
 s=_fields(lines[-1],'intel-nonzero-summary:',S);t=_fields(lines[-2],'intel-nonzero-time:',T)
 if any(s[k]>1 for k in B) or s['reason']&~1023 or s['obligations']!=255 or any(s[k] for k in ('managed_pages','pte_writes','hardware_ownership')):raise ValueError('nonzero counts/authority')
 if s['stage']>10 or s['index']>4 or s['post_mode']>2 or s['admission_attempts']>2 or s['control_samples']>5 or s['bcs_samples']>2 or max(s['bcs0'],s['bcs1'])>9 or s['readers']>37 or s['controls']>10 or s['resets']>2 or s['checks']>10000:raise ValueError('nonzero ranges')
 if not 0<=s['control_acks']<=s['control_writes']<=s['controls'] or not 0<=s['reset_acks']<=s['reset_writes']<=s['resets'] or s['native_controls']>s['controls'] or s['native_resets']>s['resets']:raise ValueError('nonzero issue lifecycle')
 if s['eligible']:
  a=e['summary']
  if not a['complete'] or a['kind']!=1 or s['epoch_before']!=1 or s['epoch_after']!=1+s['epoch_advanced'] or (t['source_begin'],t['source_end'],t['source_last'],t['entry_end'])!=(a['post_begin'],a['post_end'],a['post_last'],a['end']):raise ValueError('nonzero original source/epoch')
 if s['uncertain'] and (not w['summary']['retained'] or w['summary']['released']):raise ValueError('nonzero lost uncertain root')
 if bool(s['uncertain'])!=bool(s['eligible'] and not s['complete']):raise ValueError('nonzero uncertainty lifecycle')
 if s['returned'] and s['retained'] or s['complete'] and (s['uncertain'] or s['reason']):raise ValueError('nonzero return lifecycle')
 for row in acks+pulses:
  if not row['polls'] and row['ack']!=row['final']:raise ValueError('nonzero unobserved ACK transition')
  if row['polls']>255 or max(row['ack'],row['final'])>=1<<32:raise ValueError('nonzero ACK range')
 # Expand only the new wire's raw post census into the established typed schema.
 from intel_engine_prepare import post_vector
 post_lines=[]
 for n,ws in enumerate(vectors['post']):
  if ws:
   if len(ws) not in (8,16,24,32,40,46):raise ValueError('nonzero post prefix')
   _,rows=post_vector(f'intel-prepare-post-vector: v=3 s={n} n={len(ws)} h='+''.join(f'{v:08x}' for v in ws));post_lines+=rows
 post_lines.append(lines[-3].replace('intel-nonzero-post-summary:','intel-completion-summary:',1))
 post_lines += [l for l in text.splitlines() if l.startswith(('intel-clock:','intel-wake-summary:'))]
 post=completion_result('\n'.join(post_lines))
 if len(vectors['readers'][0])!=s['readers'] or [len(v) for v in vectors['bcs']]!=[min(s[k],8) for k in ('bcs0','bcs1')]:raise ValueError('nonzero raw count binding')
 extra=0
 if source:
  from intel_nonzero_source import source_summary
  extra=source_summary(text)['native_reads']
 if w['summary']['engine_reads']!=extra+e['pre']['summary']['reads']+e['summary']['bcs_reads']+e['post']['summary']['reads']+s['control_reads']+s['bcs_reads']+post['summary']['reads']:raise ValueError('nonzero total reads')
 if s['complete']:
  if not all(s[k] for k in ('policy','eligible','blocked','epoch_advanced','preserved','returned')) or s['retained'] or s['stage']!=10 or s['index']!=0 or s['post_mode'] not in (1,2) or tuple(s[k] for k in ('admission_attempts','control_samples','bcs_samples','bcs0','bcs1','readers','controls','control_writes','control_acks','resets','reset_writes','reset_acks','native_controls','native_resets'))!=(2,5,2,9,9,37,10,10,10,2,2,2,10,2):raise ValueError('nonzero false completion')
  if not 0<t['source_begin']<=t['source_last']<=t['source_end']<t['entry_end']<=t['admit_begin']<t['admit_end']<=t['begin']<t['settle_begin']<t['settle_end']<t['guard_begin']<=t['guard_end']<=t['return_begin']<=t['end']==t['last']<=t['begin']+100000 or t['begin']>t['admit_begin']+100000 or t['begin']>t['source_end']+100000 or t['clock_at'] or t['clock_prev'] or not 50<=t['settle_end']-t['settle_begin']<=10000:raise ValueError('nonzero deadlines/order')
  if not 0<t['parent_begin']<t['source_begin'] or t['end']>t['parent_begin']+300000:raise ValueError('nonzero GT original deadline')
  if not w['lease_complete'] or not e['complete'] or not post['summary']['complete'] or post['summary']['enabled'] or not t['begin']<post['summary']['begin_us']<post['summary']['end_us']<=t['guard_begin']:raise ValueError('nonzero enclosing/post completion')
  ad,cs,bc=vectors['admit'],vectors['control'],vectors['bcs']
  if any(len(v)!=6 for v in ad+cs) or ad[0]!=ad[1] or cs[0]!=ad[1] or cs[1]!=cs[2] or any(v&0xffff for v in ad[0]) or any((v&0xffff)!=3 for v in cs[1][:5]) or cs[1][5] or cs[3][:4]!=cs[2][:4] or cs[3][4]&0xffff or cs[3][5] or any(v&0xffff for v in cs[4]) or any(v==0xffffffff or v&0xfffc for row in ad+cs for v in row[:5]):raise ValueError('nonzero readiness/post/cancel controls')
  old=e['post'];bs=[old['rows'][9]['w'+str(i)] for i in range(6)]+[old['contexts'][1][k] for k in ('c3','c5')]
  target=list(bs)
  if s['post_mode']==2:target[2]=target[3]=0
  if bc!=[bs,target] or vectors['domain']!=[[0,0,0,0]]:raise ValueError('nonzero exact BCS post classification')
  for n in range(2):
   for eng in range(5):
    for i in range(8):
     target_word=old['rows'][5*n+eng]['w'+str(i)]
     if s['post_mode']==2 and eng==4 and i in (2,3):target_word=0
     if post['rows'][5*n+eng]['w'+str(i)]!=target_word:raise ValueError('nonzero full engine preservation')
   if any(post['contexts'][n]['c'+str(i)]!=old['contexts'][n]['c'+str(i)] for i in range(6)):raise ValueError('nonzero full context preservation')
  readers=[v for row in e['readers'][3:] for v in row['words']]+e['audio'][1]['words']
  if vectors['readers'][0]!=readers:raise ValueError('nonzero reader preservation')
  sequence=acks[:5]+pulses+acks[5:];previous=t['begin']
  for i,row in enumerate(sequence):
   reset=5<=i<7;want=0 if reset or i>=7 else 3
   if not previous<=row['begin']<=row['previous']<=row['store']<=row['end']<=row['begin']+10000 or row['final']&(0xffffffff if reset else 0xffff)!=want or reset and row['ack']&~8 or not reset and (row['ack']&0xfffc or row['final']&0xfffc):raise ValueError('nonzero final-store/ACK phase')
   previous=row['end']
  if not pulses[-1]['end']<=t['settle_begin'] or acks[-1]['end']>post['summary']['begin_us']:raise ValueError('nonzero reset/settle/census order')
  expected_control=42+sum(1+v['polls'] for v in acks)
  expected_bcs=18+sum(1+v['polls'] for v in pulses)
  if (s['control_reads'],s['bcs_reads'])!=(expected_control,expected_bcs):raise ValueError('nonzero read accounting')
  owner=e['owner']['summary']
  if not t['end']<owner['hda_end'] or t['end']>owner['origin']+500000 or owner['end']>owner['hda_begin']+400000:raise ValueError('nonzero root deadline')
 if w['summary']['complete'] and not s['complete']:raise ValueError('nonzero false outer completion')
 return dict(complete=bool(s['complete']),native_complete=False,entry=e,summary=s,time=t,vectors=vectors,acks=acks,pulses=pulses,post=post,wake=w,reset=bool(s['complete']),hardware_ownership=False,managed_pages=0,physical_tearfree=False)
def native_nonzero_result(text):
 r=nonzero_result(text)
 if r and r['complete']:
  from intel_gt_wake import native_wake_result
  if not native_wake_result(text)['native_wake_complete']:raise ValueError('nonzero native prerequisites')
  r['native_complete']=True
 return r
def verify_nonzero_checkpoint(pre,text,native=False, *, source=False):
 from intel_gt_wake import verify_wake_checkpoint
 if 'intel-nonzero-summary:' in pre or 'intel-nonzero-entry-summary:' in pre:raise ValueError('nonzero late prerequisite')
 verify_wake_checkpoint(pre,text,native=native)
 boots=[re.findall(r'^laptop-capture: boot options=(.*)$',s,re.M) for s in (pre,text)]
 if any(len(b)!=1 for b in boots) or boots[0]!=boots[1]:raise ValueError('nonzero boot provenance')
 if source:
  from intel_nonzero_source import source_recipe
  source_recipe(boots[0][0])
 else:nonzero_recipe(boots[0][0])
 marker='init: deferred=nonzero-source-preinit;' if source else 'init: deferred=nonzero-reset-preinit;'
 if any(s.count(marker)!=1 or 'init: started pid=' in s for s in (pre,text)):raise ValueError('nonzero preinit provenance')
 if not nonzero_result(text,source=source):raise ValueError('nonzero missing final result')
 return True
