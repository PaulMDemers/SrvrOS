#!/usr/bin/env python3
"""Qualify real salvage CLI destination write/flush failures on disposable GPTs."""
import argparse
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import dirty_salvage_smoke as dirty
import installed_disk_smoke as proof
import installation_image as provision
from exfat_test_image import ExfatFixture
from uefi_usb_smoke import default_ovmf_path

ROOT=Path(__file__).resolve().parents[1]

def target_rule(open_writes,mode):
    if open_writes<1 or mode not in ('write','flush'): raise ValueError('Invalid target calibration')
    # Admit creation and the fsync dirty marker; fault allocation write, or the
    # next flush after allocation begins. Count writes, never redundant flushes.
    operation=open_writes+2
    transitions=operation-1
    rules=[f'[set-state]\nevent = "write_aio"\nstate = "{i}"\nnew_state = "{i+1}"\n' for i in range(1,transitions+1)]
    event='write_aio'
    rules.append(f'[inject-error]\nevent = "{event}"\niotype = "{mode}"\nstate = "{transitions+1}"\nerrno = "5"\nonce = "on"\n')
    return '\n'.join(rules)

def probe(out,mode):
    folder=out/f'probe-{mode}';folder.mkdir()
    image=folder/'target.img';image.write_bytes(bytes(8192))
    cfg=folder/'rule.conf';cfg.write_text(target_rule(2,mode))
    commands=[f'write -P {i+1} {i*512} 512' for i in range(4)]
    if mode=='flush':commands+=['flush']
    cmd=['qemu-io','-t','writeback','-f','raw']
    for command in commands:cmd+=['-c',command]
    cmd+=[f'blkdebug:{cfg}:{image}']
    control=['qemu-io','-t','writeback','-f','raw']
    for command in commands[:3]+['flush']:control+=['-c',command]
    control+=[cmd[-1]]
    before=subprocess.run(control,capture_output=True,text=True)
    (folder/'control.log').write_text(before.stdout+before.stderr)
    if before.returncode:raise RuntimeError('Rule fired before calibrated boundary')
    image.write_bytes(bytes(8192))
    result=subprocess.run(cmd,capture_output=True,text=True)
    (folder/'probe.log').write_text(result.stdout+result.stderr)
    # qemu-io flush communicates failure by exit status without printing errno.
    errors=(result.stdout+result.stderr).count('Input/output error')
    if result.returncode!=1 or errors!=(1 if mode=='write' else 0) or result.stdout.count('wrote 512/512')!=(3 if mode=='write' else 4):
        raise RuntimeError(f'{mode} rule did not inject the intended operation')
    for i in range(4):
        expected=bytes([i+1])*512 if mode=='flush' or i<3 else bytes(512)
        if image.read_bytes()[i*512:(i+1)*512]!=expected:raise RuntimeError('Counted rule fired at wrong write')
    return {'status':'PASS','command':cmd,'control_command':control,'rule':cfg.read_text()}

def copy_image(source,destination):
    # Independent copy-on-write clones bound disposable evidence storage on APFS.
    # Never use hard links: every case must have its own writable disk identity.
    if sys.platform=='darwin':
        subprocess.run(['cp','-c',str(source),str(destination)],check=True)
    else:
        shutil.copyfile(source,destination)

def prepare(out):
    images=[]
    for name in ('system','work','target'):
        stage=out/name
        provision.prepare(stage,ROOT/'build/kernel/srvros.elf',ROOT/'build/limine-binary/BOOTX64.EFI',ROOT/'build/srvros.exfat',ROOT/'build/userspace/init.elf',identity_bound=True)
        provision.apply(stage);images.append(stage/'image.img')
    return images

def allocation(out,number,data):
    path=out/f'data-{number}.exfat';path.write_bytes(data)
    r=subprocess.run([str(ROOT/'build/tests/exfat-driver'),str(path),'allocation-check'],capture_output=True,text=True)
    (out/f'allocation-{number}.log').write_text(r.stdout+r.stderr)
    if r.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in r.stdout:raise RuntimeError('Healthy allocation check failed')

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
    base=ROOT/'build/review/destination-salvage';base.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='proof-',dir=base));print(f'Destination-salvage evidence: {out}',flush=True)
    receipt={'status':'INCOMPLETE','build':build,'cases':[]}
    try:
        receipt['probes']={mode:probe(out,mode) for mode in ('write','flush')}
        original=out/'original';original.mkdir();images=prepare(original)
        calibration=out/'calibration';calibration.mkdir()
        clones=[]
        for name,image in zip(('system','work','target'),images):
            clone=calibration/f'{name}.img';copy_image(image,clone);clones.append(clone)
        proof.boot(clones[0],calibration,1,args,other_disk=clones[1],target_disk=clones[2],destination_mode='calibrate',require_identity=True)
        counts=json.loads((calibration/'target-open.json').read_text());writes=counts['writes']
        receipt['calibration']=counts
        for mode in ('write','flush'):
            case=out/mode;case.mkdir();copies=[]
            for name,image in zip(('system','work','target'),images):
                clone=case/f'{name}.img';copy_image(image,clone);copies.append(clone)
            system,work,target=copies
            _,(offset,size)=proof.partitions(system);_,(to,ts)=proof.partitions(target)
            protected=proof.outside_data(system,offset,size);target_protected=proof.outside_data(target,to,ts);work_hash=dirty.sha(work)
            work_cfg=case/'work.conf';work_cfg.write_text(dirty.RULE)
            target_cfg=case/'target.conf';target_cfg.write_text(target_rule(writes,mode))
            target_hash=None
            for number,reverse,device in ((1,False,'ahci0p2'),(2,True,'ahci1p2')):
                proof.boot(system,case,number,args,other_disk=work,reverse=reverse,expected_device=device,require_identity=True,
                    dirty_salvage=True,work_fault_config=work_cfg if number==1 else None,target_disk=target,
                    target_fault_config=target_cfg if number==1 else None,destination_mode=mode if number==1 else 'reopen')
                if dirty.sha(work)!=work_hash or proof.outside_data(system,offset,size)!=protected or proof.outside_data(target,to,ts)!=target_protected:
                    raise RuntimeError('Work or protected image regions changed')
                data=proof.data_image(system,offset,size);fs=ExfatFixture(data)
                try:value=fs.read('home/.dirty-salvage-copy')
                except StopIteration:value=None
                if value!=(dirty.PAYLOAD if number==1 else None):raise RuntimeError('Healthy copy/cleanup mismatch')
                allocation(case,number,data)
                if number==1:
                    failed=proof.data_image(target,to,ts);(case/'uncertain-target.exfat').write_bytes(failed)
                    if ExfatFixture(failed).read('.dirty-salvage-attempt')!=b'' or not struct.unpack_from('<H',failed,106)[0]&2:
                        raise RuntimeError('Expected retained empty artifact and dirty target')
                    target_hash=dirty.sha(target)
                elif dirty.sha(target)!=target_hash:raise RuntimeError('Failed target changed during healthy restart')
            receipt['cases'].append({'mode':mode,'status':'PASS','open_writes':writes,'fault_after_creation_writes':2,'work_sha256':work_hash,'uncertain_target_sha256':target_hash,'healthy_bytes':len(dirty.PAYLOAD)})
            proof.save_json(out/'receipt.json',receipt)
        receipt['status']='PASS'
    except Exception as error:
        receipt.update(status='FAIL',error=f'{type(error).__name__}: {error}');raise
    finally:proof.save_json(out/'receipt.json',receipt)
    print(f'destination-salvage: PASS; evidence {out}',flush=True)

if __name__=='__main__':main()
