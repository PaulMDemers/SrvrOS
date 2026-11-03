#!/usr/bin/env python3
"""Audit pinned5GHz high-band gain selection and21 A-band20MHz BBP rows."""
import argparse,hashlib,json,re,struct
from pathlib import Path
from mt76_frontend_reference import project,require


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--reference',type=Path,required=True);a=ap.parse_args();f=Path(__file__).with_name('fixtures');m=json.loads((f/'mt7610-5ghz-bbp-cycle.json').read_text())
    for name,item in m['sources'].items():require(hashlib.sha256((a.reference/name).read_bytes()).hexdigest()==item['sha256'],'source hash mismatch:'+name)
    _,bbp,_=project(a.reference);blob=(f/'mt7610-5ghz-bbp-cycle.bin').read_bytes();require(len(blob)==252 and hashlib.sha256(blob).hexdigest()==m['sha256'],'BBP fixture hash/size mismatch')
    require([(a,v) for a,_,v in struct.iter_unpack('<III',blob)]==list(struct.iter_unpack('<II',bbp)),'BBP projection mismatch')
    e=(a.reference/'eeprom.c').read_text();h=(a.reference/'eeprom.h').read_text();phy=(a.reference/'phy.c').read_text();r=(a.reference/'regs.h').read_text();x=(a.reference/'eeprom0.c').read_text()
    for name,offset in [('MT_EE_LNA_GAIN','044'),('MT_EE_RSSI_OFFSET_5G_0','04a'),('MT_EE_RSSI_OFFSET_5G_1','04c')]:require(re.search(name+r'\s*=\s*0x'+offset,h),'unknown gain offset:'+name)
    for token in ('lna_5g[0] = val >> 8;','lna_5g[2] = val >> 8;','lna_5g[2] = lna_5g[0];','lna = lna_5g[2];','return lna != 0xff ? lna : 0;'):require(token in e,'unknown high-band selection:'+token)
    require('return val != 0 && val != 0xff;' in h and 'val < -10 || val > 10' in x,'unknown field/RSSI normalization')
    require(re.search(r'MT_BBP_AGC_GAIN\s+GENMASK\(14, 8\)',r) and 'gain -= dev->cal.rx.lna_gain * 2;' in phy and 'u8 gain;' in phy,'unknown gain field/arithmetic')
    require(sum(v==0xffffffff for _,v in struct.iter_unpack('<II',bbp))==3,'unknown sentinel rows')
    print('BBP5 reference: PASS pinned8 sources,45/4d high-band fallback,4a RSSI clamp,8-bit subtract/7-bit field,21 A-band20MHz rows and three exact all-ones entries')


if __name__=='__main__':main()
