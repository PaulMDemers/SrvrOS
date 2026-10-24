#!/usr/bin/env python3
"""Explicit mounted-Home runtime boundary, on disposable VM USB only."""
import argparse,hashlib,json,os,re,struct,subprocess,tempfile,time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from vm_sockets import connect_unix
from serial_records import complete_lines
from exfat_test_image import ExfatFixture,check_clean_image
from battery_workspace_smoke import ANCHOR,MARKER,verify_disk
from battery_home_records import check


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--case',choices=('normal','alarm','query-refusal','native-refusal','unmounted-refusal','profile-refusal','runtime-fault','effect-refusal'),default='normal')
    p.add_argument('--air61',action='store_true')
    args=p.parse_args();root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('battery-home-'+('air61-' if args.air61 else '')+args.case);out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True);output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-home-') as directory:
        tmp=Path(directory);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes())
        fixture.add_directory_files('srvros-work',{'.workspace':MARKER,'anchor.txt':ANCHOR})
        fixture.add_directory_files('battery-study',{'anchor.txt':ANCHOR})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as f:f.write(mbr);f.seek(2048*512);f.write(fixture.data)
        (tmp/'battery.asl').write_text(('#define BATTERY_AIR61 1\n' if args.air61 else '')+'#define BATTERY_FAULT '+str({'normal':62,'alarm':63,'query-refusal':75,'native-refusal':62,'unmounted-refusal':62,'profile-refusal':15,'runtime-fault':69,'effect-refusal':82}[args.case])+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/('build/acpi-air61-battery' if args.air61 else 'build/acpi-battery')/('srvros.elf' if args.case=='native-refusal' else 'vm.elf')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,root/'build/initramfs.tar')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
             '-device','qemu-xhci,id=xhci','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                def wait_for(needle,seconds=20):
                    data=bytearray();due=time.monotonic()+seconds
                    while needle not in complete_lines(data) and time.monotonic()<due and process.poll() is None:
                        data.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output+data)
                    output.extend(data);assert needle in complete_lines(data),bytes(data[-3000:]);return bytes(data)
                start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output
                if args.case!='unmounted-refusal':
                    serial.sendall(b'workspace mount 51525354\n');wait_for(b'workspace: ready at /work/srvros-work')
                serial.sendall(b'echo HOME_BOUNDARY_BEGIN\n');wait_for(b'\nHOME_BOUNDARY_BEGIN\r\n')
                serial.sendall(b'acpiruntime battery-home\n')
                if args.case in ('runtime-fault','effect-refusal'):
                    data=wait_for(b'battery: HALTED '+(b'SMBus event ' if args.case=='runtime-fault' else b'query return receipt '),40);output.extend(read_for(serial,1))
                    assert process.poll() is None
                    assert b'battery-workspace-hold: acquired token=1' in data
                    for bad in (b'battery-workspace-hold: released',b'battery-service: retired',b'workspace: unmounted',b'battery-home: outcome='):assert bad not in data
                    result=dict(status='PASS',expected=args.case+' retained; no cleanup/reset claim',native_acceptance=False)
                else:
                    wait_for(b'battery-home: REFUSED' if args.case=='unmounted-refusal' else b'battery-workspace-hold: released token=1',50)
                    serial.sendall(b'dmesg 65536\necho HOME_BOUNDARY_END\n');snapshot=wait_for(b'\nHOME_BOUNDARY_END\r\n')
                    starts=list(re.finditer(rb'^HOME_BOUNDARY_BEGIN\r?\n',snapshot,re.M));assert len(starts)==1
                    data=snapshot[starts[0].end():];(out/'boot.log').write_bytes(data)
                    result=check(data,args.case)
                    serial.sendall(b'run /fat/bin/deskd --smoke\n');wait_for(b'deskd: exited ')
                    if args.case!='unmounted-refusal':
                        serial.sendall(b'workspace unmount\n');wait_for(b'workspace: unmounted; USB writes disabled')
                    stop_process(process);disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
                    data=disk[2048*512:];volume.write_bytes(data)
                    assert data==bytes(fixture.data),'Read-only boundary trial changed workspace bytes'
                    (out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result.update(replacement_compositor='PASS',unmount='PASS' if args.case!='unmounted-refusal' else 'not mounted',offline_allocation='PASS')
                result.update(kernel_sha256=hashlib.sha256(kernel.read_bytes()).hexdigest(),initramfs_sha256=hashlib.sha256((root/'build/initramfs.tar').read_bytes()).hexdigest())
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
            finally:
                (out/'serial.log').write_bytes(output);stop_process(process)
                (out/'last-workspace.img').write_bytes(stick.read_bytes())

if __name__=='__main__':main()
