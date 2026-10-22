#!/usr/bin/env python3
"""VM-only continuous System session; never accepts a physical disk argument."""
import argparse, hashlib, json, os, re, struct, subprocess, tempfile, time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso, read_for
from serial_records import complete_lines
from vm_sockets import connect_unix
from battery_desktop_smoke import chord
from battery_workspace_controls_records import check_hold, check_home_returns
from exfat_test_image import ExfatFixture, check_clean_image


def check_rollups(data, epochs):
    rows=re.findall(rb'^battery-monitor-events: epoch=(\d+) at_us=(\d+) events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) invalidations=(\d+) saturated=(\d+) final=(\d+)\r?$',data,re.M)
    assert len(rows)==len(re.findall(rb'^battery-monitor-events:',data,re.M))
    rows=[tuple(map(int,r)) for r in rows]
    assert [r[0] for r in rows]==sorted(r[0] for r in rows)
    for forbidden in (b'battery: HALTED',b'battery-smbus: FAILED',b'contained fault; returning'):
        assert forbidden not in data
    assert rows and {r[0] for r in rows}==set(range(1,epochs+1))
    retired=list(re.finditer(rb'^battery-service: retired epoch=(\d+) attempts=\d+ rejected=0 queries=(\d+) notifications=\d+ attached=1 detached=1 heap=0 errors=0 denied=0\r?$',data,re.M))
    assert len(retired)==data.count(b'battery-service: retired ')==epochs
    assert [int(r[1]) for r in retired]==list(range(1,epochs+1))
    for epoch in range(1,epochs+1):
        values=[r for r in rows if r[0]==epoch]
        assert values[-1][-1]==1 and sum(r[-1] for r in values)==1
        last=None;previous=(0,)*5
        for _,now,events,alarms,acks,unknown,invalid,saturated,final in values:
            assert events>=alarms==acks==unknown+invalid and not saturated
            totals=(events,alarms,acks,unknown,invalid)
            assert all(a>=b for a,b in zip(totals,previous));previous=totals
            if not final:
                assert last is None or now-last>=5000000
                last=now
        summary=re.findall(rb'^battery-controls-events: epoch='+str(epoch).encode()+rb' events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) .*; drained=yes\r?$',data,re.M)
        assert len(summary)==1 and tuple(map(int,summary[0]))==values[-1][2:6]
        assert int(retired[epoch-1][2])==values[-1][2]
        final=re.search(rb'^battery-monitor-events: epoch='+str(epoch).encode()+rb' .* final=1\r?$',data,re.M)
        summary_at=data.index(b'battery-controls-events: epoch='+str(epoch).encode()+b' ')
        assert final and final.end()<summary_at<retired[epoch-1].start()
    assert b'smbus-e:' not in data and b'smbus-i:' not in data and b'battery-smbus: event=' not in data
    return rows


