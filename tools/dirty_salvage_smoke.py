#!/usr/bin/env python3
"""Installed-root dirty-source salvage; blkdebug wraps only a disposable work disk."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import tarfile
import installation_image as provision
import installed_disk_smoke as proof
from exfat_test_image import ExfatFixture
from uefi_usb_smoke import default_ovmf_path

ROOT=Path(__file__).resolve().parents[1]
RULE='[inject-error]\nevent = "write_aio"\niotype = "write"\nerrno = "5"\nonce = "on"\n'
PAYLOAD=bytes((i*43+23)&255 for i in range(12289))

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def probe(out):
    folder=out/'probe';folder.mkdir()
    work=folder/'work.img';healthy=folder/'healthy.img';cfg=folder/'write.conf'
    work.write_bytes(bytes(4096));healthy.write_bytes(bytes(4096));cfg.write_text(RULE)
    cmd=['qemu-io','-f','raw','-c','read 0 512','-c','write -P 17 0 512','-c','write -P 34 512 512',f'blkdebug:{cfg}:{work}']
    r=subprocess.run(cmd,capture_output=True,text=True)
    (folder/'work.log').write_text(r.stdout+r.stderr)
    if not r.returncode or 'Input/output error' not in r.stdout+r.stderr or work.read_bytes()[:512]!=bytes(512) or work.read_bytes()[512:1024]!=bytes([34])*512:
        raise RuntimeError('Fault probe did not prove one-shot write-only EIO')
    healthy_cmd=['qemu-io','-f','raw','-c','write -P 51 0 512',str(healthy)]
    h=subprocess.run(healthy_cmd,capture_output=True,text=True)
    (folder/'healthy.log').write_text(h.stdout+h.stderr)
    if h.returncode or healthy.read_bytes()[:512]!=bytes([51])*512: raise RuntimeError('Healthy-image probe failed')
    return {'commands':[cmd,healthy_cmd],'status':'PASS','rule':RULE}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu',default='qemu-system-x86_64')
    parser.add_argument('--ovmf',default=default_ovmf_path())
    parser.add_argument('--ovmf-vars',required=True)
    parser.add_argument('--boot-wait',type=int,default=120)
    args=parser.parse_args()
    for name in (args.ovmf,args.ovmf_vars):
        if not Path(name).is_file() or ',' in name: parser.error('Invalid firmware template')
    build=proof.verified_build(ROOT)
    base=ROOT/'build/review/dirty-salvage';base.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='proof-',dir=base))
    print(f'Dirty-salvage evidence: {out}',flush=True)
    receipt={'status':'INCOMPLETE','build':build,'boots':[],
             'firmware_sha256':sha(Path(args.ovmf)),'scope':'two fixture GPT images; host-only one-shot work EIO'}
    try:
        receipt['mechanism']=probe(out)
        images=[]
        for name in ('system','work'):
            stage=out/name
            provision.prepare(stage,ROOT/'build/kernel/srvros.elf',ROOT/'build/limine-binary/BOOTX64.EFI',ROOT/'build/srvros.exfat',ROOT/'build/userspace/init.elf',identity_bound=True)
            ready=provision.apply(stage);images.append(stage/'image.img');receipt[name]=ready
            with tarfile.open(stage/'bootstrap.tar') as archive:
                if archive.getnames()!=['init']: raise RuntimeError('Unexpected installed bootstrap')
        system,work=images
        _,(offset,size)=proof.partitions(system)
        protected=proof.outside_data(system,offset,size);work_hash=sha(work)
        _,(wo,ws)=proof.partitions(work)
        if ExfatFixture(proof.data_image(work,wo,ws)).read('bin/true')!=proof.runtime_elf((ROOT/'build/userspace/true.elf').read_bytes()):
            raise RuntimeError('Work source differs from matched preseeded payload')
        cfg=out/'work.conf';cfg.write_text(RULE)
        for number,reverse,device in ((1,False,'ahci0p2'),(2,True,'ahci1p2')):
            proof.boot(system,out,number,args,other_disk=work,reverse=reverse,expected_device=device,require_identity=True,
                       dirty_salvage=True,work_fault_config=cfg if number==1 else None)
            if sha(work)!=work_hash or proof.outside_data(system,offset,size)!=protected:
                raise RuntimeError('Work image or protected system regions changed')
            data=proof.data_image(system,offset,size);fs=ExfatFixture(data)
            try: value=fs.read('home/.dirty-salvage-copy')
            except StopIteration: value=None
            if value!=(PAYLOAD if number==1 else None): raise RuntimeError('Offline healthy copy/cleanup mismatch')
            extracted=out/f'data-{number}.exfat';extracted.write_bytes(data)
            check=subprocess.run([str(ROOT/'build/tests/exfat-driver'),str(extracted),'allocation-check'],capture_output=True,text=True)
            (out/f'allocation-{number}.log').write_text(check.stdout+check.stderr)
            if check.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in check.stdout: raise RuntimeError('Healthy allocation check failed')
            receipt['boots'].append({'number':number,'selected':device,'fault':number==1,'work_sha256':work_hash,'status':'PASS'})
            proof.save_json(out/'receipt.json',receipt)
        receipt['status']='PASS'
    except Exception as error:
        receipt.update(status='FAIL',error=f'{type(error).__name__}: {error}')
        raise
    finally: proof.save_json(out/'receipt.json',receipt)
    print(f'dirty-salvage: PASS; evidence {out}',flush=True)

if __name__=='__main__':main()
