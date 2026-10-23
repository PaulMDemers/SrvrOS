#!/usr/bin/env python3
"""Exercise parked SMBus capture through actual boot flow on disposable USB only."""
import argparse,json,os,struct,subprocess,tempfile,time
from pathlib import Path
from battery_workspace_smoke import ANCHOR,MARKER
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from exfat_test_image import ExfatFixture,check_clean_image
from vm_sockets import connect_unix

from battery_native_query_smoke import verify_image
from battery_native_smbus_records import check,check_files
from battery_desktop_smoke import verify_app_image
CASES={'completion':62,'alarm-darwin':63,'alarm-other':64,'unknown':65,'busy':66,'error':67,'read-timeout':68,'ack-timeout':69,'bad-hid':70,'bad-bank':71,'late-alarm':72,'native-refusal':62,'profile-refusal':15,'no-event':73,'bad-lock':74,'unsupported':75,'ordinary-refusal':62}
FATAL={c:('namespace/EC connection' if c.startswith('bad-') else 'query policy' if c in ('unsupported','ordinary-refusal') else 'SMBus event') for c in ('busy','error','read-timeout','ack-timeout','bad-hid','bad-bank','bad-lock','unsupported','ordinary-refusal')}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--air61',action='store_true',help='exercise bounded Air6,1 admission in disposable model')
    parser.add_argument('--transitions',action='store_true',help='attended parked Air6,1 transition model')
    parser.add_argument('--files',action='store_true')
    parser.add_argument('--case',choices=(*CASES,'incoherent','timeout','sustained','stop-read','stop-ack','stop-uncertain','event-limit'),default='completion');args=parser.parse_args()
    if args.case not in CASES and not args.files and not args.transitions:parser.error('case requires --files')
    cases=dict(CASES);fatal=dict(FATAL)
    if args.files:
        cases.update(completion=80,sustained=76,**{'stop-read':77,'stop-ack':78,'stop-uncertain':79,'event-limit':81})
        fatal.update({'stop-uncertain':'SMBus event','event-limit':'SMBus capture event limit'})
    if args.transitions:
        if not args.air61 or args.files:parser.error('transition model requires --air61 and parked storage')
        cases={'completion':85,'incoherent':86,'timeout':87,'unsupported':88,'alarm-darwin':89,'native-refusal':85,'profile-refusal':15}
        if args.case not in cases:parser.error('unsupported transition case')
    checker=check_files if args.files else check
    if args.transitions:
        from battery_smbus_transition_records import check as checker
    capture=b'battery-smbus-files-capture' if args.files else b'battery-smbus-capture'
    if args.transitions:capture=b'battery-smbus-transitions-capture'
    root=Path(__file__).resolve().parents[1];out=root/'build/review'/('battery-native-smbus-'+('transitions-' if args.transitions else '')+('air61-' if args.air61 else '')+('files-' if args.files else '')+args.case)
    out.mkdir(parents=True,exist_ok=True);(out/'result.json').unlink(missing_ok=True);output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-native-smbus-') as folder:
        tmp=Path(folder);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as stream:stream.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes());entries={'.workspace':MARKER,'anchor.txt':ANCHOR}
        original_inner=None
        if args.files:
            from battery_filesystem_image import create
            inner=tmp/'inner.exfat';create(inner);original_inner=inner.read_bytes();entries['battery-fs.img']=original_inner
        fixture.add_directory_files('srvros-work',entries)
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as stream:stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
        (tmp/'battery.asl').write_text(('#define BATTERY_AIR61 1\n' if args.air61 else '')+'#define BATTERY_FAULT '+str(cases[args.case])+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='native-refusal' else 'vm.elf')
        if args.air61:kernel=root/'build/acpi-air61-battery'/('srvros.elf' if args.case=='native-refusal' else 'vm.elf')
        cmdline='srvros.storage=ram srvros.workspace-volume=51525354 srvros.desktop-release=0123456789abcdef srvros.capture=battery-smbus srvros.capture-failure=reboot'
        if args.transitions:cmdline=cmdline.replace('srvros.capture=battery-smbus','srvros.capture=battery-smbus-transitions')
        if args.case=='ordinary-refusal':cmdline=cmdline.replace('srvros.capture=battery-smbus','srvros.capture=battery')
        if args.files:cmdline=cmdline.replace('srvros.capture=battery-smbus','srvros.capture=battery-smbus-files')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,root/'build/initramfs.tar',cmdline=cmdline)
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
             '-device','qemu-xhci,id=xhci','-drive',f'if=none,id=stick,format=raw,file={stick}',
             '-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp');start=time.monotonic()
                while process.poll() is None and time.monotonic()-start<(200 if args.transitions else 160 if args.files else 100):
                    output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                    qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert process.poll()==0,bytes(output[-6000:])
                output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
            finally:
                stop_process(process)
                (out/'last-workspace.img').write_bytes(stick.read_bytes())
        data=bytes(output);positive=args.case not in (*fatal,'profile-refusal','no-event','native-refusal','stop-read','stop-ack','timeout')
        assert b'laptop-capture: battery media parked=yes fault-reset=armed' in data
        result=dict(status='PASS',case=args.case,automatic_reset=True,native_acceptance='not tested')
        disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
        volume.write_bytes(disk[2048*512:]);(out/'workspace.exfat').write_bytes(volume.read_bytes())
        if args.case in fatal:
            assert b'battery: FAILED '+fatal[args.case].encode() in data
            assert b'contained fault; returning' in data
            for forbidden in (b'battery-service: retired ',b'laptop-capture: completed=',b'laptop-capture: saved=',b'battery-native-smbus: outcome='):
                assert forbidden not in data
            image=ExfatFixture(volume.read_bytes())
            assert image.read('srvros-work/.workspace')==MARKER and image.read('srvros-work/anchor.txt')==ANCHOR
            (out/'fsck.txt').write_text(check_clean_image(volume))
            allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
            (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
            result.update(outcome='contained fault reset',runtime_retired=False,offline_allocation='PASS')
        else:
            if positive:
                if not args.files:result.update(checker(data,vm=True))
            elif args.transitions and args.case=='profile-refusal':
                assert b'battery: REFUSED Air6,1 parked AC transition owner' in data
                assert b'battery-service: retired ' not in data
                result.update(outcome='transition profile refused')
            elif args.transitions and args.case=='timeout':
                result.update(checker(data,vm=True,complete=False))
                result['status']='PASS';result['outcome']='incomplete as expected'
            elif args.case in ('profile-refusal','no-event','stop-read','stop-ack'):
                assert b'outcome=not-exercised' in data
                if args.case in ('profile-refusal','no-event'):assert b'battery-smbus: event=' not in data and b'smbus-e:' not in data
                if args.case.startswith('stop-'):
                    assert b'battery-vm-smbus-stop: phase=' in data and b'heap=0 errors=0 denied=0' in data
                    assert b'battery-app-files: FAIL descendants=3; retired=yes revoked=yes' in data
                result.update(outcome='not-exercised')
            else:
                assert b'battery: REFUSED platform/resource ownership preflight' in data
                assert b'battery-service: retired ' not in data
                result.update(outcome='native platform refused')
            assert data.count(b'laptop-capture: saved=yes unmounted=yes')==1
            assert data.index(b'laptop-capture: saved=yes unmounted=yes')<data.index(b'laptop-capture: returning to Ubuntu')
            def inner_check(raw):
                if positive or args.case=='no-event':verify_app_image(raw)
                else:assert raw==original_inner,'Incomplete early trial changed inner image'
            saved=verify_image(volume.read_bytes(),data,positive,checker=checker,capture=capture,
                               image_checker=inner_check if args.files else None,compare_serial=not args.files)
            (out/'exit.log').write_bytes(saved)
            if args.transitions and args.case=='timeout':checker(saved,vm=True,complete=False)
            if positive and args.files:
                result.update(checker(saved,vm=True));result['proof_source']='atomic saved boot log'
                # Final summary prints after all competing clients have retired.
                summary=next(r for r in saved.splitlines() if r.startswith(b'battery-native-smbus: outcome='))
                assert data.replace(b'\r\n',b'\n').splitlines().count(summary)==1
            if args.files:
                inner.write_bytes(ExfatFixture(volume.read_bytes()).read('srvros-work/battery-fs.img'))
                (out/'inner-fsck.txt').write_text(check_clean_image(inner))
                allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(inner),'allocation-check'],capture_output=True,text=True)
                (out/'inner-allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
            (out/'fsck.txt').write_text(check_clean_image(volume))
            allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
            (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
            result.update(exact_tree_metadata='PASS',offline_allocation='PASS',saved_log='PASS')
        (out/'workspace.exfat').write_bytes(volume.read_bytes())
        import hashlib
        result['kernel_sha256']=hashlib.sha256(kernel.read_bytes()).hexdigest()
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
