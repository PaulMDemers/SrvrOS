#!/usr/bin/env python3
"""Real GUI transport: independent documents in one process, acknowledged close."""
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
    out=root/'build/review/desktop-multiwindow';out.mkdir(parents=True,exist_ok=True)
    output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-multiwindow-') as directory:
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
                            output+=chunk;(out/'serial.log').write_bytes(output)
                            if b'multi-window: FAIL' in output or b'gui-queues: FAIL' in output:raise RuntimeError('Guest multiwindow failure; see serial.log')
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':f'sendkey {keys}'});time.sleep(.3)
                def text(value):
                    for char in value:qmp.key(char.lower(),char.isupper())
                def shot(name):
                    time.sleep(.3);qmp.call('screendump',{'filename':str(out/f'{name}.ppm')})
                wait(b'srv> ',100)
                serial.sendall(b'run /fat/bin/guifail --queues\n')
                wait(b'gui-queues: PASS three rounds, 72 clients, all reaped',120)
                for round_number in range(1,4):
                    assert f'gui-queues: round={round_number} 24 clients authenticated; 25 live FIFOs'.encode() in output
                    assert f'gui-queues: round={round_number} 24 clients retired and reaped'.encode() in output
                wait(b'srv> ')
                serial.sendall(b'run /fat/bin/deskd --multiwindow-smoke\n')
                wait(b'multi-window: ready two windows one process')
                opened=re.findall(rb'deskd: opened pid=(\d+) window=(\d+)',output)
                assert len(opened)==2 and opened[0][0]==opened[1][0],opened
                pid=opened[0][0]
                text('Beta');wait(b'input window=2 text=Beta')
                shortcut('alt-tab');text('Alpha');wait(b'input window=1 text=Alpha')
                shortcut('ctrl-c');wait(b'copied window=1')
                shortcut('alt-tab');shortcut('ctrl-v');wait(b'paste window=2 text=BetaAlpha');shot('independent-documents')
                shortcut('ctrl-w');wait(b'retired window=2 text=BetaAlpha')
                text('X');wait(b'input window=1 text=AlphaX')
                shortcut('ctrl-n');wait(b'admitted window=3');wait(b'configured window=3 generation=1')
                text('Gamma');wait(b'input window=3 text=Gamma')
                shortcut('meta_l-alt-down');wait(b'configured window=3 generation=2');shot('new-handle-resized')
                shortcut('ctrl-q');wait(b'multi-window: PASS all windows retired; process exiting')
                wait(b'deskd: child-exit pid='+pid+b' status=0')
                shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                assert b'clients=0 children=1 clipboard_peers=0 window_limit=16 cache_bytes=0' in output
                assert b'multi-window: retired window=1 text=AlphaX' in output
                assert b'multi-window: retired window=3 text=Gamma' in output
                # A fresh process may start ID1; no stale connection state survives.
                serial.sendall(b'run /fat/bin/deskd --multiwindow-smoke\n')
                wait(b'multi-window: ready two windows one process');shortcut('ctrl-q')
                wait(b'multi-window: PASS all windows retired; process exiting')
                assert output.count(b'multi-window: surface-pool 128 live, readback/reuse/stale IDs verified')==2
                assert output.count(b'multi-window: surface-pool retired')==2
                wait(b'deskd: child-exit');shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                if b'FAIL' in output or b'exception:' in output:raise RuntimeError('Guest failure')
                serial.close();qmp.stream.close();qmp.sock.close()
            except Exception:
                if qmp:
                    try:shot('failure')
                    except Exception:pass
                raise
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
    print(f'desktop-multiwindow: PASS 25 live process FIFOs/72 clients reaped, 128 live kernel surfaces/readback/reuse, one process/two documents, focus, clipboard, close preserves sibling, increasing ID, resize, cleanup and relaunch. Evidence: {out}')


if __name__=='__main__':main()
