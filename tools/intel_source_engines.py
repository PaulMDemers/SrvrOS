"""v34 maintained readiness control under the actual inhibited source owner."""
import base64,re,struct
from intel_engine_census import _fields
from intel_source_requester import requester_recipe,requester_result,native_requester_result,verify_requester_checkpoint,shared_view as requester_shared_view
TOKEN='srvros.intel-ring=source-engines-v1'
PREVIOUS='srvros.intel-ring=source-requester-v1'
FIELDS='v state reason epoch kind mode origin parent begin last end clock previous child_begin child_end phase step index samples reads attempts writes acks polls borrows returns busy registered blocked holds child child_done restored complete issued native_reads admits uncertain parent_borrows parent_returns obligations assurances managed_pages pte_writes hardware_ownership'.split()
BOOL='busy registered blocked holds child child_done restored complete uncertain'.split()
def engines_recipe(cmd):
 if not isinstance(cmd,str) or cmd.split().count(TOKEN)!=1 or PREVIOUS in cmd.split():raise ValueError('source engines exact recipe')
 return requester_recipe(cmd.replace(TOKEN,PREVIOUS))
def engines_view(text):
 boots=re.findall(r'^laptop-capture: boot options=(.*)$',text,re.M)
 if len(boots)!=1:raise ValueError('source engines boot coverage')
 engines_recipe(boots[0]);marker='init: deferred=source-engines-preinit;'
 if text.count(marker)!=1 or 'init: deferred=source-requester-preinit;' in text or 'init: started pid=' in text:raise ValueError('source engines preinit provenance')
 wakes=[l for l in text.splitlines() if l.startswith('intel-wake-summary:')]
 if wakes and (len(wakes)!=1 or not wakes[0].startswith('intel-wake-summary: v=34 ')):raise ValueError('source engines image')
 return text.replace('laptop-capture: boot options='+boots[0],'laptop-capture: boot options='+boots[0].replace(TOKEN,PREVIOUS),1).replace(marker,'init: deferred=source-requester-preinit;',1).replace('intel-wake-summary: v=34 ','intel-wake-summary: v=33 ',1)
