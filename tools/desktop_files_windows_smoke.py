#!/usr/bin/env python3
"""Actual multi-window Files ownership, navigation and accepted-copy retirement."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP
from exfat_test_image import ExfatFixture, check_clean_image


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--windows',type=int,default=20,choices=range(3,33))
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=root/'build/review/desktop-files-windows';out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    output=b'';consumed=0;ids=[]
    large=bytes(range(256))*32768
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix='srvros-files-windows-') as directory:
        tmp=Path(directory);disk=tmp/'disk.exfat';source=ExfatFixture((root/'build/srvros.exfat').read_bytes())
        # Independently formatted metadata is required for before/after fsck.
        with disk.open('xb') as stream:stream.truncate(64*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','4K',str(disk)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(disk.read_bytes())
        fixture.add_directory_files('bin',{name:source.read('bin/'+name) for name in ('deskd','desktools','deskfs','sync','sh','ps')})
        fixture.add_directory_files('etc',{'init.sh':b'echo files-windows-fixture-ready\n'})
        fixture.add_directory_files('home',{})
        fixture.add_directory_files('left',{'left.txt':b'Left original.\n','collision.txt':b'Keep collision.\n','large.bin':large})
        fixture.add_directory_files('right',{'right.txt':b'Right original.\n'});disk.write_bytes(fixture.data)
        (out/'before-fsck.txt').write_text(check_clean_image(disk))
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy())
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
            '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
            '-qmp',f'unix:{tmp/"qmp"},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=40):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if marker==b'srv> ' and not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None:
                            qmp.call('screendump',{'filename':str(out/'failure.ppm')});raise RuntimeError(f'Missing {marker!r}; see {out}')
                        try:
                            chunk=serial.recv(16384)
                            if not chunk:raise RuntimeError('serial disconnected')
                            output+=chunk;(out/'serial.log').write_bytes(output)
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def wait_all(markers,timeout=40):
                    nonlocal consumed
                    start=consumed;end=start
                    for marker in markers:consumed=start;wait(marker,timeout);end=max(end,consumed)
                    consumed=end
                def shortcut(keys):qmp.call('human-monitor-command',{'command-line':'sendkey '+keys});time.sleep(.3)
                def text(value):
                    mapping={' ':('spc',False),'/':('slash',False),'.':('dot',False),'-':('minus',False),'_':('minus',True),'\n':('ret',False)}
                    for char in value:
                        key,shift=mapping.get(char,(char.lower(),char.isupper()));qmp.key(key,shift)
                def shot(name):qmp.call('screendump',{'filename':str(out/(name+'.ppm'))})
                def launch(query,marker):shortcut('meta_l-spc');wait(b'launcher opened');text(query);qmp.key('ret');wait(marker)
                def admitted():
                    wait(b'files-runtime: admitted window=');wait(b'\n')
                    identity=int(re.findall(rb'files-runtime: admitted window=(\d+)',output)[-1]);assert not ids or identity>max(ids)
                    ids.append(identity);wait_all([b'desktools: files-location path=',b'deskd: commit']);return identity
                def navigate(path):shortcut('ctrl-l');shortcut('ctrl-a');text(path);qmp.key('ret');wait(('files-location path='+path+' ').encode())
                def modal_ready(marker):wait_all([marker,b'deskd: commit'])
                def baseline():
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                    return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                wait(b'srv> ',100);before=baseline();assert before
                # Synchronize on the first actual surface, not the earlier human log.
                # That diagnostic can interleave with compositor preference output
                # even when Text has published successfully. Exact typed/saved
                # bytes and full owner/process retirement remain required below.
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n');wait(b'stage=first-publish edge=event');wait(b'deskd: commit')
                text('Sibling kept.')
                launch('Files new app',b'desktools: starting role=files');left=admitted()
                owner=int(re.findall(rb'app-timing: pid=(\d+) role=1 window=0 stage=main-entry',output)[-1]);retired=f'deskd: retired pid={owner}'.encode()
                navigate('/fat/left');text('left');shortcut('f2');modal_ready(b'files-prompt operation=5');qmp.key('esc');wait(retired)
                shortcut('f2');modal_ready(b'files-prompt operation=5');text('renamed.txt');qmp.key('ret')
                wait_all([b'file-result role=files op=5 status=0',b'file-result role=files op=3 status=0',retired])
                shortcut('f2');modal_ready(b'files-prompt operation=5');text('collision.txt');qmp.key('ret')
                wait_all([b'file-result role=files op=5 status=2',retired])
                shortcut('ctrl-shift-n');right=admitted();navigate('/fat/right')
                launch('left window',b'launcher window=');shortcut('alt-left');wait(b'files-location path=/fat/home history=1/2')
                shortcut('alt-right');wait(b'files-location path=/fat/left history=2/2')
                launch('right window',b'launcher window=');shortcut('alt-left');wait(b'files-location path=/fat/left history=1/2')
                shortcut('alt-right');wait(b'files-location path=/fat/right history=2/2')
                for _ in range(args.windows-2):shortcut('ctrl-shift-n');admitted()
                opened=re.findall(rb'deskd: opened pid='+str(owner).encode()+rb' window=\d+ id=\d+ parent=0',output)
                assert len(opened)==args.windows
                shot('grown-windows')
                launch('left window',b'launcher window=');text('large')
                # File menu: New window/Open/New folder/Info/Copy path/Rename/Duplicate.
                shortcut('f10');shortcut('right');shortcut('home')
                for _ in range(6):shortcut('down')
                shortcut('ret');modal_ready(b'files-prompt operation=7')
                qmp.call('block_set_io_throttle',{'device':'disk','bps':0,'bps_rd':0,'bps_wr':1048576,'iops':0,'iops_rd':0,'iops_wr':0})
                qmp.key('ret');wait_all([b'file-submitted role=files op=7',retired]);shortcut('ctrl-w')
                close=f'window-lifecycle: close-request role=1 window={left} busy=1 operation=7'.encode();wait(close)
                # While the accepted copy owns its closed-intent model, exercise
                # another process's input and save instead of abandoning the job.
                launch('Text window',b'launcher window=');text(' concurrent.')
                shortcut('ctrl-l');shortcut('ctrl-a');text('/fat/typed-during-copy.txt');shortcut('ctrl-s')
                wait_all([b'picker ready mode=2',b'deskd: commit']);qmp.key('ret')
                wait_all([b'file-result role=text op=2 status=0',b'file-result role=files op=7 status=0',f'files-runtime: closed window={left} '.encode()],90)
                assert output.index(close)<output.index(b'file-result role=files op=7 status=0')<output.index(f'files-runtime: closed window={left} '.encode())
                qmp.call('block_set_io_throttle',{'device':'disk','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':0})
                launch('right window',b'launcher window=');shortcut('ctrl-r');wait(b'file-result role=files op=3 status=0');shortcut('ctrl-q')
                wait(b'desktools: exited role=files failure=0')
                assert f'files-runtime: documents admitted={args.windows} retired={args.windows} refused=0'.encode() in output
                pools=re.findall(rb'file-pool: retired role=files spawned=(\d+) reaped=(\d+) peak=(\d+) live=0 bound=0',output)
                assert len(pools)==1 and pools[0][0]==pools[0][1] and 1<=int(pools[0][2])<=2
                launch('Text window',b'launcher window=');shortcut('ctrl-q');wait(b'desktools: exited role=text failure=0')
                shot('all-owners-retired');shortcut('f10');shortcut('end');shortcut('ret');wait(b'deskd: exited',60);wait(b'srv> ')
                end=time.monotonic()+10
                while baseline()!=before:
                    if time.monotonic()>end:raise RuntimeError('Process baseline not restored')
                    time.sleep(.5)
                assert b'failure=1' not in output and b'exception:' not in output and b'deskd: FAIL' not in output
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output);shutil.copyfile(disk,out/'last-workspace.img')
        result=ExfatFixture(disk.read_bytes())
        expected={'left/renamed.txt':b'Left original.\n','left/collision.txt':b'Keep collision.\n','right/right.txt':b'Right original.\n',
                  'left/large.bin':large,'left/large copy.bin':large,'typed-during-copy.txt':b'Sibling kept. concurrent.'}
        for name,value in expected.items():assert result.read(name)==value,name
        assert not any(name=='left.txt' for name,_ in result.entries(result.read('left')))
        (out/'fsck.txt').write_text(check_clean_image(disk))
        allocation=subprocess.check_output([str(root/'build/tests/exfat-driver'),str(disk),'allocation-check'],text=True);(out/'allocation.txt').write_text(allocation)
        receipt=dict(status='PASS',windows=args.windows,owner=owner,identities=ids,accepted_copy_bytes=len(large),close_while_copy_busy=True,
            process_baseline_restored=True,physical_acceptance=False,source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
            source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],text=True)),
            sha256={name:hashlib.sha256(value).hexdigest() for name,value in expected.items()})
        (out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'desktop-files-windows: PASS {args.windows} same-process Files windows, independent history, owned cancel/collision, accepted 8MiB copy retained during close, concurrent Text save, exact files and complete cleanup. Evidence: {out}')

if __name__=='__main__':main()
