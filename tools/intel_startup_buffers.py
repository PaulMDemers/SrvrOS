"""v31 original-source private buffers. No address transfer or GPU writer."""
import re
from intel_engine_census import _fields
from intel_startup_source import startup_recipe,startup_result,native_startup_result,verify_startup_checkpoint
TOKEN='srvros.intel-ring=startup-buffers-v1'
OLD='srvros.intel-ring=startup-source-v1'
def buffers_recipe(cmd):
 if not isinstance(cmd,str) or cmd.split().count(TOKEN)!=1 or OLD in cmd.split():raise ValueError('buffers exact recipe')
 return startup_recipe(cmd.replace(TOKEN,OLD))
def buffers_view(text):
 """Checked wire-schema adaptation only; old receivers still refuse raw v31.
 Raw vectors/times/counts are unchanged. Never save this view as raw evidence or
 expose it as a v30 native result. The new receiver validates added child data.
 """
 boots=re.findall(r'^laptop-capture: boot options=(.*)$',text,re.M)
 if len(boots)!=1:raise ValueError('buffers boot coverage')
 buffers_recipe(boots[0])
 marker='init: deferred=startup-buffers-preinit;'
 if text.count(marker)!=1 or 'init: deferred=startup-source-preinit;' in text or 'init: started pid=' in text:raise ValueError('buffers preinit provenance')
 wake=[l for l in text.splitlines() if l.startswith('intel-wake-summary:')]
 if wake and (len(wake)!=1 or not wake[0].startswith('intel-wake-summary: v=31 ')):raise ValueError('buffers image binding')
 return text.replace('laptop-capture: boot options='+boots[0],'laptop-capture: boot options='+boots[0].replace(TOKEN,OLD),1).replace(marker,'init: deferred=startup-source-preinit;',1).replace('intel-wake-summary: v=31 ','intel-wake-summary: v=30 ',1)
FIELDS='v epoch kind post_mode expected origin parent_begin begin end last clock_at clock_prev registered blocked returned complete reason borrows returns entered observed prepared patterns words allocated released dma_before dma_after grant_refused callbacks retained success obligations assurances managed_pages pte_writes hardware_ownership'.split()
ROW='v n physical epoch serial bytes pages state error release_attempts releases active fault replacement original index offset'.split()
def buffers_result(text):
 view=buffers_view(text);source=startup_result(view)
 summaries=[_fields(l,'intel-source-buffers:',FIELDS) for l in text.splitlines() if l.startswith('intel-source-buffers:')]
 rows=[_fields(l,'intel-source-buffer:',ROW) for l in text.splitlines() if l.startswith('intel-source-buffer:')]
 if len(summaries)!=1 or len(rows)!=2 or [r['n'] for r in rows]!=[0,1] or any(r['v']!=1 for r in rows):raise ValueError('buffers coverage')
 typed=[l.split(' ',1)[0] for l in text.splitlines() if l.startswith('intel-source-buffer')]
 if typed!=['intel-source-buffer:','intel-source-buffer:','intel-source-buffers:']:raise ValueError('buffers record order')
 b=summaries[0];s=source['summary'];route=source['route']
 if b['v']!=1 or b['reason']&~31 or b['kind']>2 or b['epoch']>2 or b['post_mode']>2 or b['prepared']>2 or b['patterns']>4 or b['borrows']>1 or b['returns']>b['borrows']:raise ValueError('buffers ranges')
 if any(b[k]>1 for k in ('registered','blocked','returned','complete','entered','observed','grant_refused','retained','success')):raise ValueError('buffers booleans')
 if b['obligations']!=255 or any(b[k] for k in ('assurances','managed_pages','pte_writes','hardware_ownership')):raise ValueError('buffers unsupported grant')
 complete=bool(b['success'])
 if complete:
  if not source['complete'] or not all(b[k] for k in ('blocked','returned','complete','entered','observed','grant_refused')) or any(b[k] for k in ('registered','reason','clock_at','clock_prev','callbacks','retained')):raise ValueError('buffers false completion')
  if (b['epoch'],b['kind'],b['post_mode'],b['expected'],b['origin'],b['parent_begin'])!=(s['epoch'],route['original'],s['post_mode'],s['expected'],s['origin'],s['parent_begin']):raise ValueError('buffers original source binding')
  if b['borrows']!=b['returns'] or b['borrows']!=1 or b['prepared']!=2 or b['patterns']!=4 or b['words']!=5554176 or b['allocated']!=b['released'] or b['allocated']!=2712 or b['dma_before']!=b['dma_after']:raise ValueError('buffers backing lifecycle')
  if not s['read_end']<=b['begin']<b['end']==b['last']<=s['return_begin']<=s['end']<=s['origin']+100000:raise ValueError('buffers original deadline/order')
  if b['end']>s['parent_begin']+300000:raise ValueError('buffers original GT deadline')
  for r in rows:
   if not r['physical'] or r['physical']&4095 or r['physical']>0x100000000-5554176 or r['epoch']!=r['n']+1 or r['serial']!=2 or r['bytes']!=5554176 or r['pages']!=1356 or r['state']!=5 or any(r[k] for k in ('error','active','fault')) or r['release_attempts']!=r['releases'] or r['releases']!=1:raise ValueError('buffers exact private RAM retirement')
   if (r['replacement'],r['original'],r['index'],r['offset'])!=(r['physical']|1,b['expected'],8704,0x811000):raise ValueError('buffers pure one-page plan')
  if not(rows[0]['physical']+rows[0]['bytes']<=rows[1]['physical'] or rows[1]['physical']+rows[1]['bytes']<=rows[0]['physical']):raise ValueError('buffers backing overlap')
 # Restore actual raw image identity on the returned data; never export v30 authority.
 source['reset']['wake']['summary']['v']=31
 return dict(complete=complete,native_complete=False,summary=b,buffers=rows,source=source,hardware_ownership=False,managed_pages=0,physical_tearfree=False)
def native_buffers_result(text):
 p=buffers_result(text)
 if p['complete']:
  if not native_startup_result(buffers_view(text))['native_complete']:raise ValueError('buffers native prerequisites')
  p['native_complete']=True
 return p
def verify_buffers_checkpoint(pre,text,native=False):
 if any(l.startswith('intel-source-buffer') for l in pre.splitlines()):raise ValueError('buffers late prerequisite')
 verify_startup_checkpoint(buffers_view(pre),buffers_view(text),native=native)
 buffers_result(text);return True
