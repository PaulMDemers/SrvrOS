"""v33 original-source maintained PCI control; no memory/completion grant."""
import base64,re,struct
from intel_engine_census import _fields
from intel_gpu_hold import _prefix
from intel_startup_transfer import transfer_recipe,transfer_result,native_transfer_result,verify_transfer_checkpoint,shared_view as transfer_shared_view
TOKEN='srvros.intel-ring=source-requester-v1'
PREVIOUS='srvros.intel-ring=startup-transfer-v1'
FIELDS='v state reason epoch kind post_mode origin parent_begin begin last end clock_at clock_prev drain_begin child_begin child_end phase samples reads attempts writes polls borrows returns busy registered parent_blocked holds_blocked child_entered child_complete restored complete issued native_reads admit_reads admit0 admit1 store0 previous0 store1 previous1 uncertain transfer_borrows transfer_returns batch_entered batch_checked obligations assurances managed_pages pte_writes hardware_ownership'.split()
BOOL='busy registered parent_blocked holds_blocked child_entered child_complete restored complete uncertain batch_entered batch_checked'.split()
def requester_recipe(cmd):
 if not isinstance(cmd,str) or cmd.split().count(TOKEN)!=1 or PREVIOUS in cmd.split():raise ValueError('source requester exact recipe')
 return transfer_recipe(cmd.replace(TOKEN,PREVIOUS))
def requester_view(text):
 boots=re.findall(r'^laptop-capture: boot options=(.*)$',text,re.M)
 if len(boots)!=1:raise ValueError('source requester boot coverage')
 requester_recipe(boots[0])
 marker='init: deferred=source-requester-preinit;'
 if text.count(marker)!=1 or 'init: deferred=startup-transfer-preinit;' in text or 'init: started pid=' in text:raise ValueError('source requester preinit provenance')
 wakes=[l for l in text.splitlines() if l.startswith('intel-wake-summary:')]
 if wakes and (len(wakes)!=1 or not wakes[0].startswith('intel-wake-summary: v=33 ')):raise ValueError('source requester image')
 return text.replace('laptop-capture: boot options='+boots[0],'laptop-capture: boot options='+boots[0].replace(TOKEN,PREVIOUS),1).replace(marker,'init: deferred=startup-transfer-preinit;',1).replace('intel-wake-summary: v=33 ','intel-wake-summary: v=32 ',1)
