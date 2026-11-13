"""v32 original-source address-owner bridge. No native address authority."""
import re
from intel_engine_census import _fields
from intel_startup_buffers import buffers_recipe,buffers_view,buffers_result,native_buffers_result,verify_buffers_checkpoint
TOKEN='srvros.intel-ring=startup-transfer-v1'
PREVIOUS='srvros.intel-ring=startup-buffers-v1'
FIELDS='v state reason epoch kind post_mode expected first physical0 physical1 origin parent_begin begin end last clock_at clock_prev registered borrows returns source_blocked holds_blocked refused opened checked closed callbacks clocks owner_state owner_pinned token pool space_pool assurances obligations managed_pages pte_writes hardware_ownership'.split()
def transfer_recipe(cmd):
 if not isinstance(cmd,str) or cmd.split().count(TOKEN)!=1 or PREVIOUS in cmd.split():raise ValueError('transfer exact recipe')
 return buffers_recipe(cmd.replace(TOKEN,PREVIOUS))
def transfer_view(text):
 boots=re.findall(r'^laptop-capture: boot options=(.*)$',text,re.M)
 if len(boots)!=1:raise ValueError('transfer boot coverage')
 transfer_recipe(boots[0])
 marker='init: deferred=startup-transfer-preinit;'
 if text.count(marker)!=1 or 'init: deferred=startup-buffers-preinit;' in text or 'init: started pid=' in text:raise ValueError('transfer preinit provenance')
 wakes=[l for l in text.splitlines() if l.startswith('intel-wake-summary:')]
 if wakes and (len(wakes)!=1 or not wakes[0].startswith('intel-wake-summary: v=32 ')):raise ValueError('transfer image')
 return text.replace('laptop-capture: boot options='+boots[0],'laptop-capture: boot options='+boots[0].replace(TOKEN,PREVIOUS),1).replace(marker,'init: deferred=startup-buffers-preinit;',1).replace('intel-wake-summary: v=32 ','intel-wake-summary: v=31 ',1)
def transfer_result(text):
 view=transfer_view(text);b=buffers_result(view)
 lines=[l for l in text.splitlines() if l.startswith('intel-source-transfer')]
 if len(lines)!=1 or not lines[0].startswith('intel-source-transfer:'):raise ValueError('transfer coverage')
 s=_fields(lines[0],'intel-source-transfer:',FIELDS)
 if s['v']!=1 or s['state']>6 or s['reason']&~31 or s['assurances'] or s['obligations'] not in (0,255):raise ValueError('transfer ranges/native authority')
 if any(s[k] for k in ('managed_pages','pte_writes','hardware_ownership','opened','callbacks','clocks','owner_state','owner_pinned','token','pool','space_pool')):raise ValueError('transfer native unexpected owner/provider activity')
 if any(s[k]>1 for k in ('registered','borrows','returns','source_blocked','holds_blocked','refused','checked','closed')) or s['returns']>s['borrows']:raise ValueError('transfer counts')
 complete=bool(b['complete'])
 if complete:
  p=b['summary'];source=b['source']['summary'];rows=b['buffers']
  if s['state']!=5 or s['reason'] or s['registered'] or s['clock_at'] or s['clock_prev'] or s['borrows']!=s['returns'] or s['borrows']!=1 or s['obligations']!=255 or not all(s[k] for k in ('source_blocked','holds_blocked','refused','checked','closed')):raise ValueError('transfer false completion')
  if tuple(s[k] for k in ('epoch','kind','post_mode','expected','origin','parent_begin'))!=tuple(p[k] for k in ('epoch','kind','post_mode','expected','origin','parent_begin')):raise ValueError('transfer original source/epoch')
  if s['first']!=8704 or (s['physical0'],s['physical1'])!=tuple(x['physical'] for x in rows):raise ValueError('transfer original backing')
  if not p['begin']<=s['begin']<s['end']==s['last']<=p['end']<=source['return_begin'] or s['end']>p['origin']+100000 or s['end']>p['parent_begin']+300000:raise ValueError('transfer original order/deadlines')
 b['source']['reset']['wake']['summary']['v']=32
 return dict(complete=complete,native_complete=False,summary=s,buffers=b,hardware_ownership=False,managed_pages=0,physical_tearfree=False)
def native_transfer_result(text):
 p=transfer_result(text)
 if p['complete']:
  if not native_buffers_result(transfer_view(text))['native_complete']:raise ValueError('transfer native prerequisites')
  p['native_complete']=True
 return p
def verify_transfer_checkpoint(pre,text,native=False):
 if any(l.startswith('intel-source-transfer') for l in pre.splitlines()):raise ValueError('transfer late prerequisite')
 verify_buffers_checkpoint(transfer_view(pre),transfer_view(text),native=native)
 transfer_result(text);return True
def shared_view(text):return buffers_view(transfer_view(text))
