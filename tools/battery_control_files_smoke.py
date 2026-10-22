#!/usr/bin/env python3
"""Scoped battery requests during submitted Text jobs, on disposable VM USB only."""
import argparse,hashlib,json,os,re,struct,subprocess,tempfile,time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from vm_sockets import connect_unix
from serial_records import complete_lines
from exfat_test_image import ExfatFixture,check_clean_image
from battery_workspace_smoke import ANCHOR,MARKER,verify_disk
from battery_control_files_records import check


def verify_cancel(data):
    f=ExfatFixture(data)
    roots={n for n,_ in f.entries(f.chain(f.root))}
    assert roots in ({'srvros-work','battery-study'},{'srvros-work','battery-study','.srvros'})
    assert {n for n,_ in f.entries(f.read('srvros-work'))}=={'.workspace','anchor.txt'}
    assert f.read('srvros-work/.workspace')==MARKER and f.read('srvros-work/anchor.txt')==ANCHOR
    names={n for n,_ in f.entries(f.read('battery-study'))}
    assert names in ({'anchor.txt'},{'anchor.txt','text-draft.txt'})
    assert f.read('battery-study/anchor.txt')==ANCHOR
    if 'text-draft.txt' in names:assert f.read('battery-study/text-draft.txt') in (b'',b'A',b'AA')
    if '.srvros' in roots:
        assert {n for n,_ in f.entries(f.read('.srvros'))}=={'meta'}
        rows=f.read('.srvros/meta').decode().splitlines();assert rows.pop(0)=='SRVROSMETA1'
        expected={'/work','/work/srvros-work','/work/srvros-work/.workspace','/work/srvros-work/anchor.txt','/work/battery-study','/work/battery-study/anchor.txt'}
        if 'text-draft.txt' in names:expected.add('/work/battery-study/text-draft.txt')
        paths=set();inodes=set()
        for row in rows:
            fields=row.split('\t');assert len(fields)==9
            path=fields[0];inode,mode,links,uid,gid,atime,mtime,ctime=map(int,fields[1:])
            directory=path in ('/work','/work/srvros-work','/work/battery-study')
            assert path in expected and path not in paths and inode>0 and inode not in inodes
            assert mode==(0o40755 if directory else 0o100644) and links==(2 if directory else 1)
            assert uid==gid==0 and min(atime,mtime,ctime)>=0
            paths.add(path);inodes.add(inode)
        assert paths==expected
    return 'absent' if 'text-draft.txt' not in names else f.read('battery-study/text-draft.txt').hex()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--case',choices=('normal','cancel','refusal','unmounted-refusal','profile-refusal','runtime-fault'),default='normal')
    args=p.parse_args();root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('battery-control-files-'+args.case);out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True);output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-control-files-') as directory:
        tmp=Path(directory);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes())
        fixture.add_directory_files('srvros-work',{'.workspace':MARKER,'anchor.txt':ANCHOR})
        fixture.add_directory_files('battery-study',{'anchor.txt':ANCHOR})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as f:f.write(mbr);f.seek(2048*512);f.write(fixture.data)
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT '+str(79 if args.case=='runtime-fault' else 15 if args.case=='profile-refusal' else 76)+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='refusal' else 'vm.elf')
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
                serial.sendall(b'echo CONTROL_FILES_BEGIN\n');wait_for(b'\nCONTROL_FILES_BEGIN\r\n')
                serial.sendall(b'acpiruntime battery-workspace-controls-files'+(b'-cancel' if args.case=='cancel' else b'')+b'\n')
                if args.case=='runtime-fault':
                    data=wait_for(b'battery: HALTED SMBus event ',40);output.extend(read_for(serial,1))
                    assert process.poll() is None
                    assert b'battery-workspace-hold: acquired token=1' in data
                    for bad in (b'battery-workspace-hold: released',b'battery-service: retired',b'workspace: unmounted',b'battery-workspace-controls: complete='):assert bad not in data
                    result=dict(status='PASS',expected='uncertain acknowledgement retained; no cleanup/reset claim',native_acceptance=False)
                else:
                    wait_for(b'battery-workspace-controls: REFUSED' if args.case in ('refusal','unmounted-refusal') else b'battery-workspace-hold: released token=1',125)
                    serial.sendall(b'dmesg 65536\necho CONTROL_FILES_END\n');snapshot=wait_for(b'\nCONTROL_FILES_END\r\n')
                    starts=list(re.finditer(rb'^CONTROL_FILES_BEGIN\r?\n',snapshot,re.M));assert len(starts)==1
                    data=snapshot[starts[0].end():];(out/'boot.log').write_bytes(data)
                    result=check(data,args.case)
                    serial.sendall(b'run /fat/bin/deskd --smoke\n');wait_for(b'deskd: exited ')
                    if args.case!='unmounted-refusal':
                        serial.sendall(b'workspace unmount\n');wait_for(b'workspace: unmounted; USB writes disabled')
                    stop_process(process);disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
                    data=disk[2048*512:];volume.write_bytes(data)
                    if args.case=='normal':verify_disk(data,True)
                    elif args.case=='cancel':result['cancelled_draft_hex']=verify_cancel(data)
                    else:assert data==bytes(fixture.data),'Refused trial changed workspace bytes'
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
