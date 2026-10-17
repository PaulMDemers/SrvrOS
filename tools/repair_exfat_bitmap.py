#!/usr/bin/env python3
"""Correct a one-byte excess in an exFAT bitmap's declared length, with backup.

Only one-FAT USB volumes, exactly one excess byte, and an unchanged cluster
allocation are supported. Never changes bitmap contents, FAT, or file records.
"""
import argparse
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import hardware_test as host
from inspect_hardware_test import Reader


def plan(stream, volume):
    reader = Reader(stream,volume)
    if reader.read(110,1) != b'\x01': raise ValueError('Only one-FAT volumes are supported')
    entries=[]
    ended=False
    for base,data in reader.chunks(reader.root):
        for pos in range(0,len(data),32):
            if data[pos]==0: ended=True; break
            if data[pos]==0x81: entries.append((base+pos,data[pos:pos+32]))
        if ended: break
    if len(entries)!=1 or entries[0][1][1]!=0: raise ValueError('Ambiguous active bitmap')
    offset,old=entries[0]
    current=struct.unpack_from('<Q',old,24)[0]
    expected=(reader.count+7)//8
    if current==expected: return None
    if current!=expected+1 or expected==0 or current>reader.cluster:
        raise ValueError('Not the supported one-byte bitmap discrepancy')
    first=struct.unpack_from('<I',old,20)[0]
    bitmap=b''.join(data for _,data in reader.chunks(first,current))
    if len(bitmap)!=current or not bitmap[(first-2)//8] & (1<<((first-2)%8)):
        raise ValueError('Bitmap is not self-allocated')
    new=bytearray(old); struct.pack_into('<Q',new,24,expected)
    return dict(volume=volume,cluster_count=reader.count,cluster_size=reader.cluster,
                old_length=current,new_length=expected,offset=offset,before_hex=old.hex(),after_hex=new.hex())


def apply(stream, proposed):
    old,new=bytes.fromhex(proposed['before_hex']),bytes.fromhex(proposed['after_hex'])
    stream.seek(proposed['offset'])
    if stream.read(len(old))!=old: raise ValueError('Bitmap entry changed since planning')
    stream.seek(proposed['offset'])
    if stream.write(new)!=len(new): raise OSError('Short repair write')
    stream.flush(); os.fsync(stream.fileno())
    stream.seek(proposed['offset'])
    if stream.read(len(new))!=new: raise OSError('Repair readback failed')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uuid',required=True)
    parser.add_argument('--backup',type=Path,required=True)
    parser.add_argument('--apply',action='store_true')
    args=parser.parse_args(); volume=host.volume_id(args.uuid)
    inventory=json.loads(subprocess.check_output(['lsblk','--json','--output','NAME,TRAN,UUID,FSTYPE,MOUNTPOINTS'],text=True))
    matches=[]
    def visit(node,transport=None):
        transport=node.get('tran') or transport
        if (node.get('uuid') or '').replace('-','').lower()==volume: matches.append((node,transport))
        for child in node.get('children',[]):visit(child,transport)
    for node in inventory['blockdevices']:visit(node)
    if len(matches)!=1 or matches[0][1]!='usb' or matches[0][0].get('fstype')!='exfat':
        raise ValueError('Expected an unambiguous USB exFAT volume')
    if args.apply and any(matches[0][0].get('mountpoints',[])): raise ValueError('Unmount before repair')
    name=volume.upper(); device=Path('/dev/disk/by-uuid')/(name[:4]+'-'+name[4:])
    fd=os.open(device,os.O_RDWR|os.O_EXCL if args.apply else os.O_RDONLY)
    with os.fdopen(fd,'r+b' if args.apply else 'rb',buffering=0) as stream:
        if not stat.S_ISBLK(os.fstat(stream.fileno()).st_mode): raise ValueError('Expected block device')
        proposed=plan(stream,volume)
        if proposed is None: print('Bitmap length is already correct'); return
        print(json.dumps(proposed,indent=2))
        if not args.apply:return
        if struct.unpack('<H',Reader(stream,volume).read(106,2))[0]&7:
            raise ValueError('Volume is dirty, has media errors, or uses the second FAT')
        host.sync_file(args.backup,(json.dumps(proposed,indent=2)+'\n').encode(),exclusive=True)
        parent=os.open(args.backup.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(parent)
        finally:os.close(parent)
        apply(stream,proposed)
    print('Corrected declared bitmap length; allocation and data are unchanged')


if __name__=='__main__':main()
