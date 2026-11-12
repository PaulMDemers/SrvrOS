"""Original-owner enabled submission transition; nonzero geometry, no reset."""
import re
from intel_engine_census import _fields
FIELDS='v policy borrowed returned blocked post_bound readers_bound complete retained uncertain reason checks read_attempts reader_attempts reader0 reader1 store_calls store_checks store_fault begin end last source_begin source_end source_last epoch_before epoch_after epoch_advanced parent_attempts clock_at clock_prev store_at store_previous return_begin post_begin post_end post_last bcs_samples bcs_reads bcs_admitted bcs_attempts bcs_writes bcs_disabled bcs_uncertain bcs_reason bcs_begin bcs_end reset_writes managed_pages'.split()
BOOL='policy borrowed returned blocked post_bound readers_bound complete retained uncertain epoch_advanced bcs_admitted bcs_disabled bcs_uncertain'.split()

def ring_recipe(cmd):
 from hardware_boot import limine_config
 if not isinstance(cmd,str) or len(cmd)>2048 or any(c.isspace() and c!=' ' for c in cmd):raise ValueError('ring recipe encoding')
 ts=cmd.split();v=[t.split('=',1)[1] for t in ts if re.fullmatch(r'srvros.workspace-volume=[0-9a-f]{8}',t)];r=[t.split('=',1)[1] for t in ts if re.fullmatch(r'srvros.desktop-release=[0-9a-f]{16}',t)]
 if len(ts)!=15 or len(set(ts))!=15 or len(cmd)+len(ts)>2048 or len(v)!=1 or len(r)!=1:raise ValueError('ring exact15 recipe')
 conf=limine_config(r[0],'ram-usb-v1-intel-ring-transition-laptop-probe-'+v[0]+'-auto-return')
 expected=next(l.split('cmdline:',1)[1].strip().split() for l in conf.splitlines() if 'cmdline:' in l)
 if set(ts)!=set(expected):raise ValueError('ring foreign recipe')
 return dict(volume=v[0],release=r[0])

