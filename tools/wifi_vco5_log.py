"""Fixed common VCO trigger/timed observation and trim restoration proof only."""
import re,struct
from pathlib import Path
from wifi_dac5_log import check as prior,SUMMARY as PARENT
from wifi_held_log import require
PROGRESS='wifi-vco5-progress: same-boot fresh DAC parent;trim44;one VCO trigger;six timed samples;no force-clear/RX/network OUT'
DELAYS=(10000,40000,50000,100000,300000,500000)
def summary(first):return f'wifi-vco5: status=0 complete=1 valid=1 words_restored=1 triggered=1 operations=105 submitted=105 trim=68 parent=1 waits=6 delay_us=1000000 first_clear={first} network_out=0'
def commands():
 blob=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-vco-cycle.bin').read_bytes();require(len(blob)==180,'wrong VCO fixture');return list(struct.iter_unpack('<III',blob))
def receipt(op,dma=0x40000050,observed=0x30):
 q,n=divmod(op,7);a,v,w=commands()[q]
 if n==3:
  if 5<=q<=11:v=observed
  h=0x4a000008|(((q+1)%15+1)<<16)
  return f'wifi-vco5-command: op={op} write={w} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x}'
 j=n if n<3 else n-4;a,v=((0x1004,0),(0x2000,0xf000f200),(0x208,dma))[j]
 return f'wifi-vco5-usb: op={op} address=0x{a:016x} value=0x{v:016x} ok=1'
def check(log):
 require('srvros.wifi-vco5=1' in log,'missing VCO profile');result=prior(log)
 lines=log.splitlines();rows=[s for s in lines if s.startswith('wifi-vco5')]
 require(len(rows)==113 and rows[0]==PROGRESS,'missing/duplicate VCO records')
 require(lines.index(PROGRESS)>lines.index(PARENT),'VCO before completed DAC parent')
 pos=1;first=0
 for op in range(105):
  q,n=divmod(op,7)
  if 6<=q<=11 and n==0:
   require(rows[pos]==f'wifi-vco5-delay: sample={q-6} us={DELAYS[q-6]} ok=1','wrong VCO delay/order');pos+=1
  line=rows[pos];pos+=1;dma=0x40000050;value=0x30
  if n in (2,6):
   m=re.search(r' value=(0x[0-9a-f]{16}) ok=1$',line);require(m is not None,'malformed VCO DMA guard');dma=int(m[1],16);require(dma<0xffffffff and not dma&15,'unsafe VCO DMA')
  if n==3 and 5<=q<=11:
   m=re.search(r' value=(0x[0-9a-f]{16}) t=',line);require(m is not None,'malformed VCO observation');value=int(m[1],16)
   require(value in (0x30,0xb0),'changed VCO nontrigger bits')
   if value==0x30 and not first:first=q-4
   require(value==0x30 or not first,'VCO reasserted after clear')
   if q==11:require(value==0x30,'VCO still pending;trim restoration not admitted')
  require(line==receipt(op,dma,value),'wrong VCO request/reply/guard/order')
 require(first and rows[-1]==summary(first),'incomplete/wrong VCO summary')
 result.update(operations=result['operations']+105,vco5_operations=105,vco5_first_clear=first,vco5_waits=6,vco5_delay_us=1000000,vco5_word_restoration=True,scope='common VCO trigger/self-clear observation and trim restoration only;RF lock/calibrated RX/connectivity NOT_TESTED')
 return result
