#!/usr/bin/env python3
"""Installed-root salvage and failed retirement through the managed USB controller."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import dirty_salvage_smoke as dirty
import destination_salvage_smoke as destination
import installation_image as provision
import installed_disk_smoke as proof
from exfat_test_image import ExfatFixture
from uefi_usb_smoke import default_ovmf_path

ROOT=Path(__file__).resolve().parents[1]
ORIGINAL=b'managed-workspace-source-v1\n'
USB_OFFSET=2048*512

def make_usb(path):
    fixture=ExfatFixture((ROOT/'build/srvros.exfat').read_bytes())
    uuid=f'{fixture.u32(100):08x}'
    fixture.add_directory_files('srvros-work',{'.workspace':f'SRVROS-WORKSPACE-V1 {uuid}\n'.encode(),'source':ORIGINAL})
    mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
    with path.open('xb') as stream:
        stream.write(mbr);stream.seek(USB_OFFSET);stream.write(fixture.data)
    return uuid

def work_data(path):
    with path.open('rb') as stream:stream.seek(USB_OFFSET);return stream.read()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu',default='qemu-system-x86_64')
    parser.add_argument('--ovmf',default=default_ovmf_path())
    parser.add_argument('--ovmf-vars',required=True)
    parser.add_argument('--boot-wait',type=int,default=120)
    args=parser.parse_args()
    for name in (args.ovmf,args.ovmf_vars):
        if not Path(name).is_file() or ',' in name:parser.error('Invalid firmware template')
    build=proof.verified_build(ROOT)
    base=ROOT/'build/review/managed-salvage';base.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='proof-',dir=base));print(f'Managed-salvage evidence: {out}',flush=True)
    receipt={'status':'INCOMPLETE','build':build,'cases':[],
        'qemu_version':subprocess.check_output([args.qemu,'--version'],text=True).splitlines()[0],
        'firmware_sha256':dirty.sha(Path(args.ovmf)),'vars_template_sha256':dirty.sha(Path(args.ovmf_vars))}
    try:
        receipt['mechanism']=dirty.probe(out)
        original=out/'original';original.mkdir();system=original/'system'
        provision.prepare(system,ROOT/'build/kernel/srvros.elf',ROOT/'build/limine-binary/BOOTX64.EFI',ROOT/'build/srvros.exfat',ROOT/'build/userspace/init.elf',identity_bound=True)
        provision.apply(system)
        usb=original/'usb.img';uuid=make_usb(usb);receipt['uuid']=uuid
        destination.allocation(original,'usb',work_data(usb))
        control=out/'control';control.mkdir()
        control_system=control/'system.img';control_usb=control/'usb.img'
        destination.copy_image(system/'image.img',control_system);destination.copy_image(usb,control_usb)
        proof.boot(control_system,control,1,args,other_disk=control_usb,require_identity=True,managed_workspace='control',workspace_uuid=uuid)
        if ExfatFixture(work_data(control_usb)).read('srvros-work/source')!=dirty.PAYLOAD:raise RuntimeError('Clean USB control did not persist source bytes')
        destination.allocation(control,'usb',work_data(control_usb))
        receipt['cases'].append({'name':'clean-managed-control','status':'PASS'})
        case=out/'fault';case.mkdir();disk=case/'system.img';work=case/'usb.img'
        destination.copy_image(system/'image.img',disk);destination.copy_image(usb,work)
        _,(offset,size)=proof.partitions(disk);protected=proof.outside_data(disk,offset,size);work_hash=dirty.sha(work)
        cfg=case/'work.conf';cfg.write_text(dirty.RULE)
        for number,mode in ((1,'save'),(2,'reopen')):
            proof.boot(disk,case,number,args,other_disk=work,require_identity=True,managed_workspace=mode,workspace_uuid=uuid,work_fault_config=cfg if number==1 else None)
            if dirty.sha(work)!=work_hash or proof.outside_data(disk,offset,size)!=protected:raise RuntimeError('Failed workspace or protected system regions changed')
            data=proof.data_image(disk,offset,size);fs=ExfatFixture(data)
            try:payload=fs.read('home/.managed-salvage-copy')
            except StopIteration:payload=None
            if payload!=(dirty.PAYLOAD if number==1 else None):raise RuntimeError('Healthy copy persistence/cleanup mismatch')
            destination.allocation(case,number,data)
            receipt['cases'].append({'name':f'managed-{mode}','status':'PASS','work_sha256':work_hash,'healthy_bytes':len(dirty.PAYLOAD)})
            proof.save_json(out/'receipt.json',receipt)
        if ExfatFixture(work_data(work)).read('srvros-work/source')!=ORIGINAL:raise RuntimeError('Failed source changed on disk')
        receipt['status']='PASS'
    except Exception as error:
        receipt.update(status='FAIL',error=f'{type(error).__name__}: {error}');raise
    finally:proof.save_json(out/'receipt.json',receipt)
    print(f'managed-salvage: PASS; evidence {out}',flush=True)

if __name__=='__main__':main()