def requester_capture(text):
 lines=[l for l in text.splitlines() if l.startswith('intel-source-requester')]
 if len(lines)!=2 or not lines[0].startswith('intel-source-requester-data: v=1 b=') or not lines[1].startswith('intel-source-requester:'):raise ValueError('source requester receipt coverage/order')
 s=_fields(lines[1],'intel-source-requester:',FIELDS)
 if s['v']!=1 or s['state']>6 or s['reason']&~511 or s['phase']>3 or any(s[k]>1 for k in BOOL):raise ValueError('source requester schema/range')
 if s['obligations']!=255 or any(s[k] for k in ('assurances','managed_pages','pte_writes','hardware_ownership')):raise ValueError('source requester unexpected authority')
 if s['attempts']>2 or s['writes']>s['attempts'] or s['issued']!=s['writes'] or s['admit_reads']>s['attempts'] or s['polls']>32 or s['borrows']>1 or s['returns']>s['borrows'] or s['transfer_borrows']>1 or s['transfer_returns']>s['transfer_borrows']:raise ValueError('source requester counts')
 encoded=lines[0].split('b=',1)[1]
 try:raw=base64.b64decode(encoded,validate=True)
 except ValueError as e:raise ValueError('source requester base64') from e
 if len(raw)!=756 or base64.b64encode(raw).decode()!=encoded:raise ValueError('source requester canonical data')
 phases=[]
 for n in range(4):
  v=struct.unpack_from('<9I2QB',raw,n*53)
  phases.append(dict(words=list(v[:9]),begin=v[9],end=v[10],count=v[11]))
 polls=[dict(zip(('command','pending','at','count'),struct.unpack_from('<2IQB',raw,212+n*17))) for n in range(32)]
 if any(p['count']>9 for p in phases) or any(p['count']>2 for p in polls):raise ValueError('source requester prefix counts')
 if s['samples']!=sum(p['count']==9 for p in phases) or s['reads']!=sum(p['count'] for p in phases+polls) or s['native_reads']!=s['reads']+s['admit_reads']:raise ValueError('source requester raw accounting')
 for p in phases:
  n=p['count'];w=p['words']
  if any(w[n:]):raise ValueError('source requester profile suffix')
  for i in range(1,n+1):
   if not _prefix(w[:i]) and (i!=n or not s['reason']&(16|32)):raise ValueError('source requester malformed capability prefix')
  if p['end'] and not s['begin']<=p['begin']<=p['end']<=s['last']:raise ValueError('source requester profile clock')
 for n,p in enumerate(polls):
  if n>=s['polls'] and any(p.values()) or any(p[k] for k in ('command','pending')[p['count']:]):raise ValueError('source requester pending suffix')
  if p['at'] and not s['drain_begin']<=p['at']<=s['last']:raise ValueError('source requester pending clock')
 if s['clock_at'] or s['clock_prev']:
  if not s['reason']&4 or not s['clock_prev']:raise ValueError('source requester unexplained clock')
 if s['state']==6 and (not s['reason'] or not s['registered'] or not s['uncertain'] or s['returns']):raise ValueError('source requester lost fault hold')
 if s['returns'] and (s['registered'] or not s['restored'] or s['transfer_returns']!=s['transfer_borrows']):raise ValueError('source requester early return')
 if s['complete']:
  if s['state']!=5 or s['phase']!=3 or s['reason'] or s['uncertain'] or s['busy'] or s['registered'] or s['clock_at'] or s['clock_prev'] or not all(s[k] for k in ('parent_blocked','holds_blocked','child_entered','child_complete','restored')):raise ValueError('source requester false completion')
  if any(s[k]!=1 for k in ('borrows','returns','transfer_borrows','transfer_returns')) or s['attempts']!=s['writes'] or s['writes']!=2 or s['samples']!=4 or s['admit_reads']!=2:raise ValueError('source requester incomplete phases')
  if s['end']!=s['last'] or not s['begin']<s['end']<=s['origin']+100000 or s['end']>s['parent_begin']+300000:raise ValueError('source requester original deadline')
  for n,p in enumerate(phases):
   w=p['words']
   if p['count']!=9 or not _prefix(w) or (w[1]&65535)!=(3 if n in (1,2) else 7):raise ValueError('source requester phase command')
   if n and any((w[i]&(~4 if i==1 else 0xffffffff))!=(phases[0]['words'][i]&(~4 if i==1 else 0xffffffff)) for i in range(8)):raise ValueError('source requester configuration drift')
   if not s['begin']<=p['begin']<=p['end']<=s['end'] or n and phases[n-1]['end']>p['begin']:raise ValueError('source requester phase ordering')
  if phases[2]['words'][8] or phases[3]['words'][8] or s['admit0']!=phases[0]['words'][1] or s['admit1']!=phases[2]['words'][1]:raise ValueError('source requester restore admission')
  if not phases[0]['end']<=s['previous0']<=s['store0']<=phases[1]['begin'] or not phases[2]['end']<=s['previous1']<=s['store1']<=phases[3]['begin']:raise ValueError('source requester final store clocks')
  if s['drain_begin']!=phases[1]['end'] or bool(phases[1]['words'][8])!=bool(s['polls']):raise ValueError('source requester drain source')
  if not s['drain_begin']<=s['child_begin']<=s['child_end']<=phases[2]['begin']:raise ValueError('source requester child outside inhibition')
  previous=s['drain_begin']
  for n,p in enumerate(polls[:s['polls']]):
   if p['count']!=2 or p['command']!=phases[1]['words'][1] or p['pending']!=(0 if n==s['polls']-1 else 256) or not previous<=p['at']<=s['drain_begin']+10000 or p['at']>s['child_begin']:raise ValueError('source requester pending completion')
   previous=p['at']
 return dict(summary=s,phases=phases,polls=polls[:s['polls']],scope_complete=bool(s['complete']))
def requester_result(text):
 t=transfer_result(requester_view(text));r=requester_capture(text);s=r['summary'];p=t['summary']
 if s['begin'] and (tuple(s[k] for k in ('origin','parent_begin','epoch','kind','post_mode'))!=tuple(p[k] for k in ('origin','parent_begin','epoch','kind','post_mode')) or s['begin']<p['begin']):raise ValueError('source requester original transfer binding')
 complete=bool(t['complete'] and r['scope_complete'] and s['batch_entered'] and s['batch_checked'])
 if t['complete'] and not complete:raise ValueError('source requester false parent completion')
 if complete and (s['end']>p['end'] or s['end']>t['buffers']['source']['summary']['return_begin']):raise ValueError('source requester original return order')
 t['buffers']['source']['reset']['wake']['summary']['v']=33
 return dict(r,transfer=t,complete=complete,native_complete=False,managed_pages=0,hardware_ownership=False,physical_tearfree=False)
def native_requester_result(text):
 r=requester_result(text)
 if r['complete']:
  if not native_transfer_result(requester_view(text))['native_complete']:raise ValueError('source requester native original chain')
  r['native_complete']=True
 return r
def verify_requester_checkpoint(pre,text,native=False):
 if any(l.startswith('intel-source-requester') for l in pre.splitlines()):raise ValueError('source requester late prerequisite')
 verify_transfer_checkpoint(requester_view(pre),requester_view(text),native=native)
 requester_result(text);return True
def shared_view(text):return transfer_shared_view(requester_view(text))