def ring_summary(line,*,entry=False):
 if line.startswith('intel-ring-summary: v=3 '):
  if not entry:raise ValueError('packed entry on transition')
  import base64,struct
  m=re.fullmatch(r'intel-ring-summary: v=3 b=([A-Za-z0-9+/=]+)',line)
  if not m:raise ValueError('packed entry encoding')
  try:raw=base64.b64decode(m[1],validate=True)
  except ValueError:raise ValueError('packed entry base64')
  keys=FIELDS+['kind']
  if len(raw)!=8*len(keys) or base64.b64encode(raw).decode()!=m[1]:raise ValueError('packed entry size/canonical')
  vs=struct.unpack('<'+'Q'*len(keys),raw)
  if vs[0]!=2:raise ValueError('packed entry schema version')
  line='intel-ring-summary: '+' '.join(f'{k}={v}' for k,v in zip(keys,vs))

 s=_fields(line,'intel-ring-summary:',FIELDS+(['kind'] if entry else []),versions=(2,) if entry else (1,))
 kind=s.get('kind',1);enabled=kind==1;disabled=entry and kind==2
 if s['v']!=(2 if entry else 1) or kind not in ((0,1,2) if entry else (1,)) or any(s[k]>1 for k in BOOL) or s['reason']&~255 or s['checks']>20 or s['read_attempts']>4 or s['reader_attempts']>2 or max(s['reader0'],s['reader1'])>37 or max(s['store_calls'],s['store_checks'],s['store_fault'],s['bcs_attempts'],s['bcs_writes'])>1 or s['bcs_samples']>s['read_attempts'] or s['bcs_reads']%8 or s['bcs_reads']>8*s['read_attempts'] or s['bcs_reason']>8 or max(s['epoch_before'],s['epoch_after'])>=1<<32:
  raise ValueError('ring counts/ranges')
 if s['reset_writes'] or s['managed_pages'] or s['returned']>s['borrowed'] or s['retained']!=s['borrowed']-s['returned'] or s['blocked']>s['borrowed'] or s['bcs_writes']>s['bcs_attempts'] or s['store_checks']>s['store_calls'] or s['store_calls']!=s['epoch_advanced'] or s['epoch_after']!=s['epoch_before']+s['epoch_advanced']:
  raise ValueError('ring authority/lifecycle/epoch')
 if not enabled and any(s[k] for k in ('store_calls','store_checks','store_fault','bcs_attempts','bcs_writes','epoch_advanced','store_at','store_previous')):raise ValueError('disabled entry cannot write/advance epoch')
 if s['bcs_writes'] and (not s['store_checks'] or s['store_fault'] or not s['store_at'] or not s['bcs_admitted']):raise ValueError('ring unbound store')
 if s['bcs_disabled'] and (s['bcs_samples']!=4 or s['bcs_reads']!=32 or s['bcs_reason'] or s['bcs_uncertain'] or enabled and not s['bcs_writes'] or not(enabled or disabled)):raise ValueError('ring false disabled')
 if s['complete']:
  expected=(20 if enabled else 19,4,2,37,37,int(enabled),int(enabled),int(enabled),int(enabled),2,0,int(enabled))
  if not (enabled or disabled) or not all(s[k] for k in ('policy','borrowed','returned','blocked','post_bound','readers_bound','bcs_admitted','bcs_disabled')) or s['epoch_advanced']!=int(enabled) or any(s[k] for k in ('retained','uncertain','reason','store_fault','clock_at','clock_prev','bcs_reason','bcs_uncertain')) or tuple(s[k] for k in ('checks','read_attempts','reader_attempts','reader0','reader1','store_calls','store_checks','bcs_attempts','bcs_writes','parent_attempts','epoch_before','epoch_after'))!=expected:raise ValueError('ring false completion')
  if not 0<s['source_begin']<=s['source_last']<=s['source_end']<s['begin']<=s['bcs_begin']<s['bcs_end']<s['post_begin']<=s['post_last']<=s['post_end']<=s['return_begin']<=s['end']==s['last']<=s['begin']+150000 or s['bcs_end']>s['bcs_begin']+100000:
   raise ValueError('ring original/post/return clocks')
  if enabled and (not s['bcs_begin']<=s['store_previous']<=s['store_at']<=s['bcs_end'] or s['store_at']>s['source_end']+100000):raise ValueError('ring source-age/final-store clock')
  if disabled and s['bcs_end']>s['source_end']+100000:raise ValueError('disabled original source-age')
 return s

def _rows(text,prefix,keys,width):
 rows=[]
 pattern=re.escape(prefix)+r': v=1 '+''.join(re.escape(k)+r'=([0-9]+) ' for k in keys)+r'd=([0-9,]+)'
 from intel_joint import numbers
 for l in text.splitlines():
  if not l.startswith(prefix+':'):continue
  m=re.fullmatch(pattern,l)
  if not m:raise ValueError('ring row encoding')
  values=numbers(m[len(keys)+1],width)
  if any(w>=1<<32 for w in values):raise ValueError('ring DWORD width')
  rows.append(dict(zip(keys,map(int,m.groups()[:len(keys)])),words=values))
 return rows

