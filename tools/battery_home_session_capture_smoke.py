#!/usr/bin/env python3
"""Exercise held Home battery capture through actual boot flow on disposable USB only."""
import argparse,json,os,re,struct,subprocess,tempfile,time
from pathlib import Path
from battery_workspace_smoke import ANCHOR,MARKER
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from exfat_test_image import ExfatFixture,check_clean_image
from vm_sockets import connect_unix

from battery_native_query_smoke import verify_image
from battery_control_files_records import check_capture
CASES={'completion':76,'query-refusal':75,'profile-refusal':15,'uncertain-ack':79,'effect-refusal':82}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=CASES,default='completion')
    parser.add_argument('--slow-connect',action='store_true',help='Add four seconds to each fixture EC connection')
    parser.add_argument('--slow-storage',action='store_true',help='Throttle disposable USB to ten writes/second during desktop work')
    args=parser.parse_args()
    assert not args.slow_connect or args.case=='completion'
    assert not args.slow_storage or args.case=='completion'
    capture=b'battery-home-session-capture'
    root=Path(__file__).resolve().parents[1];out=root/'build/review'/('battery-home-session-capture-'+args.case+('-slow-connect' if args.slow_connect else '')+('-slow-storage' if args.slow_storage else ''))
    out.mkdir(parents=True,exist_ok=True);(out/'result.json').unlink(missing_ok=True);output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-home-capture-') as folder:
        tmp=Path(folder);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as stream:stream.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes());entries={'.workspace':MARKER,'anchor.txt':ANCHOR}
        fixture.add_directory_files('srvros-work',entries)
        fixture.add_directory_files('battery-study',{'anchor.txt':ANCHOR})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as stream:stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
        (tmp/'battery.asl').write_text(('#define BATTERY_CONNECT_DELAY 1\n' if args.slow_connect else '')+'#define BATTERY_FAULT '+str(CASES[args.case])+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-battery'/'vm.elf'
        cmdline='srvros.storage=ram srvros.workspace-volume=51525354 srvros.desktop-release=0123456789abcdef srvros.capture=battery-home-session srvros.capture-failure=reboot'
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,root/'build/initramfs.tar',cmdline=cmdline)
        fatal=args.case in ('uncertain-ack','effect-refusal')
        halt=b'battery: HALTED '+(b'query return receipt ' if args.case=='effect-refusal' else b'SMBus event ')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
             '-device','qemu-xhci,id=xhci','-drive',f'if=none,id=stick,format=raw,file={stick}',
             '-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp');start=time.monotonic();throttled=False;unthrottled=False
                while process.poll() is None and time.monotonic()-start<150:
                    output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                    qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                    if args.slow_storage and not throttled and b'desktop-coexist: ready' in output:
                        qmp.call('block_set_io_throttle',{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':10})
                        throttled=True
                    if args.slow_storage and throttled and not unthrottled and b'battery-workspace-hold: released' in output:
                        qmp.call('block_set_io_throttle',{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':0})
                        unthrottled=True
                    if fatal and halt in output:break
                assert (process.poll() is None if fatal else process.poll()==0),bytes(output[-6000:])
                output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
            finally:
                stop_process(process)
                (out/'last-workspace.img').write_bytes(stick.read_bytes())
        data=bytes(output)
        assert b'laptop-capture: battery Home mounted=yes logging-paused=yes fault-reset=disabled' in data
        assert b'laptop-capture: battery media ' not in data
        result=dict(status='PASS',case=args.case,slow_connect=args.slow_connect,slow_storage=throttled,native_acceptance=False)
        if fatal:
            assert halt in data
            assert b'battery-workspace-hold: acquired token=1' in data
            for forbidden in (b'battery-workspace-hold: released',b'battery-service: retired ',b'laptop-capture: completed=',b'laptop-capture: saved=',b'laptop-capture: returning',b'workspace: unmounted'):
                assert forbidden not in data
            result.update(automatic_reset=False,runtime_retired=False,storage_cleanup='not claimed')
        else:
            assert data.count(b'laptop-capture: saved=yes unmounted=yes')==1
            assert data.index(b'laptop-capture: saved=yes unmounted=yes')<data.index(b'laptop-capture: returning to Ubuntu')
            disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
            volume.write_bytes(disk[2048*512:])
            case='normal' if args.case=='completion' else args.case
            def checker(raw,vm=True):return check_capture(raw,vm=True,case=case)
            saved=verify_image(volume.read_bytes(),data,args.case=='completion',checker=checker,capture=capture,compare_serial=False,study='complete' if args.case=='completion' else 'incomplete' if args.case=='query-refusal' else 'untouched')
            (out/'exit.log').write_bytes(saved)
            result.update(checker(saved))
            if args.slow_connect:
                setups=[int(n) for n in re.findall(rb'^battery: setup=complete elapsed_us=(\d+)$',saved,re.M)]
                assert len(setups)==3 and min(setups)>=4000000,setups
                result['setup_us']=setups
            if args.slow_storage:
                timings=re.findall(rb'^desktop-coexist: file-checkpoint serial=(\d+) elapsed_us=(\d+)$',saved,re.M)
                assert [int(n) for n,_ in timings]==[12,24,36,48,60],timings
                waits=[int(n) for _,n in timings]
                assert 4700000<max(waits)<15700000,waits
                result['file_checkpoint_us']=waits
            (out/'fsck.txt').write_text(check_clean_image(volume))
            allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
            (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
            result.update(automatic_reset=True,exact_tree_metadata='PASS',offline_allocation='PASS',saved_log='PASS')
        import hashlib
        result['kernel_sha256']=hashlib.sha256(kernel.read_bytes()).hexdigest()
        result['initramfs_sha256']=hashlib.sha256((root/'build/initramfs.tar').read_bytes()).hexdigest()
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
