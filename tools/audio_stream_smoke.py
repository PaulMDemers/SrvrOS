#!/usr/bin/env python3
"""Capture sustained stereo PCM from the real guest WAV player and HDA driver."""
import argparse
import hashlib
from array import array
import io
import json
import math
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import wave
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from exfat_test_image import ExfatFixture


def fixture(seconds=12):
    frames=48000*seconds;samples=array('h')
    for n in range(frames):
        ramp=min(n,frames-n-1,480)/480 if n<frames-12000 else 0
        a=n%96;b=n%64
        samples.extend((int((a-24 if a<48 else 72-a)*160*ramp),int((b-16 if b<32 else 48-b)*240*ramp)))
    output=io.BytesIO()
    with wave.open(output,'wb') as w:w.setparams((2,2,48000,0,'NONE',''));w.writeframes(samples.tobytes())
    return output.getvalue(),samples


def mono_fixture(seconds):
    samples=array('h',(int(3840*math.sin(2*math.pi*500*n/44100)) if n<(seconds*44100-11025) else 0 for n in range(seconds*44100)))
    expected=array('h')
    for n in range(seconds*48000):
        value=int(3840*math.sin(2*math.pi*500*n/48000)) if n<(seconds*48000-12000) else 0
        expected.extend((value,value))
    output=io.BytesIO()
    with wave.open(output,'wb') as w:w.setparams((1,2,44100,0,'NONE',''));w.writeframes(samples.tobytes())
    return output.getvalue(),expected


