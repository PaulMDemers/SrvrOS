#!/usr/bin/env python3
"""VM-only mounted workspace/Text/battery study and fatal reset interlock.

Never accepts a host disk path. Every run creates a fresh disposable USB image.
Faults are expected to halt in the VM; they are not clean storage retirement.
"""
import argparse,json,os,re,struct,subprocess,tempfile,time
from pathlib import Path
from battery_desktop_smoke import check_text_capture
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from exfat_test_image import ExfatFixture,check_clean_image
from serial_records import complete_lines
from vm_sockets import connect_unix

ANCHOR=b'Untouched ordinary workspace data\n'
MARKER=b'SRVROS-WORKSPACE-V1 51525354\n'


def check(data):
    data=complete_lines(data)
    result=check_text_capture(data,files=True,workspace=True)
    assert [r for r in data.splitlines() if r.startswith(b'battery-workspace:')]==[b'battery-workspace: PASS descendants=3; retired=yes mounted=yes']
    assert [r for r in data.splitlines() if r.startswith(b'text-files:')]==[b'text-files: cycles=5 pending=0 storage=workspace']
    rows=re.findall(rb'^desktools: file-result role=text op=(\d+) status=(\d+) bytes=(\d+)\r?$',data,re.M)
    assert len(rows)==sum(r.startswith(b'desktools: file-result ') for r in data.splitlines())
    assert [tuple(map(int,r)) for r in rows]==[(op,0,length) for _ in range(5) for op,length in ((2,0),(8,32),(5,0),(1,2))]
    assert b'workspace-image:' not in data and b'workspace: unmounted' not in data
    assert b'battery-reset: BLOCKED' not in data
    ui=re.findall(rb'^desktop-battery: reads=(\d+) first_gen=(\d+) last_gen=(\d+) cached=yes\r?$',data,re.M)
    assert len(ui)==1 and sum(r.startswith(b'desktop-battery:') for r in data.splitlines())==1
    reads,first,last=map(int,ui[0]);assert reads>=10 and first>=result['first_generation'] and last>=first+10 and last<=result['last_generation']
    result.update(cached_ui_reads=reads)
    result.update(worker_operations=20,file_cycles=5,descendants_retired=3,native_acceptance='not tested')
    return result


