"""Strict empty stopped-context child;no TD,MAC-start,TX or join acceptance."""
from wifi_power5_log import check as parent,fields,SELECTED,ADDRESSES
from wifi_held_log import require
PROGRESS='wifi-handoff5-progress: frozen stopped parent;fresh empty RX ring;selected power readbacks;exact old context restoration;no TD/MAC start/network OUT'
def check(log):
 require('srvros.wifi-handoff5=1' in log,'missing handoff profile');r=parent(log);lines=log.splitlines();rows=[s for s in lines if s.startswith('wifi-handoff5')]
 require(r['power5_complete'] and r['cal5_completions']==8 and rows and rows[0]==PROGRESS,'missing stopped handoff parent/progress')
 summary=fields(rows[-1],'wifi-handoff5');expected=dict(ok=1,complete=1,retargeted=1,inspected=1,restored=1,unsafe=0,reads=4,commands=2,controls=15,steps=7,generation=1,protected=25 if r.get('cal5_cleanup_enabled') else 17,retained=1,posted=0,network_out=0)
 require(set(summary)==set(expected)|{'old_ring','old_dequeue','new_ring'} and all(summary[k]==v for k,v in expected.items()),'incomplete/unknown handoff completion')
 old_ring,old_dequeue,new_ring=(summary[k] for k in ('old_ring','old_dequeue','new_ring'))
 require(0<old_ring<=2**64-4096 and 0<new_ring<=2**64-4096 and not old_ring%4096 and not new_ring%4096 and old_ring!=new_ring,'invalid/aliased handoff rings')
 require(old_dequeue&15==1 and old_ring<=(old_dequeue&~15)<=old_ring+16*(8+r.get('cal5_cleanup_posts',0)),'invalid old dequeue')
 stops=[r['survey5']['final_stop']] if r.get('survey5') else [fields(s,'wifi-cal5-stop') for s in lines if s.startswith('wifi-cal5-stop:')];require(len(stops)==1 and stops[0]==dict(dict(ok=1,posts=8,state=3,dequeue=old_dequeue,ring=old_ring,retained=1),**({'cleanup':r['cal5_cleanup_posts']} if r.get('cal5_cleanup_enabled') else {})),'handoff differs from exact old stop')
 writes=[fields(s,'wifi-power5-usb') for s in lines if s.startswith('wifi-power5-usb:')];writes=[s for s in writes if s['cleanup']==0 and s['write']==1]
 require(len(writes)==9 and [s['address'] for s in writes]==list(ADDRESSES),'missing held selected power writes');selected=[s['value'] for s in writes]
 pos=1
 def take(prefix,keys):
  nonlocal pos
  require(pos<len(rows)-1,'truncated handoff ledger');d=fields(rows[pos],prefix);pos+=1;require(set(d)==set(keys.split()),'wrong handoff schema');return d
 def context(op):
  d=take('wifi-handoff5-context','op w0 w1 w2 w3 w4 w5 w6 w7');require(d.pop('op')==op and all(0<=v<=0xffffffff for v in d.values()),'wrong context operation/range');return [d['w'+str(i)] for i in range(8)]
 old=context(0);require(old[0]&7==3 and (old[1]>>3)&7==6 and old[1]>>16==512 and old[2]|(old[3]<<32)==old_dequeue,'invalid old context profile')
 slot=0
 def command(op,target):
  nonlocal slot
  d=take('wifi-handoff5-command','op dequeue slot dci ok');require(d['op']==op and d['dequeue']==target and d['dci']==9 and d['ok']==1 and 0<d['slot']<256 and (not slot or d['slot']==slot),'wrong command/slot/dequeue');require(not r.get('cal5_cleanup_slot') or r['cal5_cleanup_slot']==d['slot'],'cleanup Stop slot differs from original handoff');slot=d['slot']
 command(1,new_ring|1);fresh=old.copy();fresh[2]=(new_ring|1)&0xffffffff;fresh[3]=new_ring>>32;require(context(2)==fresh,'fresh context changed other fields')
 for i in range(15):
  d=take('wifi-handoff5-usb','op address value ok');g=i if i<3 else i-12 if i>=12 else 3
  a,v=((0x1004,0),(0x2000,0xf000f200),(0x208,0x40000050))[g] if g<3 else (ADDRESSES[i-3],selected[i-3])
  require(d==dict(op=i,address=a,value=v,ok=1),'wrong selected power/guard read')
 require(context(4)==fresh,'fresh endpoint not identically stopped after inspection');command(5,old_dequeue);require(context(6)==old,'old context not fully restored')
 require(pos==len(rows)-1,'extra handoff rows')
 cleanup=[s for s in lines if s.startswith('wifi-power5-usb:') and fields(s,'wifi-power5-usb')['cleanup']==1];require(cleanup,'missing outer power restoration')
 require(lines.index(SELECTED)<lines.index(PROGRESS)<lines.index(rows[-1])<lines.index(cleanup[0]),'handoff outside selected power lifetime')
 r.update(handoff5_complete=True,handoff5_original_context_restored=True,handoff5_commands=2,handoff5_context_reads=4,handoff5_controls=15,handoff5_posted=0,scope='empty stopped RX context handoff,held selected power readbacks and exact old restoration;no TD,MAC start,network OUT,association,key,IP or data')
 return r
