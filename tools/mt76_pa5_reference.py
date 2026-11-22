#!/usr/bin/env python3
"""Verify immutable external5GHz PA projection against pinned Linux sources."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from mt76_frontend_reference import project,require


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--reference',type=Path,required=True);a=ap.parse_args()
    fixtures=Path(__file__).with_name('fixtures');meta=json.loads((fixtures/'mt7610-5ghz-frontend.json').read_text())
    for name,item in meta['sources'].items():require(hashlib.sha256((a.reference/name).read_bytes()).hexdigest()==item['sha256'],'source hash mismatch:'+name)
    rf,_,_=project(a.reference);phy=(a.reference/'phy.c').read_text()
    for text in ('mt76_clear(dev, MT_RF_MISC, 0xc);','mt76_set(dev, MT_RF_MISC, BIT(2));','0x686A7800','mac_reg &= 0x890400FF;'):require(text in phy,'unknown5GHz PA policy:'+text)
    rows=[(0x518,1,(1&~12)|4)]+[(0x80000000|(rf[i]<<16)|rf[i+1],0,rf[i+2]) for i in range(0,len(rf),3)]+[(0x13a8,0x6c6c6c6c,0x686a7800),(0x13b4,0xc9540000,0xc9540000&0x890400ff)]
    blob=b''.join(struct.pack('<III',*row) for row in rows);p=fixtures/'mt7610-5ghz-pa-cycle.bin';m=json.loads(p.with_suffix('.json').read_text())
    require(p.read_bytes()==blob and hashlib.sha256(blob).hexdigest()==m['sha256'],'PA fixture/projection mismatch')
    print('PA5 reference: PASS11 RF words,external-A selector,attenuation/ALC masks;originals bound to native v113 evidence')


if __name__=='__main__':main()
