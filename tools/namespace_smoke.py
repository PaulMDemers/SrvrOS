#!/usr/bin/env python3
"""Global namespace, legacy mounts and persistence on two disposable disks."""
import argparse
import datetime
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time

from displayd_resolution_smoke import build_iso, has_fatal_exception
from cli_smoke import send_serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--output', type=Path, default=Path('build/review/namespace-smoke'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = args.output.resolve() / datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    output.mkdir(parents=True)
    # On the macOS host, refuse mixed or stale build images.
    manifest = root / 'build/macos-build.json'
    if manifest.exists():
        sys.path.insert(0,str(root/'tools/macos'))
        from macos.run import verified_build
        build = verified_build(root)
    else:
        build = {'source': subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()}
    disks = [output / 'system.exfat', output / 'work.exfat']
    for disk in disks:
        shutil.copyfile(root / 'build/srvros.exfat', disk)
    iso = build_iso(str(root),str(output),1024,768,os.environ.copy(),
        kernel_path=str(root/'build/kernel/srvros.elf'),initramfs_path=str(root/'build/initramfs.tar'))
    record = {'build':build,'disks':[str(d) for d in disks],'boots':[]}
    (output/'result.json').write_text(json.dumps(record,indent=2)+'\n')
    for phase in ('save','reopen'):
        with tempfile.TemporaryDirectory(prefix='srvns-',dir='/tmp') as sockets:
            serial = Path(sockets)/'serial'
            command = [args.qemu,'-M','q35','-accel','tcg,thread=single','-cpu','qemu64',
                '-m','768M','-cdrom',iso,'-boot','d','-device','ich9-ahci,id=ahci',
                '-serial',f'unix:{serial},server=on,wait=on','-monitor','none','-display','none','-no-reboot']
            for i,disk in enumerate(disks):
                command += ['-drive',f'if=none,id=disk{i},file={str(disk).replace(",",",,")},format=raw',
                    '-device',f'ide-hd,drive=disk{i},bus=ahci.{i}']
            with (output/f'{phase}-qemu.log').open('wb') as errors, (output/f'{phase}-serial.log').open('wb') as log:
                process = subprocess.Popen(command,cwd=root,stdout=errors,stderr=errors)
                print(f'namespace-smoke: {phase} PID {process.pid}; evidence {output}',flush=True)
                try:
                    with socket.socket(socket.AF_UNIX) as sock:
                        sock.settimeout(.2)
                        deadline=time.monotonic()+15
                        while True:
                            try: sock.connect(str(serial));break
                            except (FileNotFoundError,ConnectionRefusedError):
                                if process.poll() is not None or time.monotonic()>deadline: raise
                                time.sleep(.1)
                        def receive(prompt,required=None):
                            data=b'';deadline=time.monotonic()+180
                            while time.monotonic()<deadline:
                                if data.endswith(prompt) and (not required or required.encode() in data): break
                                try:
                                    part=sock.recv(65536)
                                    if not part: break
                                    data+=part;log.write(part);log.flush()
                                except socket.timeout: pass
                            text=data.decode(errors='replace')
                            if not data.endswith(prompt) or has_fatal_exception(text) or (required and required not in text):
                                raise RuntimeError(f'{phase}: missing prompt/marker {required!r}; see {log.name}')
                            if 'namespace: FAIL' in text or 'sysabi: fail' in text:
                                raise RuntimeError(f'{phase}: guest failure; see {log.name}')
                            return text
                        def send(command,prompt=b'srv> ',required=None):
                            print(f'namespace-smoke: {command}',flush=True)
                            send_serial(sock,command+'\n',.003);return receive(prompt,required)
                        receive(b'srv> ','namespace: /bin /usr /var /home active')
                        send('mount ahci1 /work',required='on /work type=exfat')
                        if phase=='save':
                            listing=send('ls /',required='home/')
                            for name in ('bin/','usr/','var/','home/','fat/','work/'):
                                if listing.count(name+'\r\n')!=1:
                                    raise RuntimeError(f'monitor root listing missing/duplicates {name}')
                            send('ls /bin',required='true ')
                            send('elf /bin/true',required='elf: type=')
                            send('write /var/log/init.log namespace-must-not-write',required='write: failed')
                            send('write /home/.namespace-monitor monitor-view',required='write: /home/')
                            send('cat /fat/home/.namespace-monitor',required='monitor-view')
                            send('cat /usr/home/.namespace-monitor',required='monitor-view')
                            send('run /bin/true',required='exited status=0')
                        send('run /fat/bin/sh',b' $ ')
                        if phase=='save':
                            send('rm /home/.namespace-monitor',b' $ ')
                        if phase=='save':
                            send('/fat/bin/sysabi',b' $ ','sysabi: ok')
                            send('/bin/sysabi --namespace',b' $ ','namespace: PASS')
                        send(f'/usr/bin/sysabi --namespace-{phase}',b' $ ',f'namespace-persistence: {phase} PASS')
                        send('sync && echo namespace-sync-ok',b' $ ','namespace-sync-ok')
                        send('exit')
                        result=send('unmount /work',required='exfat: unmounted /work')
                        if 'busy' in result or 'failed' in result:
                            raise RuntimeError(f'unmount refused: {result}')
                        # Init retains a log on /fat. Use the normal checked
                        # reboot flush instead of bypassing its live reference.
                        send_serial(sock,'reboot\n',.003)
                        tail=b'';deadline=time.monotonic()+30
                        while process.poll() is None and time.monotonic()<deadline:
                            try:
                                part=sock.recv(65536)
                                if not part: break
                                tail+=part;log.write(part);log.flush()
                            except socket.timeout: pass
                        if process.wait(timeout=5)!=0 or b'failed' in tail:
                            raise RuntimeError('checked guest reboot failed')
                        record['boots'].append({'phase':phase,'command':command,'passed':True})
                finally:
                    process.terminate()
                    try: process.wait(timeout=3)
                    except subprocess.TimeoutExpired: process.kill();process.wait()
                    (output/'result.json').write_text(json.dumps(record,indent=2)+'\n')
    print(f'namespace-smoke: PASS; evidence {output}',flush=True)
    return 0


if __name__=='__main__':
    raise SystemExit(main())
