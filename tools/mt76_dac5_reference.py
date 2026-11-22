"""Pinned common DAC reset, not VCO or calibrated receiver admission."""
from pathlib import Path
import hashlib,json,struct
from mt76_trim5_reference import audit as trim_audit,require

def audit(reference,fixtures=None):
 f=fixtures or Path(__file__).with_name('fixtures');result=trim_audit(reference,f)
 m=json.loads((f/'mt7610-5ghz-dac-cycle.json').read_text());parent=json.loads((f/'mt7610-5ghz-trim-cycle.json').read_text())
 require(m['sources']==parent['sources'] and m['dependencies']==parent['dependencies'],'wrong DAC source/dependency pins')
 require(m['original_source']=='1fe64aac' and m['original_session']=='session-00065f693a903b08-00' and m['original_frontend_index']==42 and m['original']==0x93,'unknown DAC original evidence')
 blob=(f/'mt7610-5ghz-dac-cycle.bin').read_bytes();require(hashlib.sha256(blob).hexdigest()==m['sha256'],'DAC fixture hash mismatch')
 trim=0x80000016;dac=0x80000049
 expected=[(trim,0,0),(trim,0x44,1),(trim,0x44,0),(dac,0x93,0)]
 value=0x93
 for bit in (True,False,True):
  value=value|0x80 if bit else value&~0x80
  expected += [(dac,value,1),(dac,value,0)]
 expected += [(trim,0x44,0),(trim,0,1),(trim,0,0)]
 # The read before each RMW is the preceding independent read; no duplicate.
 require(list(struct.iter_unpack('<III',blob))==expected,'unknown DAC RMW/readback/trim lifetime')
 require(all(a!=0x80000004 for a,_,_ in expected),'VCO command in DAC plan')
 result.update(dac_commands=len(expected),dac_operations=len(expected)*7,dac_original=0x93,dac_final=value,word_restoration_only=True,native_caller_authorized_by_audit=False,hardware_operations=False)
 return result
