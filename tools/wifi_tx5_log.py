"""Empty dedicated data-OUT endpoint ownership;no frame submission."""
from wifi_peer5_log import check as parent
from wifi_power5_log import fields,ADDRESSES
from wifi_held_log import require
PROGRESS='wifi-tx5-progress: dedicated OUT04 empty ring;configure stop disable;preserve RX/MCU/slot and restore input;no TD/doorbell/MAC/network OUT'
def check(log):
 require('srvros.wifi-tx5-owner=1' in log,'missing TX owner profile');r=parent(log);lines=log.splitlines();rows=[s for s in lines if s.startswith('wifi-tx5')]
 require(len(rows)==24 and rows[0]==PROGRESS,'missing/extra TX ownership ledger')
 s=fields(rows[-1],'wifi-tx5');expected=dict(ok=1,configured=1,stopped=1,disabled=1,input_restored=1,semantic=0,commands=3,controls=15,phase=7,others=4,retained=1,posted=0,doorbells=0,network_out=0)
 require(set(s)==set(expected)|{'ring'} and all(s[k]==v for k,v in expected.items()),'incomplete/unknown TX ownership summary');ring=s['ring']
 old=fields(next(x for x in lines if x.startswith('wifi-handoff5:')),'wifi-handoff5')
 require(0<ring<=2**64-4096 and not ring%4096 and ring not in (old['old_ring'],old['new_ring']),'invalid/aliased TX ring')
 def context(index,op):
  d=fields(rows[index],'wifi-tx5-context');require(set(d)=={'op'}|{'w'+str(i) for i in range(8)} and d.pop('op')==op and all(0<=v<=0xffffffff for v in d.values()),'wrong context schema/range');return [d['w'+str(i)] for i in range(8)]
 require(context(1,0)==[0]*8,'TX endpoint not initially disabled/clear')
 slot=0
 def command(index,op,kind):
  nonlocal slot
  d=fields(rows[index],'wifi-tx5-command');require(set(d)=={'op','type','slot','dci','ok'} and d['op']==op and d['type']==kind and d['dci']==8 and d['ok']==1 and 0<d['slot']<256 and (not slot or slot==d['slot']),'wrong TX endpoint command');slot=d['slot']
 command(2,1,12);configured=[1,(3<<1)|(2<<3)|(512<<16),(ring|1)&0xffffffff,ring>>32,124,0,0,0];require(context(3,2)==configured,'wrong configured TX context')
 selected=[fields(x,'wifi-power5-usb') for x in lines if x.startswith('wifi-power5-usb:')];selected=[x for x in selected if x['cleanup']==0 and x['write']==1];require(len(selected)==9,'missing selected power words')
 for i in range(15):
  g=i if i<3 else i-12 if i>=12 else 3;a,v=((0x1004,0),(0x2000,0xf000f200),(0x208,0x40000050))[g] if g<3 else (ADDRESSES[i-3],selected[i-3]['value'])
  require(fields(rows[4+i],'wifi-tx5-usb')==dict(op=i,address=a,value=v,ok=1),'wrong guard/selected power read')
 command(19,3,15);stopped=configured.copy();stopped[0]=3;require(context(20,4)==stopped,'TX context not identically stopped')
 command(21,5,12);require(context(22,6)[0]&7==0,'TX endpoint not disabled')
 peer=next(x for x in lines if x.startswith('wifi-peer5:'));cleanup=next(x for x in lines if x.startswith('wifi-power5-usb:') and fields(x,'wifi-power5-usb')['cleanup']==1)
 require(lines.index(peer)<lines.index(PROGRESS)<lines.index(rows[-1])<lines.index(cleanup),'TX owner outside copied peer/held power lifetime')
 r.update(tx5_complete=True,tx5_commands=3,tx5_controls=15,tx5_input_restored=True,tx5_disabled=True,scope='dedicated OUT04 empty endpoint configure/stop/disable and shared-input restoration under accepted peer/handoff/power parent;no TD,doorbell,MAC start,network frame,association,key,IP or data')
 return r
