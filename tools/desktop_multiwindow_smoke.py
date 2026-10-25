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



def validate_saved_console(data):
    """Require complete guest records; live UART fragments are not a parser oracle."""
    for record in (
        b'multi-window: surface-pool 128 live, readback/reuse/stale IDs verified',
        b'multi-window: surface-pool retired',
        b'multi-window: PASS all windows retired; process exiting',
    ):
        if data.count(record)!=2:raise ValueError(f'Missing/duplicate saved completion: {record!r}')
    for record in (b'multi-window: retired window=1 text=AlphaX',
                   b'multi-window: retired window=3 text=Gamma',
                   b'clients=0 children=1 clipboard_peers=0 window_capacity=16 cache_bytes='):
        if record not in data:raise ValueError(f'Missing saved lifetime evidence: {record!r}')
    # Window removal transfers buffers to the worker. Require both completed
    # compositor lifetimes to release every byte, not premature accounting.
    caches=re.findall(rb'deskd: cache peak_bytes=\d+ retired_bytes=(\d+) budget_bytes=100663296 windows=16',data)
    if caches!=[b'0',b'0']:raise ValueError('Missing, duplicate or retained cache lifetime')
    if b'FAIL' in data or b'exception:' in data:raise ValueError('Saved guest failure')


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
                            if b'multi-window: FAIL' in output or b'gui-queues: FAIL' in output or b'gui-wait-capture: FAIL' in output:raise RuntimeError('Guest multiwindow failure; see serial.log')
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':f'sendkey {keys}'});time.sleep(.3)
                def text(value):
                    for char in value:qmp.key(char.lower(),char.isupper())
                def shot(name):
                    time.sleep(.3);qmp.call('screendump',{'filename':str(out/f'{name}.ppm')})
                wait(b'srv> ',100)
                serial.sendall(b'run /fat/bin/guifail --wait\n')
                wait(b'gui-wait-capture: PASS deadlines, delayed pipe, thread cancellation, descriptor replacement, simultaneous GUI/pipe and peer exit',120)
                wait(b'srv> ')
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
                shortcut('ctrl-q');wait(b'deskd: child-exit pid='+pid+b' status=0')
                shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                # A fresh process may start ID1; no stale connection state survives.
                serial.sendall(b'run /fat/bin/deskd --multiwindow-smoke\n')
                wait(b'multi-window: ready two windows one process')
                second=re.findall(rb'deskd: opened pid=(\d+)',output)[-1]
                assert second!=pid
                shortcut('ctrl-q');wait(b'deskd: child-exit pid='+second+b' status=0')
                shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                # Console records are captured atomically; UART transmission is
                # deliberately interruptible and may interleave live writers.
                snapshot_start=len(output)
                serial.sendall(b'dmesg 65536\necho GUI_WAIT_SAVED_END\n')
                wait(b'\nGUI_WAIT_SAVED_END\r\n');wait(b'srv> ')
                snapshot=output[snapshot_start:]
                (out/'saved-console.log').write_bytes(snapshot)
                validate_saved_console(snapshot)
                if b'FAIL' in output or b'exception:' in output:raise RuntimeError('Guest failure')
                serial.close();qmp.stream.close();qmp.sock.close()
            except Exception:
                if qmp:
                    try:shot('failure')
                    except Exception:pass
                raise
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
    print(f'desktop-multiwindow: PASS combined GUI/pipe wait with actual threads and descriptor replacement, 25 live process FIFOs/72 clients reaped, 128 live kernel surfaces/readback/reuse, one process/two documents, focus, clipboard, close preserves sibling, increasing ID, resize, cleanup and relaunch. Evidence: {out}')


if __name__=='__main__':main()
