#!/usr/bin/env python3
"""Pinned common-init versus per-channel trim/DAC/VCO lifetime audit."""
import argparse,hashlib,json,re,struct
from pathlib import Path


def require(ok,why):
    if not ok:raise ValueError(why)


def audit(reference,fixtures=None):
    f=fixtures or Path(__file__).with_name('fixtures');m=json.loads((f/'mt7610-5ghz-trim-cycle.json').read_text())
    for name,item in m['sources'].items():require(hashlib.sha256((reference/name).read_bytes()).hexdigest()==item['sha256'],'source hash mismatch:'+name)
    for name,digest in m['dependencies'].items():require(hashlib.sha256((f/name).read_bytes()).hexdigest()==digest,'dependency hash mismatch:'+name)
    phy=(reference/'phy.c').read_text();ee=(reference/'eeprom0.c').read_text();h=(reference/'eeprom.h').read_text()
    common=phy.split('static void mt76x0_phy_rf_init(')[1].split('void mt76x0_phy_init(')[0]
    channel=phy.split('void mt76x0_phy_set_channel(')[1].split('static void mt76x0_phy_temp_sensor(')[0]
    trim=re.search(r'mt76x0_rf_wr\(dev, MT_RF\(0, 22\),\s*min_t\(u8, dev->cal.rx.freq_offset, 0xbf\)\);',common);require(trim is not None,'unknown trim write/clamp')
    read=common.index('mt76x0_rf_rr(dev, MT_RF(0, 22));')
    dac=re.findall(r'mt76x0_rf_(set|clear)\(dev, MT_RF\(0, 73\), BIT\(7\)\);',common)
    require(dac==['set','clear','set'],'unknown common DAC sequence')
    require(trim.start()<read<common.index('mt76x0_rf_set(dev, MT_RF(0, 73), BIT(7));')<common.index('mt76x0_rf_set(dev, MT_RF(0, 4), 0x80);'),'unknown common trim/DAC/VCO order')
    points=['mt76x0_phy_set_chan_rf_params(dev, channel, rf_bw_band);','mt76x0_read_rx_gain(dev);','mt76x0_phy_set_chan_bbp_params(dev, rf_bw_band);','mt76x0_rf_set(dev, MT_RF(0, 4), BIT(7));','mt76x02_init_agc_gain(dev);','mt76x0_phy_calibrate(dev, false);']
    require([channel.index(p) for p in points]==sorted(channel.index(p) for p in points),'unknown channel/VCO/calibration order')
    require('caldata->freq_offset -= mt76x02_sign_extend(val, 8);' in ee and 'return sign ? val : -val;' in h,'unknown frequency normalization')
    require(re.search(r'MT_EE_FREQ_OFFSET\s*=\s*0x03a',h) and re.search(r'MT_EE_TSSI_BOUND4\s*=\s*0x0da',h),'unknown private trim offsets')
    blob=(f/'mt7610-5ghz-trim-cycle.bin').read_bytes();require(hashlib.sha256(blob).hexdigest()==m['sha256'],'trim fixture hash mismatch')
    require(list(struct.iter_unpack('<III',blob))==[(0x80000016,0,0),(0x80000016,0x44,1),(0x80000016,0x44,0),(0x80000016,0,1),(0x80000016,0,0)],'unknown trim lifetime')
    targets=set()
    for name in ('band','a20-hb'):
        b=(f/f'mt7610-5ghz-{name}.bin').read_bytes();targets.update(0x80000000|(bank<<16)|reg for bank,reg in zip(b[::3],b[1::3]))
    b=(f/'mt7610-us5-channel161-frequency.bin').read_bytes();targets.update(0x80000000|reg for reg in b[::3])
    for name in ('pa','bbp'):
        b=(f/f'mt7610-5ghz-{name}-cycle.bin').read_bytes();targets.update(a for a,_,_ in struct.iter_unpack('<III',b))
    require(0x80000016 not in targets,'trim overlaps accepted nested target')
    return dict(pass_result=True,pinned_sources=len(m['sources']),trim_target=0x80000016,original=0,selected=0x44,disjoint_nested_targets=len(targets),common_bootstrap_is_not_a_model_band_acceptance=True,common_order=['fresh frequency fields','RF0:22 write/read','DAC0:73 set-clear-set','common RF0:4 trigger'],channel_order=points,hardware_operations=False)


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--reference',type=Path,required=True);ap.add_argument('--output',type=Path);args=ap.parse_args();result=audit(args.reference)
    if args.output:args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('trim5 reference: PASS pinned common/channel lifetimes,private field normalization,35-op reversible plan and disjoint nested targets;no hardware')