def _ring_result(text,*,entry=False,composition=False):
 lines=[l for l in text.splitlines() if l.startswith('intel-ring-')]
 if not lines:return None
 allowed=('intel-ring-summary:','intel-ring-bcs:','intel-ring-post-row:','intel-ring-post-context:','intel-ring-post-summary:','intel-ring-reader:','intel-ring-audio:','intel-ring-hda:','intel-ring-owner:')
 if any(not l.startswith(allowed) for l in lines):raise ValueError('ring unknown record')
 if not composition and any(l.startswith(('intel-joint-','intel-claim-','intel-held-','intel-prepare-','intel-rehold-','intel-hda-bcs-','intel-blitter-','intel-shutdown-')) for l in text.splitlines()):raise ValueError('ring foreign operation')
 sums=[ring_summary(l,entry=entry) for l in lines if l.startswith('intel-ring-summary:')]
 if len(sums)!=1:raise ValueError('ring summary coverage')
 s=sums[0]
 from intel_gt_wake import wake_result
 from intel_completion_audit import completion_result
 from intel_joint import joint_capture,ENDPOINT
 w=wake_result(text)
 if not w or w['summary']['v'] not in ((26,27) if composition else (25,) if entry else (24,)):raise ValueError('ring wrong image')
 if any(l.startswith('intel-ring-summary: v=3 ') for l in text.splitlines())!=(w['summary']['v']==27):raise ValueError('entry wire image binding')
 owner=joint_capture(text if composition else text.replace('intel-ring-hda:','intel-joint-hda:').replace('intel-ring-owner:','intel-joint-summary:'))
 if not owner:raise ValueError('ring missing HDA owner')
 h=owner['summary']
 if not composition and (h['complete'] or h['gpu_complete'] or h['attached']):raise ValueError('ring unsupported claim')
 pre=completion_result(text)
 post_text='\n'.join(l.replace('intel-ring-post-','intel-completion-',1) for l in text.splitlines() if l.startswith(('intel-ring-post-','intel-wake-summary:','intel-clock:')))
 post=completion_result(post_text)
 if not pre or not post:raise ValueError('ring full source/post coverage')
 bcs=_rows(text,'intel-ring-bcs',['n'],8);readers=_rows(text,'intel-ring-reader',['n','p'],9);audio=_rows(text,'intel-ring-audio',['n'],10)
 if [r['n'] for r in bcs]!=list(range(s['read_attempts'])) or [(r['n'],r['p']) for r in readers]!=[(n,p) for n in range(s['reader_attempts']) for p in range(3)] or [a['n'] for a in audio]!=list(range(s['reader_attempts'])):raise ValueError('ring row coverage/order')
 # Keep exact interleaving and one final summary; never combine boot halves.
 expected=([] if composition else ['intel-ring-hda:']*len(owner['phases'])+['intel-ring-owner:'])+['intel-ring-bcs:']*len(bcs)
 for n in range(2):
  expected+=['intel-ring-post-row:']*min(5,max(0,post['summary']['groups']-n*5))
  if n<post['summary']['contexts']:expected+=['intel-ring-post-context:']
 expected+=['intel-ring-post-summary:']
 for _ in range(s['reader_attempts']):expected+=['intel-ring-reader:']*3+['intel-ring-audio:']
 expected+=['intel-ring-summary:']
 if [l.split(' ',1)[0] for l in lines]!=expected:raise ValueError('ring record order')
 if not composition and w['summary']['engine_reads']!=pre['summary']['reads']+s['bcs_reads']+post['summary']['reads']:raise ValueError('ring combined read binding')
 if s['source_begin'] and (s['source_begin'],s['source_end'])!=(pre['summary']['begin_us'],pre['summary']['end_us']):raise ValueError('ring replaced original source')
 if s['post_bound'] and (s['post_begin'],s['post_end'])!=(post['summary']['begin_us'],post['summary']['end_us']):raise ValueError('ring replaced fresh post source')
 if s['uncertain'] and (not w['summary']['retained'] or w['summary']['released']):raise ValueError('ring uncertainty released root')
 retired=bool(h['entry'] and not h['admission'] and h['state']==4 and not h['reason'] and not h['pinned'] and not h['retained'] and h['hda_state']==2 and not h['hda_reason'] and h['hda_epoch'] and h['hda_phases']==3 and h['hda_checks']==1 and not h['hda_pins'] and not h['hda_retained'] and h['hda_complete'] and h['hda_attempts']==h['hda_writes']==2 and h['root_maps']==h['root_retired']==1 and h['borrows']==h['returns']==4 and not h['inspection'] and h['cache']==22 and not h['clock_cause'] and not h['hda_clock_cause'])
 complete=bool(s['complete'] and (composition or retired and w['lease_complete']))
 if s['complete']:
  a=pre['summary'];z=post['summary']
  if not a['complete'] or a['enabled']!=(16 if s.get('kind',1)==1 else 0) or not z['complete'] or z['enabled'] or any(a[k] or z[k] for k in ('pending','flip_wait','tlb_pending','reason')) or a['parser_idle']!=3 or z['parser_idle']!=3:raise ValueError('ring fresh census eligibility')
  for n in range(2):
   for e in range(5):
    for i in range(8):
     old=pre['rows'][n*5+e]['w'+str(i)];new=post['rows'][n*5+e]['w'+str(i)]
     if old!=(0x3eef000 if s.get('kind',1)==1 and e==4 and i==2 else 0xf001 if s.get('kind',1)==1 and e==4 and i==3 else 1 if i==4 else 0) or new!=(old&~1 if e==4 and i==3 else old):raise ValueError('ring geometry/full engine preservation')
   if any(pre['contexts'][n]['c'+str(i)]!=post['contexts'][n]['c'+str(i)] for i in range(6)):raise ValueError('ring context preservation')
  for n,row in enumerate(bcs):
   src=pre if n<2 else post;sample=n%2
   values=[src['rows'][sample*5+4]['w'+str(i)] for i in range(6)]+[src['contexts'][sample]['c3'],src['contexts'][sample]['c5']]
   if row['words']!=values:raise ValueError('ring BCS original/fresh source binding')
  if any(v==0xffffffff for row in readers for v in row['words']):raise ValueError('ring floating reader')
  if any(readers[p]['words']!=readers[p+3]['words'] for p in range(3)) or audio[0]['words']!=audio[1]['words'] or audio[0]['words'][:6]!=[0x160c8086,0x040300,2,1,0,1]:raise ValueError('ring plane/HDA preservation')
 if complete and retired:
  if not 0<h['origin']<=h['begin']<=h['hda_begin']<s['source_begin']<s['begin']<s['end']<h['hda_end']<=h['end']==h['last']<=h['origin']+500000 or h['end']>h['hda_begin']+400000:raise ValueError('ring original HDA/outer lifetime')
  phases=owner['phases']
  if len(phases)!=4 or not all(p['quiet'] and p['summary']['complete'] for p in phases) or [p['endpoint']['command'] for p in phases]!=[6,2,2,6] or any(p['samples']!=phases[0]['samples'] for p in phases):raise ValueError('ring HDA inhibition/preservation')
  previous=h['hda_begin']
  for p in phases:
   if not previous<=p['summary']['begin']<p['summary']['end']<=h['hda_end']:raise ValueError('ring HDA phase clock')
   previous=p['summary']['end']
   if any(p['endpoint'][k]!=phases[0]['endpoint'][k] for k in ENDPOINT if k!='command'):raise ValueError('ring HDA resource drift')
  if not phases[1]['summary']['end']<s['source_begin']<s['end']<=phases[2]['summary']['begin'] or h['end']>w['summary']['end_us']:raise ValueError('ring owner phase order')
 if w['summary']['complete'] and not complete:raise ValueError('ring false enclosing completion')
 return dict(complete=complete,native_complete=False,enabled_writer_complete=complete and s.get('kind',1)==1,summary=s,pre=pre,post=post,bcs=bcs,readers=readers,audio=audio,owner=owner,wake=w,reset=False,hardware_ownership=False,managed_pages=0,physical_tearfree=False)

