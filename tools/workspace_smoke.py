#!/usr/bin/env python3
"""Persistent USB workspace lifecycle on disposable, exfatprogs-formatted media."""
import argparse
import hashlib
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
from cli_smoke import read_until, send_serial, has_fatal_exception
from exfat_test_image import ExfatFixture, check_clean_image
from usb_storage_smoke import make_iso
from uefi_usb_smoke import default_ovmf_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    logs = root/'build/review'; logs.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='srvros-workspace-') as directory:
        temp = Path(directory)
        volume_image = temp/'volume.img'
        with volume_image.open('wb') as out: out.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume_image)], check=True)
        fixture = ExfatFixture(volume_image.read_bytes())
        uuid = f'{fixture.u32(100):08x}'
        original = bytes(fixture.data)
        fixture.add_directory_file('srvros-work','.workspace',f'SRVROS-WORKSPACE-V1 {uuid}\n'.encode())
        volume_image.write_bytes(fixture.data)
        print(check_clean_image(volume_image), end='', flush=True)
        disk = temp/'usb.img'
        def reset_disk(data):
            mbr = bytearray(512); mbr[450] = 7
            struct.pack_into('<II',mbr,454,2048,len(data)//512)
            mbr[510:] = b'\x55\xaa'
            with disk.open('wb') as out:
                out.write(mbr); out.seek(2048*512); out.write(data)
        def digest(): return hashlib.sha256(disk.read_bytes()).digest()
        def inspect():
            with disk.open('rb') as source:
                source.seek(2048*512); data = source.read()
            volume_image.write_bytes(data)
            print(check_clean_image(volume_image),end='',flush=True)
            return ExfatFixture(data)
        normal_dir = temp/'normal'; normal_dir.mkdir()
        work_dir = temp/'workspace'; work_dir.mkdir()
        normal_iso = make_iso(root,normal_dir,False)
        work_iso = make_iso(root,work_dir,False,f' srvros.workspace-volume={uuid}')
        def run(label, mode):
            serial = temp/'serial.sock'; serial.unlink(missing_ok=True)
            command = [args.qemu,'-M','q35','-m','512M',
                '-drive',f'if=pflash,format=raw,readonly=on,file={args.ovmf}',
                '-cdrom',str(normal_iso if mode == 'default' else work_iso),'-boot','d',
                '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
                '-drive',f'if=none,id=stick,format=raw,file={disk}'+(',readonly=on' if mode=='failure' else ''),
                '-device','usb-storage,drive=stick,bus=xhci.0',
                '-serial',f'unix:{serial},server=on,wait=on','-display','none','-monitor','none','-no-reboot']
            with (logs/f'workspace-{label}-qemu.log').open('wb') as errors, \
                 (logs/f'workspace-{label}-serial.log').open('wb') as log:
                process = subprocess.Popen(command,stdout=errors,stderr=errors)
                try:
                    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as sock:
                        sock.settimeout(0.2)
                        deadline = time.monotonic()+15
                        while True:
                            try: sock.connect(str(serial)); break
                            except (FileNotFoundError,ConnectionRefusedError):
                                if process.poll() is not None or time.monotonic()>deadline: raise
                                time.sleep(0.1)
                        def receive(prompt=b'srv> ', required=None, timeout=90):
                            data=read_until(sock,prompt,timeout); log.write(data); log.flush()
                            text=data.decode(errors='replace').replace('\r','')
                            if not data.endswith(prompt) or has_fatal_exception(text):
                                raise RuntimeError(f'Guest failed or timed out: {label}; see {log.name}')
                            if required and required not in text: raise RuntimeError(f'Missing {required}: {text}')
                            return text
                        def cmd(text,required=None,shell=False):
                            send_serial(sock,text+'\n',0.003)
                            return receive(b' $ ' if shell else b'srv> ',required)
                        output=receive(timeout=180)
                        assert 'storage: RAM-only mode; AHCI disabled' in output
                        if mode in ('default','missing'):
                            assert 'workspace: ready' not in output
                            cmd('workspace status','workspace: not mounted')
                            cmd('mount usb0p1 /probe','mounted')
                            cmd('write /probe/denied.txt denied','write: failed')
                            cmd('unmount /probe','unmounted')
                        else:
                            assert 'workspace: ready at /work/srvros-work' in output
                            if mode=='failure':
                                cmd('write /work/srvros-work/denied.txt denied','write: failed')
                                cmd('workspace sync','workspace: flush failed; remains mounted')
                                cmd('workspace unmount')
                                cmd('workspace status','state=write-failed')
                                cmd('reboot','reboot: workspace could not be unmounted')
                                return
                            cmd('run /fat/bin/sh',shell=True)
                            cmd('cd /work/srvros-work',shell=True)
                            if mode=='write':
                                cmd('echo MixedCase-document > notes.txt',shell=True)
                                cmd('echo second-line >> notes.txt',shell=True)
                                cmd("echo 'echo persistent-script-ok' > saved.sh",shell=True)
                            cmd('cat notes.txt','MixedCase-document\nsecond-line\n',shell=True)
                            cmd('sh saved.sh','\npersistent-script-ok\n',shell=True)
                            cmd("sh -c 'cat notes.txt'",'MixedCase-document\nsecond-line\n',shell=True)
                            cmd('sync && echo workspace-sync-ok','\nworkspace-sync-ok\n',shell=True)
                            cmd('exit')
                            cmd('workspace sync','workspace: flushed')
                            cmd('unmount /work','workspace: unmounted; USB writes disabled')
                            cmd('mount usb0p1 /probe','mounted')
                            cmd('write /probe/denied.txt denied','write: failed')
                            cmd('unmount /probe','unmounted')
                            cmd(f'workspace mount {uuid}','workspace: ready')
                            # Exercise reboot's automatic workspace flush/unmount.
                            send_serial(sock,'reboot\n',0.003)
                            data=read_until(sock,b'reboot: ACPI reset',20)
                            log.write(data);log.flush()
                            assert b'workspace: unmounted; USB writes disabled' in data
                            assert b'reboot: ACPI reset' in data
                            process.wait(15)
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try: process.wait(10)
                        except subprocess.TimeoutExpired: process.kill();process.wait()
            print(f'workspace-smoke: {label} PASS',flush=True)

        reset_disk(fixture.data); before=digest();run('default-readonly','default');assert digest()==before
        reset_disk(original); before=digest();run('missing-marker','missing');assert digest()==before
        reset_disk(fixture.data)
        for mode in ('write','persisted'):
            run(mode,mode)
            result=inspect()
            assert result.read('srvros-work/notes.txt')==b'MixedCase-document\nsecond-line\n'
            assert result.read('srvros-work/saved.sh')==b'echo persistent-script-ok\n'
            assert 'denied.txt' not in {n for n,e in result.entries(result.chain(result.root))}
        reset_disk(fixture.data);before=digest();run('write-failure','failure');assert digest()==before
        print('workspace-smoke: PASS (default/marker guards, shell files/scripts, sync, reboot persistence, revoke, write failure)',flush=True)


if __name__=='__main__': main()
