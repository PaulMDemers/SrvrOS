#!/usr/bin/env python3
"""Boot the real HDA driver in QEMU and inspect captured PCM (no host speakers)."""
import argparse
from array import array
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import wave
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def analyze_tones(path,expected,max_peak=2000):
    segments=[];peak=0;position=0;first=None;last=0;crossings=0;previous=0
    with wave.open(str(path),'rb') as wav:
        assert wav.getnchannels()==2 and wav.getsampwidth()==2
        rate=wav.getframerate()
        while True:
            raw=wav.readframes(4096)
            if not raw:break
            samples=array('h',raw)
            for left,right in zip(samples[::2],samples[1::2]):
                assert left==right,'unexpected stereo channel mismatch'
                peak=max(peak,abs(left))
                if first is not None and position-last>rate//20:
                    segments.append((first,last,crossings));first=None;crossings=0
                if abs(left)>20:
                    if first is None:first=position
                    last=position
                if first is not None and previous<=0<left:crossings+=1
                previous=left;position+=1
        if first is not None:segments.append((first,last,crossings))
    assert len(segments)==expected,(len(segments),expected)
    for start,end,count in segments:
        duration=(end-start)/rate;assert .45<duration<.6,duration
        hz=count/duration;assert 480<hz<520,hz
    assert 0<peak<max_peak,'test amplitude exceeded its configured limit'
    assert (position-last)/rate>.2,'playback did not end in silence'
    return {'sample_rate':rate,'tones':len(segments),'peak':peak,'identical_stereo':True,
        'tone_segments_seconds':[(a/rate,b/rate) for a,b,_ in segments]}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controller',choices=['intel-hda','ich9-intel-hda'],default='intel-hda')
    parser.add_argument('--codec',choices=['hda-output','hda-duplex'],default='hda-output')
    parser.add_argument("--soak-seconds",type=int,default=0)
    args=parser.parse_args()
    if not 0<=args.soak_seconds<=3600:parser.error("soak duration must be 0..3600 seconds")
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('audio-'+args.controller+'-'+args.codec+('-soak' if args.soak_seconds else ''))
    out.mkdir(parents=True,exist_ok=True)
    original=boot.make_limine_conf
    boot.make_limine_conf=lambda w,h:original(w,h)+'    cmdline: srvros.audio=hda\n'
    output=b''
    with tempfile.TemporaryDirectory(prefix='srvros-audio-') as tmp:
        tmp=Path(tmp)
        iso=boot.build_iso(str(root),str(tmp),1280,800,os.environ.copy())
        shutil.copyfile(root/'build/srvros.exfat',tmp/'disk.exfat')
        port=boot.choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
            '-qmp',f'unix:{tmp}/qmp.sock,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
            '-drive',f'if=none,id=disk,file={tmp}/disk.exfat,format=raw',
            '-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
            '-device',args.controller,'-device',f'{args.codec},audiodev=capture',
            '-audiodev',f'wav,id=capture,path={out}/tone.wav']
        with (out/'qemu.txt').open('w') as log:
            process=subprocess.Popen(command,cwd=root,stdout=log,stderr=log)
            try:
                q=QMP(tmp/'qmp.sock')
                sock=boot.connect_serial(port,15);sock.settimeout(.2)
                start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<90:
                    output+=boot.read_for(sock,.5)
                    q.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'audio: state=2' in output,output[-5000:].decode(errors='replace')
                # Verify no test was started automatically. The WAV backend only
                # records while DMA is active, so it cannot measure boot silence.
                assert b'tests_started=0 tests_completed=0' in output
                output+=boot.read_for(sock,1)
                def command(text,timeout=30):
                    nonlocal output
                    sock.sendall(text.encode()+b'\n');reply=b'';end=time.monotonic()+timeout
                    marker=b'audio: request queued' if text=='audio test' else b'audio: commands='
                    while marker not in reply or not reply.endswith(b'srv> '):
                        if time.monotonic()>end:raise RuntimeError(f'Audio command timed out: {text}; {reply[-1000:]!r}')
                        reply+=boot.read_for(sock,.1)
                    output+=reply;return reply
                began=time.monotonic();rounds=0;keyboard_rounds=0
                while rounds<2 or time.monotonic()-began<args.soak_seconds:
                    reply=command('audio test');assert b'audio: request queued' in reply,reply
                    if rounds%10==0:
                        for char in 'echo usb-audio\n':
                            q.key({' ':'spc','-':'minus','\n':'ret'}.get(char,char))
                        keyboard=boot.read_until(sock,b'\nusb-audio',20)
                        if not keyboard.endswith(b'srv> '):keyboard+=boot.read_until(sock,b'srv> ',20)
                        output+=keyboard;assert b'\nusb-audio' in keyboard,keyboard
                        keyboard_rounds+=1
                    rounds+=1;marker=f'tests_started={rounds} tests_completed={rounds}'.encode()
                    end=time.monotonic()+30
                    while True:
                        output+=boot.read_for(sock,.5);reply=command('audio')
                        if marker in reply:break
                        assert b'audio: state=4' not in reply and time.monotonic()<end,reply
                    (out/'serial.txt').write_bytes(output)
                    if not rounds%25:print(f'audio: {rounds} complete tones, {keyboard_rounds} USB keyboard checks, elapsed={time.monotonic()-began:.1f}s',flush=True)
                assert b'exception:' not in output
                q.call('quit');process.wait(timeout=10)
            finally:
                stop_process(process)
                (out/'serial.txt').write_bytes(output)
    report=analyze_tones(out/'tone.wav',rounds)
    report.update({'controller':args.controller,'codec':args.codec,'status':'PASS','usb_keyboard_checks':keyboard_rounds,
        'soak_seconds':time.monotonic()-began})
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!="tone_segments_seconds"},indent=2))

if __name__=='__main__':main()
