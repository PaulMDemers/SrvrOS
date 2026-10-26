#!/usr/bin/env python3
"""Real offscreen dragging, dock restore/cycling and filtered modal-safe selection."""
import hashlib,json,os,re,shutil,socket,subprocess,tempfile,time
from pathlib import Path
from displayd_frame_smoke import choose_port,connect_serial,stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP
from exfat_test_image import ExfatFixture,check_clean_image
from serial_records import desktop_progress_end


def main():
    root=Path(__file__).resolve().parents[1];out=root/'build/review/desktop-window-dock';out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True);output=b'';consumed=0;pointer=[24,56];documents=[];extras=[]
    with tempfile.TemporaryDirectory(prefix='srvros-window-dock-') as directory:
        tmp=Path(directory);disk=tmp/'disk.exfat'
        with disk.open('xb') as stream:stream.truncate(64*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','4K',str(disk)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(disk.read_bytes());source=ExfatFixture((root/'build/srvros.exfat').read_bytes())
        fixture.add_directory_files('bin',{name:source.read('bin/'+name) for name in ('deskd','desktools','deskfs','deskview','imageworker','deskhelp','sh','sync','ps')})
        fixture.add_directory_files('home',{});fixture.add_directory_files('etc',{'init.sh':b'echo window-dock-ready\n'});disk.write_bytes(fixture.data)
        (out/'before-fsck.txt').write_text(check_clean_image(disk));iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d','-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp/"qmp"},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors);qmp=None
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=50):
                    nonlocal output,consumed
                    deadline=time.monotonic()+timeout
                    while True:
                        end=desktop_progress_end(output,marker,consumed)
                        if end is not None:consumed=end;return
                        if marker==b'srv> ' and not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-(deadline-timeout))
                        if time.monotonic()>deadline or process.poll() is not None:raise RuntimeError(f'Missing {marker!r}')
                        try:
                            chunk=serial.recv(65536)
                            if not chunk:raise RuntimeError('Serial disconnected')
                            output+=chunk;(out/'serial.log').write_bytes(output)
                        except socket.timeout:pass
                def wait_all(markers,timeout=50):
                    nonlocal consumed
                    start=consumed;end=start
                    for marker in markers:consumed=start;wait(marker,timeout);end=max(end,consumed)
                    consumed=end
                def shortcut(keys):qmp.call('human-monitor-command',{'command-line':'sendkey '+keys});time.sleep(.3)
                def text(value):
                    mapping={' ':('spc',False),'/':('slash',False),'.':('dot',False),'-':('minus',False),'\n':('ret',False)}
                    for char in value:
                        key,shift=mapping.get(char,(char.lower(),char.isupper()));qmp.key(key,shift)
                def move(x,y):qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                def click(x,y,button=1):move(x,y);qmp.mouse(0,0,button);qmp.mouse(0,0,0)
                def shot(name):time.sleep(.2);qmp.call('screendump',{'filename':str(out/(name+'.ppm'))})
                def launch(query,marker):shortcut('meta_l-spc');text(query);qmp.key('ret');wait(marker)
                def geometry(pid,window):
                    marker=f'deskd: geometry pid={pid} window={window} frame='.encode();wait(marker);wait(b'\n')
                    return tuple(map(int,re.findall(re.escape(marker)+rb'(-?\d+),(-?\d+),(\d+),(\d+)',output)[-1]))
                def admitted():
                    wait(b'text-runtime: admitted window=');wait(b'\n');identity=int(re.findall(rb'text-runtime: admitted window=(\d+)',output)[-1]);documents.append(identity);wait(b'deskd: commit');return identity
                def save(name):
                    shortcut('ctrl-l');shortcut('ctrl-a');text('/fat/home/'+name);shortcut('ctrl-s');wait_all([b'picker ready mode=2',b'deskd: commit']);qmp.key('ret');wait_all([b'file-result role=text op=2 status=0',retired])
                def drag(frame,x,y,grab=450):
                    move(frame[0]+min(grab,frame[2]-140),frame[1]+20);qmp.mouse(0,0,1);move(x,y);qmp.mouse(0,0,0)
                    wait(b'deskd: moved identity=');wait(b'\n');return tuple(map(int,re.findall(rb'deskd: moved identity=\d+ frame=(-?\d+),(-?\d+),(\d+),(\d+)',output)[-1]))
                def center():shortcut('meta_l-alt-up');wait(b'deskd: arranged pid=');wait(b'\n');return tuple(map(int,re.findall(rb'deskd: arranged pid=\d+ mode=3 frame=(-?\d+),(-?\d+),(\d+),(\d+)',output)[-1]))
                def dock(app,button=1):
                    apps=list(range(1,7))+sorted(extras);index=apps.index(app);cell=min(100,(1280-24)//len(apps));click((1280-cell*len(apps))//2+index*cell+cell//2,750,button)
                def processes():
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ');return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                wait(b'srv> ',100);baseline=processes();serial.sendall(b'run /fat/bin/deskd --apps-trace\n')
                wait(b'desktools: starting role=text');owner=int(re.findall(rb'app-timing: pid=(\d+) role=2 window=0 stage=main-entry',output)[-1]);retired=f'deskd: retired pid={owner}'.encode()
                first=admitted();text('Offscreen content kept.')
                # Initial Text geometry is the established default; subsequent
                # positions are acknowledged by real compositor drag receipts.
                left=drag((32,64,722,525),5,180);assert left[0]<0 and left[2:]==(722,525);shot('left-offscreen')
                frame=center();corner=drag(frame,1279,799,100);assert corner[:2]==(1120,688);shot('right-bottom-offscreen')
                shortcut('ctrl-s');wait_all([b'picker ready mode=2',b'deskd: commit']);shot('visible-owned-picker')
                dock(2);wait(b'deskd: dock activate application=2');qmp.key('esc');wait(retired)
                frame=center();assert frame[0]>=0 and frame[1]>=44 and frame[0]+frame[2]<=1280 and frame[1]+frame[3]<=720
                save('dock-one.txt')
                for label in ('Two.','Three.'):
                    shortcut('ctrl-shift-n');admitted();text(label);save('dock-'+('two' if label=='Two.' else 'three')+'.txt')
                click(714,133);shot('one-minimized')
                cycled=[]
                for _ in range(3):dock(2);wait(b'deskd: dock activate application=2 identity=');wait(b'\n');cycled.append(int(re.findall(rb'deskd: dock activate application=2 identity=(\d+)',output)[-1]))
                assert len(set(cycled))==3,'Dock must cycle all three owners, not alternate two'
                launch('Preview new app',b'deskview: starting');wait(b'deskd: opened pid=');wait(b'\n');preview=int(re.findall(rb'deskd: opened pid=(\d+)',output)[-1]);preview_frame=geometry(preview,1);wait(b'deskd: commit');extras.append(7)
                launch('Help new app',b'deskhelp: starting');wait(b'deskd: opened pid=');wait(b'\n');help_pid=int(re.findall(rb'deskd: opened pid=(\d+)',output)[-1]);wait(b'deskd: commit');extras.append(8);shot('running-apps-and-count')
                dock(7);wait(b'deskd: dock activate application=7');x,y,w,h=preview_frame;click(x+w-88,y+21);dock(7);wait(b'deskd: dock activate application=7');shot('preview-restored')
                dock(7,2);wait(b'deskd: dock windows application=7 items=2');shot('preview-window-list');qmp.key('esc');wait(b'deskd: launcher closed')
                dock(2,2);wait(b'deskd: dock windows application=2 items=4');text('dock-one');qmp.key('ret');wait(f'deskd: launcher window={owner} id={first}'.encode())
                click(302,278);text('X');shortcut('ctrl-end');text(' edit');shortcut('ctrl-w');wait_all([f'parent={first}'.encode(),b'deskd: commit']);shot('unsaved-owner-dialog')
                dock(2,2);wait(b'deskd: dock windows application=2 items=4');qmp.key('ret');wait(f'deskd: launcher window={owner} id={first}'.encode());qmp.key('esc');wait(retired)
                shortcut('ctrl-s');wait(b'file-result role=text op=2 status=0')
                launch('Help window',f'deskd: launcher window={help_pid} id=1'.encode())
                dock(7);wait(b'deskd: dock activate application=7');shortcut('ctrl-q');wait(b'deskview: exited failure=0');extras.remove(7)
                dock(8);wait(b'deskd: dock activate application=8');shortcut('ctrl-w');wait(b'deskhelp: exited failure=0');extras.remove(8);shot('unpinned-apps-retired')
                dock(2);wait(b'deskd: dock activate application=2');shortcut('ctrl-q');wait(b'desktools: exited role=text failure=0')
                assert b'text-runtime: documents admitted=3 retired=3 refused=0' in output
                shortcut('f10');shortcut('end');shortcut('ret');wait(b'deskd: exited');wait(b'srv> ')
                deadline=time.monotonic()+4
                while processes()!=baseline:
                    assert time.monotonic()<deadline,'Process baseline not restored';time.sleep(.5)
                assert b'failure=1' not in output and b'exception:' not in output
                qmp.call('quit');assert process.wait(timeout=10)==0
                serial.close();qmp.stream.close();qmp.sock.close()
            except Exception:
                if qmp:
                    try:qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                    except Exception:pass
                raise
            finally:stop_process(process);(out/'serial.log').write_bytes(output);shutil.copyfile(disk,out/'last-workspace.img')
        result=ExfatFixture(disk.read_bytes());expected={'home/dock-one.txt':b'XOffscreen content kept. edit','home/dock-two.txt':b'Two.','home/dock-three.txt':b'Three.'}
        for name,value in expected.items():assert result.read(name)==value,name
        (out/'fsck.txt').write_text(check_clean_image(disk));(out/'allocation.txt').write_text(subprocess.check_output([str(root/'build/tests/exfat-driver'),str(disk),'allocation-check'],text=True))
        receipt=dict(status='PASS',qemu_exit=0,physical_acceptance=False,source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],text=True)),offscreen_left=list(left),offscreen_right_bottom=list(corner),cycled_identities=cycled,documents=documents,modal_safe_dock_chooser=True,process_baseline_restored=True,sha256={name:hashlib.sha256(value).hexdigest() for name,value in expected.items()})
        (out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('desktop-window-dock: PASS offscreen drag/recovery, clipped windows, three-owner cycle, minimized restore, unpinned app lifetime, modal-safe filtered chooser, exact files, process baseline and offline integrity')

if __name__=='__main__':main()
