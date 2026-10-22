#!/usr/bin/env python3
"""VM-only System controls with mounted disposable Home and SMBus events."""
import argparse,hashlib,json,os,re,struct,subprocess,tempfile,time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from serial_records import complete_lines
from vm_sockets import connect_unix
from battery_desktop_smoke import chord

from battery_workspace_controls_records import check,check_hold,check_events,check_home_returns
from exfat_test_image import ExfatFixture,check_clean_image

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=('normal','manual','early-exit','refusal','unmounted-refusal','profile-refusal','runtime-fault'),default='normal')
    parser.add_argument('--home',action='store_true',help='Exercise the explicit native Home session with disposable fixtures')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/(('battery-home-controls-' if args.home else 'battery-workspace-controls-')+args.case);out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    output=bytearray();result={}
    mode=b'explicit' if args.home else b'disabled'
    unavailable=args.case=='profile-refusal' or (args.home and args.case=='refusal')
    with tempfile.TemporaryDirectory(prefix='srvros-battery-controls-') as folder:
        tmp=Path(folder)
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT '+('79' if args.case=='runtime-fault' else '15' if args.case=='profile-refusal' else '76')+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes())
        fixture.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 51525354\n','anchor.txt':b'Preserve mounted Home across System lifetimes\n'})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as f:f.write(mbr);f.seek(2048*512);f.write(fixture.data)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='refusal' else 'vm.elf')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,root/'build/initramfs.tar')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot','-device','qemu-xhci,id=xhci','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output
                if args.case!='unmounted-refusal':
                    serial.sendall(b'workspace mount 51525354\n');mounted=bytearray();due=time.monotonic()+15
                    while b'workspace: ready at /work/srvros-work' not in mounted and time.monotonic()<due:mounted.extend(read_for(serial,.1))
                    output.extend(mounted);assert b'workspace: ready at /work/srvros-work' in mounted
                results=[]
                for trial in range(2 if args.case=='normal' else 1):
                    if trial:
                        # A subsequent session must acquire a new lifetime even
                        # when the same USB device and UUID are mounted again.
                        serial.sendall(b'workspace unmount\nworkspace mount 51525354\n')
                        remount=bytearray();due=time.monotonic()+15
                        while b'workspace: ready at /work/srvros-work' not in remount and time.monotonic()<due:remount.extend(read_for(serial,.1))
                        output.extend(remount)
                        assert b'workspace: unmounted; USB writes disabled' in remount and b'workspace: ready at /work/srvros-work' in remount
                    begin=f'BATTERY_CONTROL_TRIAL_{trial}'.encode()
                    serial.sendall(b'echo '+begin+b'\n');marker=bytearray();due=time.monotonic()+5
                    while b'srv> ' not in marker and time.monotonic()<due:marker.extend(read_for(serial,.1))
                    assert begin in marker and b'srv> ' in marker;output.extend(marker)
                    serial.sendall(b'acpiruntime '+(b'battery-home-controls' if args.home else b'battery-workspace-controls')+(b'-manual' if args.case in ('manual','runtime-fault') else b'')+b'\n');data=bytearray();deadline=time.monotonic()+125;interrupted=False;shot=False;manual_phase=0;pointer=[24,56]
                    def click(x,y):
                        qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                        qmp.mouse(0,0,1);qmp.mouse(0,0,0)
                    while time.monotonic()<deadline:
                        data.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output+data)
                        if not shot and b'battery-ui: sampled cycle=1' in data:
                            qmp.call('screendump',{'filename':str(out/'sensors.ppm')});shot=True
                        if args.case in ('manual','runtime-fault'):
                            if manual_phase==0 and b'battery-ui: state=0' in data:
                                data.extend(read_for(serial,.5));click(113,518);manual_phase=1
                            elif args.case=='manual' and manual_phase==1 and b'battery-ui: state=2 epoch=1 generation=2' in data:
                                qmp.call('screendump',{'filename':str(out/'sensors.ppm')});click(306,518);manual_phase=2
                            elif manual_phase==2 and b'battery-ui: state=4 epoch=1' in data:
                                click(113,518);manual_phase=3
                            elif manual_phase==3 and b'battery-ui: state=2 epoch=2 generation=2' in data:
                                # Close while ACTIVE: the session owner must revoke
                                # admission and retire the service itself.
                                click(770,85);manual_phase=4
                        if args.case=='early-exit' and not interrupted and b'battery-ui: request=1 result=0' in data:
                            chord(qmp,'esc','ctrl','alt');interrupted=True
                        if args.case=='runtime-fault' and (b'battery: HALTED' in data or process.poll() is not None):break
                        if (b'battery-workspace-controls: complete=' in complete_lines(data) or b'battery-workspace-controls: REFUSED' in data) and b'srv> ' in data:break
                    output.extend(data)
                    if args.case!='runtime-fault':
                        # Short records are atomic in the boot log. UART mirroring
                        # intentionally allows interleaving to keep interrupts on.
                        # Dump only after clients/runtime retire; use the same
                        # authoritative records that hardware captures persist.
                        end=f'BATTERY_CONTROL_DUMP_END_{trial}'.encode()
                        serial.sendall(b'dmesg 65536\necho '+end+b'\n')
                        snapshot=bytearray();due=time.monotonic()+15
                        while not re.search(rb'^'+end+rb'\r?$',complete_lines(snapshot),re.M) and time.monotonic()<due:
                            snapshot.extend(read_for(serial,.1))
                        assert re.search(rb'^'+end+rb'\r?$',complete_lines(snapshot),re.M)
                        output.extend(snapshot)
                        starts=list(re.finditer(rb'^'+begin+rb'\r?\n',snapshot,re.M));assert len(starts)==1
                        data=bytes(snapshot[starts[0].end():])
                        (out/f'trial-{trial}.boot.log').write_bytes(data)
                    if args.case=='normal':results.append(check(data,trial*4,trial*2,home=args.home))
                    elif args.case=='manual':
                        assert b'battery-workspace-controls: complete=yes epoch=2 state=4; revoked=yes retired=yes mounted=yes native='+mode in data
                        assert manual_phase==4
                        assert b'battery-controls: PASS requests=3 descendants=2 epoch=2; revoked=yes retired=yes' in data
                        assert len(re.findall(rb'battery-ui: request=[12] result=0',data))==3
                        assert len(re.findall(rb'battery-service: retired epoch=[12] .*heap=0 errors=0 denied=0',data))==2
                        result=dict(status='PASS',mouse_buttons='start/stop/restart',active_window_close='service retired; scope revoked')
                    elif args.case=='runtime-fault':
                        tail=read_for(serial,1);data+=tail;output.extend(tail)
                        assert process.poll() is None
                        assert manual_phase==1 and b'battery-ui: state=2 epoch=1 generation=2' in data
                        assert b'battery: HALTED SMBus event' in data
                        assert b'contained fault; returning' not in data and b'workspace: unmounted' not in data
                        assert b'battery-workspace-controls: complete=' not in data
                        assert b'battery-controls: PASS' not in data and b'; retired=yes' not in data
                        assert b'battery-workspace-hold: acquired token=1' in data
                        assert b'battery-workspace-hold: released' not in data
                        result=dict(status='PASS',expected='live fault contained; no clean retirement claimed',automatic_reset=False,storage_retirement='NOT claimed')
                    elif args.case=='unmounted-refusal' or (args.case=='refusal' and not args.home):
                        assert b'battery-workspace-controls: REFUSED '+(b'explicit Home ownership' if args.home else b'VM-only mounted study') in data
                        assert b'battery-service:' not in data and b'battery-native:' not in data and b'battery-control: dispatched' not in data
                        result=dict(status='PASS',expected=args.case,runtime_started=False)
                    else:
                        assert b'battery-controls: FAIL' in data and b'revoked=yes retired=yes' in data
                        assert b'battery-controls: PASS' not in data
                        assert b'battery-workspace-controls: complete=NO' in data and b'retired=yes mounted=yes native='+mode in data
                        if args.case=='early-exit':assert b'battery-service: retired epoch=1' in data
                        if unavailable:
                            reason=(b'battery: REFUSED platform/resource ownership preflight' if args.case=='refusal' else b'battery-native-smbus: REFUSED captured controller profile') if args.home else b'battery-smbus-study: REFUSED model ownership profile'
                            assert reason in data and b'battery-smbus: event=' not in data
                        result=dict(status='PASS',expected=args.case,scope_revoked=True)
                    if args.case in ('manual','early-exit') or unavailable:
                        result['mount_token']=check_hold(data,True if args.case=='manual' else False if unavailable else None)
                    if args.home and args.case in ('manual','early-exit','profile-refusal','refusal'):
                        epochs=2 if args.case=='manual' else 1 if args.case=='early-exit' else 0
                        check_home_returns(data,epochs)
                        if args.case=='manual':check_events(data,0,2)
                if args.case=='normal':
                    assert [r['mount_token'] for r in results]==[1,2]
                    result=dict(status='PASS',runs=results,remount_new_lifetime=True)
                if args.case!='runtime-fault':
                    serial.sendall(b'run /fat/bin/deskd --smoke\n');data=bytearray();deadline=time.monotonic()+30
                    while time.monotonic()<deadline and b'run: pid=' not in data:data.extend(read_for(serial,.1))
                    output.extend(data);assert b'deskd: exited' in data and b'exited status=0' in data
                    result['replacement_compositor']='PASS'
                if args.case!='runtime-fault':
                    if args.case!='unmounted-refusal':
                        serial.sendall(b'workspace unmount\n');data=bytearray();deadline=time.monotonic()+15
                        while time.monotonic()<deadline and b'workspace: unmounted; USB writes disabled' not in data:data.extend(read_for(serial,.1))
                        output.extend(data);assert b'workspace: unmounted; USB writes disabled' in data
                    stop_process(process);disk=stick.read_bytes()
                    assert disk[:512]==mbr and not any(disk[512:2048*512])
                    assert disk[2048*512:]==bytes(fixture.data),'Mounted lifecycle altered workspace bytes'
                    volume.write_bytes(disk[2048*512:])
                    (out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result.update(unchanged_disk=True,offline_allocation='PASS')
                result.update(kernel_sha256=hashlib.sha256(kernel.read_bytes()).hexdigest(),initramfs_sha256=hashlib.sha256((root/'build/initramfs.tar').read_bytes()).hexdigest(),native_acceptance='pending' if args.home else 'disabled')
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                if not (out/'serial.log').exists() or len(output)>=(out/'serial.log').stat().st_size:(out/'serial.log').write_bytes(output)
                stop_process(process)
                (out/'last-workspace.img').write_bytes(stick.read_bytes())

if __name__=='__main__':main()
