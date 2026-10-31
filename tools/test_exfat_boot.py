#!/usr/bin/env python3
"""Exercise actual boot selection with every supported sector size on host files."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from test_storage_leases import empty_image
from mk_exfat_image import write_boot_regions
ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver',type=Path,default=ROOT/'build/tests/exfat-driver')
    parser.add_argument('--output',type=Path,default=ROOT/'build/review/boot-system/sector-matrix')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    image=out/'candidate.exfat';results=[];driver=args.driver.resolve()

    def run(name,data,mode,expected=0):
        image.write_bytes(data)
        r=subprocess.run([str(driver),str(image),mode],capture_output=True,text=True,timeout=30)
        results.append(dict(name=name,mode=mode,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
        (out/'result.json').write_text(json.dumps(dict(cases=results,driver_sha256=hashlib.sha256(driver.read_bytes()).hexdigest()),indent=2)+'\n')
        assert r.returncode==expected,(name,r.stdout+r.stderr)
        if mode!='creation-mode':assert image.read_bytes()==data, name
        print(name,'PASS',flush=True)
        return r.stdout

    for shift in range(9,13):
        sector=1<<shift
        seed=empty_image(4*1024*1024,sector_shift=shift).data
        run(f'{sector}-writable-preflight',seed,'writable-check')
        for flags in (2,4,6):
            data=bytearray(seed);struct.pack_into('<H',data,106,flags)
            run(f'{sector}-flags-{flags}-graph',data,'allocation-check')
            run(f'{sector}-flags-{flags}-writable-refused',data,'writable-check',1)
        run(f'{sector}-healthy',seed,'creation-mode')
        data=bytearray(seed);data[108]=255
        run(f'{sector}-backup-fallback',data,'readonly')
        data=bytearray(seed);data[12*sector+120]^=1
        run(f'{sector}-backup-damaged',data,'readonly')
        run(f'{sector}-backup-damaged-preflight',data,'writable-check',1)
        data=bytearray(seed);data[120]^=1;data[12*sector+120]^=1
        run(f'{sector}-both-damaged',data,'allocation-check',2)
        data=bytearray(seed);struct.pack_into('<H',data,12*sector+106,6);data[12*sector+112]=255
        run(f'{sector}-stale-backup-writable',data,'writable-check')
        run(f'{sector}-stale-backup-flags',data,'creation-mode')
        data=bytearray(seed);backup=bytes(data[12*sector:24*sector]);data[100]^=1;write_boot_regions(data);data[12*sector:24*sector]=backup
        run(f'{sector}-copies-disagree',data,'readonly')
    data=empty_image(4*1024*1024).data
    other=empty_image(len(data),sector_shift=10).data
    data[108]=255;data[12*1024:24*1024]=other[12*1024:24*1024]
    run('ambiguous-backups',data,'allocation-check',2)
    data=empty_image(4*1024*1024,cluster_shift=0).data
    run('multi-cluster-bitmap-readonly',data,'readonly')
    run('multi-cluster-bitmap-check-refused',data,'allocation-check',1)
    sector=512;heap=struct.unpack_from('<I',data,88)[0]*sector
    count=struct.unpack_from('<I',data,92)[0]
    used=2+((count+7)//8+sector-1)//sector
    data[heap+sector+5000//8]|=1<<(5000%8)
    text=run('multi-cluster-bitmap-statfs',data,'bitmap-stats')
    assert f'bitmap-used={used+1}\n' in text,text
    # Move the second bitmap cluster away from its physical successor.
    data[heap+6*sector:heap+7*sector]=data[heap+2*sector:heap+3*sector]
    data[heap+sector]&=~4;data[heap+sector]|=64
    struct.pack_into('<I',data,24*sector+3*4,8)
    struct.pack_into('<I',data,24*sector+8*4,0xffffffff)
    text=run('fragmented-bitmap-statfs',data,'bitmap-stats')
    assert f'bitmap-used={used+1}\n' in text,text


if __name__=='__main__':main()
