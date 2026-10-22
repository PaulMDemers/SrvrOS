#!/usr/bin/env python3
"""Exercise parked query capture through actual boot flow on disposable USB only."""
import argparse,json,os,struct,subprocess,tempfile,time
from pathlib import Path
from battery_native_query_records import check
from battery_workspace_smoke import ANCHOR,MARKER
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from exfat_test_image import ExfatFixture,check_clean_image
from vm_sockets import connect_unix

CASES={'unknown':48,'high':49,'os-darwin':50,'os-other':51,'notify-darwin':52,
       'notify-other':53,'shared20':54,'shared21':55,'early':57,'no-event':58,
       'transport':59,'disconnect':60,'lock':61,'native-refusal':48}
FATAL={'shared20':'query return receipt','shared21':'query return receipt',
       'transport':'query transport','disconnect':'EC disconnect','lock':'recovery resources retained'}

def verify_image(raw,serial,positive,checker=check,capture=b'battery-query-capture',image_checker=None):
    image=ExfatFixture(raw)
    assert {n for n,_ in image.entries(image.chain(image.root))}=={'srvros-work','.srvros'}
    assert {n for n,_ in image.entries(image.read('srvros-work'))}==({'.workspace','anchor.txt','Logs','battery-fs.img'} if image_checker else {'.workspace','anchor.txt','Logs'})
    assert image.read('srvros-work/.workspace')==MARKER
    assert image.read('srvros-work/anchor.txt')==ANCHOR
    if image_checker:image_checker(image.read('srvros-work/battery-fs.img'))
    sessions=[n for n,_ in image.entries(image.read('srvros-work/Logs'))];assert len(sessions)==1
    folder='srvros-work/Logs/'+sessions[0]
    assert {n for n,_ in image.entries(image.read(folder))}=={'boot.log','session-a.log','exit.log'}
    saved=image.read(folder+'/exit.log')
    if positive:assert checker(saved,vm=True)==checker(serial,vm=True)
    else:
        assert b'laptop-capture: completed=NO\n' in saved
        assert capture+b': returned=yes clean=NO;' in saved
    paths={'/work':True}
    def walk(folder,depth=0):
        assert depth<5
        data=image.read(folder) if folder else image.chain(image.root)
        for name,entry in image.entries(data):
            if not folder and name=='.srvros':continue
            path=(folder+'/' if folder else '')+name
            directory=bool(struct.unpack_from('<H',entry,4)[0]&0x10)
            assert '/work/'+path not in paths;paths['/work/'+path]=directory
            if directory:walk(path,depth+1)
    walk('')
    rows=image.read('.srvros/meta').decode().splitlines();assert rows.pop(0)=='SRVROSMETA1'
    seen=set();inodes=set()
    for row in rows:
        fields=row.split('\t');assert len(fields)==9
        path=fields[0];assert path in paths and path not in seen
        inode,mode,links,uid,gid,atime,mtime,ctime=map(int,fields[1:]);directory=paths[path]
        assert inode>0 and inode not in inodes and mode==(0o40755 if directory else 0o100644)
        assert links==(2 if directory else 1) and uid==gid==0 and min(atime,mtime,ctime)>=0
        seen.add(path);inodes.add(inode)
    assert seen==set(paths)
    return saved

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=CASES,default='unknown');args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=root/'build/review'/('battery-native-query-'+args.case)
    out.mkdir(parents=True,exist_ok=True);output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-native-query-') as folder:
        tmp=Path(folder);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        with volume.open('wb') as stream:stream.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes());fixture.add_directory_files('srvros-work',{'.workspace':MARKER,'anchor.txt':ANCHOR})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as stream:stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT '+str(CASES[args.case])+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='native-refusal' else 'vm.elf')
        cmdline='srvros.storage=ram srvros.workspace-volume=51525354 srvros.desktop-release=0123456789abcdef srvros.capture=battery-query srvros.capture-failure=reboot'
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
                while process.poll() is None and time.monotonic()-start<100:
                    output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                    qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert process.poll()==0,bytes(output[-6000:])
                output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
            finally:stop_process(process)
        data=bytes(output);positive=args.case not in (*FATAL,'no-event','native-refusal')
        assert b'laptop-capture: battery media parked=yes fault-reset=armed' in data
        result=dict(status='PASS',case=args.case,automatic_reset=True,native_acceptance='not tested')
        disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
        volume.write_bytes(disk[2048*512:]);(out/'workspace.exfat').write_bytes(volume.read_bytes())
        if args.case in FATAL:
            assert b'battery: FAILED '+FATAL[args.case].encode() in data
            assert b'contained fault; returning' in data
            for forbidden in (b'battery-service: retired ',b'laptop-capture: completed=',b'laptop-capture: saved=',b'battery-native-query: outcome='):
                assert forbidden not in data
            image=ExfatFixture(volume.read_bytes())
            assert image.read('srvros-work/.workspace')==MARKER and image.read('srvros-work/anchor.txt')==ANCHOR
            result.update(outcome='contained fault reset',runtime_retired=False)
        else:
            if positive:
                result.update(check(data,vm=True))
                code=128 if args.case=='high' else 16 if args.case in ('early','os-darwin','os-other') else 255
                calls=5 if args.case.startswith('notify-') else 0
                notifications=2 if args.case=='notify-darwin' else 3 if args.case=='notify-other' else 0
                assert (result['code'],result['generation'],result['aml_calls'],result['notifications_drained'])==(code,0 if args.case=='early' else 2,calls,notifications)
            elif args.case=='no-event':
                assert b'outcome=not-exercised' in data and b'battery-query-return:' not in data
                result.update(outcome='not-exercised')
            else:
                assert b'battery: REFUSED platform/resource ownership preflight' in data
                assert b'battery-service: retired ' not in data
                result.update(outcome='native platform refused')
            assert data.count(b'laptop-capture: saved=yes unmounted=yes')==1
            assert data.index(b'laptop-capture: saved=yes unmounted=yes')<data.index(b'laptop-capture: returning to Ubuntu')
            (out/'exit.log').write_bytes(verify_image(volume.read_bytes(),data,positive))
            (out/'fsck.txt').write_text(check_clean_image(volume))
            allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
            (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
            result.update(exact_tree_metadata='PASS',offline_allocation='PASS',saved_log='PASS')
        (out/'workspace.exfat').write_bytes(volume.read_bytes())
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
