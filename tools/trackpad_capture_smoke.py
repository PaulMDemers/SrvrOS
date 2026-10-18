#!/usr/bin/env python3
"""Exercise capture persistence/refusal paths with disposable USB media, not Apple hardware."""
import argparse
import hashlib
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import threading

from exfat_test_image import ExfatFixture, check_clean_image
from usb_storage_smoke import make_iso
from uefi_usb_smoke import default_ovmf_path
from hardware_trackpad import validate_result
from desktop_foundation_smoke import QMP
from desktop_io_result import validate as validate_io, validate_timing


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gestures',action='store_true')
    parser.add_argument('--desktop-io',action='store_true')
    parser.add_argument('--io-rounds',type=int,default=16,choices=range(16,129),metavar='16..128')
    parser.add_argument('--pointer',action='store_true')
    parser.add_argument('--qemu',default='qemu-system-x86_64')
    args=parser.parse_args()
    args.gestures |= args.desktop_io
    args.pointer |= args.gestures
    root=Path(__file__).resolve().parents[1]
    logs=root/((f'build/review/desktop-io-trial-{args.io_rounds}' if args.io_rounds>16 else 'build/review/desktop-io-trial') if args.desktop_io else 'build/review/gesture-trial' if args.gestures else 'build/review/pointer-trial' if args.pointer else 'build/review'); logs.mkdir(parents=True,exist_ok=True)
    run='0123456789abcdef'; release='fedcba9876543210'
    with tempfile.TemporaryDirectory(prefix='srvros-trackpad-') as directory:
        temp=Path(directory); volume=temp/'volume.img'; disk=temp/'usb.img'
        with volume.open('wb') as f: f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes()); uuid=f'{fixture.u32(100):08x}'
        fixture.add_directory_files('srvros-work',{
            '.workspace':f'SRVROS-WORKSPACE-V1 {uuid}\n'.encode(),
            f'trackpad-{run}.req':f'SRVROS-TRACKPAD-V1 {run}\n'.encode()})
        initial=bytes(fixture.data)
        volume.write_bytes(initial); print(check_clean_image(volume),end='')
        def reset():
            mbr=bytearray(512); mbr[450]=7
            struct.pack_into('<II',mbr,454,2048,len(initial)//512); mbr[510:]=b'\x55\xaa'
            with disk.open('wb') as f: f.write(mbr); f.seek(2048*512); f.write(initial)
        reset()
        iso=make_iso(root,temp,False,f' srvros.workspace-volume={uuid} srvros.trackpad={"gestures" if args.gestures else "pointer" if args.pointer else "capture"}'
            f' srvros.trackpad-run={run} srvros.trackpad-release={release}'+(f' srvros.desktop-io={run}' if args.desktop_io else ''))
        def boot(label,interactive=False,readonly=False):
            sockpath=temp/'serial.sock'; sockpath.unlink(missing_ok=True)
            qmppath=temp/'qmp'; qmppath.unlink(missing_ok=True)
            command=[args.qemu,'-M','q35','-m','512M','-drive',f'if=pflash,format=raw,readonly=on,file={default_ovmf_path()}',
                '-cdrom',str(iso),'-boot','d','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
                '-drive',f'if=none,id=stick,format=raw,file={disk}'+(',readonly=on' if readonly else ''),
                '-device','usb-storage,drive=stick,bus=xhci.0','-serial',f'unix:{sockpath},server=on,wait=on',
                '-display','none','-monitor','none','-no-reboot','-qmp',f'unix:{qmppath},server=on,wait=off']
            data=b''; qmp=None; reader=None; stop_reader=threading.Event()
            with (logs/f'trackpad-{label}-qemu.log').open('wb') as errors:
                p=subprocess.Popen(command,stdout=errors,stderr=errors)
                try:
                    with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as sock:
                        sock.settimeout(.2); deadline=time.monotonic()+15
                        while True:
                            try: sock.connect(str(sockpath)); break
                            except (FileNotFoundError,ConnectionRefusedError):
                                if time.monotonic()>deadline: raise
                                time.sleep(.1)
                        # Keep draining during QMP input injection: a stalled host
                        # receiver can trigger the guest UART transmit timeout.
                        def capture_serial():
                            nonlocal data
                            with (logs/f'trackpad-{label}-serial.log').open('wb') as log:
                                while not stop_reader.is_set():
                                    try: chunk=sock.recv(8192)
                                    except socket.timeout: continue
                                    except OSError: break
                                    if not chunk: break
                                    data+=chunk
                                    log.write(chunk); log.flush()
                        reader=threading.Thread(target=capture_serial,daemon=True)
                        reader.start()
                        def wait(marker):
                            deadline=time.monotonic()+180
                            while marker not in data:
                                if args.desktop_io and interactive and (b'deskio: failed' in data or
                                        b'deskd: io start failed' in data or b'deskd: io worker ended state=4' in data):
                                    raise RuntimeError('Desktop I/O worker failed; see serial log')
                                if marker==b'deskd: opened' and b'trackpad-test: ready for gestures' in data:
                                    raise RuntimeError('Desktop launch failed; returned to capture before opening a window')
                                if p.poll() is not None or time.monotonic()>deadline:
                                    if args.desktop_io and interactive and qmp is not None:
                                        qmp.call('screendump',{'filename':str(logs/'stalled.ppm')})
                                    raise RuntimeError(f'Missing {marker!r}; {label}')
                                time.sleep(.02)
                        if interactive:
                            if args.pointer:
                                wait(b'deskd: opened')
                                qmp=QMP(qmppath)
                                time.sleep(.5)
                                qmp.key('a'); qmp.key('a',True)
                                wait(b'deskclient: text=aA')
                                if args.desktop_io:
                                    wait(b'deskd: io rounds=1 state=1')
                                    qmp.call('human-monitor-command',{'command-line':'sendkey ctrl-n'})
                                    wait(b'window=1'); time.sleep(.5)
                                    qmp.key('tab')
                                    for key in 'abcdefgh': qmp.key(key)
                                    qmp.mouse(588,153); qmp.mouse(0,0,1); qmp.mouse(0,40)
                                    qmp.key('tab'); qmp.mouse(0,0,0); qmp.mouse(-100,0)
                                    wait(b'deskd: io rounds=16 state=1')
                                    # Longer runs keep injecting while waiting for each
                                    # round, covering its writes as well as pacing gaps.
                                    for target in range(17,args.io_rounds+1):
                                        marker=f'deskd: io rounds={target} state=1'.encode()
                                        limit=time.monotonic()+180
                                        qmp.key('tab')
                                        while marker not in data:
                                            if time.monotonic()>limit or b'deskio: failed' in data:
                                                raise RuntimeError('Extended desktop workload failed/stalled')
                                            qmp.key('x'); qmp.key('backspace')
                                            qmp.mouse(6,0); qmp.mouse(-6,0)
                                qmp.call('screendump',{'filename':str(logs/'desktop-pointer-trial.ppm')})
                                qmp.key('esc')
                                wait(b'deskd: exited')
                                qmp.stream.close(); qmp.sock.close()
                            wait(b'trackpad-test: ready for gestures')
                            if args.gestures:
                                wait(b'TYPING: NOT YET (0 frames)   BROAD: NOT YET (0 frames)')
                                # Trial typing can fill the buffer; Escape must clear it
                                # before the exact keyboard check, without saving early.
                                sock.sendall(b'a'*40)
                                wait(b'Typed: '+b'a'*31)
                                qmp=QMP(qmppath)
                                qmp.call('screendump',{'filename':str(logs/'filter-checks.ppm')})
                                qmp.stream.close(); qmp.sock.close()
                                sock.sendall(b'\x1b')
                            sock.sendall(b'AaZz09\n')
                        wait(b'trackpad-test: HALTED')
                        assert p.poll() is None, 'Guest rebooted/exited instead of halting'
                finally:
                    stop_reader.set()
                    if reader: reader.join(timeout=1)
                    p.terminate()
                    try: p.wait(timeout=5)
                    except subprocess.TimeoutExpired: p.kill(); p.wait()
                    (logs/f'trackpad-{label}-serial.log').write_bytes(data)
            assert b'exception:' not in data
            return data
        output=boot('capture',True)
        assert b'USB unmounted: yes' in output and b'SAVE_FAILED' not in output
        volume.write_bytes(disk.read_bytes()[2048*512:])
        print(check_clean_image(volume),end='')
        saved=ExfatFixture(volume.read_bytes())
        report=saved.read(f'srvros-work/trackpad-{run}.txt')
        complete=saved.read(f'srvros-work/trackpad-{run}.done')
        fields=validate_result({'run':run,'release':release,'pointer':args.pointer,'gestures':args.gestures},report,complete)
        if args.gestures: assert fields['gesture_trial']=='INCOMPLETE' and fields['gesture_scroll_events']=='0'
        if args.pointer: assert fields['desktop_exit']=='PASS' and fields['pointer_trial']=='INCOMPLETE'
        if args.desktop_io:
            folder=f'srvros-work/desktop-{run}'
            checked=validate_io(run,saved.read(f'{folder}/result.txt'),saved.read(f'{folder}/ui.txt'),saved.read(f'{folder}/payload.bin'))
            timings=validate_timing(run,saved.read(f'{folder}/timing.txt'))
            (logs/'timing.txt').write_bytes(saved.read(f'{folder}/timing.txt'))
            print(f'desktop-io-smoke: timing {timings}',flush=True)
            if args.io_rounds>16: assert timings['active_count']>=100
            assert checked['worker']['rounds']>=args.io_rounds
            activity=checked['input_during_workload']
            assert activity['keys']>=8 and activity['motions']>0 and activity['focus_changes']>=2 and activity['cancels']>0, checked
            print('desktop-io-smoke: verified persisted payload and concurrent key/motion/focus/cancel coverage',flush=True)
        assert fields['status']=='INCOMPLETE' and fields['found']=='0' and fields['typed']=='AaZz09'
        assert saved.read(f'srvros-work/trackpad-{run}.req')==f'STARTED {run}\n'.encode()
        (logs/'trackpad-capture-report.txt').write_bytes(report)
        before=hashlib.sha256(disk.read_bytes()).digest()
        output=boot('replay')
        assert b'request absent/consumed' in output and b'ready for gestures' not in output
        assert hashlib.sha256(disk.read_bytes()).digest()==before
        reset(); before=hashlib.sha256(disk.read_bytes()).digest()
        output=boot('read-only',readonly=True)
        assert b'request consumption/flush FAILED' in output and b'ready for gestures' not in output
        assert hashlib.sha256(disk.read_bytes()).digest()==before
    print('trackpad-capture-smoke: PASS interactive input, durable CRC report, clean exFAT, incomplete hardware coverage, replay/read-only refusal, halt')


if __name__=='__main__': main()
