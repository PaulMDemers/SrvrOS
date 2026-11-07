#!/usr/bin/env python3
"""Audit fixed channel161 external-PA/BBP projections and readonly targets."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def require(ok,why):
    if not ok:raise ValueError(why)


def project(ref):
    text={n:(ref/n).read_text() for n in ('phy.h','initvals_phy.h','initvals.h','regs.h','eeprom.h','eeprom.c','phy.c')}
    flags={k:int(v,0) for k,v in re.findall(r'#define\s+(RF_[A-Z0-9_]+)\s+(0x[0-9a-fA-F]+|\d+)',text['phy.h'])}
    bases={k:int(v,0) for k,v in re.findall(r'#define\s+MT_BBP_(\w+)_BASE\s+(0x[0-9a-fA-F]+)',text['regs.h'])}
    def bits(expr):
        value=0
        for term in expr.split('|'):value|=flags[term.strip()]
        return value
    rfband=flags['RF_A_BAND']|flags['RF_A_BAND_HB'];bbpband=flags['RF_A_BAND']|flags['RF_BW_20']
    rf=[];body=text['initvals_phy.h'].split('mt76x0_rf_ext_pa_tab[]')[1].split('};')[0]
    for bank,reg,expr,value in re.findall(r'\{\s*MT_RF\(\s*(\d+),\s*(\d+)\),\s*([^,]+),\s*(0x[0-9a-fA-F]+)\s*\}',body):
        if rfband&bits(expr):rf.append((int(bank),int(reg),int(value,16)))
    bbp=[];body=text['initvals.h'].split('mt76x0_bbp_switch_tab[]')[1].split('};')[0]
    for expr,kind,index,value in re.findall(r'\{\s*(RF_[A-Z0-9_| \t]+),\s*\{\s*MT_BBP\((\w+),\s*(\d+)\),\s*(0x[0-9a-fA-F]+)\s*\}\s*\}',body):
        if bbpband&bits(expr)==bbpband:bbp.append((bases[kind]+int(index)*4,int(value,16)))
    require(len(rf)==11 and len(bbp)==21,'unexpected frontend projection counts')
    require(re.search(r'MT_EE_NIC_CONF_0_PA_INT_5G\s+BIT\(9\)',text['eeprom.h']) and 'return !(conf0 & MT_EE_NIC_CONF_0_PA_INT_5G)' in text['eeprom.c'],'unknown5GHz PA policy')
    require(not 0xfd11&512,'captured board is not external5GHz PA')
    require('rf_bw_band = (channel <= 14) ? RF_G_BAND : RF_A_BAND;' in text['phy.c'],'unknown BBP band selector')
    direct=[0x518,0x13a8,0x13b4,0x13b0]+[a for a,_ in bbp]+[0x2004,0x2300,0x2700,0x2124,0x2324]
    targets=direct+[0x80000000|(b<<16)|r for b,r,_ in rf]+[0x80000016,0x80000049,0x80000004]
    require(len(direct)==30 and len(targets)==len(set(targets))==44,'unexpected inventory targets')
    return bytes(x for row in rf for x in row),b''.join(struct.pack('<II',*x) for x in bbp),b''.join(struct.pack('<I',a) for a in targets)


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--reference',type=Path,required=True);ap.add_argument('--fixture-base',type=Path,required=True);args=ap.parse_args()
    meta=json.loads(Path(str(args.fixture_base)+'.json').read_text())
    for name,item in meta['sources'].items():require(hashlib.sha256((args.reference/name).read_bytes()).hexdigest()==item['sha256'],'source hash mismatch:'+name)
    data=project(args.reference)
    for suffix,blob in zip(('-rf.bin','-bbp.bin','-targets.bin'),data):
        p=Path(str(args.fixture_base)+suffix);require(p.read_bytes()==blob and hashlib.sha256(blob).hexdigest()==meta['fixtures'][suffix],'fixture/projection mismatch:'+suffix)
    print('frontend reference: PASS pinned7.0 sources,external5GHz PA11 RF words,A-band20MHz21 BBP rows,44 fixed readonly targets;no hardware writes')


if __name__=='__main__':main()
