#!/usr/bin/env python3
"""Actual compositor growth, complete window discovery, stale-slot reuse and teardown."""
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
    out=root/'build/review/desktop-window-growth';out.mkdir(parents=True,exist_ok=True)
    output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-window-growth-') as directory:
        tmp=Path(directory);shutil.copyfile(root/'build/srvros.exfat',tmp/'disk.exfat')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
            '-drive',f'if=none,id=disk,file={tmp/"disk.exfat"},format=raw',
            '-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
            '-qmp',f'unix:{tmp/"qmp"},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors);qmp=None
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=60):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if marker==b'srv> ' and not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None:raise RuntimeError(f'Missing {marker!r}')
                        try:
                            chunk=serial.recv(16384)
                            if not chunk:raise RuntimeError('Serial disconnected')
                            output+=chunk;(out/'serial.log').write_bytes(output)
                            if b'window-growth: FAIL' in output or b'exception:' in output:raise RuntimeError('Guest growth failure')
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':f'sendkey {keys}'});time.sleep(.3)
                def text(value):
                    for char in value:qmp.key('spc' if char==' ' else char.lower(),char.isupper())
                def shot(name):
                    time.sleep(.3);qmp.call('screendump',{'filename':str(out/f'{name}.ppm')})
                def processes():
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                    return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                wait(b'srv> ',100);baseline=processes()
                serial.sendall(b'run /fat/bin/deskd --window-growth-smoke\n')
                wait(b'window-growth: ready 128 windows one process',180)
                for count in (32,64,96,128):assert f'window-growth: presented={count}'.encode() in output
                opened=re.findall(rb'deskd: opened pid=(\d+) window=(\d+)',output)
                assert len(opened)==128 and len({pid for pid,_ in opened})==1,opened
                pid=opened[0][0]
                assert b'clients=128 children=1 clipboard_peers=128 window_capacity=128' in output
                shot('128-windows')
                # Search reaches windows beyond both former widget/registry ceilings.
                for number,content in ((1,'Alpha'),(64,'Middle'),(128,'Omega')):
                    shortcut('meta_l-spc');wait(b'launcher opened items=140')
                    text(f'Growth {number:03}');qmp.key('ret');wait(b'launcher window='+pid+f' id={number}'.encode())
                    text(content);wait(f'window-growth: input id={number} text={content}'.encode())
                # Traverse the long Window menu to its last item, then activate.
                shortcut('f10');shortcut('right');shortcut('right');shortcut('end');shot('window-menu-last-row');shortcut('ret')
                text('Z');wait(b'window-growth: input id=128 text=OmegaZ')
                shortcut('meta_l-alt-down');wait(b'window-growth: configured id=128 generation=2')
                shortcut('meta_l-alt-down');wait(b'window-growth: configured id=128 generation=3');shot('resized-at-capacity')
                shortcut('ctrl-w');wait(b'window-growth: retired id=128 text=OmegaZ')
                shortcut('ctrl-n');wait(b'window-growth: replacement129 presented');text('New');wait(b'window-growth: input id=129 text=New')
                shot('replacement-new-identity')
                shortcut('ctrl-q');wait(b'window-growth: PASS 129 lifetimes, all windows retired',120)
                wait(b'deskd: child-exit pid='+pid+b' status=0')
                shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                assert re.search(rb'deskd: registry peak_bytes=[1-9]\d* retired_bytes=0 budget_bytes=8388608',output)
                assert b'cache_bytes=0' in output
                deadline=time.monotonic()+10
                while processes()!=baseline:
                    if time.monotonic()>deadline:raise RuntimeError('Process baseline not restored')
                    time.sleep(.3)
                # Fresh compositor after the grown registries and buffers retire.
                serial.sendall(b'run /fat/bin/deskd --multiwindow-smoke\n')
                wait(b'multi-window: ready two windows one process');shortcut('ctrl-q')
                wait(b'multi-window: PASS all windows retired; process exiting');wait(b'deskd: child-exit')
                shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                if b'FAIL' in output or b'failure=1' in output:raise RuntimeError('Guest failure')
                serial.close();qmp.stream.close();qmp.sock.close()
            except Exception:
                if qmp:
                    try:shot('failure')
                    except Exception:pass
                raise
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
    print(f'desktop-window-growth: PASS 32/64/128 presented windows, complete launcher/menu discovery, input/resize, fresh replacement ID, 129 retirements, zero registry/cache bytes, process baseline and compositor relaunch. Evidence: {out}')


if __name__=='__main__':main()
