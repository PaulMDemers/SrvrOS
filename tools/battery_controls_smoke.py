#!/usr/bin/env python3
"""Exercise explicit System battery controls through real syscalls in Q35."""
import argparse,json,os,re,subprocess,tempfile,time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from serial_records import complete_lines
from vm_sockets import connect_unix
from battery_desktop_smoke import chord


def check(data,serial_base=0,epoch_base=0):
    data=complete_lines(data)
    assert b'battery-ui: FAIL' not in data and b'battery-controls: FAIL' not in data
    requests=re.findall(rb'^battery-ui: request=(\d+) result=(-?\d+) serial=(\d+) epoch=(\d+)\r?$',data,re.M)
    expected=[(1,0,serial_base+1,epoch_base),(2,0,serial_base+2,epoch_base+1),
              (1,0,serial_base+3,epoch_base+1),(2,0,serial_base+4,epoch_base+2)]
    assert [tuple(map(int,r)) for r in requests]==expected,requests
    dispatched=re.findall(rb'^battery-control: dispatched serial=(\d+) operation=(\d+) epoch=(\d+) result=(-?\d+)\r?$',data,re.M)
    assert [tuple(map(int,r)) for r in dispatched]==[(s,o,e,0) for o,_,s,e in expected],dispatched
    sampled=re.findall(rb'^battery-ui: sampled cycle=(\d+) epoch=(\d+) generation=(\d+)\r?$',data,re.M)
    assert len(sampled)==2 and [(int(c),int(e)) for c,e,_ in sampled]==[(1,epoch_base+1),(2,epoch_base+2)]
    assert all(int(g)>=2 for _,_,g in sampled)
    stopped=re.findall(rb'^battery-ui: stopped cycle=(\d+) epoch=(\d+)\r?$',data,re.M)
    assert [(int(c),int(e)) for c,e in stopped]==[(1,epoch_base+1),(2,epoch_base+2)]
    assert data.count(b'battery-ui: PASS cycles=2 requests=4 stopped=yes')==1
    assert data.count(f'battery-controls: PASS requests=4 descendants=2 epoch={epoch_base+2}; revoked=yes retired=yes'.encode())==1
    desktop=re.findall(rb'^desktop-controls: completed=yes reaped=1 errors=0 cached_reads=(\d+)\r?$',data,re.M)
    assert len(desktop)==1 and int(desktop[0])>=2
    retired=re.findall(rb'^battery-service: retired epoch=(\d+) attempts=(\d+) rejected=0 queries=\d+ notifications=\d+ attached=1 detached=1 heap=0 errors=0 denied=0\r?$',data,re.M)
    assert [(int(e),int(n)>=2) for e,n in retired]==[(epoch_base+1,True),(epoch_base+2,True)]
    for prefix,count in ((b'battery-ui: request=',4),(b'battery-control: dispatched ',4),
                         (b'battery-ui: sampled ',2),(b'battery-ui: stopped ',2),
                         (b'battery-ui: PASS',1),(b'battery-controls:',1),
                         (b'desktop-controls:',1),(b'battery-service: retired ',2)):
        assert sum(r.startswith(prefix) for r in data.splitlines())==count
    return dict(status='PASS',requests=4,lifetimes=2,cached_reads=int(desktop[0]),final_epoch=epoch_base+2,physical_acceptance='pending')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=('normal','manual','early-exit','refusal','runtime-fault'),default='normal')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('battery-controls-'+args.case);out.mkdir(parents=True,exist_ok=True)
    output=bytearray();result={}
    with tempfile.TemporaryDirectory(prefix='srvros-battery-controls-') as folder:
        tmp=Path(folder)
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT '+('27' if args.case=='runtime-fault' else '15')+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='refusal' else 'vm.elf')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,root/'build/initramfs.tar')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output
                results=[]
                for trial in range(2 if args.case=='normal' else 1):
                    begin=f'BATTERY_CONTROL_TRIAL_{trial}'.encode()
                    serial.sendall(b'echo '+begin+b'\n');marker=bytearray();due=time.monotonic()+5
                    while b'srv> ' not in marker and time.monotonic()<due:marker.extend(read_for(serial,.1))
                    assert begin in marker and b'srv> ' in marker;output.extend(marker)
                    serial.sendall(b'acpiruntime battery-controls-manual\n' if args.case in ('manual','runtime-fault') else b'acpiruntime battery-controls\n');data=bytearray();deadline=time.monotonic()+125;interrupted=False;shot=False;manual_phase=0;pointer=[24,56]
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
                        if args.case=='runtime-fault' and process.poll() is not None:break
                        if b'battery-controls:' in complete_lines(data) and b'srv> ' in data:break
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
                    if args.case=='normal':results.append(check(data,trial*4,trial*2))
                    elif args.case=='manual':
                        assert manual_phase==4
                        assert b'battery-controls: PASS requests=3 descendants=2 epoch=2; revoked=yes retired=yes' in data
                        assert len(re.findall(rb'battery-ui: request=[12] result=0',data))==3
                        assert len(re.findall(rb'battery-service: retired epoch=[12] .*heap=0 errors=0 denied=0',data))==2
                        result=dict(status='PASS',mouse_buttons='start/stop/restart',active_window_close='service retired; scope revoked')
                    elif args.case=='runtime-fault':
                        assert process.wait(timeout=5)==0
                        assert manual_phase==1 and b'battery-ui: state=2 epoch=1 generation=2' in data
                        assert b'battery: FAILED operation deadline' in data
                        assert b'contained fault; returning to Ubuntu without live-runtime cleanup; final report unavailable' in data
                        assert b'battery-controls: PASS' not in data and b'; retired=yes' not in data
                        result=dict(status='PASS',expected='live fault contained; no clean retirement claimed',automatic_reset=True)
                    else:
                        assert b'battery-controls: FAIL' in data and b'revoked=yes retired=yes' in data
                        assert b'battery-controls: PASS' not in data
                        if args.case=='early-exit':assert b'battery-service: retired epoch=1' in data
                        result=dict(status='PASS',expected=args.case,scope_revoked=True)
                if args.case=='normal':result=dict(status='PASS',runs=results)
                if args.case!='runtime-fault':
                    serial.sendall(b'run /fat/bin/deskd --smoke\n');data=bytearray();deadline=time.monotonic()+30
                    while time.monotonic()<deadline and b'run: pid=' not in data:data.extend(read_for(serial,.1))
                    output.extend(data);assert b'deskd: exited' in data and b'exited status=0' in data
                    result['replacement_compositor']='PASS'
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                if not (out/'serial.log').exists() or len(output)>=(out/'serial.log').stat().st_size:(out/'serial.log').write_bytes(output)
                stop_process(process)

if __name__=='__main__':main()
