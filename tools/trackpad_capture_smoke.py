#!/usr/bin/env python3
"""Exercise capture persistence/refusal paths with disposable USB media, not Apple hardware."""
import argparse
import hashlib
import json
import binascii
import uuid as uuidlib
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
import hardware_boot as staged_boot
from hardware_boot_smoke import make_disk
from mk_usb_image import gpt_entry, gpt_header
from hardware_trackpad import validate_result
from desktop_foundation_smoke import QMP
from desktop_io_result import validate as validate_io, validate_timing, validate_text, text_gate, validate_timeline


def staged_capture(release):
    if not staged_boot.RELEASE_RE.fullmatch(release):
        raise ValueError('Invalid staged release ID')
    source=staged_boot.STAGE/release
    manifest=staged_boot.verify(source)
    match=staged_boot.TRACKPAD_RE.fullmatch(manifest['config_version'])
    if not match or not manifest['config_version'].startswith(('ram-usb-v10-', 'ram-usb-v11-', 'ram-usb-v12-', 'ram-usb-v50-')):
        raise ValueError('Exact rehearsal requires a v10 capture, v11 pointer or v12 gesture or v50 tap/typing stage')
    return source,manifest,match[1],match[2]


def workspace_disk(path,data,table):
    mbr=bytearray(512);mbr[450]=7
    struct.pack_into('<II',mbr,454,2048,len(data)//512);mbr[510:]=b'\x55\xaa'
    with path.open('wb') as stream:
        if table=='gpt':
            last=2048+len(data)//512-1;backup=last+33
            entries=bytearray(128*128)
            entries[:128]=gpt_entry(uuidlib.UUID('ebd0a0a2-b9e5-4433-87c0-68b6b72699c7'),
                uuidlib.uuid4(),2048,last,'Workspace')
            crc=binascii.crc32(entries)&0xffffffff;guid=uuidlib.uuid4()
            mbr[450]=0xee;struct.pack_into('<II',mbr,454,1,0xffffffff)
            stream.write(mbr);stream.write(gpt_header(1,backup,34,last,2,128,crc,guid));stream.write(entries)
            stream.seek(2048*512);stream.write(data);stream.write(entries)
            stream.write(gpt_header(backup,1,34,last,backup-32,128,crc,guid))
        elif table=='mbr':
            stream.write(mbr);stream.seek(2048*512);stream.write(data)
        else:
            raise ValueError('Unsupported workspace table')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gestures',action='store_true')
    parser.add_argument('--desktop-io',action='store_true')
    parser.add_argument('--io-rounds',type=int,default=16,choices=range(16,129),metavar='16..128')
    parser.add_argument('--pointer',action='store_true')
    parser.add_argument('--qemu',default='qemu-system-x86_64')
    parser.add_argument('--release',help='verify and boot this exact v10/v11/v12/v50 stage through GRUB')
    parser.add_argument('--workspace-table',choices=('mbr','gpt'),default='mbr')
    args=parser.parse_args()
    args.gestures |= args.desktop_io
    args.pointer |= args.gestures
    root=Path(__file__).resolve().parents[1]
    logs=root/((f'build/review/desktop-io-trial-{args.io_rounds}' if args.io_rounds>16 else 'build/review/desktop-io-trial') if args.desktop_io else 'build/review/gesture-trial' if args.gestures else 'build/review/pointer-trial' if args.pointer else 'build/review'); logs.mkdir(parents=True,exist_ok=True)
    run='0123456789abcdef'; release='fedcba9876543210';stage=None;tap_typing=False
    if args.release:
        if args.pointer or args.gestures or args.desktop_io:
            parser.error('--release infers its profile; omit pointer/gesture/desktop options')
        stage,manifest,stage_uuid,run=staged_capture(args.release);release=args.release
        tap_typing=manifest['config_version'].startswith('ram-usb-v50-')
        args.gestures=tap_typing or manifest['config_version'].startswith('ram-usb-v12-')
        args.pointer=args.gestures or manifest['config_version'].startswith('ram-usb-v11-')
        logs=root/'build/review'/f'trackpad-stage-{release}';logs.mkdir(parents=True,exist_ok=True)
    receipt=logs/'trackpad-result.json';receipt.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='srvros-trackpad-') as directory:
        temp=Path(directory); volume=temp/'volume.img'; disk=temp/'usb.img'
        with volume.open('wb') as f: f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        if stage:
            subprocess.run(['/usr/sbin/tune.exfat','-I','0x'+stage_uuid,str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes()); uuid=f'{fixture.u32(100):08x}'
        fixture.add_directory_files('srvros-work',{
            '.workspace':f'SRVROS-WORKSPACE-V1 {uuid}\n'.encode(),
            f'trackpad-{run}.req':f'SRVROS-TRACKPAD-V1 {run}\n'.encode()})
        initial=bytes(fixture.data)
        volume.write_bytes(initial); print(check_clean_image(volume),end='')
        def reset(): workspace_disk(disk,initial,args.workspace_table)
        reset()
        if stage:
            cfg=temp/'grub.cfg';cfg.write_text(staged_boot.grub_config('5352-5652',release))
            subprocess.run(['grub-script-check',str(cfg)],check=True)
            loader=temp/'BOOTX64.EFI'
            subprocess.run(['grub-mkstandalone','-O','x86_64-efi','--locales=','--fonts=',
                '--modules=part_gpt fat chain search search_fs_uuid normal serial terminal',
                '-o',str(loader),f'boot/grub/grub.cfg={cfg}'],check=True)
            esp=temp/'esp.img';make_disk(esp,loader,stage,release)
            boot_media=['-drive',f'format=raw,file={esp}']
        else:
            iso=make_iso(root,temp,False,f' srvros.workspace-volume={uuid} srvros.trackpad={"gestures" if args.gestures else "pointer" if args.pointer else "capture"}'
                f' srvros.trackpad-run={run} srvros.trackpad-release={release}'+(f' srvros.desktop-io={run}' if args.desktop_io else ''))
            boot_media=['-cdrom',str(iso),'-boot','d']
        def boot(label,interactive=False,readonly=False):
            sockpath=temp/'serial.sock'; sockpath.unlink(missing_ok=True)
            qmppath=temp/'qmp'; qmppath.unlink(missing_ok=True)
            command=[args.qemu,'-M','q35','-m','512M','-drive',f'if=pflash,format=raw,readonly=on,file={default_ovmf_path()}',
                *boot_media,'-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
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
                                if tap_typing: wait(b'deskd: tap/typing options=14')
                                qmp=QMP(qmppath)
                                if args.desktop_io:
                                    wait(b'deskd: trial ready; Space starts USB work')
                                    time.sleep(6) # Exceed the old startup timeout; no automatic workload.
                                    assert b'deskd: io worker pid=' not in data
                                    assert b'deskd: exited' not in data and b'deskd: FAIL' not in data
                                    qmp.call('screendump',{'filename':str(logs/'waiting-for-start.ppm')})
                                    qmp.key('spc')
                                    wait(b'deskd: io rounds=0 state=1')
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
                                if args.desktop_io:
                                    # Emulation deliberately lacks physical wheel coverage.
                                    # Early Escape must warn; explicit second Escape aborts.
                                    wait(b'deskd: coverage incomplete; Escape again')
                                    time.sleep(.1)
                                    qmp.key('esc')
                                wait(b'deskd: exited')
                                qmp.stream.close(); qmp.sock.close()
                            wait(b'trackpad-test: ready for gestures')
                            if tap_typing: wait(b'trackpad-test: tap/typing console options=14')
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
        volume.write_bytes(disk.read_bytes()[2048*512:2048*512+len(initial)])
        print(check_clean_image(volume),end='')
        saved=ExfatFixture(volume.read_bytes())
        report=saved.read(f'srvros-work/trackpad-{run}.txt')
        complete=saved.read(f'srvros-work/trackpad-{run}.done')
        fields=validate_result({'run':run,'release':release,'pointer':args.pointer,'gestures':args.gestures,'tap_typing':tap_typing},report,complete)
        if tap_typing: assert fields['tap_typing_trial']=='INCOMPLETE' and fields['input_policy_options']=='14'
        if args.gestures: assert fields['gesture_trial']=='INCOMPLETE' and fields['gesture_scroll_events']=='0'
        if args.pointer: assert fields['desktop_exit']=='PASS' and fields['pointer_trial']=='INCOMPLETE'
        if args.desktop_io:
            folder=f'srvros-work/desktop-{run}'
            checked=validate_io(run,saved.read(f'{folder}/result.txt'),saved.read(f'{folder}/ui.txt'),saved.read(f'{folder}/payload.bin'))
            timings=validate_timing(run,saved.read(f'{folder}/timing.txt'))
            (logs/'timing.txt').write_bytes(saved.read(f'{folder}/timing.txt'))
            print(f'desktop-io-smoke: timing {timings}',flush=True)
            text=validate_text(run,saved.read(f'{folder}/text.txt'))
            (logs/'text.txt').write_bytes(saved.read(f'{folder}/text.txt'))
            print(f'desktop-io-smoke: text {text}',flush=True)
            timeline=validate_timeline(run,saved.read(f'{folder}/work-clock.txt'),saved.read(f'{folder}/input-clock.txt'))
            for name in ('work-clock.txt','input-clock.txt'):
                (logs/name).write_bytes(saved.read(f'{folder}/{name}'))
            assert timeline['first_key_phase']=='during' and timeline['first_pointer_phase']=='during', timeline
            print(f'desktop-io-smoke: timeline {timeline}',flush=True)
            assert text['sent']>=8 and text['presented']==text['sent'] and not text['overflow']
            if args.io_rounds>16: assert text_gate(text), text
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
    if stage:
        assert staged_boot.verify(stage)==manifest, 'Stage changed during rehearsal'
    receipt.write_text(json.dumps({'status':'PASS','release':release,'run':run,
        'manifest_sha256':staged_boot.digest(stage/'manifest.json') if stage else None,
        'exact_stage':bool(stage),'workspace_table':args.workspace_table,
        'pointer':args.pointer,'gestures':args.gestures,'tap_typing':tap_typing,
        'tap_typing_trial':fields.get('tap_typing_trial'),
        'desktop_exit':fields.get('desktop_exit'),'pointer_trial':fields.get('pointer_trial'),
        'gesture_trial':fields.get('gesture_trial'),
        'report_status':fields['status'],'saved':True,'unmounted':True,
        'replay_refused':True,'readonly_refused':True,'halted':True,
        'physical_acceptance':'pending'},indent=2)+'\n')
    print('trackpad-capture-smoke: PASS interactive input, durable CRC report, clean exFAT, incomplete hardware coverage, replay/read-only refusal, halt')


if __name__=='__main__': main()