def check_lifetime(data):
    acquired=list(re.finditer(rb'^battery-desktop-lifetime: acquired token=(\d+) root=(\d+)\r?$',data,re.M))
    drained=list(re.finditer(rb'^battery-desktop-lifetime: drained token=(\d+) members=0 peak=2 revoked=yes\r?$',data,re.M))
    released=list(re.finditer(rb'^battery-desktop-lifetime: released token=(\d+) runtime_retired=yes\r?$',data,re.M))
    assert len(acquired)==len(drained)==len(released)==1
    a,d,r=acquired[0],drained[0],released[0]
    assert int(a[1])>0 and a[1]==d[1]==r[1] and a.end()<d.start()<d.end()<r.start()
    assert r.end()<data.index(b'battery-workspace-hold: released ')
    retire=list(re.finditer(rb'^battery-service: retired .*$',data,re.M))
    if retire:assert retire[-1].end()<r.start()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=('normal','alarm','early-exit','refusal','unmounted-refusal','profile-refusal','query-refusal','runtime-fault','effect-refusal'),default='normal')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('battery-home-monitor-'+args.case);out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    fault={'normal':83,'alarm':76,'profile-refusal':15,'query-refusal':75,'runtime-fault':79,'effect-refusal':82}.get(args.case,76)
    fatal=args.case in ('runtime-fault','effect-refusal');refused=args.case in ('refusal','unmounted-refusal')
    unavailable=args.case=='profile-refusal';output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-home-monitor-') as folder:
        tmp=Path(folder)
        (tmp/'battery.asl').write_text(f'#define BATTERY_FAULT {fault}\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes())
        fixture.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 51525354\n','anchor.txt':b'Preserve continuous Home session\n'})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as f:f.write(mbr);f.seek(2048*512);f.write(fixture.data)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='refusal' else 'vm.elf')
        symbols=subprocess.check_output(['nm','-a',str(kernel)],text=True)
        uart=re.search(r'^([0-9a-f]+) [a-zA-Z] serial_ready$',symbols,re.M)
        assert uart
        def inspect_uart(qmp):
            return qmp.call('human-monitor-command',{'command-line':'x /1bx 0x'+uart[1]})
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,root/'build/initramfs.tar')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
             '-device','qemu-xhci,id=xhci','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp');start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output
                if args.case!='unmounted-refusal':
                    serial.sendall(b'workspace mount 51525354\n');mounted=bytearray();due=time.monotonic()+15
                    while b'workspace: ready at /work/srvros-work' not in mounted and time.monotonic()<due:mounted.extend(read_for(serial,.1))
                    output.extend(mounted);assert b'workspace: ready at /work/srvros-work' in mounted
                serial.sendall(b'acpiruntime battery-home-monitor\n');data=bytearray();begin=time.monotonic();phase=0;pointer=[24,56];last_size=0;last_progress=begin;inspected=False
                def click(x,y):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,1);qmp.mouse(0,0,0)
                while time.monotonic()-begin<(600 if args.case=="normal" else 180):
                    data.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output+data)
                    if len(data)!=last_size:last_size=len(data);last_progress=time.monotonic()
                    if not inspected and time.monotonic()-last_progress>15:
                        (out/'stalled-registers.txt').write_text(str(qmp.call('human-monitor-command',{'command-line':'info registers'})))
                        (out/'stalled-uart.txt').write_text(str(inspect_uart(qmp)))
                        qmp.call('screendump',{'filename':str(out/'stalled.ppm')});inspected=True
                    if fatal and b'battery: HALTED' in data:break
                    if args.case in ('normal','alarm'):
                        times=[int(n) for n in re.findall(rb'battery-monitor-events: epoch=1 at_us=(\d+)',data)]
                        elapsed=bool(times) and times[-1]-times[0]>=120000000
                        if phase==0 and (elapsed if args.case=='normal' else time.monotonic()-begin>12) and b'battery-ui: state=2 epoch=1 generation=2' in data:
                            qmp.call('screendump',{'filename':str(out/'active.ppm')});click(306,518);phase=1
                        elif phase==1 and b'battery-ui: state=4 epoch=1' in data:click(113,518);phase=2
                        elif phase==2 and b'battery-ui: state=2 epoch=2 generation=2' in data:click(770,85);phase=3
                    elif not phase and args.case=='early-exit' and b'battery-ui: state=2 epoch=1 generation=1' in data:
                        chord(qmp,'esc','ctrl','alt');phase=1
                    elif not phase and ((unavailable and b'battery-ui: state=5' in data) or (args.case=='query-refusal' and b'battery-ui: state=6' in data)):
                        click(770,85);phase=1
                    if (b'battery-workspace-controls: complete=' in complete_lines(data) or b'REFUSED VM-only session' in data or b'REFUSED explicit Home ownership' in data) and b'srv> ' in data:break
                output.extend(data)
                result=dict(case=args.case,native_acceptance='disabled',elapsed_host_seconds=time.monotonic()-begin,service_outcome='query-refused' if args.case=='query-refusal' else 'unavailable' if unavailable else args.case)
                if fatal:
                    assert process.poll() is None and b'battery: HALTED' in data
                    assert b'battery-workspace-hold: acquired' in data
                    for forbidden in (b'battery-workspace-hold: released',b'battery-home-return:',b'battery-workspace-controls: complete=',b'workspace: unmounted',b'contained fault; returning'):
                        assert forbidden not in data,forbidden
                    result.update(status='PASS',expected='uncertain ownership retained; no reset or storage cleanup')
                else:
                    serial.sendall(b'dmesg 65536\necho HOME_MONITOR_DUMP_END\n');snapshot=bytearray();due=time.monotonic()+15
                    while not re.search(rb'^HOME_MONITOR_DUMP_END\r?$',complete_lines(snapshot),re.M) and time.monotonic()<due:snapshot.extend(read_for(serial,.1))
                    output.extend(snapshot);assert re.search(rb'^HOME_MONITOR_DUMP_END\r?$',complete_lines(snapshot),re.M)
                    data=complete_lines(snapshot);(out/'boot.log').write_bytes(data)
                    if refused:
                        assert b'REFUSED '+(b'VM-only session' if args.case=='refusal' else b'explicit Home ownership') in data
                        assert b'battery-monitor: auto_start' not in data and b'battery-workspace-hold: acquired' not in data
                    else:
                        assert b'battery-monitor: auto_start=1 native=disabled' in data
                        assert b'; revoked=yes retired=yes' in data
                        assert b'retired=yes mounted=yes native=explicit' in data
                        check_hold(data,False if unavailable else None,automatic_start=True)
                        check_lifetime(data)
                        epochs=2 if args.case in ('normal','alarm') else 0 if unavailable else 1
                        check_home_returns(data,epochs,refused=args.case=='query-refusal',continuous=True)
                        if args.case in ('normal','alarm'):
                            assert b'battery-controls: PASS requests=2 descendants=2 epoch=2; revoked=yes retired=yes' in data
                        if args.case in ('normal','alarm','query-refusal'):
                            rows=check_rollups(data,epochs);result['rollups']=rows
                            assert b'battery-controls: PASS' in data
                            if args.case=='normal':
                                first=[r for r in rows if r[0]==1]
                                assert first[-1][2]>512 and first[-1][1]-first[0][1]>110000000
                                assert phase==3 and len(first)<30
                            elif args.case=='alarm':assert sum(r[6] for r in rows if r[-1])>0 and phase==3
                            else:assert b'battery-query-return:' in data and b'battery-ui: state=6' in data
                    # Replacing the compositor and unmounting prove admission was revoked.
                    serial.sendall(b'run /fat/bin/deskd --smoke\n');replacement=bytearray();due=time.monotonic()+30
                    while b'run: pid=' not in replacement and time.monotonic()<due:replacement.extend(read_for(serial,.1))
                    output.extend(replacement);assert b'deskd: exited' in replacement and b'exited status=0' in replacement
                    if args.case!='unmounted-refusal':
                        serial.sendall(b'workspace unmount\n');unmount=bytearray();due=time.monotonic()+15
                        while b'workspace: unmounted; USB writes disabled' not in unmount and time.monotonic()<due:unmount.extend(read_for(serial,.1))
                        output.extend(unmount);assert b'workspace: unmounted; USB writes disabled' in unmount
                    stop_process(process);disk=stick.read_bytes()
                    assert disk[:512]==mbr and not any(disk[512:2048*512]) and disk[2048*512:]==bytes(fixture.data)
                    volume.write_bytes(disk[2048*512:]);(out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result.update(status='PASS',unchanged_disk=True,offline_allocation='PASS',replacement_compositor='PASS')
                result.update(kernel_sha256=hashlib.sha256(kernel.read_bytes()).hexdigest(),initramfs_sha256=hashlib.sha256((root/'build/initramfs.tar').read_bytes()).hexdigest())
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                if not (out/'result.json').exists() and process.poll() is None:
                    try:
                        (out/'failure-registers.txt').write_text(str(qmp.call('human-monitor-command',{'command-line':'info registers'})))
                        qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                    except Exception as error:(out/'failure-inspection.txt').write_text(repr(error))
                (out/'serial.log').write_bytes(output);stop_process(process);(out/'last-workspace.img').write_bytes(stick.read_bytes())

if __name__=='__main__':main()