def _native_ring_result(text,*,entry=False):
 r=_ring_result(text,entry=entry)
 if r and r['complete']:
  from intel_gt_wake import native_wake_result
  if not native_wake_result(text)['native_wake_complete']:raise ValueError('ring native prerequisites')
  from intel_display_capture import capture_result
  display=capture_result(text)
  for row in r['readers']:
   old=display['rows']['pipe'][3+row['p']]
   if row['words'][:5]!=[old[k] for k in ('control','surface','live','cursor_control','cursor_base')]:raise ValueError('ring original inherited display binding')
  r['native_complete']=True
 return r

def verify_ring_checkpoint(pre,text,native=False):
 if 'intel-ring-' in pre:raise ValueError('ring late prerequisite checkpoint')
 from intel_gt_wake import verify_wake_checkpoint
 verify_wake_checkpoint(pre,text,native=native)
 boots=[re.findall(r'^laptop-capture: boot options=(.*)$',s,re.M) for s in (pre,text)]
 if any(len(b)!=1 for b in boots) or boots[0]!=boots[1]:raise ValueError('ring boot provenance')
 ring_recipe(boots[0][0])
 if any(s.count('init: deferred=ring-transition-preinit;')!=1 or 'init: started pid=' in s for s in (pre,text)):raise ValueError('ring preinit provenance')
 if not ring_result(text):raise ValueError('ring missing final result')
 return True

def ring_result(text):return _ring_result(text)
def native_ring_result(text):return _native_ring_result(text)
