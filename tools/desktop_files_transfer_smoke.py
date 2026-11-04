#!/usr/bin/env python3
"""Exercise Files cross-folder transfers on a disposable matched-generated disk."""
import argparse
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture
from test_exfat_generator import verify_image
from serial_records import COMPOSITOR_TRACE, desktop_progress_end


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--refuse-dialogs', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = (args.output or root / ('build/review/desktop-files-transfer-refusal' if args.refuse_dialogs else 'build/review/desktop-files-transfer')).resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    output = b''; consumed = 0; shots = []; qmp = None; process = None
    payload = bytes((i*29+i//71)&255 for i in range(128*1024+17))
    collision = b'Existing destination must remain unchanged.\n'
    pins = {'source': subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
            'source_status': subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True).splitlines(),
            'submodules': subprocess.check_output(['git','submodule','status','--recursive'],cwd=root,text=True).splitlines(),
            'payload_sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                (root/'build/kernel/srvros.elf',root/'build/initramfs.tar',root/'build/srvros.exfat')}}
    (out/'source.json').write_text(json.dumps(pins,indent=2)+'\n')
    with tempfile.TemporaryDirectory(prefix='srvros-files-transfer-') as directory:
        tmp = Path(directory); disk = tmp/'disk.exfat'
        fixture = ExfatFixture((root/'build/srvros.exfat').read_bytes())
        names = {name.casefold() for name,_ in fixture.entries(fixture.chain(fixture.root))}
        assert not names.intersection({'transfer-source','transfer-dest','transfer-collision','transfer-empty','transfer-close'})
        fixture.add_directory_files('transfer-source', {'payload.bin':payload})
        fixture.add_directory_files('transfer-dest', {})
        fixture.add_directory_files('transfer-empty', {})
        fixture.add_directory_files('transfer-close', {})
        fixture.add_directory_files('transfer-collision', {'payload.bin':collision})
        disk.write_bytes(fixture.data)
        def allocation(label):
            check = subprocess.run([str(root/'build/tests/exfat-driver'),str(disk),'allocation-check'],capture_output=True,text=True)
            (out/(label+'.txt')).write_text(check.stdout+check.stderr)
            if check.returncode: raise RuntimeError(label+' failed')
        def generator(label):
            result = io.StringIO()
            with contextlib.redirect_stdout(result): verify_image(disk)
            (out/(label+'.txt')).write_text(result.getvalue())
        try:
            generator('before-generator-check'); allocation('before-allocation')
            iso = build_iso(str(root),str(tmp),1280,800,os.environ.copy())
            pins['iso_sha256'] = hashlib.sha256(Path(iso).read_bytes()).hexdigest()
            port = choose_port(30001,38000)
            command = ['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
                '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
                '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0', '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
                '-qmp',f'unix:{tmp/"qmp"},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
            with (out/'qemu.log').open('wb') as errors:
                process = subprocess.Popen(command,stdout=errors,stderr=errors)
                serial = connect_serial(port,20); serial.settimeout(.2); qmp = QMP(tmp/'qmp')
                def wait(marker,timeout=45):
                    nonlocal output,consumed
                    end = time.monotonic()+timeout
                    while True:
                        at=desktop_progress_end(output,marker,consumed)
                        if at is not None:
                            consumed=at;return
                        if marker == b'srv> ' and not consumed: qmp.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None: raise RuntimeError(f'Missing {marker!r}')
                        try:
                            chunk = serial.recv(16384)
                            if not chunk: raise RuntimeError('Serial disconnected')
                            output += chunk
                            (out/'serial.log').write_bytes(output)
                        except socket.timeout: pass
                def wait_all(markers,timeout=45):
                    nonlocal consumed
                    start=consumed; end=start
                    for marker in markers:
                        consumed=start; wait(marker,timeout); end=max(end,consumed)
                    consumed=end
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':'sendkey '+keys}); time.sleep(.3)
                def text(value):
                    mapping={'/':('slash',False),'.':('dot',False),'-':('minus',False),' ':('spc',False)}
                    for c in value:
                        key,shift=mapping.get(c,(c.lower(),c.isupper())); qmp.key(key,shift)
                pointer=[24,56]
                def click(x,y):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,1);time.sleep(.3);qmp.mouse(0,0,0);time.sleep(.3)
                def shot(name):
                    time.sleep(.3);qmp.call('screendump',{'filename':str(out/(name+'.ppm'))});shots.append(name+'.ppm')
                def prompt_button(index):
                    # Find the two 112x32 dialog buttons in either the owned or embedded prompt.
                    from PIL import Image
                    qmp.mouse(24-pointer[0],56-pointer[1]);pointer[:]=[24,56]
                    end=time.monotonic()+8
                    while time.monotonic()<end:
                        p=out/'pointer-target.ppm';qmp.call('screendump',{'filename':str(p)})
                        im=Image.open(p).convert('RGB');pixels=im.load();rows={};w=112;h=32
                        for y in range(100,im.height-h):
                            x=0
                            while x<im.width-w:
                                color=pixels[x+4,y];right=x+w-1;bottom=y+h-1
                                if pixels[x+4,y-1]!=color and pixels[x+w//2,y+h//2]!=color and all(pixels[t,y]==color and pixels[t,bottom]==color for t in range(x+4,right-3)) and all(pixels[x,t]==color and pixels[right,t]==color for t in range(y+4,bottom-3)):
                                    rows.setdefault(y,[]).append((x,y));x+=w
                                else:x+=1
                        candidates=[row for row in rows.values() if len(row)==2]
                        if candidates:
                            x,y=max(candidates,key=lambda row:row[0][1])[index];click(x+w//2,y+h//2);return
                        time.sleep(.1)
                    raise RuntimeError('Prompt button row not found')
                def baseline():
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                    return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                def navigate(path):
                    shortcut('ctrl-l');shortcut('ctrl-a');text(path);qmp.key('ret');wait(('files-location path='+path+' ').encode())
                def launch(query):
                    shortcut('meta_l-spc');wait(b'launcher opened');text(query);qmp.key('ret');wait(b'launcher window=')
                def prompt(action):
                    shortcut('ctrl-shift-'+action)
                    marker=('files-transfer-prompt action='+('copy' if action=='c' else 'move')+' source=').encode()
                    if args.refuse_dialogs: wait(marker)
                    else: wait_all([marker,b'deskd: commit'])
                def target(path):
                    shortcut('ctrl-a');text(path)
                wait(b'srv> ',100);before=baseline();assert before
                serial.sendall(b'run /fat/bin/deskd '+(b'--dialog-refusal-test' if args.refuse_dialogs else b'--apps-trace')+b'\n')
                wait(b'stage=first-publish edge=event');wait(b'deskd: commit')
                click(390,750);wait(b'desktools: starting role=files')
                wait_all([b'file-result role=files op=3 status=0',b'files-runtime: admitted window='])
                wait(b'\n')
                original=int(re.findall(rb'files-runtime: admitted window=(\d+)',re.sub(COMPOSITOR_TRACE,b'',output))[-1])
                navigate('/fat/transfer-source');text('payload')
                shortcut('ctrl-shift-n');wait(b'files-runtime: admitted window=');wait(b'\n')
                navigate('/fat/transfer-collision')
                launch('transfer-source window')
                # Both cancel mechanisms must leave the same selected item usable.
                prompt('c');shot('copy-prompt');qmp.key('esc')
                prompt('m');shot('move-prompt');prompt_button(0)
                # Refusals leave the source selected; refresh and prompt again to prove it.
                prompt('c');target('/fat/transfer-collision');qmp.key('ret');wait(b'file-result role=files op=7 status=2')
                shortcut('ctrl-r');wait(b'file-result role=files op=3 status=0')
                prompt('c');target('/fat/no-such-transfer-folder');qmp.key('ret');wait(b'file-result role=files op=7 status=5')
                shortcut('ctrl-r');wait(b'file-result role=files op=3 status=0')
                prompt('c');target('/fat/transfer-dest');prompt_button(1)
                wait_all([b'file-result role=files op=7 status=0',b'files-location path=/fat/transfer-dest '],90);shot('copied-selected')
                # The sibling retains its own folder and collision file while
                # the original window navigates to the copy destination.
                launch('transfer-collision window');shortcut('ctrl-r');wait(b'file-result role=files op=3 status=0')
                text('payload');shortcut('f2')
                if args.refuse_dialogs: wait(b'files-prompt operation=5 name=payload.bin')
                else: wait_all([b'files-prompt operation=5 name=payload.bin',b'deskd: commit'])
                qmp.key('esc')
                launch('transfer-dest window')
                # Selection after copy is proved by moving that very file without reselecting.
                prompt('m');target('/fat/transfer-source');qmp.key('ret');wait(b'file-result role=files op=5 status=2')
                shortcut('alt-left');wait(b'files-location path=/fat/transfer-source ')
                shortcut('ctrl-r');wait(b'file-result role=files op=3 status=0')
                # Source still exists after copy and collision. Move it to a fresh folder.
                text('payload');prompt('m');target('/fat/transfer-dest-moved');qmp.key('ret');wait(b'file-result role=files op=5 status=5')
                # Return to destination and move the selected copy into the collision-free source
                # after deleting neither file: use a second empty folder created by fixture.
                shortcut('alt-right');wait(b'files-location path=/fat/transfer-dest ')
                text('payload');prompt('m');target('/fat/transfer-empty');qmp.key('ret')
                wait_all([b'file-result role=files op=5 status=0',b'files-location path=/fat/transfer-empty '],90);shot('moved-selected')
                shortcut('alt-left');wait(b'files-location path=/fat/transfer-dest ')
                shortcut('ctrl-r');wait(b'file-result role=files op=3 status=0')
                # Closing a window cannot abandon an accepted copy. Slow writes
                # so the close request is observed while its worker is busy.
                navigate('/fat/transfer-source');text('payload');prompt('c');target('/fat/transfer-close')
                qmp.call('block_set_io_throttle',{'device':'disk','bps':0,'bps_rd':0,'bps_wr':65536,'iops':0,'iops_rd':0,'iops_wr':0})
                start=len(output);qmp.key('ret');wait(b'file-submitted role=files op=7');shortcut('ctrl-w')
                close=f'window-lifecycle: close-request role=1 window={original} busy=1 operation=7'.encode()
                closed=f'files-runtime: closed window={original} '.encode()
                wait_all([close,b'file-result role=files op=7 status=0',closed],90)
                tail=re.sub(COMPOSITOR_TRACE,b'',output[start:])
                assert tail.index(close)<tail.index(b'file-result role=files op=7 status=0')<tail.index(closed)
                qmp.call('block_set_io_throttle',{'device':'disk','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':0})
                # Closing the original focuses its surviving sibling. Verify
                # that window directly, without reselecting it via launcher.
                shortcut('ctrl-r');wait_all([b'file-result role=files op=3 status=0',b'files-location path=/fat/transfer-collision '])
                shortcut('ctrl-q');wait(b'desktools: exited role=files failure=0')
                application_trace=re.sub(COMPOSITOR_TRACE,b'',output)
                assert b'files-runtime: documents admitted=2 retired=2 refused=0' in application_trace
                pools=re.findall(rb'file-pool: retired role=files spawned=(\d+) reaped=(\d+) peak=(\d+) live=0 bound=0',application_trace)
                assert len(pools)==1 and pools[0][0]==pools[0][1] and 1<=int(pools[0][2])<=2,pools
                shortcut('f10');shortcut('end');shortcut('ret');wait(b'deskd: exited',60);wait(b'srv> ')
                deadline=time.monotonic()+10
                while baseline()!=before:
                    if time.monotonic()>deadline:raise RuntimeError('Process baseline not restored')
                    time.sleep(.5)
                serial.sendall(b'run /fat/bin/sync\n');wait(b'run: pid=');wait(b' exited status=0');wait(b'srv> ')
                serial.sendall(b'fsck /fat\n');wait(b'errors=0 ok');wait(b'srv> ')
                assert b'failure=1' not in output and b'exception:' not in output and b'deskd: FAIL' not in output and b'unknown command:' not in output
                if args.refuse_dialogs: assert b'dialog admission refused parent=' in output
                serial.close();qmp.stream.close();qmp.sock.close();qmp=None
            stop_process(process);process=None
            saved=ExfatFixture(disk.read_bytes())
            expected={'transfer-source/payload.bin':payload,'transfer-empty/payload.bin':payload,'transfer-collision/payload.bin':collision,'transfer-close/payload.bin':payload}
            for path,value in expected.items(): assert saved.read(path)==value,path
            assert 'payload.bin' not in {name for name,_ in saved.entries(saved.read('transfer-dest'))}
            generator('generator-check');allocation('allocation')
            receipt={'status':'PASS','refuse_dialogs':args.refuse_dialogs,'fixture':'matched-generated',
                'guest_fsck':'PASS','offline_allocation':'PASS','process_baseline_restored':True,
                'files_windows':2,'sibling_independent':True,'files_pool_retired':True,'accepted_copy_drained_on_close':True,
                'source':pins,'screenshots':shots,'sha256':{k:hashlib.sha256(v).hexdigest() for k,v in expected.items()}}
            (out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
        except Exception as exc:
            if qmp:
                try: qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                except Exception: pass
            (out/'result.json').write_text(json.dumps({'status':'FAIL','error':repr(exc),'source':pins,'screenshots':shots},indent=2)+'\n')
            raise
        finally:
            if process: stop_process(process)
            (out/'serial.log').write_bytes(output)
            shutil.copyfile(disk,out/'last-workspace.img')
    print(f'desktop-files-transfer: PASS; evidence: {out}')

if __name__=='__main__': main()