def analyze_repeated(path,source,repeats,tolerance=2):
    expected=array('h',(int(n/4) for n in source))
    first=next(n for n in range(len(expected)//2) if max(abs(expected[2*n]),abs(expected[2*n+1]))>20)
    last=max(n for n in range(len(expected)//2) if max(abs(expected[2*n]),abs(expected[2*n+1]))>20)
    segments=[];active=None;position=0;last_active=0;peak=0;maximum_error=0
    with wave.open(str(path) if isinstance(path,Path) else path,'rb') as w:
        assert (w.getnchannels(),w.getsampwidth(),w.getframerate())==(2,2,48000)
        while raw:=w.readframes(16384):
            values=array('h',raw)
            for left,right in zip(values[::2],values[1::2]):
                level=max(abs(left),abs(right));peak=max(peak,level)
                if active is None and level>20:active=position
                if active is not None:
                    index=position-active+first
                    assert index<len(expected)//2,'unexpected extra samples'
                    error=max(abs(left-expected[index*2]),abs(right-expected[index*2+1]))
                    maximum_error=max(maximum_error,error)
                    assert error<=tolerance,('waveform mismatch',len(segments),position,error)
                    if level>20:last_active=position
                    if position-last_active>=4800:
                        assert abs((last_active-active)-(last-first))<=2,'missing/truncated signal'
                        segments.append((active,last_active));active=None
                position+=1
    assert active is None and len(segments)==repeats,(len(segments),repeats)
    assert position-last_active>=4800,'missing quiet tail'
    return dict(status='PASS',frames=position,complete_streams=len(segments),max_sample_error=maximum_error,peak=peak,distinct_stereo=True)


def analyze_prefix(path,expected):
    # Lifecycle cases append interrupted and shorter files after the first one.
    with wave.open(str(path),'rb') as w:
        params=w.getparams();data=w.readframes(len(expected)//2-2400)
    output=io.BytesIO()
    with wave.open(output,'wb') as w:w.setparams(params);w.writeframes(data)
    output.seek(0)
    return analyze_repeated(output,expected,1)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--seconds',type=int,default=12);parser.add_argument('--lifecycle',action='store_true');parser.add_argument('--repeat',type=int,default=1);parser.add_argument('--mono-44100',action='store_true');parser.add_argument('--codec-timer',choices=['on','off'],default='on');parser.add_argument('--output',type=Path);args=parser.parse_args()
    assert 5<=args.seconds<=60 and 1<=args.repeat<=30 and not (args.lifecycle and args.repeat!=1)
    root=Path(__file__).resolve().parents[1];out=root/('build/review/audio-stream-lifecycle' if args.lifecycle else 'build/review/audio-stream-soak' if args.repeat>1 else 'build/review/audio-stream-44100' if args.mono_44100 else 'build/review/audio-stream');out=args.output or out;out=out.resolve();out.mkdir(parents=True,exist_ok=True)
    payload,expected=(mono_fixture(args.seconds) if args.mono_44100 else fixture(args.seconds));(out/'source.wav').write_bytes(payload)
    original=boot.make_limine_conf;boot.make_limine_conf=lambda w,h:original(w,h)+'    cmdline: srvros.audio=hda\n'
    output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-pcm-') as directory:
        tmp=Path(directory);disk=ExfatFixture((root/'build/srvros.exfat').read_bytes());disk.add_directory_files('audio-test',{'stereo.wav':payload,'short.wav':fixture(1)[0]});(tmp/'disk.exfat').write_bytes(disk.data)
        iso=boot.build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=boot.choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d',
            '-drive',f'if=none,id=disk,file={tmp}/disk.exfat,format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-device','ich9-intel-hda','-device',f'hda-duplex,audiodev=capture,use-timer={args.codec_timer}',
            '-trace','enable=hda_audio_overrun',
            '-audiodev',f'wav,id=capture,path={out}/capture.wav,out.frequency=48000',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        provenance={'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'codec_timer':args.codec_timer,'seconds':args.seconds,'repeat':args.repeat,'sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'build/kernel/srvros.elf',root/'build/srvros.exfat',root/'build/userspace/playwav.elf']}}
        (out/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
        with (out/'qemu.txt').open('w') as log:
            process=subprocess.Popen(command,stdout=log,stderr=log)
            try:
                sock=boot.connect_serial(port,20);sock.settimeout(.2);q=QMP(tmp/'qmp')
                def wait(marker,timeout=60):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if not consumed:q.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None:raise RuntimeError(f'Missing {marker!r}; {output[-2000:]!r}')
                        try:output+=sock.recv(16384)
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                wait(b'srv> ',100);assert b'audio: state=2' in output
                sock.sendall(b'run /fat/bin/sysabi\n');wait(b'sysabi: ok');wait(b'srv> ')
                for round_number in range(args.repeat):
                    sock.sendall(b'run /fat/bin/playwav /fat/audio-test/stereo.wav\n');wait(b'playwav: playing ')
                    for c in 'echo pcm-input\n':q.key({' ':'spc','-':'minus','\n':'ret'}.get(c,c))
                    wait(f'playwav: complete bytes={len(expected)*2} underruns=0'.encode(),args.seconds+60)
                    wait(b'\npcm-input');wait(b'srv> ')
                    print(f'audio-stream: complete round {round_number+1}/{args.repeat}',flush=True)
                sock.sendall(b'audio\n');wait(b'audio: state=2');wait(b'srv> ')
                if args.lifecycle:
                    sock.sendall(b'run /fat/bin/sh\n');wait(b' $ ')
                    sock.sendall(b'playwav /fat/audio-test/stereo.wav &\n');wait(b'playwav: playing ')
                    began=time.monotonic()
                    for c in 'echo live-input\n':q.key({' ':'spc','-':'minus','\n':'ret'}.get(c,c))
                    wait(b'\nlive-input',10)
                    sock.sendall(b'/fat/bin/sysabi\n');wait(b'sysabi: ok',10)
                    sock.sendall(b'playwav /fat/audio-test/short.wav\n');wait(b'sound output is unavailable or already in use',10)
                    sock.sendall(b'kill $!; wait; echo PCM-REAPED\n');wait(b'\nPCM-REAPED',10)
                    assert time.monotonic()-began<args.seconds-1,'Cancellation test missed the running player'
                    for _ in range(3):
                        sock.sendall(b'playwav /fat/audio-test/short.wav\n');wait(b'playwav: complete bytes=192000 underruns=0',15);wait(b' $ ')
                    sock.sendall(b'exit\n');wait(b'srv> ')
                    sock.sendall(b'audio\n');wait(b'audio: state=2');wait(b'srv> ')
                assert b'exception:' not in output and b'audio: state=4' not in output
                q.call('quit');process.wait(timeout=10)
            finally:stop_process(process);(out/'serial.txt').write_bytes(output)
    report=analyze_prefix(out/'capture.wav',expected) if args.lifecycle else analyze_repeated(out/'capture.wav',expected,args.repeat,4 if args.mono_44100 else 2);report.update(seconds=args.seconds,repeat=args.repeat,usb_input_retained=True,syscall_boundaries='PASS',lifecycle=args.lifecycle,mono_44100=args.mono_44100,codec_timer=args.codec_timer,distinct_stereo=not args.mono_44100)
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
