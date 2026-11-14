"""Pinned common VCO trigger projection; timed observation is a diagnostic bound."""
from pathlib import Path
import hashlib,json,struct
from mt76_dac5_reference import audit as dac_audit,require

def audit(reference,fixtures=None):
 f=fixtures or Path(__file__).with_name('fixtures');result=dac_audit(reference,f)
 m=json.loads((f/'mt7610-5ghz-vco-cycle.json').read_text());parent=json.loads((f/'mt7610-5ghz-dac-cycle.json').read_text())
 require(m['sources']==parent['sources'] and m['dependencies']==parent['dependencies'],'wrong VCO source/dependency pins')
 require(m['original_source']=='4dd9cfd5' and m['original_session']=='session-00065f6a45979c71-00' and m['original_frontend_index']==43 and m['original']==0x30 and m['selected']==0xb0 and m['fresh_native_fields_required'] is True,'unknown VCO original evidence')
 require(m['allowed_observation']==[0x30,0xb0] and m['delay_us']==[10000,40000,50000,100000,300000,500000],'unknown diagnostic observation bound')
 blob=(f/'mt7610-5ghz-vco-cycle.bin').read_bytes();require(hashlib.sha256(blob).hexdigest()==m['sha256'],'VCO fixture hash mismatch')
 trim,vco=0x80000016,0x80000004
 expected=[(trim,0,0),(trim,0x44,1),(trim,0x44,0),(vco,0x30,0),(vco,0x30|0x80,1)]+[(vco,0x30,0)]*7+[(trim,0x44,0),(trim,0,1),(trim,0,0)]
 require(list(struct.iter_unpack('<III',blob))==expected,'unknown VCO trigger/readback/trim lifetime')
 require(sum(a==vco and w for a,_,w in expected)==1,'unknown multiple/clear VCO writes')
 result.update(vco_commands=15,vco_operations=105,vco_original=0x30,observation_delay_us=1000000,force_clear=False,hardware_operations=False,native_caller_authorized_by_audit=False)
 return result