def engines_capture(text):
 lines=[l for l in text.splitlines() if l.startswith('intel-source-engines')]
 if len(lines)!=2 or not lines[0].startswith('intel-source-engines-data: v=1 b=') or not lines[1].startswith('intel-source-engines:'):raise ValueError('source engines receipt coverage/order')
 s=_fields(lines[1],'intel-source-engines:',FIELDS)
 if s['v']!=1 or s['state']>6 or s['reason']&~511 or s['phase']>3 or s['step']>9 or s['index']>4 or any(s[k]>1 for k in BOOL):raise ValueError('source engines schema/range')
 if s['obligations']!=255 or any(s[k] for k in ('assurances','managed_pages','pte_writes','hardware_ownership')):raise ValueError('source engines unexpected authority')
 if s['attempts']>10 or s['writes']>s['attempts'] or s['acks']>s['writes'] or s['polls']>2560 or s['admits']>s['attempts'] or not s['writes']<=s['issued']<=s['writes']+1 or s['issued']>s['attempts'] or s['borrows']>1 or s['returns']>s['borrows'] or s['parent_borrows']>1 or s['parent_returns']>s['parent_borrows']:raise ValueError('source engines counts')
 encoded=lines[0].split('b=',1)[1]
 try:raw=base64.b64decode(encoded,validate=True)
 except ValueError as e:raise ValueError('source engines base64') from e
 if len(raw)!=624 or base64.b64encode(raw).decode()!=encoded:raise ValueError('source engines canonical data')
 samples=[]
 for n in range(4):
  v=struct.unpack_from('<6I2QB',raw,n*41);samples.append(dict(words=list(v[:6]),begin=v[6],end=v[7],count=v[8]))
 phases=[]
 for n in range(10):
  v=struct.unpack_from('<3I4QH',raw,164+n*46);phases.append(dict(engine=n if n<5 else 9-n,ack=v[0],final=v[1],admission=v[2],begin=v[3],end=v[4],at=v[5],previous=v[6],polls=v[7]))
 if any(p['count']>6 for p in samples) or s['samples']!=sum(p['count']==6 for p in samples) or s['reads']!=sum(p['count'] for p in samples)+s['writes']+s['polls']:raise ValueError('source engines raw accounting')
 if not s['reads']+s['admits']<=s['native_reads']<=s['reads']+s['admits']+1:raise ValueError('source engines native accounting')
 for p in samples:
  if any(p['words'][p['count']:]):raise ValueError('source engines sample suffix')
  if p['end'] and not s['begin']<=p['begin']<=p['end']<=s['last']:raise ValueError('source engines sample clock')
 for n,p in enumerate(phases):
  if p['polls']>256 or n>=s['attempts'] and any(v for k,v in p.items() if k!='engine'):raise ValueError('source engines phase suffix')
 if s['polls']!=sum(p['polls'] for p in phases):raise ValueError('source engines poll accounting')
 if s['clock'] or s['previous']:
  if not s['reason']&4 or not s['previous']:raise ValueError('source engines unexplained clock')
 if s['state']==6 and (not s['reason'] or not s['registered'] or s['returns']):raise ValueError('source engines lost fault registration')
 if s['returns'] and (s['registered'] or not s['restored'] or s['parent_returns']!=s['parent_borrows']):raise ValueError('source engines early return')
 if s['complete']:
  if s['state']!=5 or s['phase']!=3 or s['step']!=9 or s['index']!=0 or any(s[k] for k in ('reason','uncertain','busy','registered','clock','previous')) or not all(s[k] for k in ('blocked','holds','child','child_done','restored')):raise ValueError('source engines false completion')
  if any(s[k]!=1 for k in ('borrows','returns','parent_borrows','parent_returns')) or any(s[k]!=10 for k in ('attempts','writes','acks','admits','issued')) or s['samples']!=4:raise ValueError('source engines missing control phases')
  if s['end']!=s['last'] or not s['begin']<s['end']<=s['origin']+100000 or s['end']>s['parent']+300000:raise ValueError('source engines original deadline')
  for n,p in enumerate(samples):
   if p['count']!=6 or p['words'][5] or any(v==0xffffffff or v&0xfffc or v&3!=(3 if n in (1,2) else 0) for v in p['words'][:5]):raise ValueError('source engines control profile')
   if n and any((a&~3)!=(b&~3) for a,b in zip(p['words'],samples[0]['words'])):raise ValueError('source engines control drift')
   if n==2 and p['words']!=samples[1]['words'] or n==3 and p['words']!=samples[0]['words']:raise ValueError('source engines restoration mismatch')
   if not s['begin']<=p['begin']<=p['end']<=s['end']:raise ValueError('source engines sample order')
  previous=samples[0]['end']
  for n,p in enumerate(phases):
   if n==5:previous=samples[2]['end']
   bound=samples[1]['begin'] if n<5 else samples[3]['begin'];e=p['engine'];original=samples[0]['words'][e];ack=p['ack'];want=3 if n<5 else 0
   if p['admission']!=samples[0 if n<5 else 2]['words'][e] or ack==0xffffffff or ack&0xfffc or (ack&1)!=(n<5) or (ack&~3)!=(original&~3) or p['final']!=(original&~3)|want:raise ValueError('source engines transition profile')
   if bool(p['polls'])!=bool((ack&3)!=want):raise ValueError('source engines acknowledgement polls')
   if not previous<=p['begin']<=p['previous']<=p['at']<=p['end']<=bound or p['end']-p['begin']>10000:raise ValueError('source engines phase timing/order')
   previous=p['end']
  if not samples[1]['end']<=s['child_begin']<=s['child_end']<=samples[2]['begin']:raise ValueError('source engines child outside held state')
 return dict(summary=s,samples=samples,phases=phases,scope_complete=bool(s['complete']))
def engines_result(text):
 p=requester_result(engines_view(text));r=engines_capture(text);s=r['summary'];a=p['summary']
 if s['begin'] and (tuple(s[k] for k in ('origin','parent','epoch','kind','mode'))!=tuple(a[k] for k in ('origin','parent_begin','epoch','kind','post_mode')) or s['begin']<a['child_begin']):raise ValueError('source engines original requester binding')
 complete=bool(p['complete'] and r['scope_complete'])
 if p['complete'] and not complete:raise ValueError('source engines false requester completion')
 if complete and s['end']>a['child_end']:raise ValueError('source engines parent return order')
 p['transfer']['buffers']['source']['reset']['wake']['summary']['v']=34
 return dict(r,requester=p,complete=complete,native_complete=False,managed_pages=0,hardware_ownership=False,physical_tearfree=False)
def native_engines_result(text):
 r=engines_result(text)
 if r['complete']:
  if not native_requester_result(engines_view(text))['native_complete']:raise ValueError('source engines native original chain')
  r['native_complete']=True
 return r
def verify_engines_checkpoint(pre,text,native=False):
 if any(l.startswith('intel-source-engines') for l in pre.splitlines()):raise ValueError('source engines late prerequisite')
 verify_requester_checkpoint(engines_view(pre),engines_view(text),native=native);engines_result(text);return True
def shared_view(text):return requester_shared_view(engines_view(text))
