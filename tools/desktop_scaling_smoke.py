#!/usr/bin/env python3
"""Verify integer desktop scaling, logical input and saved capture in a VM."""
import argparse
import io
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time
from PIL import Image, ImageChops
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP
from exfat_test_image import ExfatFixture
from uefi_usb_smoke import default_ovmf_path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resolution',default='2880x1800')
    parser.add_argument('--scale',type=int,choices=(1,2),default=2)
    args=parser.parse_args()
    width,height=map(int,args.resolution.split('x'));scale=args.scale
    logical=((width+scale-1)//scale,(height+scale-1)//scale)
    root=Path(__file__).resolve().parents[1]
    out=root/f'build/review/desktop-scaling-{width}x{height}-{scale}'
    out.mkdir(parents=True,exist_ok=True)
    output=b'';consumed=0
    port=choose_port(30001,38000)
    with tempfile.TemporaryDirectory(prefix='srvros-scale-') as directory:
        tmp=Path(directory);shutil.copyfile(root/'build/srvros.exfat',tmp/'disk.exfat')
        iso=build_iso(str(root),str(tmp),width,height,os.environ.copy())
        command=['qemu-system-x86_64','-M','q35','-m','512M',
            '-drive',f'if=pflash,format=raw,readonly=on,file={default_ovmf_path()}', '-cdrom',str(iso),'-boot','d',
            '-vga','none','-device',f'VGA,vgamem_mb=64,xres={width},yres={height}',
            '-drive',f'if=none,id=disk,file={tmp/"disk.exfat"},format=raw',
            '-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
            '-qmp',f'unix:{tmp/"qmp"},server=on,wait=off','-display','none','-monitor','none','-no-reboot','-nic','none']
        with (out/'qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=50):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if marker==b'srv> ' and not consumed:
                            qmp.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None:
                            qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                            raise RuntimeError(f'Missing {marker!r}; see {out}')
                        try:
                            chunk=serial.recv(16384)
                            if not chunk: raise RuntimeError('Serial disconnected')
                            output+=chunk
                        except socket.timeout: pass
                        (out/'serial.log').write_bytes(output)
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':f'sendkey {keys}'});time.sleep(.3)
                pointer=[24,56]
                def click(x,y):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,1);qmp.mouse(0,0,0)
                def shot(name):
                    time.sleep(.5);qmp.call('screendump',{'filename':str(out/f'{name}.ppm')})
                    image=Image.open(out/f'{name}.ppm').convert('RGB');assert image.size==(width,height),image.size
                    # Every complete 2x2 block must be the same RGB, including
                    # cursor/chrome/text. This also catches unscaled partial damage.
                    small=image.resize(logical,Image.Resampling.NEAREST)
                    if width%scale==0 and height%scale==0:
                        assert ImageChops.difference(image,small.resize(image.size,Image.Resampling.NEAREST)).getbbox() is None
                    image.save(out/f'{name}.png');return image
                wait(b'srv> ',100)
                serial.sendall(b'run /fat/bin/deskd --scale=3\n');wait(b'deskd: scale must be 1 or 2');wait(b'srv> ')
                serial.sendall(f'run /fat/bin/deskd --apps-trace --scale={scale}\n'.encode())
                wait(f'deskd: display physical={width}x{height} logical={logical[0]}x{logical[1]} scale={scale}'.encode())
                wait(b'deskd: commit');wait(b'\n')
                pid=re.findall(rb'deskd: opened pid=(\d+)',output)[-1]
                cw,ch=map(int,re.findall(rb'deskd: commit pid='+pid+rb' generation=1 size=(\d+)x(\d+)',output)[-1])
                for key,shift in [('a',False),('a',True),('z',False),('z',True),('1',False)]: qmp.key(key,shift)
                shortcut('ctrl-s');wait(b'picker ready mode=2');qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                shot('typed')
                click(32+cw+2-56,86)
                zoom_w=min(logical[0]-18,1440)+2;zoom_h=min(logical[1]-177,900)+45
                wait(b'zoom pid='+pid+f' maximized=1 frame={(logical[0]-zoom_w)//2},52,{zoom_w},{zoom_h}'.encode())
                wait(b'commit pid='+pid+b' generation=2 size='+f'{zoom_w-2}x{zoom_h-45}'.encode())
                shot('maximized')
                click((logical[0]-zoom_w)//2+zoom_w-56,74)
                wait(b'zoom pid='+pid+b' maximized=0');wait(b'commit pid='+pid+b' generation=3')
                # Unfocus the text caret; snapshot must match immutable logical canvas.
                shortcut('shift-tab');shortcut('shift-tab');click(logical[0]-20,logical[1]//2)
                reference=shot('reference')
                shortcut('meta_l-shift-3');wait(b'deskd: screenshot captured path=');wait(f'height={logical[1]}'.encode())
                path=re.findall(rb'deskd: screenshot captured path=([^\s]+)',output)[-1].decode()
                wait(b'deskd: screenshot saved status=0 path=',90);shot('saved')
                shortcut('f10');shortcut('end');shortcut('ret');wait(b'deskd: exited',60);wait(b'srv> ')
                if b'deskd: FAIL' in output or b'exception:' in output: raise RuntimeError('Guest failure')
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
        fixture=ExfatFixture((tmp/'disk.exfat').read_bytes())
        assert fixture.read('home/Untitled.txt')==b'aAzZ1'
        capture=Image.open(io.BytesIO(fixture.read(path.removeprefix('/fat/')))).convert('RGB')
        assert capture.size==logical,capture.size
        physical=capture.resize((logical[0]*scale,logical[1]*scale),Image.Resampling.NEAREST).crop((0,0,width,height))
        assert ImageChops.difference(physical,reference).getbbox() is None
        capture.save(out/'saved-logical-capture.png')
    print(f'desktop-scaling: PASS {width}x{height} at {scale}x: invalid scale rejected, logical pointer/maximize/restore, scaled damage, durable exact logical screenshot, clean exit. Evidence: {out}')


if __name__=='__main__': main()
