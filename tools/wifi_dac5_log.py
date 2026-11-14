"""Exact common DAC command/reset-word proof; no VCO or calibrated RF claim."""
import re,struct
from pathlib import Path
from wifi_trim5_log import check as prior,summary as parent_summary
from wifi_held_log import require
PROGRESS='wifi-dac5-progress: same-boot fresh parent;trim44;DAC set-clear-set;independent reads;word originals;no VCO/RX/network OUT'
SUMMARY='wifi-dac5: status=0 complete=1 valid=1 words_restored=1 dac_reset=1 operations=91 submitted=91 trim=68 parent=1 network_out=0'

def commands():
 blob=(Path(__file__).with_name('fixtures')/'mt7610-5ghz-dac-cycle.bin').read_bytes()
 require(len(blob)==156,'wrong DAC fixture size');return list(struct.iter_unpack('<III',blob))

def receipt(op,dma=0x40000050):
 q,n=divmod(op,7);a,v,w=commands()[q]
 if n==3:
  h=0x4a000008|((q+2)<<16)
  return f'wifi-dac5-command: op={op} write={w} sent=16 received=16 cc=13 residue=1008 h=0x{h:016x} address=0x{a:016x} value=0x{v:016x} t=0x{h:016x}'
 j=n if n<3 else n-4;a,v=((0x1004,0),(0x2000,0xf000f200),(0x208,dma))[j]
 return f'wifi-dac5-usb: op={op} address=0x{a:016x} value=0x{v:016x} ok=1'

def check(log):
 require('srvros.wifi-dac5=1' in log,'missing DAC profile')
 result=prior(log);lines=log.splitlines();rows=[s for s in lines if s.startswith('wifi-dac5')]
 require(len(rows)==93 and rows[0]==PROGRESS and rows[-1]==SUMMARY,'missing/unknown/duplicate DAC boundary')
 require(lines.index(PROGRESS)>lines.index(parent_summary(result['gain5_lna'])),'DAC before complete restored parent')
 for op,line in enumerate(rows[1:-1]):
  value=0x40000050
  if op%7 in (2,6):
   m=re.search(r' value=(0x[0-9a-f]{16}) ok=1$',line);require(m is not None,'bad DAC DMA guard');value=int(m[1],16)
   require(value<0xffffffff and not value&15,'bad DAC DMA state')
  require(line==receipt(op,value),'wrong DAC request/reply/readback/order')
 result.update(operations=result['operations']+91,dac5_operations=91,dac5_word_restoration=True,dac5_reset_command=True,scope='fresh trim/common DAC command and word-readback contract only;VCO/calibrated RX/connectivity NOT_TESTED')
 return result
