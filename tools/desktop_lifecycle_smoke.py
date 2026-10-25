#!/usr/bin/env python3
"""Application lifecycle: owned native dialogs, quit cancellation and existing-process file opens."""
import os
import argparse
import hashlib
import json
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

COMPOSITOR_TRACE=rb'(?:deskd: |desktop-launch: )[^\n]*\n'
def application_output(data):
    return re.sub(COMPOSITOR_TRACE,b'',data)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pointer',action='store_true',help='hold and release real pointer clicks on modal and picker buttons')
    parser.add_argument('--crash',action='store_true',help='kill Preview with an owned picker, then save through healthy apps')
    parser.add_argument('--refuse-dialogs',action='store_true',help='force modal admission refusal and validate embedded recovery')
    args=parser.parse_args()
    if sum((args.crash,args.refuse_dialogs,args.pointer))>1:parser.error('choose one directed fixture')
    root=Path(__file__).resolve().parents[1]
    out=root/('build/review/desktop-lifecycle-pointer' if args.pointer else 'build/review/desktop-lifecycle-crash' if args.crash else 'build/review/desktop-lifecycle-refusal' if args.refuse_dialogs else 'build/review/desktop-lifecycle');out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    shots=[];pointer_actions=[]
    output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-text-windows-') as directory:
        tmp=Path(directory)
        from exfat_test_image import ExfatFixture
        source=ExfatFixture((root/'build/srvros.exfat').read_bytes())
        # The legacy boot packer omits standard boot/upcase metadata. Use a
        # formatted fixture with matched binaries, so before/after fsck is valid.
        with (tmp/'disk.exfat').open('xb') as stream:stream.truncate(64*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','4K',str(tmp/'disk.exfat')],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture((tmp/'disk.exfat').read_bytes())
        fixture.add_directory_files('bin',{name:source.read('bin/'+name) for name in
            ('deskd','desktools','deskfs','deskview','imageworker','deskterm','sh','kill','sync','ps')})
        fixture.add_directory_files('etc',{'init.sh':b'echo lifecycle-fixture-ready\n'})
        fixture.add_directory_files('home',{})
        fixture.add_directory_files('route-text',{'Routed.txt':b'Routed text\n'})
        fixture.add_directory_files('route-image',{'Routed.bmp':(root/'tools/fixtures/images/pattern.bmp').read_bytes()})
        (tmp/'disk.exfat').write_bytes(fixture.data)
        from exfat_test_image import check_clean_image
        (out/'before-fsck.txt').write_text(check_clean_image(tmp/'disk.exfat'))
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
                    # Serial writes from independent processes may be interleaved.
                    # Match exact app bytes while permitting complete compositor
                    # trace lines between them; keep the original stream as evidence.
                    pattern=(b'(?:'+COMPOSITOR_TRACE+b')*').join(re.escape(bytes([v])) for v in marker)
                    while True:
                        at=output.find(marker,consumed)
                        if at>=0:consumed=at+len(marker);return
                        if not marker.startswith(b'deskd:'):
                            found=re.search(pattern,output[consumed:])
                            if found:consumed+=found.end();return
                        if marker==b'srv> ' and not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None:raise RuntimeError(f'Missing {marker!r}')
                        try:
                            chunk=serial.recv(16384)
                            if not chunk:raise RuntimeError('Serial disconnected')
                            output+=chunk
                        except socket.timeout:pass
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':f'sendkey {keys}'});time.sleep(.3)
                def text(value):
                    mapping={' ':('spc',False),'/':('slash',False),'.':('dot',False),'-':('minus',False),'\n':('ret',False)}
                    for char in value:
                        key,shift=mapping.get(char,(char.lower(),char.isupper()));qmp.key(key,shift)
                def shot(name):
                    time.sleep(.3);qmp.call('screendump',{'filename':str(out/f'{name}.ppm')});shots.append(name+'.ppm')
                wait(b'srv> ',100)
                serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                baseline=set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                serial.sendall(b'run /fat/bin/deskd '+(b'--dialog-refusal-test' if args.refuse_dialogs else b'--apps-trace')+b'\n')
                def admitted():
                    wait(b'text-runtime: admitted window=');wait(b'\n')
                    return int(re.findall(rb'text-runtime: admitted window=(\d+)',output)[-1])
                first=admitted();wait(b'deskd: commit');pid=re.findall(rb'deskd: opened pid=(\d+)',output)[-1]
                pointer=[24,56]
                def click_modal(kind,index,label):
                    # Locate the visible widget row, rather than infer its
                    # position from private compositor placement state.
                    from PIL import Image
                    width,height,count=(112,32,3) if kind=='dialog' else (84,28,2)
                    end=time.monotonic()+8;buttons=[]
                    while time.monotonic()<end:
                        path=out/'pointer-target.ppm';qmp.call('screendump',{'filename':str(path)})
                        im=Image.open(path).convert('RGB');pixels=im.load();rows={}
                        for y in range(100,im.height-height):
                            x=0
                            while x<im.width-width:
                                color=pixels[x,y]
                                if color not in ((116,139,121),(56,100,79)) or (x and pixels[x-1,y]==color):x+=1;continue
                                right=x+width-1;bottom=y+height-1
                                if all(pixels[t,y]==color and pixels[t,bottom]==color for t in range(x,x+width)) and all(pixels[x,t]==color and pixels[right,t]==color for t in range(y,y+height)):
                                    rows.setdefault(y,[]).append((x,y));x+=width
                                else:x+=1
                        candidates=[row for row in rows.values() if len(row)==count]
                        if candidates:buttons=max(candidates,key=lambda row:row[0][1]);break
                        time.sleep(.1)
                    assert buttons,f'No visible {kind} button row'
                    x,y=buttons[index];x+=width//2;y+=height//2
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,1);time.sleep(.35);qmp.mouse(0,0,0)
                    pointer_actions.append(label);time.sleep(.3)
                def launch(query,marker):
                    shortcut('meta_l-spc');wait(b'launcher opened');text(query);qmp.key('ret');wait(marker)
                def save_as(path):
                    shortcut('ctrl-l');shortcut('ctrl-a');text(path);shortcut('ctrl-shift-s')
                    wait(b'picker ready mode=2');qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                def open_from_files(folder):
                    launch('Files window',b'launcher window=')
                    shortcut('ctrl-l');shortcut('ctrl-a');text(folder);qmp.key('ret')
                    wait(b'file-result role=files op=3 status=0');time.sleep(.4);qmp.key('home');qmp.key('ret')
                if args.pointer:
                    shortcut('ctrl-shift-n');clicked=admitted();wait(b'deskd: commit')
                    text('Click draft');shortcut('ctrl-w');wait(f'parent={clicked}'.encode())
                    click_modal('dialog',2,'Cancel');text(' remains')
                    shortcut('ctrl-w');wait(f'parent={clicked}'.encode());click_modal('dialog',0,'Save-to-picker')
                    wait(b'picker ready mode=2');click_modal('picker',0,'Cancel-picker')
                    shortcut('ctrl-w');wait(f'parent={clicked}'.encode());click_modal('dialog',0,'Save')
                    wait(b'picker ready mode=2');shortcut('ctrl-a');text('pointer.txt')
                    click_modal('picker',1,'Save-picker');wait(b'file-result role=text op=2 status=0')
                    wait(f'text-runtime: closed window={clicked}'.encode())
                    shortcut('ctrl-shift-n');discarded=admitted();wait(b'deskd: commit')
                    text('Discard only');shortcut('ctrl-w');wait(f'parent={discarded}'.encode())
                    click_modal('dialog',1,'Discard');wait(f'text-runtime: closed window={discarded}'.encode())
                    shot('pointer-actions-complete')
                text('Draft');shortcut('ctrl-shift-n');second=admitted();wait(b'deskd: commit')
                text('Sibling');shortcut('ctrl-w');wait(f'parent={second}'.encode());shot('owned-unsaved-dialog')
                shortcut('alt-tab');text(' extra');shortcut('alt-tab');qmp.key('esc');time.sleep(.5)
                text(' kept');save_as('/fat/sibling.txt')
                shortcut('ctrl-q');wait(f'text-runtime: closed window={second}'.encode())
                wait(f'parent={first}'.encode());shot('quit-prompts-next-document');qmp.key('esc');time.sleep(.5)
                text(' survives');save_as('/fat/survivor.txt')
                launch('Files new app',b'desktools: starting role=files');wait(b'file-result role=files op=3 status=0')
                shortcut('ctrl-l');shortcut('ctrl-a');text('/fat/route-text');qmp.key('ret')
                wait(b'file-result role=files op=3 status=0');time.sleep(.4);qmp.key('home');qmp.key('ret')
                wait(b'text-runtime: open-document accepted=1');routed=admitted();wait(b'file-result role=text op=1 status=0')
                assert application_output(output).count(b'desktools: starting role=text')==1
                assert re.findall(rb'deskd: document-route request=\d+ pid=(\d+) application=2',output)[-1]==pid
                shortcut('ctrl-end');text(' edited');shortcut('ctrl-l');shortcut('ctrl-a');text('/fat/bin')
                shortcut('ctrl-q');wait(f'parent={routed}'.encode());shortcut('shift-tab');shortcut('shift-tab');qmp.key('ret')
                wait(b'picker ready mode=2');qmp.key('ret');wait(b'file-result role=text op=2 status=');wait(b'\n')
                assert int(re.findall(rb'file-result role=text op=2 status=(\d+)',application_output(output))[-1])!=0
                shot('failed-save-retains-document');save_as('/fat/routed-copy.txt')
                shortcut('ctrl-q');wait(b'desktools: exited role=text failure=0')
                launch('Preview new app',b'deskview: starting');wait(b'deskd: commit')
                open_from_files('/fat/route-image')
                wait(b'deskview: open-document accepted=1');wait(b'deskview: loaded path=/fat/route-image/Routed.bmp')
                assert application_output(output).count(b'deskview: starting')==1
                shortcut('ctrl-o');wait(b'deskview: picker ready');shot('preview-owned-picker')
                if args.crash:
                    preview_pid=int(re.findall(rb'deskd: document-route request=\d+ pid=(\d+) application=7',output)[-1])
                    launch('Terminal new app',b'deskterm: ready');time.sleep(.5)
                    text(f'kill -9 {preview_pid}\n')
                    wait(f'deskd: child-exit pid={preview_pid} status=137'.encode())
                    open_from_files('/fat/route-text');wait(b'desktools: starting role=text');admitted();wait(b'file-result role=text op=1 status=0')
                    shortcut('ctrl-end');text(' after crash');save_as('/fat/after-crash.txt');shot('healthy-after-preview-crash')
                    shortcut('ctrl-q');wait(b'desktools: exited role=text failure=0')
                else:
                    if args.pointer:click_modal('picker',0,'Preview-cancel-picker')
                    else:qmp.key('esc')
                    wait(b'deskview: picker canceled pending=0 retained=1');time.sleep(.5);shortcut('ctrl-q')
                    wait(b'deskview: workers retired=1');wait(b'deskview: exited failure=0')
                shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                after=set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                if args.crash:
                    deadline=time.monotonic()+4
                    while after!=baseline and time.monotonic()<deadline:
                        time.sleep(.5);serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                        after=set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                assert after==baseline,(baseline,after)
                if args.refuse_dialogs:assert output.count(b'dialog admission refused parent=')>=5
                assert b'desktop-application: FAIL' not in output and b'failure=1' not in output and b'exception:' not in output
                serial.sendall(b'sync\n');wait(b'srv> ')
                serial.close();qmp.stream.close();qmp.sock.close()
            except Exception:
                if qmp:
                    try:shot('failure')
                    except Exception:pass
                raise
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
            disk=ExfatFixture((tmp/'disk.exfat').read_bytes())
            for name,expected in {'sibling.txt':b'Sibling kept','survivor.txt':b'Draft extra survives','routed-copy.txt':b'Routed text\n edited','route-text/Routed.txt':b'Routed text\n'}.items():
                assert disk.read(name)==expected,(name,disk.read(name))
            if args.pointer:assert disk.read('home/pointer.txt')==b'Click draft remains'
            if args.crash:assert disk.read('after-crash.txt')==b'Routed text\n after crash'
            from exfat_test_image import check_clean_image
            (out/'fsck.txt').write_text(check_clean_image(tmp/'disk.exfat'))
            allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(tmp/'disk.exfat'),'allocation-check'],capture_output=True,text=True)
            (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr)
            if allocation.returncode:
                shutil.copyfile(tmp/'disk.exfat',out/'failure.exfat')
                raise RuntimeError('Offline allocation failed')
            names=(['home/pointer.txt'] if args.pointer else [])+['sibling.txt','survivor.txt','routed-copy.txt','route-text/Routed.txt']+(['after-crash.txt'] if args.crash else [])
            result={'status':'PASS','crash':args.crash,'refused_dialogs':args.refuse_dialogs,
                'physical_acceptance':False,'pointer_actions':pointer_actions,'source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
                'source_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),
                'saved_files':{name:hashlib.sha256(disk.read(name)).hexdigest() for name in names},
                'process_baseline_restored':True,'offline_allocation':'PASS','screenshots':shots,
                'payload_sha256':{str(path.relative_to(root)):hashlib.sha256(path.read_bytes()).hexdigest() for path in (root/'build/kernel/srvros.elf',root/'build/initramfs.tar',root/'build/srvros.exfat')},
                'iso_sha256':hashlib.sha256(Path(iso).read_bytes()).hexdigest()}
            (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f'desktop-lifecycle: PASS modal sibling dispatch, quit cancel/failed-save retention, existing Text/Preview process routing, exact files and process baseline; crash={args.crash}, refused_dialogs={args.refuse_dialogs}. Evidence: {out}')

if __name__=='__main__':main()