def verify_disk(data,normal):
    image=ExfatFixture(data)
    assert {n for n,_ in image.entries(image.chain(image.root))}==({'srvros-work','battery-study','.srvros'} if normal else {'srvros-work','battery-study'})
    assert {n for n,_ in image.entries(image.read('srvros-work'))}=={'.workspace','anchor.txt'}
    assert image.read('srvros-work/.workspace')==MARKER and image.read('srvros-work/anchor.txt')==ANCHOR
    names={n for n,_ in image.entries(image.read('battery-study'))}
    assert 'anchor.txt' in names and image.read('battery-study/anchor.txt')==ANCHOR
    if normal:
        assert names=={'anchor.txt',*(f'text-{i}.txt' for i in range(1,6))}
        for i in range(1,6):assert image.read(f'battery-study/text-{i}.txt')==b'AA'
    else:
        assert names=={'anchor.txt'}  # No mutation, so no metadata sidecar yet.
        return
    assert {n for n,_ in image.entries(image.read('.srvros'))}=={'meta'}
    rows=image.read('.srvros/meta').decode('ascii').splitlines();assert rows.pop(0)=='SRVROSMETA1'
    expected={'/work','/work/srvros-work','/work/srvros-work/.workspace','/work/srvros-work/anchor.txt','/work/battery-study','/work/battery-study/anchor.txt'}
    if normal:expected.update(f'/work/battery-study/text-{i}.txt' for i in range(1,6))
    paths=set();inodes=set()
    for row in rows:
        fields=row.split('\t');assert len(fields)==9
        path=fields[0];assert path in expected and path not in paths
        inode,mode,links,uid,gid,atime,mtime,ctime=map(int,fields[1:])
        directory=path in ('/work','/work/srvros-work','/work/battery-study')
        assert inode>0 and inode not in inodes and mode==(0o40755 if directory else 0o100644)
        assert links==(2 if directory else 1) and uid==gid==0 and min(atime,mtime,ctime)>=0
        paths.add(path);inodes.add(inode)
    assert paths==expected


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=('normal','timeout','refusal','runtime-fault'),default='normal')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('battery-workspace-'+args.case);out.mkdir(parents=True,exist_ok=True)
    output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-battery-workspace-') as folder:
        tmp=Path(folder);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as stream:stream.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes())
        fixture.add_directory_files('srvros-work',{'.workspace':MARKER,'anchor.txt':ANCHOR})
        fixture.add_directory_files('battery-study',{'anchor.txt':ANCHOR})
        mbr=bytearray(512);mbr[450]=7
        struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as stream:stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT '+('28' if args.case=='runtime-fault' else '15')+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='refusal' else 'vm.elf')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,root/'build/initramfs.tar')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
             '-device','qemu-xhci,id=xhci','-drive',f'if=none,id=stick,format=raw,file={stick}',
             '-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                def wait_for(needle,seconds=30):
                    part=bytearray();until=time.monotonic()+seconds
                    while needle not in complete_lines(part) and time.monotonic()<until and process.poll() is None:
                        part.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output+part)
                    output.extend(part);assert needle in complete_lines(part),bytes(part)
                    return bytes(part)
                start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output
                serial.sendall(b'workspace mount 51525354\n');wait_for(b'workspace: ready at /work/srvros-work')
                serial.sendall(b'echo WORKSPACE_STUDY_BEGIN\n');wait_for(b'\nWORKSPACE_STUDY_BEGIN\r\n')
                command=b'battery-workspace'+(b'-fault' if args.case=='runtime-fault' else b'-timeout' if args.case=='timeout' else b'')
                serial.sendall(b'acpiruntime '+command+b'\n')
                if args.case=='runtime-fault':
                    data=wait_for(b'battery: HALTED operation deadline',90)
                    assert b'battery-reset: BLOCKED workspace=1 io=' in data
                    assert b'desktop-coexist: ready ' in data
                    assert b'contained fault; returning' not in data and b'; retired=yes' not in data
                    # A halted VM must remain alive (no-reboot would exit on reset).
                    output.extend(read_for(serial,1));assert process.poll() is None
                    result=dict(status='PASS',expected_failure='runtime deadline',reset_blocked=True,storage_retirement='NOT claimed',native_acceptance='not tested')
                    stop_process(process);(out/'interrupted-workspace.exfat').write_bytes(stick.read_bytes()[2048*512:])
                else:
                    wait_for(b'battery-workspace:',100)
                    serial.sendall(b'dmesg 65536\necho WORKSPACE_STUDY_END\n')
                    snapshot=wait_for(b'\nWORKSPACE_STUDY_END\r\n',15)
                    starts=list(re.finditer(rb'^WORKSPACE_STUDY_BEGIN\r?\n',snapshot,re.M));assert len(starts)==1
                    data=snapshot[starts[0].end():];(out/'boot.log').write_bytes(data)
                    if args.case=='normal':result=check(data)
                    elif args.case=='refusal':
                        assert b'battery-workspace: REFUSED VM-only study' in data
                        assert b'battery-service:' not in data and b'battery-native:' not in data
                        result=dict(status='PASS',native_study_admission='refused before runtime')
                    else:
                        assert b'battery-desktop: FAIL outcome=desktop deadline' in data
                        assert b'battery-workspace: FAIL descendants=3; retired=yes mounted=yes' in data
                        assert b'heap=0 errors=0 denied=0' in data and b'battery: HALTED' not in data
                        result=dict(status='PASS',expected_failure='desktop deadline',clients_and_service='retired')
                    serial.sendall(b'run /fat/bin/deskd --smoke\n');wait_for(b'deskd: exited ')
                    serial.sendall(b'workspace unmount\n');wait_for(b'workspace: unmounted; USB writes disabled')
                    stop_process(process);disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
                    volume.write_bytes(disk[2048*512:]);verify_disk(volume.read_bytes(),args.case=='normal')
                    if args.case!='normal':assert volume.read_bytes()==bytes(fixture.data),'Read-only trial changed disk bytes'
                    (out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result.update(replacement_compositor='PASS',workspace_unmount='PASS',offline_tree_metadata_allocation='PASS')
                    (out/'workspace.exfat').write_bytes(volume.read_bytes())
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
            finally:
                (out/'serial.log').write_bytes(output);stop_process(process)
                (out/'last-workspace.img').write_bytes(stick.read_bytes())

if __name__=='__main__':main()
