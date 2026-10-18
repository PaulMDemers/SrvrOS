#!/usr/bin/env python3
"""Exercise the Sound controls against the real HDA worker in a disposable VM."""
import os
import argparse
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import json
from audio_smoke import analyze_tones
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--usb-io',action='store_true',help='Overlap tones with a throttled disposable USB write')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=root/('build/review/audio-settings-usb' if args.usb_io else 'build/review/audio-settings');out.mkdir(parents=True,exist_ok=True)
    original=boot.make_limine_conf
    boot.make_limine_conf=lambda w,h:original(w,h)+'    cmdline: srvros.audio=hda'+(' srvros.usb-storage=rw' if args.usb_io else '')+'\n'
    output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-audio-settings-') as directory:
        tmp=Path(directory);shutil.copyfile(root/'build/srvros.exfat',tmp/'disk.exfat')
        iso=boot.build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=boot.choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d',
            '-drive',f'if=none,id=disk,file={tmp}/disk.exfat,format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
            '-device','ich9-intel-hda','-device','hda-duplex,audiodev=capture','-audiodev',f'wav,id=capture,path={out}/tone.wav',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off',
            '-display','none','-monitor','none','-no-reboot']
        if args.usb_io:
            usb_disk=tmp/'usb.img';source=tmp/'usb-volume.img'
            with source.open('wb') as target:target.truncate(128*1024*1024)
            subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(source)],check=True,stdout=subprocess.DEVNULL)
            mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,source.stat().st_size//512);mbr[510:]=b'\x55\xaa'
            with usb_disk.open('wb') as target,source.open('rb') as content:
                target.write(mbr);target.seek(2048*512);shutil.copyfileobj(content,target)
            command+=['-drive',f'if=none,id=stick,file={usb_disk},format=raw','-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=boot.connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=45):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None:
                            qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                            raise RuntimeError(f'Missing {marker!r}; see {out}')
                        try:output+=serial.recv(16384)
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':f'sendkey {keys}'});time.sleep(.3)
                pointer=[24,56]
                def click(x,y):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y];qmp.mouse(0,0,1);qmp.mouse(0,0,0)
                def shot(name):
                    time.sleep(.5);qmp.call('screendump',{'filename':str(out/f'{name}.ppm')})
                def written():
                    return next(s['stats']['wr_bytes'] for s in qmp.call('query-blockstats') if s.get('device')=='stick')
                def throttle(iops):
                    qmp.call('block_set_io_throttle',{'device':'stick','bps':0,'bps_rd':0,'bps_wr':65536 if iops else 0,'iops':0,'iops_rd':0,'iops_wr':iops})
                def type_text(text):
                    keys={' ':'spc','/':'slash','.':'dot','=':'equal',';':'semicolon','-':'minus','\n':'ret','$':'4','?':'slash'}
                    for char in text:qmp.key(keys.get(char,char.lower()),char.isupper() or char in '$?')
                wait(b'srv> ',100);assert b'audio: state=2' in output
                if args.usb_io:
                    serial.sendall(b'mount usb0p1 /usb\n');wait(b'exfat: mounted /usb');wait(b'srv> ')
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n');wait(b'desktools: starting role=text');wait(b'deskd: commit')
                click(790,750);wait(b'desktools: starting role=settings');wait(b'deskd: commit')
                shortcut('pgdn');shortcut('pgdn');shot('sound-ready')
                if args.usb_io:
                    click(890,750);wait(b'deskterm: ready shell=');wait(b'deskd: commit')
                    throttle(40);before=written()
                    type_text('dd if=/dev/zero of=/usb/audio-load.bin bs=4096 count=256; exit $?\n')
                    deadline=time.monotonic()+60
                    while written()-before<65536:
                        assert time.monotonic()<deadline,'USB writer did not start'
                        output+=boot.read_for(serial,.1)
                    assert b'deskterm: shell exited' not in output,'Missed USB write overlap'
                    click(790,750)
                click(160,478);wait(b'desktools: sound operation=1 accepted=1')
                time.sleep(1);shot('sound-after-test')
                # The volume thumb remains draggable across asynchronous replies.
                click(330,392);wait(b'desktools: sound operation=3 accepted=1')
                time.sleep(1);shot('sound-volume')
                click(160,478);wait(b'desktools: sound operation=1 accepted=1');time.sleep(1)
                click(250,478);wait(b'desktools: sound operation=2 accepted=1')
                if args.usb_io:
                    assert b'deskterm: shell exited' not in output,'Writer ended before overlap checks'
                    overlap_bytes=written()-before
                    shot('sound-during-usb-write')
                    throttle(0);wait(b'deskterm: shell exited status=0',90)
                    assert written()-before>overlap_bytes,'No pending USB writes remained after the tones'
                shortcut('f10');shortcut('end');shortcut('ret');wait(b'deskd: exited',60);wait(b'srv> ')
                serial.sendall(b'audio\n');wait(b'tests_started=2 tests_completed=2');wait(b'srv> ')
                if args.usb_io:
                    serial.sendall(b'fsck /usb\n');wait(b'errors=0 ok');wait(b'srv> ')
                    serial.sendall(b'unmount /usb\n');wait(b'exfat: unmounted /usb');wait(b'srv> ')
                assert b'exception:' not in output and b'deskd: FAIL' not in output
                assert not any(marker in output for marker in (b'reason=ordered-input-overflow',b'reason=input-timeout',b'reason=client-timeout'))
                qmp.call('quit');process.wait(timeout=10)
            finally:
                stop_process(process);(out/'serial.txt').write_bytes(output)
        if args.usb_io:
            from exfat_test_image import ExfatFixture,check_clean_image
            volume=tmp/'usb-volume.img';volume.write_bytes(usb_disk.read_bytes()[2048*512:])
            stored=ExfatFixture(volume.read_bytes())
            assert stored.read('audio-load.bin')==bytes(1024*1024),'USB writer output was not preserved'
            check_clean_image(volume)
    pcm=analyze_tones(out/'tone.wav',2,max_peak=4801)
    assert pcm['peak']>1200,'GUI volume did not change the captured signal'
    pcm.update({'status':'PASS','usb_write_overlap':args.usb_io})
    (out/'result.json').write_text(json.dumps(pcm,indent=2)+'\n')
    print('audio-settings: PASS queued test/stop, volume slider, repeated DMA playback and clean desktop exit')
    if args.usb_io:print('audio-settings: PASS both tones overlapped a pending throttled USB write; 1 MiB verified and cleanly unmounted')

if __name__=='__main__':main()
