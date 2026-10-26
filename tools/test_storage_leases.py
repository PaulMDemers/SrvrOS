#!/usr/bin/env python3
"""Portable synthetic-media checks for the real workspace image lease adapter.

These minimal images exercise SrvrOS admission and byte containment; they are
not exfatprogs conformance or crash-recovery acceptance.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
from exfat_test_image import ExfatFixture
from mk_exfat_image import ascii_upcase_table, checksum32, write_boot_regions
from battery_filesystem_image import SIZE, MARKER, ANCHOR, verify
ROOT=Path(__file__).resolve().parents[1]

def empty_image(size, serial=0, sector_shift=9, cluster_shift=None):
    sector=1<<sector_shift
    if cluster_shift is None:cluster_shift=12-sector_shift
    cluster=sector<<cluster_shift
    data=bytearray(size)
    fat=((size//cluster+2)*4+sector-1)//sector
    heap=(24+fat+(1<<cluster_shift)-1)//(1<<cluster_shift)*(1<<cluster_shift)
    count=(size//sector-heap)//(1<<cluster_shift)
    bitmap_size=(count+7)//8
    bitmap_clusters=(bitmap_size+cluster-1)//cluster
    upcase=3+bitmap_clusters
    data[:3]=b'\xeb\x76\x90'
    data[3:11]=b'EXFAT   '
    struct.pack_into('<Q',data,72,size//sector)
    struct.pack_into('<IIIIII',data,80,24,fat,heap,count,2,serial)
    struct.pack_into('<H',data,104,0x100)
    data[108:111]=bytes([sector_shift,cluster_shift,1]);data[510:512]=b'\x55\xaa'
    for number in range(upcase+1):struct.pack_into('<I',data,24*sector+number*4,0xffffffff)
    for number in range(3,upcase-1):struct.pack_into('<I',data,24*sector+number*4,number+1)
    root=heap*sector
    data[root]=0x81;struct.pack_into('<I',data,root+20,3);struct.pack_into('<Q',data,root+24,bitmap_size)
    data[root+32]=0x82;struct.pack_into('<I',data,root+52,upcase)
    table=ascii_upcase_table()
    start=root+(upcase-2)*cluster
    data[start:start+len(table)]=table
    struct.pack_into('<Q',data,root+56,len(table));struct.pack_into('<I',data,root+36,checksum32(table))
    write_boot_regions(data)
    for bit in range(upcase-1):data[root+cluster+bit//8]|=1<<(bit%8)
    return ExfatFixture(data)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/review/storage-claims/image')
    parser.add_argument('--driver',type=Path,default=ROOT/'build/tests/workspace-image-host')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    inner=empty_image(SIZE,0x42415431)
    inner.add_directory_files('fixture',{'.battery-test':MARKER,'anchor.txt':ANCHOR})
    outer=empty_image(64*1024*1024)
    outer.add_directory_files('srvros-work',{'anchor.txt':b'unchanged','battery-fs.img':bytes(inner.data)})
    baseline=bytes(outer.data)
    entry=dict(outer.entries(outer.read('srvros-work')))['battery-fs.img']
    start=outer.offset(struct.unpack_from('<I',entry,52)[0])
    volume=out/'outer.exfat';results=[]
    cases=[('workload',[]),('pause',['0']),('pause',['18']),('partial',['1']),('flush',['0','0','1']),('refuse',['marker']),('refuse',['identity'])]
    for index,(mode,extra) in enumerate(cases):
        candidate=bytearray(baseline)
        if mode=='refuse':
            if extra==['marker']:
                at=bytes(inner.data).index(MARKER);candidate[start+at]^=1
            else:candidate[start+100]^=1
        volume.write_bytes(candidate)
        result=subprocess.run([str(args.driver.resolve()),str(volume),mode,*extra],capture_output=True,text=True,timeout=60)
        (out/f'{index}-{mode}.log').write_text(result.stdout+result.stderr)
        results.append(dict(mode=mode,args=extra,exit=result.returncode))
        (out/'result.json').write_text(json.dumps(results,indent=2)+'\n')
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        actual=volume.read_bytes()
        assert actual[:start]==baseline[:start] and actual[start+SIZE:]==baseline[start+SIZE:]
        assert ExfatFixture(actual).read('srvros-work/anchor.txt')==b'unchanged'
        if mode=='workload' or (mode=='pause' and extra==['18']):verify(actual[start:start+SIZE],1)
        print(mode,extra,'PASS: lease exclusion, retirement/quarantine and outside-byte containment',flush=True)
if __name__=='__main__':main()
