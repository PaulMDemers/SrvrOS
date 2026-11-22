"""Full held-channel VCO ledger and inverse originals; not calibrated RX."""
import re,struct
from pathlib import Path
from wifi_vco5_log import check as prior,summary as parent_summary,DELAYS
from wifi_trim5_log import plan as trim_plan,receipt as trim_receipt
from wifi_held_log import require
PROGRESS='wifi-channel-vco5-progress: same-boot common VCO parent;hold fresh trim44 and channel161/20MHz bank;one channel trigger;six samples;inverse originals;no RX/network OUT'
HOOK=2559

def summary(lna,first):return f'wifi-channel-vco5: status=0 complete=1 valid=1 restored=1 operations=3844 submitted=3844 trim=68 lna={lna} parent=1 child_operations=77 child_valid=1 waits=6 delay_us=1000000 first_clear={first} network_out=0'
def child_commands():
 b=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-channel-vco.bin').read_bytes();require(len(b)==132,'wrong channel VCO fixture');return tuple(struct.iter_unpack('<III',b))
def child_receipt(op,dma=0x40000050,observed=0x30):
 q,n=divmod(op,7);a,v,w=child_commands()[q];glob=HOOK+op
 if n==3:
  if 3<=q<=9:v=observed
  h=0x4a000008|((q+2)<<16)
  return f'wifi-channel-vco5-command: op={glob} phase=15 local={op} write={w} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x}'
 j=n if n<3 else n-4;a,v=((0x1004,0),(0x2000,0xf000f200),(0x208,dma))[j]
 return f'wifi-channel-vco5-usb: op={glob} phase=15 local={op} write=0 address=0x{a:016x} value=0x{v:016x} ok=1'
def legacy_receipt(op,row,dma=0x40000050):return trim_receipt(op+(77 if op>=HOOK else 0),row,dma).replace('wifi-trim5-','wifi-channel-vco5-')
def dma_value(line):
 m=re.search(r' value=(0x[0-9a-f]{16}) ok=1$',line);require(m is not None,'malformed channel DMA receipt');v=int(m[1],16);require(v<0xffffffff and not v&15,'unsafe channel DMA');return v

def check(log):
 require('srvros.wifi-channel-vco5=1' in log,'missing channel VCO profile');result=prior(log);lna=result['gain5_lna']
 lines=log.splitlines();rows=[s for s in lines if s.startswith('wifi-channel-vco5')]
 require(len(rows)==3852 and rows[0]==PROGRESS,'missing/duplicate channel VCO records')
 require(lines.index(PROGRESS)>lines.index(parent_summary(result['vco5_first_clear'])),'channel before complete common VCO parent')
 legacy=trim_plan(lna);require(len(legacy)==3767 and legacy[HOOK][:2]==(11,441),'wrong selected checkpoint plan')
 pos=1;first=0
 for old,row in enumerate(legacy):
  if old==HOOK:
   for op in range(77):
    q,n=divmod(op,7)
    if 4<=q<=9 and not n:
     require(rows[pos]==f'wifi-channel-vco5-delay: sample={q-4} us={DELAYS[q-4]} ok=1','wrong channel delay/order');pos+=1
    line=rows[pos];pos+=1;dma=0x40000050;value=0x30
    if n in (2,6):dma=dma_value(line)
    if n==3 and 3<=q<=9:
     m=re.search(r' value=(0x[0-9a-f]{16}) t=',line);require(m is not None,'malformed channel observation');value=int(m[1],16);require(value in (0x30,0xb0),'changed channel VCO nontrigger bits')
     if value==0x30 and not first:first=q-2
     require(value==0x30 or not first,'channel trigger reasserted')
     if q==9:require(value==0x30,'channel trigger pending;inverse writes forbidden')
    require(line==child_receipt(op,dma,value),'wrong channel child request/reply/guard/order')
  line=rows[pos];pos+=1;dma=dma_value(line) if row[4] is None else 0x40000050
  require(line==legacy_receipt(old,row,dma),'wrong held-state request/selection/inverse original')
 require(first and pos==len(rows)-1 and rows[-1]==summary(lna,first),'bad channel restoration/summary')
 result.update(operations=result['operations']+3844,channel_vco5_operations=3844,channel_vco5_child_operations=77,channel_vco5_first_clear=first,channel_vco5_restored=True,scope='channel161 selected-bank VCO self-clear and inverse originals only;calibrated RX/security/IP NOT_TESTED')
 return result
