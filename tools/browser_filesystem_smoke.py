#!/usr/bin/env python3
"""Run the filesystem fixture twice on a fresh private exFAT/Q35 guest."""
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import time

from browser_evidence import verify_runtime
from browser_runtime_smoke import receive, receive_run
from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT/'build/browser-filesystem'
BASE.mkdir(parents=True, exist_ok=True)
run = Path(tempfile.mkdtemp(prefix='guest-', dir=BASE))
receipt = {'result': 'failed', 'scope': 'private Q35/TCG exFAT filesystem probe; no physical acceptance', 'tests': []}
output = bytearray()
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()
def tool(name,args,timeout=90):
    with (run/(name+'.log')).open('wb') as log:
        subprocess.run(list(map(str,args)),cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=timeout)
def verify_fs(runtime):
    path=BASE/'receipt.json'; data=json.loads(path.read_text()); hashes=data['sha256']
    if data.get('result')!='success' or data.get('runtime',{}).get('sdk_headers_sha256')!=runtime['sdk_headers_sha256']:
        raise ValueError('filesystem receipt failed or SDK header mismatch')
    required=['ports/srvros/libcxx/build_filesystem.py','ports/srvros/libcxx/filesystem_guest.cc','ports/srvros/libcxx/patches/filesystem-copy.patch','ports/srvros/libcxx/config.h','build/browser-filesystem/libcxx-filesystem.a','build/browser-filesystem/fsprobe.elf','build/sysroot/srvros/lib/libsrvros.a','build/sysroot/srvros/lib/crt0.o','build/sysroot/srvros/lib/app.ld']
    if not all(name in hashes for name in required): raise ValueError('filesystem receipt missing required inputs')
    for name,expected in hashes.items():
        p=Path(name)
        if p.is_absolute() or '..' in p.parts or not (ROOT/p).is_file() or sha(ROOT/p)!=expected:
            raise ValueError('filesystem receipt mismatch: '+name)
    return {'path':str(path),'sha256':sha(path)}
