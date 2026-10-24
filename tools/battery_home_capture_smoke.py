#!/usr/bin/env python3
"""Exercise held Home battery capture through actual boot flow on disposable USB only."""
import argparse,json,os,struct,subprocess,tempfile,time
from pathlib import Path
from battery_workspace_smoke import ANCHOR,MARKER
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from exfat_test_image import ExfatFixture,check_clean_image
from vm_sockets import connect_unix

from battery_native_query_smoke import verify_image
from battery_home_records import check_capture,check
CASES={'completion':62,'alarm':63,'query-refusal':75,'uncertain-ack':69}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--air61',action='store_true')
    parser.add_argument('--case',choices=CASES,default='completion');args=parser.parse_args()
    capture=b'battery-home-capture'
    root=Path(__file__).resolve().parents[1];out=root/'build/review'/('battery-home-capture-'+('air61-' if args.air61 else '')+args.case)
    out.mkdir(parents=True,exist_ok=True);(out/'result.json').unlink(missing_ok=True);output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-home-capture-') as folder:
        tmp=Path(folder);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as stream:stream.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes());entries={'.workspace':MARKER,'anchor.txt':ANCHOR}
        fixture.add_directory_files('srvros-work',entries)
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as stream:stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
        (tmp/'battery.asl').write_text(('#define BATTERY_AIR61 1\n' if args.air61 else '')+'#define BATTERY_FAULT '+str(CASES[args.case])+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/('build/acpi-air61-battery' if args.air61 else 'build/acpi-battery')/'vm.elf'
        cmdline='srvros.storage=ram srvros.workspace-volume=51525354 srvros.desktop-release=0123456789abcdef srvros.capture=battery-home srvros.capture-failure=reboot'
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
                while process.poll() is None and time.monotonic()-start<70:
                    output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                    qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                    if args.case=='uncertain-ack' and b'battery: HALTED SMBus event ' in output:break
                assert (process.poll() is None if args.case=='uncertain-ack' else process.poll()==0),bytes(output[-6000:])
                output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
            finally:
                stop_process(process)
                (out/'last-workspace.img').write_bytes(stick.read_bytes())
        data=bytes(output)
        assert b'laptop-capture: battery Home mounted=yes logging-paused=yes fault-reset=disabled' in data
        assert b'laptop-capture: battery media ' not in data
        result=dict(status='PASS',case=args.case,native_acceptance=False)
        if args.case=='uncertain-ack':
            assert b'battery: HALTED SMBus event ' in data
            assert b'battery-workspace-hold: acquired token=1' in data
            for forbidden in (b'battery-workspace-hold: released',b'battery-service: retired ',b'laptop-capture: completed=',b'laptop-capture: saved=',b'laptop-capture: returning',b'workspace: unmounted'):
                assert forbidden not in data
            result.update(automatic_reset=False,runtime_retired=False,storage_cleanup='not claimed')
        else:
            assert data.count(b'laptop-capture: saved=yes unmounted=yes')==1
            assert data.index(b'laptop-capture: saved=yes unmounted=yes')<data.index(b'laptop-capture: returning to Ubuntu')
            disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
            volume.write_bytes(disk[2048*512:])
            def checker(raw,vm=True):return check_capture(raw,vm=True,case='alarm' if args.case=='alarm' else 'normal')
            saved=verify_image(volume.read_bytes(),data,args.case!='query-refusal',checker=checker,capture=capture,compare_serial=False)
            (out/'exit.log').write_bytes(saved)
            result.update(check(saved,'query-refusal') if args.case=='query-refusal' else check_capture(saved,vm=True,case='alarm' if args.case=='alarm' else 'normal'))
            (out/'fsck.txt').write_text(check_clean_image(volume))
            allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
            (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
            result.update(automatic_reset=True,exact_tree_metadata='PASS',offline_allocation='PASS',saved_log='PASS')
        import hashlib
        result['kernel_sha256']=hashlib.sha256(kernel.read_bytes()).hexdigest()
        result['initramfs_sha256']=hashlib.sha256((root/'build/initramfs.tar').read_bytes()).hexdigest()
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
