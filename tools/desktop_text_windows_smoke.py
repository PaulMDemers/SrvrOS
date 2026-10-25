#!/usr/bin/env python3
"""Normal Text documents: independent save/dialog/undo state and shared worker retirement."""
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


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review/desktop-text-windows';out.mkdir(parents=True,exist_ok=True)
    output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-text-windows-') as directory:
        tmp=Path(directory);shutil.copyfile(root/'build/srvros.exfat',tmp/'disk.exfat')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
            '-drive',f'if=none,id=disk,file={tmp/"disk.exfat"},format=raw',
            '-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
            '-qmp',f'unix:{tmp/"qmp"},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            qmp=None
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=45):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if marker==b'srv> ' and not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None:raise RuntimeError(f'Missing {marker!r}')
                        try:
                            chunk=serial.recv(16384)
                            if not chunk:raise RuntimeError('Serial disconnected')
                            output+=chunk
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':f'sendkey {keys}'});time.sleep(.3)
                def text(value):
                    mapping={' ':('spc',False),'/':('slash',False),'.':('dot',False),'-':('minus',False)}
                    for char in value:
                        key,shift=mapping.get(char,(char.lower(),char.isupper()));qmp.key(key,shift)
                def shot(name):
                    time.sleep(.3);qmp.call('screendump',{'filename':str(out/f'{name}.ppm')})
                wait(b'srv> ',100)
                serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                baseline=set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n')
                wait(b'text-runtime: admitted window=1');wait(b'deskd: commit')
                opened=re.findall(rb'deskd: opened pid=(\d+) window=(\d+)',output);pid=opened[-1][0]
                def save_as(path):
                    shortcut('ctrl-l');shortcut('ctrl-a');text(path);shortcut('ctrl-shift-s')
                    wait(b'picker ready mode=2');qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                text('First document');save_as('/fat/text-first.txt')
                shortcut('ctrl-a');shortcut('ctrl-c');wait(b'clipboard role=2 action=8 bytes=14')
                shortcut('ctrl-end');shortcut('ctrl-shift-n')
                wait(b'text-runtime: admitted window=2');wait(b'deskd: commit')
                opened=re.findall(rb'deskd: opened pid=(\d+) window=(\d+)',output)
                assert len(opened)==2 and all(p[0]==pid for p in opened),opened
                text('Second document');shortcut('ctrl-z');shortcut('ctrl-shift-z')
                shortcut('ctrl-end');shortcut('ctrl-v');wait(b'clipboard role=2 action=32 bytes=14')
                save_as('/fat/text-second.txt');shot('two-saved-documents')
                # Same-PID windows must be distinct launcher destinations.
                for name,window in [('text-first',1),('text-second',2)]:
                    shortcut('meta_l-spc');wait(b'launcher opened');text(name);qmp.key('ret')
                    wait(b'launcher window='+pid+f' id={window}'.encode())
                text(' changed');shortcut('ctrl-w');shot('unsaved-close');qmp.key('esc')
                text(' kept');shortcut('ctrl-s');wait(b'file-result role=text op=2 status=0')
                shortcut('ctrl-w');wait(b'text-runtime: closed window=2')
                shortcut('ctrl-end');text(' tail');shortcut('ctrl-shift-n')
                wait(b'text-runtime: admitted window=3');wait(b'deskd: commit')
                shortcut('ctrl-l');shortcut('ctrl-a');text('/fat/text-second.txt')
                shortcut('ctrl-o');wait(b'picker ready mode=1');text('text-second');qmp.key('ret')
                wait(b'file-result role=text op=1 status=0');shot('reopened-second-document')
                shortcut('ctrl-w');wait(b'text-runtime: closed window=3')
                shortcut('ctrl-w');shot('save-on-close');shortcut('shift-tab');shortcut('shift-tab');qmp.key('ret')
                wait(b'file-result role=text op=2 status=0');wait(b'text-runtime: closed window=1')
                wait(b'desktools: exited role=text failure=0');wait(b'deskd: child-exit pid='+pid+b' status=0')
                assert b'text-runtime: documents admitted=3 retired=3 refused=0' in output
                pools=re.findall(rb'file-pool: retired role=text spawned=(\d+) reaped=(\d+) peak=(\d+) live=0 bound=0',output)
                assert pools and all(a==b and 1<=int(c)<=2 for a,b,c in pools),pools
                shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                deadline=time.monotonic()+10
                while True:
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                    current=set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                    if current==baseline:break
                    if time.monotonic()>deadline:raise RuntimeError(f'Leaked processes: {current-baseline}')
                    time.sleep(.5)
                assert b'cache_bytes=0' in output
                if b'FAIL' in output or b'failure=1' in output or b'exception:' in output:raise RuntimeError('Guest failure')
                serial.close();qmp.stream.close();qmp.sock.close()
            except Exception:
                if qmp:
                    try:shot('failure')
                    except Exception:pass
                raise
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
            from exfat_test_image import ExfatFixture
            disk=ExfatFixture((tmp/'disk.exfat').read_bytes())
            assert disk.read('text-first.txt')==b'First document tail',disk.read('text-first.txt')
            assert disk.read('text-second.txt')==b'Second documentFirst document changed kept',disk.read('text-second.txt')
    print(f'desktop-text-windows: PASS same-process documents, independent undo/clipboard/save, unsaved cancel, fresh ID/reopen, save-on-close, shared worker retirement and exact persisted contents. Evidence: {out}')


if __name__=='__main__':main()
