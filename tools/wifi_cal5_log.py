"""Strict selected161 calibration/MAC/retained receive ledger;not connectivity."""
import re
from wifi_channel_vco5_log import check as parent,child_receipt,legacy_receipt,HOOK
from wifi_trim5_log import plan as trim_plan
from wifi_held_log import require
from wifi_cal5_cleanup_log import check as cleanup_proof
PROGRESS='wifi-cal5-progress: hold selected161/trim44;configure retained RX;MAC start;fresh IBI probe;R0 VCO161 FULL901 LC1 RXDCOC1;controlled RX;drain disable stop;inverse originals'
RECEIVE='wifi-cal5-progress: calibration replies verified;passive161/20MHz window up to10seconds;no network OUT'

def fields(line,prefix):
 require(line.startswith(prefix+': '),'wrong ledger record')
 data={}
 for k,v in re.findall(r'([a-z_]+)=([^ ]+)',line[len(prefix)+2:]):
  require(k not in data,'duplicate ledger key');data[k]=int(v,16 if v.startswith('0x') else 10)
 require(' '.join(f'{k}={v}' for k,v in re.findall(r'([a-z_]+)=([^ ]+)',line[len(prefix)+2:]))==line[len(prefix)+2:],'malformed ledger fields')
 return data

def check(log,*,early=False):
 require('srvros.wifi-cal5=1' in log,'missing calibration profile');r=parent(log);all_lines=log.splitlines();rows=[x for x in all_lines if x.startswith('wifi-cal5') and not x.startswith('wifi-cal5-cleanup')]
 require(rows and rows[0]==PROGRESS,'missing calibration progress')
 # One hook strictly after all77 selected VCO I/O and before inverse BBP.
 before=child_receipt(76);after=legacy_receipt(HOOK,trim_plan(r['gain5_lna'])[HOOK])
 require(all_lines.index(before)<all_lines.index(PROGRESS)<all_lines.index(after),'calibration outside selected checkpoint')
 pos=1;op=0
 def take(prefix):
  nonlocal pos
  require(pos<len(rows),'truncated calibration ledger');s=rows[pos];pos+=1;return fields(s,prefix)
 def usb(part,clean,write,a,value=None):
  nonlocal op
  d=take('wifi-cal5-usb');require(set(d)==set('op part cleanup write address value ok'.split()),'wrong USB keys')
  require((d['op'],d['part'],d['cleanup'],d['write'],d['address'],d['ok'])==(op,part,clean,write,a,1),'wrong calibration I/O request/order');op+=1
  v=d['value'];require(0<=v<=0xffffffff and (value is None or v==value),'wrong calibration readback')
  return v
 def delay(part,clean,us):
  nonlocal op
  d=take('wifi-cal5-delay');require(set(d)==set('op part cleanup us elapsed_us ok'.split()),'wrong delay keys')
  require((d['op'],d['part'],d['cleanup'],d['us'],d['ok'])==(op,part,clean,us,1) and us<=d['elapsed_us']<=us+100000,'wrong/bounded delay');op+=1
 def put(part,clean,a,v):usb(part,clean,1,a,v);usb(part,clean,0,a,v)
 def guards(part,clean,mac):
  for a,v in [(0x1004,mac),(0x2000,0xf000f200),(0x208,0x40000050)]:usb(part,clean,0,a,v)
 def poll(a,mask,budget,clean):
  for n in range(2048):
   v=usb(0,clean,0,a);require(v!=0xffffffff,'unknown poll word')
   if not v&mask:return
   require(n*50<budget,'poll budget');delay(0,clean,50)
  require(False,'poll exhaustion')
 require(take('wifi-cal5-admission')==dict(configured=1,parent=1,selected=1,vco=1,fields=1,context=1,owner=1),'missing/failed exact5GHz admission')
 guards(0,0,0);usb(0,0,0,0x1400,0x17f97);txop=usb(0,0,0,0x1340);require(txop!=0xffffffff,'unknown TXOP')
 put(0,0,0x1004,4);poll(0x208,10,200000,0);put(0,0,0x1400,0x17f97);put(0,0,0x1004,12);poll(0x208,10,50,0)
 usb(0,0,0,0x2000,0xf000f200);usb(0,0,0,0x13b0,0x2f2f1b1b);usb(0,0,0,0x2124,0xffffffff)
 guards(1,0,12);usb(1,0,0,0x13b0,0x2f2f1b1b);put(1,0,0x13b0,0);delay(1,0,500);usb(1,0,0,0x2124,0xffffffff);put(1,0,0x2124,0xffffff7e)
 put(1,1,0x2124,0xffffffff);put(1,1,0x13b0,0x2f2f1b1b);guards(1,0,12)
 require(take('wifi-cal5-probe')==dict(status=0,complete=1,matched=1,changed=1,restored=3,alc=0x2f2f1b1b,ibi=0xffffffff,observed=0xffffff7e,ok=1),'probe admission/restoration failed')
 def command(n,arg,id):
  nonlocal op
  d=take('wifi-cal5-command');require(set(d)==set('op step arg sent received cc residue elapsed_us h id value t ok'.split()),'wrong command keys')
  h=0x4a000008|((n+1)<<16)
  require((d['op'],d['step'],d['arg'],d['sent'],d['received'],d['residue'],d['h'],d['id'],d['value'],d['t'],d['ok'])==(op,n,arg,16,16,1008,h,id,arg,h,1) and d['cc'] in (1,13) and 0<=d['elapsed_us']<=1000000,'wrong MCU reply/argument/sequence');op+=1
 guards(2,0,12);command(0,0,1);command(1,161,12);delay(2,0,10);guards(2,0,12);usb(2,0,0,0x13b0,0x2f2f1b1b)
 put(2,0,0x13b0,0);delay(2,0,500);usb(2,0,0,0x2124,0xffffffff);put(2,0,0x2124,0xffffff7e);command(2,0x901,255);command(3,1,3);delay(2,0,15000)
 put(2,1,0x2124,0xffffffff);put(2,1,0x13b0,0x2f2f1b1b);guards(2,0,12);command(4,1,2);guards(2,0,12)
 require(take('wifi-cal5-calibration')==dict(status=0,complete=1,restored=3,commands=5,alc=0x2f2f1b1b,ibi=0xffffffff,alc_restored=0x2f2f1b1b,ibi_restored=0xffffffff,ok=1),'incomplete calibration restoration')
 posts=0
 if not early:
  require(rows[pos]==RECEIVE,'missing receive admission');pos+=1
  while pos<len(rows) and rows[pos].startswith('wifi-cal5-post:'):
   require(take('wifi-cal5-post')==dict(index=posts,ok=1,bytes=4096,retained=1,reused=0),'wrong receive post');posts+=1
  require(1<=posts<=8,'wrong receive count')
 delay(0,0,10000)
 put(0,1,0x1340,txop&~(1<<20))
 for n in range(200):
  a=usb(0,1,0,0x438);b=usb(0,1,0,0xa30);c=usb(0,1,0,0xa34);require(0xffffffff not in (a,b,c),'unknown TX drain')
  if not a and not b&255 and not c&0xff00ff:break
  delay(0,1,10000)
 else:require(False,'TX drain exhausted')
 poll(0x1200,1,1000,1);put(0,1,0x1004,0);quiet=0
 for n in range(200):
  a=usb(0,1,0,0x430);b=usb(0,1,0,0xa30);c=usb(0,1,0,0xa34);require(0xffffffff not in (a,b,c),'unknown RX drain')
  if not a&0xff0000 and not b and not c:quiet+=1
  else:quiet=0;delay(0,1,1000)
  if quiet==7:break
 else:require(False,'RX drain exhausted')
 poll(0x1200,2,1000,1);put(0,1,0x1400,0x17f97);put(0,1,0x1340,txop);guards(0,1,0)
 if early:
  from wifi_early5_observation import observe
  observed=observe(log);require(observed['present'],'missing early queue outcome')
  require('srvros.wifi-early5=1' in log and observed['summary']['complete']==1 and
          observed['summary']['operations']==op,'incomplete early calibration/queue')
  require(pos==len(rows) and all_lines.index(rows[-1])<all_lines.index(after),'extra/misplaced early calibration rows')
  published=[x for x in all_lines if x.startswith('wifi-early5-publish:')]
  require(len(published)==1 and all_lines.index(rows[1])<all_lines.index(published[0])<all_lines.index(rows[2]),'early queue outside pre-MAC checkpoint')
  r.update(operations=r['operations']+op,early5_observation=observed,early5_complete=True,
           scope='bounded pre-MAC retained main-RX/calibration/Stop comparison;no network or physical-cause acceptance')
  return r
 stop=take('wifi-cal5-stop');cleanup=cleanup_proof(log,stop)
 stop_keys=set('ok posts state dequeue ring retained'.split()) | ({'cleanup'} if cleanup['cal5_cleanup_enabled'] else set())
 require(set(stop)==stop_keys and stop['ok']==1 and stop['posts']==posts and stop['state']==3 and stop['retained']==1 and stop['ring']>0 and not stop['ring']&4095,'unproven exact endpoint cessation')
 if not cleanup['cal5_cleanup_enabled']:require(stop['dequeue'] in (stop['ring']+16*(posts-1)+1,stop['ring']+16*posts+1),'unproven exact endpoint frontier')
 completed=beacons=0
 for i in range(posts):
  d=take('wifi-cal5-rx');require(set(d)==set('index submitted completed cc remaining actual inspected view beacon_status beacon mpdu interval ssid_size privacy rsn wpa'.split()) and d['index']==i and d['submitted']==1,'wrong RX item')
  for k in ('completed','inspected','beacon','privacy','rsn','wpa'):require(d[k] in (0,1),'nonboolean RX')
  if d['completed']:
   completed+=1;require(d['cc'] in (1,13) and 0<=d['remaining']<=4096 and d['actual']==4096-d['remaining'],'invalid completion length')
   require(d['inspected']==bool(d['actual']),'missing byte inspection')
   if d['beacon']:beacons+=1;require(d['view']==d['beacon_status']==0 and 24<=d['mpdu']<=d['actual']-36 and 0<d['interval']<=65535 and 0<=d['ssid_size']<=32,'invalid beacon metadata')
  else:require(not d['actual'] and not d['inspected'] and not d['beacon'] and i==posts-1,'invalid pending RX bytes')
 final=take('wifi-cal5');require(set(final)==set('ok operations commands probe calibration mac disabled tx_drained rx_drained filter_restored txop_restored stopped window attempts completions beacons elapsed_us retained network_out'.split()),'wrong final keys')
 require(all(final[k]==1 for k in 'ok probe calibration mac disabled tx_drained rx_drained filter_restored txop_restored stopped window retained'.split()) and final['operations']==op and final['commands']==5 and final['attempts']==posts and final['completions']==completed and final['beacons']==beacons and final['network_out']==0 and 0<=final['elapsed_us']<=10100000,'incomplete final calibration/receive')
 require(pos==len(rows) and all_lines.index(rows[-1])<all_lines.index(after),'extra/misplaced calibration rows')
 r.update(cleanup)
 r.update(operations=r['operations']+op,cal5_operations=op,cal5_commands=5,cal5_window_complete=True,cal5_completions=completed,cal5_beacons=beacons,cal5_received=beacons>0,scope='channel161 firmware acknowledgement,temporary saved-word restoration and controlled receive lifecycle;quiet window is not RF/discovery acceptance;security/IP NOT_TESTED')
 return r
