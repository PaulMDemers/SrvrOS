#!/usr/bin/env python3
"""Automatic ordinary Home capture on disposable Q35 USB; no physical disks."""
import argparse,hashlib,json,os,re,struct,subprocess,tempfile,time
from pathlib import Path
from battery_workspace_smoke import ANCHOR,MARKER
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from exfat_test_image import ExfatFixture,check_clean_image
from vm_sockets import connect_unix
from battery_home_desktop_records import check_capture
from hardware_desktop import verify_home_desktop_screenshot

CASES={'completion':83,'profile-refusal':15,'uncertain-ack':79,'query-refusal':75,'no-transition':83,'early-exit':83,'work-overrun':83}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=CASES,default='completion')
    parser.add_argument('--air61',action='store_true')
    parser.add_argument('--transitions',action='store_true')
    parser.add_argument('--work',action='store_true');args=parser.parse_args()
    assert not (args.work and args.transitions)
    assert args.work or args.case not in ('early-exit','work-overrun')
    assert args.transitions or args.case!='no-transition'
    root=Path(__file__).resolve().parents[1];out=root/'build/review'/('battery-home-'+('work' if args.work else 'transitions' if args.transitions else 'desktop')+'-capture-'+('air61-' if args.air61 else '')+args.case)
    out.mkdir(parents=True,exist_ok=True);(out/'result.json').unlink(missing_ok=True);output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-desktop-capture-') as folder:
        tmp=Path(folder);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as stream:stream.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes());fixture.add_directory_files('srvros-work',{'.workspace':MARKER,'anchor.txt':ANCHOR})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as stream:stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
        (tmp/'battery.asl').write_text(('#define BATTERY_AIR61 1\n' if args.air61 else '')+f'#define BATTERY_FAULT {84 if args.transitions and args.case=="completion" else CASES[args.case]}\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/('build/acpi-air61-battery/vm.elf' if args.air61 else 'build/acpi-battery/vm.elf');initramfs=root/'build/initramfs.tar'
        cmdline='srvros.storage=ram srvros.workspace-volume=51525354 srvros.desktop-release=0123456789abcdef srvros.capture=battery-home-desktop srvros.capture-failure=reboot'
        if args.transitions:cmdline=cmdline.replace('battery-home-desktop','battery-home-transitions')
        if args.work:cmdline=cmdline.replace('battery-home-desktop','battery-home-work')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,initramfs,cmdline=cmdline)
        fatal=args.case in ('uncertain-ack','work-overrun')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
             '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors);qmp=None
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp');start=time.monotonic();work_phase=0;consumed=0
                def wait(marker,timeout=60):
                    nonlocal consumed
                    began=time.monotonic()
                    while marker not in output[consumed:]:
                        assert process.poll() is None and time.monotonic()-began<timeout,f'missing {marker!r}'
                        output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':'sendkey '+keys});time.sleep(.3)
                def type_text(text):
                    mapping={' ':('spc',False),'/':('slash',False),'.':('dot',False),'-':('minus',False)}
                    for c in text:
                        key,shift=mapping.get(c,(c.lower(),c.isupper()));qmp.key(key,shift);output.extend(read_for(serial,.001))
                while process.poll() is None and time.monotonic()-start<(550 if args.case=='work-overrun' else 420):
                    output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                    qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                    if args.work and args.case in ('completion','early-exit','work-overrun') and not work_phase and b'desktop-work: begin ' in output:
                        wait(b'desktools: starting role=text');wait(b'deskd: opened pid=');time.sleep(1)
                        type_text('Attended battery work.');shortcut('ctrl-l');shortcut('ctrl-a')
                        type_text('/work/srvros-work/work-proof.txt');shortcut('ctrl-s');wait(b'picker ready mode=2')
                        qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                        shortcut('ctrl-o');wait(b'picker ready mode=1');type_text('work-proof');qmp.key('ret');wait(b'file-result role=text op=1 status=0')
                        shortcut('meta_l-spc');type_text('Files');qmp.key('ret');wait(b'file-result role=files op=3 status=0')
                        qmp.mouse(20,20);work_phase=1
                        if args.case=='work-overrun':
                            shortcut('meta_l-spc');type_text('Text window');qmp.key('ret');time.sleep(.5);shortcut('ctrl-end');type_text(' Unsaved work.')
                        if args.case=='early-exit':shortcut('f10');shortcut('end');shortcut('ret');work_phase=2
                    if args.work and args.case=='completion' and work_phase==1 and b'desktop-work: ready-to-exit ' in output:
                        shortcut('f10');shortcut('end');shortcut('ret');work_phase=2
                    if fatal and b'battery: HALTED ' in output:break
                output.extend(read_for(serial,.1));data=bytes(output)
                assert (process.poll() is None if fatal else process.poll()==0),data[-6000:]
                result=dict(status='PASS',case=args.case,transitions=args.transitions,work=args.work,native_acceptance=False)
                if fatal:
                    if args.case=='work-overrun':
                        assert b'battery: HALTED attended desktop deadline; save work before exit' in data
                        assert b'deskd: exited' not in data and b'desktop-work: completed=' not in data
                    else:assert re.search(rb'^battery: HALTED (?:SMBus event [^\r\n]*|sample errors=0 denied=0 heap=[1-9]\d* ec_phase=7 ec_error=1 queries=1); live state retained\r?$',data,re.M)
                    for forbidden in (b'battery-workspace-hold: released',b'battery-service: retired ',b'laptop-capture: completed=',b'laptop-capture: returning',b'workspace: unmounted'):
                        assert forbidden not in data
                    result.update(automatic_reset=False,runtime_retired=False,storage_cleanup='not claimed')
                else:
                    assert data.count(b'laptop-capture: saved=yes unmounted=yes')==1
                    assert data.index(b'laptop-capture: saved=yes unmounted=yes')<data.index(b'laptop-capture: returning to Ubuntu')
                    disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
                    final=ExfatFixture(disk[2048*512:])
                    assert final.read('srvros-work/.workspace')==MARKER and final.read('srvros-work/anchor.txt')==ANCHOR
                    assert {n for n,_ in final.entries(final.chain(final.root))}=={'srvros-work','.srvros'}
                    expected={'.workspace','anchor.txt','Logs'}
                    if args.work and args.case in ('completion','early-exit'):
                        expected.add('work-proof.txt');assert final.read('srvros-work/work-proof.txt')==b'Attended battery work.'
                    if args.case in ('completion','no-transition'):expected.add('Screenshots')
                    assert {n for n,_ in final.entries(final.read('srvros-work'))}==expected
                    sessions=list(final.entries(final.read('srvros-work/Logs')));assert len(sessions)==1
                    path='srvros-work/Logs/'+sessions[0][0];logs={n for n,_ in final.entries(final.read(path))}
                    assert {'boot.log','exit.log'}<=logs<= {'boot.log','exit.log','session-a.log','session-b.log'}
                    saved=final.read(path+'/exit.log');(out/'exit.log').write_bytes(saved)
                    if args.case=='completion':
                        assert logs=={'boot.log','exit.log','session-a.log','session-b.log'}
                        result.update(check_capture(saved,vm=True,transitions=args.transitions,work=args.work))
                        shots=list(final.entries(final.read('srvros-work/Screenshots')));assert len(shots)==(3 if args.transitions else 1)
                        images=tmp/'collected-home/Screenshots';images.mkdir(parents=True)
                        for index,(name,_) in enumerate(shots):
                            bmp=final.read('srvros-work/Screenshots/'+name);assert bmp[:2]==b'BM'
                            assert struct.unpack_from('<ii',bmp,18)==(1280,800)
                            (out/f'desktop-{index}.bmp').write_bytes(bmp)
                            (images/name).write_bytes(bmp)
                        # Use the physical collector's strict receipt + image checks,
                        # including normal non-traced attended desktops.
                        result['screenshot']=verify_home_desktop_screenshot(images.parent,saved.decode(),out,3 if args.transitions else 1)
                    else:
                        assert (b'battery-home-work-capture' if args.work else b'battery-home-transitions-capture' if args.transitions else b'battery-home-desktop-capture')+b': returned=yes clean=NO' in saved
                        assert b'laptop-capture: completed=NO' in saved and b'battery-controls: PASS' not in saved
                        assert b'battery-home-desktop: logger_retired=yes final_deferred=yes' in saved
                        assert saved.index(b'battery-desktop-lifetime: drained')<saved.index(b'battery-desktop-lifetime: released')<saved.index(b'battery-workspace-hold: released')
                    if args.case=='no-transition':
                        assert b'desktop-ac: timeout-or-invalid phase=1' in saved
                        assert len(list(final.entries(final.read('srvros-work/Screenshots'))))==1
                    volume.write_bytes(disk[2048*512:]);(out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result.update(automatic_reset=True,exact_tree_metadata='PASS',offline_allocation='PASS',saved_log='PASS')
                result.update(source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),payload_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (kernel,initramfs)})
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                if not (out/'result.json').exists() and qmp and process.poll() is None:
                    try:qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                    except Exception:pass
                stop_process(process);(out/'serial.log').write_bytes(output);(out/'last-workspace.img').write_bytes(stick.read_bytes())
if __name__=='__main__':main()
