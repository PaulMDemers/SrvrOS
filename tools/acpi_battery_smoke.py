#!/usr/bin/env python3
"""Run the battery diagnostic runtime with a disposable Q35 EC model."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import struct
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from vm_sockets import connect_unix
from serial_records import complete_lines



def check_sci_records(part):
    rows=[]
    pattern=rb"battery-sci: snapshot=(ready|retired|fault) epoch=(\d+) phase=(setup|active|retire) delivered=(\d+) handled=(\d+) unclaimed=(\d+) in_flight=(\d+) first_unclaimed_us=(\d+) last_unclaimed_us=(\d+)"
    keys=('snapshot','epoch','phase','delivered','handled','unclaimed','in_flight','first','last')
    for match in re.findall(pattern,part):
        row={k:(v.decode() if k in ('snapshot','phase') else int(v)) for k,v in zip(keys,match)}
        assert row['delivered']==row['handled']+row['unclaimed']+row['in_flight'],row
        if row['snapshot']=='retired':assert row['in_flight']==0,row
        if row['unclaimed']:assert 0<row['first']<=row['last'],row
        else:assert row['first']==row['last']==0,row
        rows.append(row)
    # Validate the bounded event/controller observer ring and
    # reconcile completed callbacks with the existing phase counters.
    headers=re.findall(rb"battery-sci-observe: snapshot=(ready|retired|fault) epoch=(\d+) total=(\d+) retained=(\d+) overwritten=(\d+) ec_mask=(0x[0-9a-fA-F]+)",part)
    events=re.findall(rb"battery-sci-event: snapshot=(ready|retired|fault) epoch=(\d+) seq=(\d+) phase=(\d+) handled=(\d+) at_us=(\d+) duration_us=(\d+) previous_valid=(\d+) previous_handled=(\d+) gap_us=(\d+) query_before=(\d+) query_after=(\d+) valid=(0x[0-9a-fA-F]+) fixed_status=(0x[0-9a-fA-F]+) fixed_enable=(0x[0-9a-fA-F]+) ec_status=(0x[0-9a-fA-F]+) ec_enable=(0x[0-9a-fA-F]+) ec_raw_status=(0x[0-9a-fA-F]+) ec_raw_enable=(0x[0-9a-fA-F]+)",part)
    controllers=re.findall(rb"battery-sci-controller: snapshot=(ready|retired|fault) epoch=(\d+) seq=(\d+) point=(before|after|post_eoi) valid=(\d+) vector=(\d+) gsi=(\d+) lapic_bits=(\d+) lint0=(0x[0-9a-fA-F]+) route_low=(0x[0-9a-fA-F]+) route_high=(0x[0-9a-fA-F]+) pic_masks=(0x[0-9a-fA-F]+)",part)
    assert len(controllers)==part.count(b'battery-sci-controller:'),'Malformed controller record'
    assert len(controllers)==3*len(events),'Missing or extra controller record'
    returns=re.findall(rb"battery-sci-return: snapshot=(ready|retired|fault) epoch=(\d+) seq=(\d+) observed=(\d+) elapsed_us=(\d+) query=(\d+)",part)
    assert len(returns)==part.count(b'battery-sci-return:')==len(events),'Missing/malformed completion record'
    for event in events:
        pair=[c for c in controllers if c[:3]==event[:3]]
        assert [c[3] for c in pair]==[b'before',b'after',b'post_eoi'],pair
        assert pair[0][5:7]==pair[1][5:7]==pair[2][5:7],pair # Stable owned vector/GSI identity.
        completion=[r for r in returns if r[:3]==event[:3]]
        assert len(completion)==1,completion
        r=completion[0];observed,elapsed,query=map(int,r[3:])
        assert observed<=1 and query<=1,r
        if event[0]!=b'fault':assert observed==1,r
        if observed:assert elapsed>=int(event[6]),r
        else:assert elapsed==query==int(pair[2][4])==0,r
        for c in pair:
            valid,vector,gsi,bits=map(int,c[4:8])
            lint,low,high,masks=(int(v,16) for v in c[8:])
            assert valid<=7 and 32<=vector<255 and gsi<=0xffffffff and bits<=7,c
            assert max(lint,low,high)<=0xffffffff and masks<=0xffff,c
            if valid&2:assert low&255==vector,c
            if not valid&1:assert bits==lint==0,c
            if not valid&2:assert low==high==0,c
            if not valid&4:assert masks==0,c
    for snapshot,epoch,total,retained,overwritten,mask in headers:
        total,retained,overwritten=int(total),int(retained),int(overwritten)
        assert retained==min(total,8) and overwritten==total-retained
        group=[r for r in rows if r['snapshot']==snapshot.decode() and r['epoch']==int(epoch)]
        assert len(group)==3 and total==sum(r['handled']+r['unclaimed'] for r in group),group
        captured=[r for r in events if r[0]==snapshot and r[1]==epoch]
        assert len(captured)==retained,(headers,events)
        assert [int(r[2]) for r in captured]==list(range(total-retained+1,total+1))
        for r in captured:
            assert int(r[3])<=2 and int(r[4])<=1 and int(r[5])>0
            assert all(int(r[i])<=1 for i in (7,8,10,11)) and int(r[12],16)<=15
    if rows:assert len(headers)==len(rows)//3,'Missing passive SCI observer records'
    return rows


def check_io_hold(part,duration):
    records=re.findall(rb'battery-vm: io-hold admitted=1 sampling=1 requested_us=(\d+) elapsed_us=(\d+) reads=(\d+) max_read_us=(\d+) commands=(\d+) first_gen=(\d+) last_gen=(\d+)',part)
    assert len(records)==1,bytes(part)
    requested,elapsed,reads,maximum,commands,first,last=map(int,records[0])
    assert requested==duration and elapsed>=duration and reads>0 and maximum<=elapsed,records
    assert b'battery-vm: ERROR' not in part,bytes(part)
    if duration>=250000:assert commands>0,records
    if duration>=5000000:assert last>first,records


def verify_scratch_disk(stick,before,offset,path,volume,out,cycles):
    from exfat_test_image import ExfatFixture,check_clean_image
    after=stick.read_bytes()
    assert len(after)==len(before) and 0<offset<=len(before)-4096
    assert after[:offset]==before[:offset] and after[offset+4096:]==before[offset+4096:], 'Changed bytes outside scratch data'
    volume.write_bytes(after[2048*512:])
    (out/'usb-fsck.txt').write_text(check_clean_image(volume))
    stored=ExfatFixture(volume.read_bytes()).read(path)
    assert stored==after[offset:offset+4096] and len(stored)==4096
    if cycles is not None:assert stored==bytes((i+cycles-1)&255 for i in range(4096))
    else:assert stored!=before[offset:offset+4096], 'Fault test did not exercise writes'
    (out/'usb-proof.bin').write_bytes(stored)
    print('acpi-battery: scratch data verified offline; all other disk bytes unchanged; exFAT clean')


def verify_filesystem_tree(data, cycles):
    from exfat_test_image import ExfatFixture
    fs = ExfatFixture(data)
    folder = 'srvros-work/fs-proof'
    assert {name for name, _ in fs.entries(fs.read(folder))} == {'payload.bin', 'empty.bin'}
    assert fs.read(folder+'/payload.bin') == bytes((i*37+(i>>8)+cycles-1)&255 for i in range(262181))
    assert fs.read(folder+'/empty.bin') == b''
    assert fs.read('srvros-work/anchor.txt') == b'Unchanged battery filesystem anchor\n'
    assert fs.read('srvros-work/.workspace') == b'SRVROS-WORKSPACE-V1 51525354\n'


def verify_isolated_disk(stick, before, offset, volume, out, cycles):
    from battery_filesystem_image import verify, SIZE
    from exfat_test_image import check_clean_image
    after = stick.read_bytes()
    assert after[:offset] == before[:offset] and after[offset+SIZE:] == before[offset+SIZE:]
    with stick.open('rb') as stream:
        stream.seek(2048*512); volume.write_bytes(stream.read())
    (out/'usb-fsck.txt').write_text(check_clean_image(volume))
    image = out/'inner.exfat'
    image.write_bytes(after[offset:offset+SIZE])
    if cycles is None:
        # The storage caller may already have run while the service initialized.
        # Preserve the interrupted inner image; recover a separate disposable copy.
        failed=image
        image=out/'inner-recovered.exfat';image.write_bytes(failed.read_bytes())
        recovery=subprocess.run(['/usr/sbin/fsck.exfat','-y','-s',str(image)],capture_output=True,text=True)
        (out/'usb-image-recovery.txt').write_text(recovery.stdout+recovery.stderr)
        assert recovery.returncode in (0,1),recovery.stdout+recovery.stderr
    (out/'usb-image-fsck.txt').write_text(check_clean_image(image))
    driver = Path(__file__).resolve().parents[1]/'build/tests/exfat-driver'
    for name, target in (('outer', volume), ('inner', image)):
        result = subprocess.run([str(driver), str(target), 'allocation-check'],capture_output=True,text=True)
        (out/f'usb-{name}-allocation.txt').write_text(result.stdout+result.stderr)
        assert result.returncode == 0,result.stdout+result.stderr
    if cycles is not None:
        verify(image.read_bytes(), cycles)
        from hardware_desktop import verify_battery_image
        with tempfile.TemporaryDirectory(prefix='collector-',dir=out) as directory:
            source=Path(directory)
            (source/'battery-fs.img').write_bytes(image.read_bytes())
            saved=out/'collector';saved.mkdir(exist_ok=True)
            verify_battery_image(source,{'cycles':cycles},saved)
    else:
        from battery_filesystem_image import ANCHOR,MARKER
        from exfat_test_image import ExfatFixture
        recovered=ExfatFixture(image.read_bytes())
        assert recovered.read('fixture/anchor.txt')==ANCHOR
        assert recovered.read('fixture/.battery-test')==MARKER
    print('acpi-battery: isolated filesystem verified; every outside byte unchanged')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=('normal', 'denied', 'transport', 'storm', 'deadline', 'query-deadline', 'disconnect-deadline', 'foreign-event', 'foreign-restore-fail', 'foreign-mask-fail', 'foreign-reenable', 'mode-button', 'mode-unexpected', 'transitions', 'transition-timeout', 'native-refusal', 'service', 'service-stale', 'service-incoherent', 'service-refusal', 'service-query-deadline', 'service-setup-load', 'service-auto-deadline', 'service-sci-unclaimed', 'service-io-short', 'service-io-long', 'service-io-sustained', 'service-usb-io', 'service-usb-read', 'service-usb-read-auto-deadline', 'service-usb-write', 'service-usb-write-auto-deadline','service-filesystem','service-filesystem-write-failure','service-isolated-filesystem','service-isolated-auto-deadline','service-isolated-active-deadline'), default='normal')
    args = parser.parse_args()
    faults = {'normal': 0, 'denied': 1, 'transport': 2, 'storm': 3, 'deadline': 4, 'query-deadline':5, 'disconnect-deadline':6, 'native-refusal': 0, 'foreign-event': 7, 'foreign-restore-fail': 8, 'foreign-mask-fail': 9, 'foreign-reenable': 10, 'mode-button':11, 'mode-unexpected':12, 'transitions':13, 'transition-timeout':14, 'service':15, 'service-stale':16, 'service-incoherent':17, 'service-refusal':0, 'service-query-deadline':5, 'service-setup-load':18, 'service-auto-deadline':19, 'service-sci-unclaimed':20, 'service-io-short':21, 'service-io-long':22, 'service-io-sustained':23, 'service-usb-io':24, 'service-usb-read':15, 'service-usb-read-auto-deadline':19,'service-usb-write':15,'service-usb-write-auto-deadline':19,'service-filesystem':25,'service-filesystem-write-failure':26,'service-isolated-filesystem':15,'service-isolated-auto-deadline':19,'service-isolated-active-deadline':27}
    transitions=args.case in ('transitions','transition-timeout')
    service=args.case.startswith('service')
    readonly=args.case in ('service-usb-read','service-usb-read-auto-deadline')
    scratch=args.case in ('service-usb-write','service-usb-write-auto-deadline')
    isolated=args.case in ('service-isolated-filesystem','service-isolated-auto-deadline','service-isolated-active-deadline')
    auto_deadline=args.case in ('service-auto-deadline','service-usb-read-auto-deadline','service-usb-write-auto-deadline','service-isolated-auto-deadline','service-isolated-active-deadline')
    filesystem=args.case in ('service-filesystem','service-filesystem-write-failure')
    fs_failure=args.case=='service-filesystem-write-failure'
    usb=args.case=='service-usb-io' or readonly or scratch or filesystem or isolated
    root = Path(__file__).resolve().parents[1]
    out = root/'build/review'/('acpi-battery-' + args.case)
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    if filesystem or isolated:subprocess.run(['make','build/tests/exfat-driver'],cwd=root,check=True)
    with tempfile.TemporaryDirectory(prefix='srvros-battery-') as directory:
        tmp = Path(directory)
        source = (root/'tools/fixtures/acpi/battery-runtime.asl').read_text()
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT ' + str(faults[args.case]) + '\n' + source)
        subprocess.run(['iasl', '-p', str(tmp/'battery'), str(tmp/'battery.asl')], check=True, stdout=subprocess.DEVNULL)
        kernel = root/'build/acpi-battery'/('srvros.elf' if args.case in ('native-refusal','service-refusal') else 'vm.elf')
        iso = boot.build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(), kernel)
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso,
                   '-boot', 'd', '-nic', 'none', '-acpitable', f'file={tmp}/battery.aml',
                   '-serial', f'unix:{tmp}/serial,server=on,wait=off', '-qmp',
                   f'unix:{tmp}/qmp,server=on,wait=off', '-display', 'none',
                   '-monitor', 'none', '-no-reboot']
        if usb:
            from exfat_test_image import ExfatFixture,check_clean_image
            volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
            with volume.open('wb') as stream:stream.truncate(128*1024*1024)
            subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
            fixture=ExfatFixture(volume.read_bytes())
            files={'.workspace':b'SRVROS-WORKSPACE-V1 51525354\n', 'anchor.txt':b'Unchanged battery filesystem anchor\n'}
            if isolated:
                from battery_filesystem_image import create
                inner=tmp/'inner.exfat';create(inner)
                files['battery-fs.img']=inner.read_bytes()
            fixture.add_directory_files('srvros-work',files)
            mbr=bytearray(512);mbr[450]=7
            struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
            with stick.open('wb') as stream:stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
            command+=['-device','qemu-xhci,id=xhci','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                serial = connect_unix(tmp/'serial', 20); serial.settimeout(.05)
                qmp = QMP(tmp/'qmp')
                start = time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start < 60:
                    output.extend(boot.read_for(serial, .2))
                    qmp.boot_if_menu_stalled(output, time.monotonic()-start)
                assert b'srv> ' in output, bytes(output)
                if usb:
                    serial.sendall(b'workspace mount 51525354\n')
                    mounted=bytearray();until=time.monotonic()+30
                    while b'workspace: ready at /work/srvros-work' not in mounted and time.monotonic()<until:
                        mounted.extend(boot.read_for(serial,.1))
                    output.extend(mounted)
                    assert b'workspace: ready at /work/srvros-work' in mounted,bytes(mounted)
                if isolated:
                    serial.sendall(b'acpiruntime battery-filesystem-prepare\n')
                    prepared=bytearray();until=time.monotonic()+30
                    while b'workspace-image: prepared ' not in complete_lines(prepared) and time.monotonic()<until:
                        prepared.extend(boot.read_for(serial,.1))
                    output.extend(prepared)
                    extent=re.search(rb'workspace-image: prepared path=/work/srvros-work/battery-fs.img raw_offset=(\d+) bytes=16777216; unmounted=yes aliases=revoked',prepared)
                    assert extent,bytes(prepared)
                    image_offset=int(extent[1]);before_disk=stick.read_bytes()
                if scratch:
                    serial.sendall(b'acpiruntime battery-usb-write-prepare\n')
                    prepared=bytearray();until=time.monotonic()+30
                    while b'workspace-scratch: prepared ' not in complete_lines(prepared) and time.monotonic()<until:
                        prepared.extend(boot.read_for(serial,.1))
                    output.extend(prepared)
                    extent=re.search(rb'workspace-scratch: prepared path=(/work/srvros-work/battery-io-[0-9a-f]{16}\.bin) raw_offset=(\d+) bytes=4096; unmounted=yes aliases=revoked',prepared)
                    assert extent,bytes(prepared)
                    scratch_path=extent[1].decode().removeprefix('/work/')
                    scratch_offset=int(extent[2]);before_disk=stick.read_bytes()
                    assert before_disk[scratch_offset:scratch_offset+4096]==bytes(i&255 for i in range(4096))
                if readonly:
                    serial.sendall(b'workspace unmount\n')
                    parked=bytearray();until=time.monotonic()+30
                    while b'workspace: unmounted; USB writes disabled' not in parked and time.monotonic()<until:
                        parked.extend(boot.read_for(serial,.1))
                    output.extend(parked)
                    assert b'workspace: unmounted; USB writes disabled' in parked,bytes(parked)
                    before_hash=hashlib.sha256(stick.read_bytes()).hexdigest()
                for iteration in range(2 if args.case in ('normal','foreign-event','mode-button','transition-timeout') else 1):
                    # After a human timeout the ownership lifetime must be reusable.
                    reentry=args.case=='transition-timeout' and iteration==1
                    part = bytearray()
                    command=(b'battery-filesystem' if isolated else b'battery-usb-write' if scratch else b'battery-usb-read' if readonly else b'battery-service-refusal' if args.case=='service-refusal' else
                             b'battery-service-stale' if args.case in ('service-stale','service-incoherent') else
                             b'battery-service' if service else b'battery-transitions' if transitions and not reentry else b'battery')
                    serial.sendall(b'acpiruntime '+command+b'\n')
                    start = time.monotonic()
                    while time.monotonic()-start < (155 if transitions else 100):
                        part.extend(boot.read_for(serial, .1))
                        lines = complete_lines(part)
                        if auto_deadline and process.poll() is not None:break
                        if service:
                            if b'battery-service-test: PASS' in lines or b'battery-service-test: FAIL' in lines or b'battery: HALTED' in lines:break
                            continue
                        if b'battery-transition: PASS' in lines or b'battery-transition: INCOMPLETE' in lines or b'battery: PASS' in lines or b'battery: HALTED' in lines or b'battery: REFUSED' in lines:
                            break
                    output.extend(part); (out/'serial.txt').write_bytes(output)
                    sci=check_sci_records(part)
                    controllers=re.findall(rb'battery-sci-controller: .*? valid=(\d+) vector=(\d+) gsi=(\d+)',part)
                    assert all(tuple(map(int,c))==(7,41,9) for c in controllers),controllers

                    if auto_deadline:
                        assert b'battery: FAILED operation deadline' in part,bytes(part)
                        assert b'returning to Ubuntu without live-runtime cleanup' in part,bytes(part)
                        assert b'battery-service-test: PASS' not in part and b'battery: HALTED' not in part,bytes(part)
                        assert process.wait(timeout=10)==0
                        if readonly:
                            assert hashlib.sha256(stick.read_bytes()).hexdigest()==before_hash
                            with stick.open('rb') as stream:stream.seek(2048*512);volume.write_bytes(stream.read())
                            (out/'usb-fsck.txt').write_text(check_clean_image(volume))
                        if isolated:
                            if args.case=='service-isolated-active-deadline':
                                assert stick.read_bytes()!=before_disk,'Fault did not overlap filesystem writes'
                            verify_isolated_disk(stick,before_disk,image_offset,volume,out,None)
                        if scratch:
                            verify_scratch_disk(stick,before_disk,scratch_offset,scratch_path,volume,out,None)
                        print('acpi-battery: PASS '+args.case+' (contained fault resets; VM only)')
                        return
                    if service and args.case!='service-query-deadline':
                        assert b'battery-service-test: PASS' in part and b'battery-service-test: FAIL' not in part,bytes(part)
                        assert b'battery: HALTED' not in part and b'ERROR foreign' not in part,bytes(part)
                        if args.case=='service-refusal':
                            assert b'battery-service-test: PASS unavailable' in part and b'battery: phase=' not in part,bytes(part)
                        else:
                            assert b'busy_reads=0 ' not in part and b'; retired=yes' in part,bytes(part)
                            assert part.count(b'battery-service: retired ')>=2,bytes(part)
                            assert sum(row['snapshot']=='retired' for row in sci)>=6,bytes(part)
                            if args.case=='service-sci-unclaimed':
                                active=[r for r in sci if r['snapshot']=='retired' and r['phase']=='active']
                                assert sum(r['unclaimed'] for r in active)>=5,bytes(part)
                                observed=re.findall(rb'battery-sci-event: snapshot=retired .*? handled=0 .*? valid=(0x[0-9a-fA-F]+)',part)
                                assert observed and all(int(v,16)&3==3 for v in observed),bytes(part)
                                assert part.count(b'irq: unhandled line=9 vector=41')==1,bytes(part)
                            if args.case in ('service-io-short','service-io-long','service-io-sustained','service-usb-io','service-filesystem','service-filesystem-write-failure'):
                                check_io_hold(part,{'service-io-short':20000,'service-io-long':250000,'service-io-sustained':5000000,'service-usb-io':5000000,'service-filesystem':60000000,'service-filesystem-write-failure':5000000}[args.case])
                            assert part.count(b'battery: foreign-event restored ')>=4,bytes(part)
                            if args.case=='service-setup-load':
                                work=re.findall(rb'battery-vm: setup-work active_us=(\d+) elapsed_us=(\d+)',part)
                                assert len(work)>=2 and all(3500000<=int(a)<=int(e)<5000000 for a,e in work),bytes(part)
                            if args.case in ('service-stale','service-incoherent'):
                                assert b'stale=1 recovered=1' in part,bytes(part)
                            else:assert b'rejected=0 ' in part,bytes(part)
                    elif reentry:
                        assert re.search(rb'battery: PASS samples=5 notifications=\d+ queries=\d+ attached=1 detached=1 heap=0 errors=0 denied=0',part),bytes(part)
                        assert part.count(b'battery: foreign-event restored ')==2,bytes(part)
                        assert b'battery: HALTED' not in part and b'ERROR foreign' not in part,bytes(part)
                    elif transitions:
                        expected=b'PASS phases=3' if args.case=='transitions' else b'INCOMPLETE phases=1'
                        assert b'battery-transition: '+expected in part,bytes(part)
                        assert b'heap=0 errors=0 denied=0; retired=yes' in part,bytes(part)
                        assert part.count(b'battery: foreign-event restored ')==2,bytes(part)
                        assert b'battery: HALTED' not in part and b'ERROR foreign' not in part,bytes(part)
                        if args.case=='transitions':
                            assert b'samples=9 ' in part,bytes(part)
                            assert part.count(b'battery-transition: accepted=')==3,bytes(part)
                            assert b'UNPLUG AC now' in part and b'RECONNECT AC now' in part,bytes(part)
                        else:
                            assert time.monotonic()-start>=120,bytes(part)
                            assert b'RECONNECT AC now' not in part,bytes(part)
                    elif args.case in ('normal','foreign-event','mode-button'):
                        assert b'battery: PASS samples=5 notifications=2 queries=1 attached=1 detached=1 heap=0 errors=0 denied=0' in part, bytes(part)
                        samples = re.findall(rb'battery: sample=(\d+) ac_present=1 soc=86 voltage_uv=12000000 current_ua=-250000 capacity=4300000 full=5000000', part)
                        assert list(map(int,samples)) == [1,2,3,4,5], bytes(part)
                        if args.case == 'mode-button':
                            assert b'battery: post-mode fixed-enable=0x0000000000000100' in part,bytes(part)
                            assert b'battery: post-mode power button masked; status not acknowledged' in part,bytes(part)
                        if args.case == 'foreign-event':
                            assert b'battery-vm: foreign restored enable=0x000000000000062f pending=0x0000000000000040' in part, bytes(part)
                            assert b'battery-vm: foreign restored enable=0x000000000000062e pending=0x0000000000000004' in part, bytes(part)
                            assert part.count(b'battery: foreign-event held ')==2,bytes(part)
                            assert len(re.findall(rb'battery: foreign-event restored .*protected_writes=[1-9][0-9]*',part))==2,bytes(part)
                            assert re.search(rb'foreign-event restored .*protected_writes=[1-9][0-9]*',part),bytes(part)
                            assert b'ERROR foreign' not in part,bytes(part)
                    elif args.case == 'native-refusal':
                        assert b'battery: REFUSED platform/resource ownership preflight' in part, bytes(part)
                        assert b'battery: phase=' not in part
                        assert b'battery-native: REFUSED check=pci-gpu' in part
                    else:
                        reasons = {'denied': b'namespace/EC connection', 'transport': b'namespace/EC connection', 'storm': b'query limit', 'deadline': b'operation deadline', 'query-deadline':b'independent query deadline', 'disconnect-deadline':b'operation deadline', 'foreign-restore-fail':b'foreign event restore', 'foreign-mask-fail':b'foreign event mask', 'foreign-reenable':b'foreign event unexpectedly enabled', 'mode-unexpected':b'unexpected post-mode fixed enable', 'service-query-deadline':b'independent query deadline'}
                        assert b'battery: HALTED '+reasons[args.case] in part, bytes(part)
                        assert b'battery: PASS' not in part
                        assert b'ERROR foreign' not in part
                        if args.case == 'mode-unexpected':
                            assert b'battery: post-mode fixed-enable=0x0000000000000200' in part and b'battery: phase=' not in part,bytes(part)
                        if args.case == 'foreign-restore-fail':
                            assert part.count(b'battery: foreign-event restored ')==1,bytes(part)
                            assert b'battery-vm: foreign restored enable=0x000000000000062f pending=0x0000000000000040' in part,bytes(part)
                        if args.case == 'foreign-mask-fail':
                            assert part.count(b'battery: foreign-event held ')==1 and b'battery: phase=' not in part,bytes(part)
                        if args.case == 'deadline':
                            assert 4.5 <= time.monotonic()-start < 9
                        output.extend(boot.read_for(serial, 3 if args.case in ('deadline','query-deadline','disconnect-deadline') else .3))
                        registers = qmp.call('human-monitor-command', {'command-line': 'info registers'})
                        flags = re.search(r'RFL=([0-9a-fA-F]+)', registers)
                        assert flags and not(int(flags[1],16)&0x200), registers
                if filesystem:
                    fs_records=re.findall(rb'battery-vm: filesystem mode=(sustained|write-failure) cycles=(\d+) file_ok=1 bytes=262181',output)
                    assert len(fs_records)==1,bytes(output)
                    fs_cycles=int(fs_records[0][1])
                    assert fs_cycles >= (1 if fs_failure else 2),fs_records
                    assert fs_records[0][0] == (b'write-failure' if fs_failure else b'sustained')
                    if fs_failure:
                        assert b'battery-vm: fs-failure injected=1 calls=3 blocked=1 preserved=1' in output,bytes(output)
                    else:
                        holds=re.findall(rb'first_gen=(\d+) last_gen=(\d+)',output)
                        assert len(holds)==1 and int(holds[0][1])-int(holds[0][0])>=10,holds
                if usb and not readonly and not scratch and not fs_failure and not isolated:
                    if not filesystem:
                        proof=re.findall(rb'battery-vm: usb-io writes=(\d+) file_ok=1 bytes=4096',output)
                        assert len(proof)==1 and int(proof[0])>1,bytes(output)
                    serial.sendall(b'workspace unmount\n')
                    unmounted=bytearray();until=time.monotonic()+30
                    while b'workspace: unmounted; USB writes disabled' not in unmounted and time.monotonic()<until:
                        unmounted.extend(boot.read_for(serial,.1))
                    output.extend(unmounted)
                    assert b'workspace: unmounted; USB writes disabled' in unmounted,bytes(unmounted)
                qmp.call('quit'); process.wait(timeout=10)
                if usb:
                    with stick.open('rb') as stream:stream.seek(2048*512);volume.write_bytes(stream.read())
                    if fs_failure:
                        dirty=struct.unpack_from('<H',volume.read_bytes(),106)[0]&2
                        assert dirty, 'Failed metadata write did not retain dirty flag'
                        verify_filesystem_tree(volume.read_bytes(),fs_cycles)
                        # Retain the failure image and exercise offline rescue on a copy.
                        (out/'usb-failed.exfat').write_bytes(volume.read_bytes())
                        report=subprocess.run(['/usr/sbin/fsck.exfat','-n',str(volume)],capture_output=True,text=True)
                        (out/'usb-failure-fsck.txt').write_text(report.stdout+report.stderr)
                        repair=subprocess.run(['/usr/sbin/fsck.exfat','-y','-s',str(volume)],capture_output=True,text=True)
                        (out/'usb-recovery.txt').write_text(repair.stdout+repair.stderr)
                        assert repair.returncode in (0,1),repair.stdout+repair.stderr
                    (out/'usb-fsck.txt').write_text(check_clean_image(volume))
                    if isolated:
                        record=re.findall(rb'battery-filesystem: PASS cycles=(\d+) commands=(\d+) busy_cycles=(\d+) progress_bursts=(\d+) bursts=(\d+) max_burst_us=(\d+) elapsed_us=(\d+) steps=(\d+) max_step_us=(\d+) slowest_step=(\d+) profile=1; retired=yes revoked=yes',output)
                        assert len(record)==1,bytes(output)
                        cycles,commands,busy,progress,bursts,maximum,elapsed,steps,step_us,slowest=map(int,record[0])
                        assert cycles>1 and commands>=cycles*3 and busy>0 and 0<progress<=bursts and elapsed>=60000000,record
                        assert steps==cycles*18 and bursts==steps and 0<step_us<=maximum and slowest<18,record
                        assert output.count(b'workspace-image: revoked clean=yes')==1,bytes(output)
                        from hardware_desktop import battery_filesystem_result
                        parsed=battery_filesystem_result(output.decode(errors='replace'),require_service=False)
                        assert parsed and 'profile' in parsed,bytes(output)
                        (out/'filesystem-profile.json').write_text(json.dumps(parsed['profile'],indent=2)+'\n')
                        verify_isolated_disk(stick,before_disk,image_offset,volume,out,cycles)
                    elif filesystem:
                        verify_filesystem_tree(volume.read_bytes(),fs_cycles)
                        allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                        (out/'usb-allocation.txt').write_text(allocation.stdout+allocation.stderr)
                        assert allocation.returncode==0,allocation.stdout+allocation.stderr
                        print('acpi-battery: filesystem tree verified offline; allocation graph clean' + (' after disposable recovery' if fs_failure else ''))
                    elif scratch:
                        from hardware_desktop import battery_usb_write_result
                        proof=battery_usb_write_result(output.decode(errors='replace'),require_service=False)
                        assert proof,bytes(output)
                        verify_scratch_disk(stick,before_disk,scratch_offset,scratch_path,volume,out,proof['cycles'])
                    elif readonly:
                        from hardware_desktop import battery_usb_read_result
                        assert battery_usb_read_result(output.decode(errors='replace'),require_service=False),bytes(output)
                        assert hashlib.sha256(stick.read_bytes()).hexdigest()==before_hash
                        print('acpi-battery: USB disk unchanged after read overlap; exFAT clean')
                    else:
                        stored=ExfatFixture(volume.read_bytes()).read('srvros-work/io-proof')
                        assert stored==bytes((i+int(proof[0])-1)&255 for i in range(4096))
                        (out/'usb-proof.bin').write_bytes(stored)
                        print('acpi-battery: USB payload verified offline and exFAT clean')
            finally:
                stop_process(process); (out/'serial.txt').write_bytes(output)
    print('acpi-battery: PASS '+args.case+' (VM evidence only)')


if __name__ == '__main__':
    main()
