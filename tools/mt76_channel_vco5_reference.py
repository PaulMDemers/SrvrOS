"""Fixed selected-channel VCO projection; no hardware or receiver admission."""
from pathlib import Path
import hashlib,json,struct
from mt76_vco5_reference import audit as common_audit,require

def audit(reference,fixtures=None):
 f=fixtures or Path(__file__).with_name('fixtures');r=common_audit(reference,f)
 m=json.loads((f/'mt7610-5ghz-channel-vco.json').read_text());parent=json.loads((f/'mt7610-5ghz-vco-cycle.json').read_text())
 require(m['sources']==parent['sources'] and m['dependencies']==parent['dependencies'],'wrong channel VCO source pins')
 require(m['original_source']=='11689cfa' and m['original_session']=='session-00065f6c26a94612-00' and m['original_frontend_index']==43 and m['original']==0x30 and m['selected_trim']==0x44,'unknown channel VCO original evidence')
 require(m['allowed_observation']==[0x30,0xb0] and m['delay_us']==[10000,40000,50000,100000,300000,500000],'wrong diagnostic bound')
 b=(f/'mt7610-5ghz-channel-vco.bin').read_bytes();require(hashlib.sha256(b).hexdigest()==m['sha256'],'channel VCO fixture hash')
 trim,vco=0x80000016,0x80000004
 plan=[(trim,0x44,0),(vco,0x30,0),(vco,0x30|0x80,1)]+[(vco,0x30,0)]*7+[(trim,0x44,0)]
 require(list(struct.iter_unpack('<III',b))==plan,'unknown channel pulse/readback plan')
 require(sum(w for _,_,w in plan)==1 and all(not w or a==vco and v==0xb0 for a,v,w in plan),'force-clear/trim write in child')
 r.update(channel_vco_commands=11,channel_vco_operations=77,coordinator_operations=3844,trim_selected=68,one_trigger=True,force_clear=False,hardware_operations=False,native_caller_authorized_by_audit=False)
 return r