try:
    if shutil.disk_usage(BASE).free < 2*1024**3+200*1024**2: raise RuntimeError('insufficient space for private disk plus 2 GiB reserve')
    receipt['source_observation_only']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    receipt['dirty_observation_only']=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)
    receipt['ports_head']=subprocess.check_output(['git','-C','ports','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    receipt['runtime_provenance']=verify_runtime()
    receipt['filesystem_provenance']=verify_fs(receipt['runtime_provenance'])
    artifacts={'kernel':ROOT/'build/kernel/srvros.elf','init':ROOT/'build/userspace/init.elf','fsprobe':BASE/'fsprobe.elf','limine_bios':ROOT/'build/limine-binary/limine-bios.sys','limine_cd':ROOT/'build/limine-binary/limine-bios-cd.bin','limine_tool':ROOT/'build/limine-binary/limine','harness':Path(__file__).resolve()}
    receipt['artifacts']={k:{'path':str(v),'sha256':sha(v)} for k,v in artifacts.items()}
    disk=run/'private.exfat'; tool('disk-create',[sys.executable,ROOT/'tools/mk_exfat_image.py',disk])
    if disk.stat().st_size != 128*1024*1024: raise ValueError('unexpected private disk size')
    fixture=ExfatFixture(disk.read_bytes()); fixture.add_directory_files('fsprobe',{})
    disk.write_bytes(fixture.data); del fixture
    receipt['disk']={'path':str(disk),'sha256_before':sha(disk),'size':disk.stat().st_size}
    iso_root=run/'iso-root'; boot=iso_root/'boot/limine'; boot.mkdir(parents=True)
    shutil.copyfile(artifacts['kernel'],iso_root/'boot/srvros.elf')
    shutil.copyfile(artifacts['limine_bios'],boot/'limine-bios.sys');shutil.copyfile(artifacts['limine_cd'],boot/'limine-bios-cd.bin')
    with tarfile.open(iso_root/'boot/initramfs.tar','w',format=tarfile.USTAR_FORMAT) as archive:
        for name,key in [('init','init'),('fsprobe','fsprobe')]:
            data=runtime_elf(artifacts[key].read_bytes()); info=tarfile.TarInfo(name);info.size=len(data);info.mode=0o755;info.mtime=0
            archive.addfile(info,io.BytesIO(data))
    (boot/'limine.conf').write_text('timeout: 0\nserial: yes\n\n/browser-filesystem\n    protocol: limine\n    path: boot():/boot/srvros.elf\n    cmdline: srvros.log=quiet\n    module_path: boot():/boot/initramfs.tar\n    module_string: initramfs\n')
    iso=run/'filesystem.iso'
    tool('iso-create',['xorriso','-as','mkisofs','-R','-r','-J','-b','boot/limine/limine-bios-cd.bin','-no-emul-boot','-boot-load-size','4','-boot-info-table',iso_root,'-o',iso])
    tool('iso-install',[artifacts['limine_tool'],'bios-install',iso])
    receipt['iso_sha256']=sha(iso)
    with tempfile.TemporaryDirectory(prefix='srv-fs-',dir='/tmp') as temp:
        serial=str(Path(temp)/'serial')
        qemu=['qemu-system-x86_64','-machine','q35','-accel','tcg,thread=single','-cpu','qemu64','-smp','1','-m','768M','-cdrom',str(iso),'-boot','d','-device','ich9-ahci,id=ahci','-drive',f'if=none,id=disk0,file={str(disk).replace(",",",,")},format=raw','-device','ide-hd,drive=disk0,bus=ahci.0','-display','none','-nic','none','-serial',f'unix:{serial},server=on,wait=off','-monitor','none','-no-reboot']
        receipt['qemu_command']=qemu
        with (run/'qemu.log').open('wb') as log:
            proc=subprocess.Popen(qemu,stdout=log,stderr=subprocess.STDOUT);receipt['pid']=proc.pid
            try:
                with socket.socket(socket.AF_UNIX) as sock:
                    end=time.monotonic()+15
                    while True:
                        try: sock.connect(serial);break
                        except (FileNotFoundError,ConnectionRefusedError):
                            if proc.poll() is not None or time.monotonic()>=end: raise RuntimeError('guest serial connection failed')
                            time.sleep(.1)
                    sock.settimeout(.2); boot_data=receive(sock,b'srv> ',90); output.extend(boot_data)
                    if b'srv> ' not in boot_data: raise RuntimeError('guest did not reach monitor')
                    for index in range(2):
                        started=time.monotonic();sock.sendall(b'run /fsprobe /fat/fsprobe\n'); result=receive_run(sock,60);output.extend(result)
                        exits=re.findall(rb'run: pid=\d+ exited status=(\d+)',result)
                        passed=(result.count(b'fsprobe: ok')==1 and exits==[b'0'] and b'srv> ' in result and b'exception:' not in result)
                        receipt['tests'].append({'run':index+1,'status':exits[0].decode() if len(exits)==1 else None,'passed':passed,'seconds':round(time.monotonic()-started,3)})
                        if not passed: raise RuntimeError('filesystem guest run failed: '+str(index+1))
            finally:
                proc.terminate()
                try:proc.wait(timeout=5)
                except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=5)
                receipt['guest_stopped']=proc.poll() is not None
    receipt['disk']['sha256_after']=sha(disk)
    result=ExfatFixture(disk.read_bytes())
    receipt['disk']['probe_dir_empty_after']=not list(result.entries(result.read('fsprobe')))
    if not receipt['disk']['probe_dir_empty_after']: raise RuntimeError('probe directory not empty after runs')
    receipt['result']='passed'
except (OSError,ValueError,KeyError,RuntimeError,subprocess.SubprocessError,tarfile.TarError) as exc:
    receipt['error']=str(exc)
finally:
    disk=run/'private.exfat'
    if disk.is_file() and 'disk' in receipt and 'sha256_after' not in receipt['disk']: receipt['disk']['sha256_after']=sha(disk)
    (run/'serial.log').write_bytes(output)
    (run/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Filesystem guest receipt:',run/'receipt.json','result:',receipt['result'])
sys.exit(0 if receipt['result']=='passed' else 1)
