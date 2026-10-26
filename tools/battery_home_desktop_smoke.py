#!/usr/bin/env python3
"""Ordinary Home apps, periodic logs and battery ownership on disposable Q35 USB."""
import argparse, hashlib, json, os, re, struct, subprocess, tempfile, time, binascii
import uuid as uuidlib
from mk_usb_image import gpt_entry, gpt_header
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso, read_for
from vm_sockets import connect_unix
from exfat_test_image import ExfatFixture, check_clean_image
from battery_workspace_smoke import ANCHOR
from serial_records import desktop_progress_end


def service_snapshot(data,audio):
    """Check final cached provider/PCM state, never an earlier boot snapshot."""
    result={}
    for provider in ('smc','thermal','audio','pcm'):
        rows=re.findall(rb'workspace-devices: '+provider.encode()+rb'_state=[^\r\n]*',data)
        assert rows,'missing '+provider+' checkpoint'
        fields={k.decode():int(v) for k,v in re.findall(rb'(\w+)=(\d+)(?= |$)',rows[-1])}
        result[provider]=fields
    assert result['smc']['smc_state']==result['thermal']['thermal_state']==1
    assert result['audio']['audio_state']==(2 if audio else 1)
    pcm=result['pcm']
    assert pcm['owner']==0 and pcm['queued']==0 and pcm['pcm_state']==(4 if audio else 0),pcm
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=('normal','cancel','profile-refusal','runtime-fault','refusal','missing-workspace','runtime-missing','opt-out','clock-missing','reopen-completed','reopen-busy'),default='normal')
    parser.add_argument('--capacity-auto',action='store_true',help='boot-only two-round 24-app capture; no injected keyboard input')
    parser.add_argument('--capacity-low-memory',action='store_true',help='384 MiB refusal/automatic-return fixture (capacity-auto only)')
    parser.add_argument('--mixed-apps',action='store_true',help='open 24 independent app processes with combined services, then retire/reopen')
    parser.add_argument('--air61',action='store_true',help='use exact Air6,1 ownership fixture/kernel')
    parser.add_argument('--linger-seconds',type=int,default=0,help='keep normal session live before normal menu exit')
    parser.add_argument('--workspace-table',choices=('mbr','gpt'),default='mbr')
    parser.add_argument('--automatic',action='store_true',help='boot-only two-session startup diagnostic without input')
    parser.add_argument('--boot-session',action='store_true',help='exercise ordinary opt-in boot, then reopen from monitor')
    parser.add_argument('--stage',help='exact v54/v55/v70 native stage; requires --boot-session --case profile-refusal on Q35')
    parser.add_argument("--services",action="store_true",help="exercise combined optional laptop services with ordinary battery ownership")
    parser.add_argument("--audio",action="store_true",help="attach HDA and play bundled PCM during the combined session")
    parser.add_argument('--workload-audio',action='store_true',help='play the full two-minute bundled sample while saving another document and cycling an app')
    parser.add_argument('--navigation-guide',action='store_true',help='verify combined window/dock/navigation checklist delivery')
    parser.add_argument('--lifecycle-guide',action='store_true',help='verify attended guide startup, normal return and ordinary reopen')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    if args.linger_seconds<0 or args.linger_seconds>900 or (args.linger_seconds and (not args.boot_session or args.case!='normal')):parser.error('linger requires normal boot session, 0..900 seconds')
    if args.navigation_guide and args.lifecycle_guide:parser.error("select one guide")
    if (args.lifecycle_guide or args.navigation_guide) and (not args.boot_session or not args.services or args.automatic or args.capacity_auto or args.mixed_apps or args.air61 or args.case not in ('normal','profile-refusal')):parser.error('lifecycle guide requires combined Air7,2 boot normal/profile-refusal')
    if args.workload_audio and (not args.audio or not args.lifecycle_guide):parser.error('workload audio requires --audio --lifecycle-guide')
    if args.air61 and not args.boot_session:parser.error('Air6,1 requires boot session')
    if args.case.startswith('reopen-') and (not args.boot_session or args.stage):parser.error('reopen deadline cases require unstaged boot session')
    if args.automatic and (not args.boot_session or not args.air61 or not args.services or args.case not in ('normal','cancel','profile-refusal','reopen-completed','reopen-busy')):parser.error('automatic requires combined Air6,1 boot and startup case')
    entry_refusal=args.case in ('missing-workspace','runtime-missing','opt-out','clock-missing')
    if args.audio and not args.services:parser.error('audio requires services')
    if args.capacity_low_memory and (not args.capacity_auto or args.stage):parser.error('low memory requires unstaged automatic capacity')
    if args.capacity_auto and (not args.boot_session or not args.services or args.automatic or args.mixed_apps or args.air61 or args.case not in ('normal','profile-refusal')):parser.error('capacity auto requires combined Air7,2 boot normal/profile-refusal')
    if args.mixed_apps and (not args.services or not args.boot_session or args.case not in ('normal','profile-refusal')):parser.error('mixed apps requires combined boot normal/profile-refusal')
    if args.services and not args.boot_session:parser.error('services requires boot-session')
    if entry_refusal and not args.boot_session:parser.error('entry refusal cases require --boot-session')
    if args.stage and (not args.boot_session or args.case!='profile-refusal'):parser.error('stage requires --boot-session --case profile-refusal')
    out=root/'build/review'/('battery-home-desktop-'+('air61-' if args.air61 else '')+('audio-' if args.audio else '')+('services-' if args.services else '')+('boot-' if args.boot_session else '')+(args.stage+'-' if args.stage else '')+args.case+('-navigation-guide' if args.navigation_guide else '-guide' if args.lifecycle_guide else '')+('-workload' if args.workload_audio else '')+('-automatic' if args.automatic else '')+('-mixed' if args.mixed_apps else '')+('-capacity-documents-low' if args.capacity_low_memory else '-capacity-documents' if args.capacity_auto else ''));out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True);output=bytearray();consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-home-apps-') as directory:
        tmp=Path(directory);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        fault={'normal':83,'cancel':83,'profile-refusal':15,'runtime-fault':79,'refusal':83,'reopen-completed':91,'reopen-busy':92}.get(args.case,83)
        (tmp/'battery.asl').write_text(('#define BATTERY_AIR61 1\n' if args.air61 else '')+f'#define BATTERY_FAULT {fault}\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        uuid='51525354';stage=None;manifest=None
        if args.stage:
            import hardware_boot as boot
            assert boot.RELEASE_RE.fullmatch(args.stage),'invalid staged release'
            stage=boot.STAGE/args.stage;manifest=boot.verify(stage)
            assert manifest['config_version'].startswith(('ram-usb-v54-desktop-','ram-usb-v55-desktop-','ram-usb-v70-desktop-','ram-usb-v71-desktop-','ram-usb-v72-desktop-','ram-usb-v73-desktop-','ram-usb-v75-desktop-','ram-usb-v76-desktop-'))
            assert args.services==manifest['config_version'].startswith(('ram-usb-v55-','ram-usb-v71-','ram-usb-v72-','ram-usb-v73-','ram-usb-v75-','ram-usb-v76-')),'services flag must match staged profile'
            assert args.air61==manifest['config_version'].startswith(('ram-usb-v70-','ram-usb-v71-','ram-usb-v72-')),'Air6,1 flag must match staged profile'
            assert args.automatic==manifest['config_version'].startswith('ram-usb-v72-'),'automatic flag must match stage'
            assert args.navigation_guide==manifest['config_version'].startswith('ram-usb-v76-'),'navigation flag must match stage'
            assert args.lifecycle_guide==manifest['config_version'].startswith('ram-usb-v75-'),'guide flag must match stage'
            assert args.capacity_auto==manifest['config_version'].startswith('ram-usb-v73-'),'capacity flag must match stage'
            uuid=boot.DESKTOP_RE.fullmatch(manifest['config_version'])[1]
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x'+uuid,str(volume)],check=True,stdout=subprocess.DEVNULL)
        marker=f'SRVROS-WORKSPACE-V1 {uuid}\n'.encode()
        fixture=ExfatFixture(volume.read_bytes());fixture.add_directory_files('srvros-work',{'.workspace':marker,'anchor.txt':ANCHOR})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as f:f.write(mbr);f.seek(2048*512);f.write(fixture.data)
        if args.workspace_table=='gpt':
            last=2048+len(fixture.data)//512-1;backup=last+33;entries=bytearray(128*128)
            entries[:128]=gpt_entry(uuidlib.UUID('ebd0a0a2-b9e5-4433-87c0-68b6b72699c7'),uuidlib.uuid4(),2048,last,'Workspace')
            crc=binascii.crc32(entries)&0xffffffff;guid=uuidlib.uuid4();mbr[450]=0xee;struct.pack_into('<II',mbr,454,1,0xffffffff)
            with stick.open('wb') as f:
                f.write(mbr);f.write(gpt_header(1,backup,34,last,2,128,crc,guid));f.write(entries)
                f.seek(2048*512);f.write(fixture.data);f.write(entries);f.write(gpt_header(backup,1,34,last,backup-32,128,crc,guid))
        original_disk=stick.read_bytes();volume_end=2048*512+len(fixture.data)
        kernel=root/('build/acpi-air61-battery' if args.air61 else 'build/acpi-battery')/('srvros.elf' if args.case=='refusal' else 'vm.elf')
        if args.case=='runtime-missing':kernel=root/'build/kernel/srvros.elf'
        initramfs=root/'build/initramfs.tar'
        cmdline=f'srvros.storage=ram srvros.workspace-volume={uuid} srvros.desktop=session srvros.battery=on' if args.boot_session else None
        if args.navigation_guide:cmdline += ' srvros.desktop-guide=window-navigation'
        if args.lifecycle_guide:cmdline += ' srvros.desktop-guide=application-lifecycle'
        if args.capacity_auto:cmdline += ' srvros.desktop-capacity-test=on'
        if args.automatic:cmdline += ' srvros.desktop-reopen-test=on'
        if args.services:cmdline += ' srvros.laptop-services=brightness srvros.audio=hda srvros.sensors=cpu srvros.smc=read-only' + ('' if args.air61 else ' srvros.usb-network=ecm')
        if args.case=='missing-workspace':cmdline=cmdline.replace(uuid,'61626364')
        if args.case=='opt-out':cmdline=cmdline.replace(' srvros.battery=on','')
        if stage:
            kernel=stage/'srvros.elf';initramfs=stage/'initramfs.tar'
            cmdline=re.search(r'^\s*cmdline: (.+)$',(stage/'limine.conf').read_text(),re.M)[1]
        if stage:
            # Rehearse the installed GRUB -> staged Limine path, not an ISO
            # rebuilt around the payload. Only the disposable ESP UUID differs.
            from hardware_boot_smoke import make_disk
            from uefi_usb_smoke import default_ovmf_path
            cfg=tmp/'grub.cfg'
            cfg.write_text('serial --unit=0 --speed=115200\nterminal_output console serial\n'
                           'echo SRVROS_GRUB_CHAINLOAD_TEST\nset default=srvros-current\n'+boot.grub_config('5352-5652',args.stage))
            subprocess.run(['grub-script-check',str(cfg)],check=True)
            loader=tmp/'BOOTX64.EFI'
            subprocess.run(['grub-mkstandalone','-O','x86_64-efi','--locales=','--fonts=',
                            '--modules=part_gpt fat chain search search_fs_uuid normal serial terminal',
                            '-o',str(loader),'boot/grub/grub.cfg='+str(cfg)],check=True)
            esp=tmp/'esp.img';make_disk(esp,loader,stage,args.stage)
            boot_args=['-drive',f'if=pflash,format=raw,readonly=on,file={default_ovmf_path()}',
                       '-drive',f'format=raw,file={esp}']
        else:
            iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,initramfs,cmdline)
            boot_args=['-cdrom',iso,'-boot','d']
        # Many separate GUI processes need their own address spaces and stacks.
        # Match exact-stage memory for the 24-app workload; retain 512M for the
        # ordinary service lifetime fixture.
        cmd=['qemu-system-x86_64','-M','q35,acpi=off' if args.case=='clock-missing' else 'q35','-m','384M' if args.capacity_low_memory else '2G' if stage or args.mixed_apps or args.capacity_auto else '512M',*boot_args,'-nic','none',
             *([] if stage or args.case=='clock-missing' else ['-acpitable',f'file={tmp}/battery.aml']),
             '-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
             '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
             '-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0']
        if args.audio:cmd += ['-device','ich9-intel-hda','-device','hda-duplex,audiodev=silent,use-timer=off','-audiodev','none,id=silent']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors);qmp=None
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=60):
                    nonlocal consumed
                    start=time.monotonic()
                    while True:
                        at=desktop_progress_end(output,marker,consumed)
                        if at is not None:consumed=at;return
                        if args.mixed_apps and (b'exec: scheduler thread table full' in output or b'exec: scheduler admission failed' in output or b'deskd: launch-failed' in output):raise RuntimeError('mixed-app resource admission failed; inspect serial evidence')
                        assert process.poll() is None and time.monotonic()-start<timeout,f'missing {marker!r}'
                        output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                        if not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':'sendkey '+keys});time.sleep(.3)
                def type_text(text):
                    mapping={' ':('spc',False),'/':('slash',False),'.':('dot',False),'-':('minus',False),'\n':('ret',False),'>':('dot',True)}
                    for c in text:
                        key,shift=mapping.get(c,(c.lower(),c.isupper()));qmp.key(key,shift)
                        output.extend(read_for(serial,.001))
                def launch(name,marker):
                    shortcut('meta_l-spc');type_text(name);qmp.key('ret');wait(marker)
                if args.capacity_auto:
                    expected=b'FAIL' if args.capacity_low_memory else b'PASS'
                    wait(b'desktop-reopen-test: result='+expected+b' unmounted=yes; returning to Ubuntu',540)
                    assert process.wait(timeout=30)==0
                    output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                    assert output.count(b'battery-desktop-lifetime: released')==2
                    assert b'battery: HALTED' not in output
                    if not args.capacity_low_memory:
                        assert output.count(b'desktop-capacity: rendered=24 processes=24')==2
                        assert output.count(b'document-capacity: result=PASS checkpoints=5 pooled=yes retired=1')==2
                        assert output.count(b'desktop-capacity: document-inputs=60 acknowledged=yes')==2
                        assert output.count(b'chooser-capture: PASS')==2
                        assert output.count(b'desktop-capacity: result=PASS launched=24 reaped=24 errors=0 screenshot=yes normal_exit=1')==2
                    else:
                        assert b'deskd: launch-failed' in output
                        assert b'desktop-capacity: result=PASS' not in output
                    disk=stick.read_bytes()
                    assert disk[:2048*512]==original_disk[:2048*512] and disk[volume_end:]==original_disk[volume_end:]
                    final=ExfatFixture(disk[2048*512:volume_end])
                    assert final.read('srvros-work/.workspace')==marker and final.read('srvros-work/anchor.txt')==ANCHOR
                    sessions=list(final.entries(final.read('srvros-work/Logs')));assert len(sessions)==2
                    for index,(session,_) in enumerate(sessions):
                        log=final.read('srvros-work/Logs/'+session+'/exit.log')
                        (out/(str(index)+'-exit.log')).write_bytes(log)
                        assert b'battery-desktop-lifetime: released' in log
                        assert b'desktop-capacity: result='+expected in log
                    if not args.capacity_low_memory:
                        folders=re.findall(rb'document-capacity: verified directory=/work/(\S+) files=5 bytes_each=2 chooser=PASS',output)
                        assert len(folders)==2 and folders[0]!=folders[1],folders
                        for folder in folders:
                            folder=folder.decode()
                            assert {n for n,_ in final.entries(final.read(folder))}=={f'text-{i}.txt' for i in range(1,6)}
                            for i in range(1,6):assert final.read(folder+f'/text-{i}.txt')==b'AA'
                        shots=list(final.entries(final.read('srvros-work/Screenshots')));assert len(shots)==2
                        for name,_ in shots:
                            data=final.read('srvros-work/Screenshots/'+name);assert data[:2]==b'BM'
                            (out/name).write_bytes(data)
                    volume.write_bytes(disk[2048*512:volume_end]);(out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result=dict(status='PASS',guest_result=expected.decode(),capacity_auto=True,windows_per_round=0 if args.capacity_low_memory else 24,rounds=2,
                        documents_per_round=0 if args.capacity_low_memory else 5,document_bytes='AA',
                        stage=args.stage,physical_acceptance=False,offline_allocation='PASS',automatic_return=True,
                        source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
                        source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),
                        payload_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (kernel,initramfs)})
                    if stage:
                        assert b'SRVROS_GRUB_CHAINLOAD_TEST' in output
                        result.update(manifest_sha256=hashlib.sha256((stage/'manifest.json').read_bytes()).hexdigest(),boot_path='GRUB -> staged Limine -> native kernel')
                    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return
                if args.automatic:
                    if args.case=='cancel':
                        wait(b'desktools: text history=multi-step');shortcut('ctrl-alt-esc')
                    if args.case=='reopen-busy':
                        wait(b'battery: HALTED operation deadline',100)
                        assert output.count(b'battery-desktop-lifetime: released')==1
                        assert b'desktop-reopen-test: result=' not in output
                        assert re.search(rb'battery-startup: kind=sleep-ms .*active=1 active_us=[1-9][0-9]* argument=6000',output)
                        assert process.poll() is None
                    else:
                        expected=b'FAIL' if args.case in ('profile-refusal','cancel') else b'PASS'
                        wait(b'desktop-reopen-test: result='+expected+b' unmounted=yes; returning to Ubuntu',120)
                        assert process.wait(timeout=30)==0
                        assert output.count(b'battery-desktop-lifetime: released')==2
                        if args.case!='profile-refusal':
                            if args.case!='cancel':assert output.count(b'battery-startup: kind=alloc-bytes')==2
                            if args.case!='cancel':assert output.count(b'battery: setup=complete')==2
                            assert b'battery: HALTED' not in output
                    stop_process(process)
                    disk=stick.read_bytes()
                    assert disk[:2048*512]==original_disk[:2048*512] and disk[volume_end:]==original_disk[volume_end:]
                    final=ExfatFixture(disk[2048*512:volume_end])
                    assert final.read('srvros-work/.workspace')==marker and final.read('srvros-work/anchor.txt')==ANCHOR
                    assert {n for n,_ in final.entries(final.read('srvros-work'))}=={'.workspace','anchor.txt','Logs'}
                    sessions=list(final.entries(final.read('srvros-work/Logs')));assert len(sessions)==2
                    for index,(session,_) in enumerate(sessions):
                        folder='srvros-work/Logs/'+session
                        names={n for n,_ in final.entries(final.read(folder))}
                        if index==1 and args.case=='reopen-busy':assert names=={'boot.log'}
                        else:
                            assert 'exit.log' in names
                            log=final.read(folder+'/exit.log');(out/(str(index)+'-exit.log')).write_bytes(log)
                            assert b'battery-desktop-lifetime: released' in log
                            if args.case=='cancel' and index==0:assert b'battery-reopen-test: sample' not in log
                            else:assert b'battery-reopen-test: sample' in log
                    volume.write_bytes(disk[2048*512:volume_end]);(out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result=dict(status='PASS',automatic=True,case=args.case,stage=args.stage,services=args.services,audio=args.audio,air61=args.air61,workspace_table=args.workspace_table,
                                physical_acceptance=False,offline_allocation='PASS',sessions=2,
                                source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
                                source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),
                                payload_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (kernel,initramfs)})
                    if stage:
                        assert b'SRVROS_GRUB_CHAINLOAD_TEST' in output
                        result.update(manifest_sha256=hashlib.sha256((stage/'manifest.json').read_bytes()).hexdigest(),boot_path='GRUB -> staged Limine -> native kernel')
                    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));return
                if entry_refusal:
                    wait(b'desktop-session: REFUSED');wait(b'\nsrv> ')
                    assert b'battery-workspace-hold: acquired' not in output
                    assert b'battery-monitor: auto_start=' not in output
                elif args.boot_session:
                    wait(b'desktop-session: requested battery=on')
                else:
                    wait(b'\nsrv> ')
                    serial.sendall(b'workspace mount 51525354\n');wait(b'workspace: ready at /work/srvros-work');wait(b'\nsrv> ')
                    serial.sendall(b'acpiruntime battery-home-desktop\n')
                if stage:assert b'SRVROS_GRUB_CHAINLOAD_TEST' in output
                result={'case':args.case,'boot_session':args.boot_session,'stage':args.stage,'physical_acceptance':False,'services':args.services,'audio':args.audio,'air61':args.air61,'workspace_table':args.workspace_table}
                if entry_refusal:
                    result['entry_refused']=True
                elif args.case=='refusal' and not args.boot_session:
                    wait(b'battery-home-desktop: REFUSED VM-only idle logger session');wait(b'\nsrv> ')
                    assert b'battery-workspace-hold: acquired' not in output
                elif args.case=='runtime-fault':
                    wait(b'battery: HALTED',180)
                    assert b'battery-workspace-hold: released' not in output and b'logger_retired=yes' not in output
                    result['retained_ownership']=True
                else:
                    if args.lifecycle_guide or args.navigation_guide:
                        wait(b'deskhelp: page=window-navigation-trial' if args.navigation_guide else b'deskhelp: page=lifecycle-trial');wait(b'deskd: opened pid=');time.sleep(1)
                        qmp.call('screendump',{'filename':str(out/'guide-start.ppm')})
                        qmp.key('end');time.sleep(1)
                        qmp.call('screendump',{'filename':str(out/'guide-return.ppm')})
                        launch('Text new app',b'desktools: starting role=text')
                        result['navigation_guide' if args.navigation_guide else 'lifecycle_guide']=True
                    else:wait(b'desktools: starting role=text')
                    wait(b'deskd: opened pid=')
                    time.sleep(1);type_text('Ordinary Home desktop.');shortcut('ctrl-l');shortcut('ctrl-a')
                    type_text('/work/srvros-work/desktop-proof.txt');shortcut('ctrl-s');wait(b'picker ready mode=2')
                    qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                    launch('Files',b'desktools: starting role=files');wait(b'file-result role=files op=3 status=0')
                    launch('Terminal',b'deskterm: ready');time.sleep(1)
                    if args.audio:
                        sample='workload.wav' if args.workload_audio else 'stereo.wav'
                        audio_begin=len(output);began=time.monotonic()
                        type_text('/fat/bin/playwav /fat/share/examples/'+sample+' > /work/srvros-work/audio-proof.txt\n')
                        if args.workload_audio:
                            wait(b'workspace-devices: pcm_state=2',30)
                            launch('Calculator new app',b'desktools: starting role=calculator');wait(b'deskd: opened pid=')
                            time.sleep(.5);shortcut('ctrl-w');wait(b'desktools: exited role=calculator failure=0')
                            launch('Text new app',b'desktools: starting role=text');wait(b'deskd: opened pid=')
                            time.sleep(.5);type_text('Saved during workload playback.');shortcut('ctrl-l');shortcut('ctrl-a')
                            type_text('/work/srvros-work/workload-proof.txt');shortcut('ctrl-s');wait(b'picker ready mode=2')
                            qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                            assert b'workspace-devices: pcm_state=4' not in output[audio_begin:],'playback ended before document I/O'
                            shortcut('ctrl-q');wait(b'desktools: exited role=text failure=0')
                            # Exact native stages omit the optional focus trace.
                            # Later terminal-proof bytes verify the selected window.
                            shortcut('meta_l-spc');type_text('Terminal window');qmp.key('ret');time.sleep(.5)
                            wait(b'workspace-devices: pcm_state=4',150)
                            result['workload_audio_wall_seconds']=round(time.monotonic()-began,3)
                            assert result['workload_audio_wall_seconds']>=115
                            result['workload_app_and_save_overlap']=True
                        else:time.sleep(9)
                    type_text('echo terminal-proof > /work/srvros-work/terminal-proof.txt\n')
                    time.sleep(2)
                    # A real shell job changes session and execs, then leaves two
                    # descendants alive after its original parent returns.
                    type_text('/fat/bin/lifetimeprobe --orphan > /work/srvros-work/orphan-proof.txt\n');time.sleep(2)
                    if args.mixed_apps:
                        # Three ordinary processes already own windows. Add 21
                        # separate apps, including file/image workers, under the
                        # same logger/battery/audio service lifetime as hardware.
                        roles=[('Calculator',b'desktools: starting role=calculator'),
                               ('Clock',b'deskclock: starting'),('Help',b'deskhelp: starting'),
                               ('Files',b'desktools: starting role=files'),
                               ('Preview',b'deskview: starting'),('Sketch',b'desksketch: starting'),
                               ('Text',b'desktools: starting role=text')]
                        for index in range(21):
                            name,ready=roles[index%len(roles)]
                            launch(name+' new app',ready);wait(b'deskd: opened pid=')
                        admitted=set(re.findall(rb'deskd: opened pid=(\d+)',bytes(output)))
                        assert len(admitted)==24,admitted
                        # Save an edited document after capacity growth, not just
                        # opening empty windows; verify its exact bytes offline.
                        type_text('Mixed services capacity.');shortcut('ctrl-l');shortcut('ctrl-a')
                        type_text('/work/srvros-work/mixed-proof.txt');shortcut('ctrl-s');wait(b'picker ready mode=2')
                        qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                        result['mixed_app_windows']=len(admitted)
                        qmp.call('screendump',{'filename':str(out/'mixed-apps.ppm')})
                    # Two distinct periodic writes while the desktop is live.
                    wait(b'workspace-log: snapshot=session-b.log',60)
                    qmp.call('screendump',{'filename':str(out/'desktop.ppm')})
                    if args.case=='cancel':
                        shortcut('meta_l-spc');type_text('Text window');qmp.key('ret');time.sleep(.5)
                        shortcut('ctrl-end');type_text(' Pending save.')
                        qmp.call('block_set_io_throttle',{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':40})
                        # Discard earlier receipts, then exit while a new save is
                        # submitted and no reply has been consumed by the app.
                        output.extend(read_for(serial,.1));consumed=len(output);pending_at=consumed
                        shortcut('ctrl-s');wait(b'desktools: file-submitted role=text op=2')
                        qmp.call('stop');output.extend(read_for(serial,.1))
                        assert b'file-result role=text op=2' not in output[pending_at:]
                        qmp.call('cont')
                        result['exit_with_submitted_save']=True
                    if args.linger_seconds:
                        began=time.monotonic()
                        while time.monotonic()-began<args.linger_seconds:
                            assert process.poll() is None
                            output.extend(read_for(serial,.2));(out/'serial.log').write_bytes(output)
                            assert b'battery: HALTED' not in output and b'desktop-session: returned' not in output
                        stamps=[int(v) for v in re.findall(rb'workspace-log: snapshot=session-[ab]\.log monotonic_us=(\d+)',output)]
                        assert stamps[-1]-stamps[0]>=(args.linger_seconds-15)*1000000
                        result['live_log_span_us']=stamps[-1]-stamps[0]
                    if args.case=='cancel':shortcut('ctrl-alt-esc')
                    else:
                        shortcut('f10');shortcut('end');shortcut('ret')
                        # Terminal defaults to Cancel while its shell is live.
                        # Allow the dialog to render, then confirm Close. Serial
                        # close-request lines can be lost amid concurrent output;
                        # the complete/drained/released checks below prove exit.
                        time.sleep(2);shortcut('shift-tab');shortcut('ret')
                    wait(b'battery-workspace-controls: complete=yes',90);wait(b'\nsrv> ')
                    data=bytes(output)
                    acquired=re.search(rb'battery-desktop-lifetime: acquired token=(\d+) root=(\d+)',data)
                    drained=re.search(rb'battery-desktop-lifetime: drained token=(\d+) members=0 peak=(\d+) revoked=yes',data)
                    released=re.search(rb'battery-desktop-lifetime: released token=(\d+) runtime_retired=yes',data)
                    assert acquired and drained and released and acquired[1]==drained[1]==released[1] and int(drained[2])>=8
                    assert acquired.end()<drained.start()<released.start()<data.index(b'battery-home-desktop: logger_retired=yes final_saved=yes')<data.index(b'battery-workspace-hold: released')
                    assert b'battery-controls: PASS' in data and b'logger_retired=yes final_saved=NO' not in data
                    result['peak_members']=int(drained[2])
                    if args.services:
                        assert (b'desktop-session: providers brightness=1 audio=1 thermal=1 smc=1 ethernet=' + (b'0' if args.air61 else b'1')) in data
                        result['providers_at_exit']=service_snapshot(data,args.audio)
                        result['sensors_unavailable']=True
                        result['audio_ready']=args.audio
                        result['pcm_owner_retired']=True
                    if args.case not in ('profile-refusal','refusal'):
                        events=re.findall(rb'battery-monitor-events: epoch=1 .* events=(\d+).* final=1',data)
                        assert len(events)==1 and int(events[0])>=64
                        result['battery_events']=int(events[0])
                if args.boot_session and not entry_refusal and args.case!='runtime-fault':
                    # Restart using the real monitor entry with the same mount.
                    # The new owner must not reuse a live lifetime or logger.
                    serial.sendall(b'desktop battery\n')
                    wait(b'desktop-session: requested battery=on')
                    if args.case=='reopen-busy':
                        wait(b'battery: HALTED operation deadline',30)
                        assert bytes(output).count(b'battery-workspace-hold: released')==1
                        assert bytes(output).count(b'battery-desktop-lifetime: released')==1
                        assert b'battery-vm: second setup delay armed' in output
                        result['reopen_retained_ownership']=True
                    else:
                        wait(b'desktools: starting role=text');wait(b'deskd: opened pid=')
                        time.sleep(4);shortcut('ctrl-alt-esc')
                        wait(b'desktop-session: returned complete=yes');wait(b'\nsrv> ')
                        tokens=re.findall(rb'battery-desktop-lifetime: acquired token=(\d+) root=',bytes(output))
                        assert len(tokens)==2 and tokens[0]!=tokens[1]
                        assert bytes(output).count(b'battery-workspace-hold: released')==2
                        assert bytes(output).count(b'battery-home-desktop: logger_retired=yes final_saved=yes')==2
                        result['restarted']=True
                        if args.services:result['providers_after_reopen']=service_snapshot(bytes(output),args.audio)
                        if args.case=='reopen-completed':
                            assert b'battery-vm: reopen owner delayed after pending drain' in output
                            assert b'battery-vm: reopen owner resumed beyond deadline' in output
                            assert b'battery: HALTED' not in output
                            result['completed_after_stale_poll']=True
                if args.case not in ('runtime-fault','missing-workspace','reopen-busy'):
                    serial.sendall(b'workspace unmount\n');wait(b'workspace: unmounted; USB writes disabled');wait(b'\nsrv> ')
                if stage:
                    serial.sendall(b'reboot\n');assert process.wait(timeout=30)==0
                    output.extend(read_for(serial,.1));result['monitor_reboot']=True
                    result['manifest_sha256']=hashlib.sha256((stage/'manifest.json').read_bytes()).hexdigest()
                    result['boot_path']='GRUB -> staged Limine -> native kernel'
                stop_process(process);disk=stick.read_bytes();assert disk[:2048*512]==original_disk[:2048*512] and disk[volume_end:]==original_disk[volume_end:]
                if entry_refusal or (args.case=='refusal' and not args.boot_session):assert disk[2048*512:volume_end]==bytes(fixture.data)
                elif args.case!='runtime-fault':
                    final=ExfatFixture(disk[2048*512:volume_end]);assert final.read('srvros-work/.workspace')==marker and final.read('srvros-work/anchor.txt')==ANCHOR
                    assert final.read('srvros-work/desktop-proof.txt')==(b'Ordinary Home desktop. Pending save.' if args.case=='cancel' else b'Ordinary Home desktop.')
                    assert {n for n,_ in final.entries(final.chain(final.root))}=={'srvros-work','.srvros'}
                    assert {n for n,_ in final.entries(final.read('srvros-work'))}==({'.workspace','anchor.txt','desktop-proof.txt','terminal-proof.txt','orphan-proof.txt','Logs'}|({'audio-proof.txt'} if args.audio else set())|({'mixed-proof.txt'} if args.mixed_apps else set())|({'workload-proof.txt'} if args.workload_audio else set()))
                    if args.mixed_apps:assert final.read('srvros-work/mixed-proof.txt')==b'Mixed services capacity.'
                    if args.audio:
                        audio=final.read('srvros-work/audio-proof.txt');(out/'audio-proof.txt').write_bytes(audio)
                        expected=23040000 if args.workload_audio else 1152000
                        assert f'playwav: complete bytes={expected} underruns=0'.encode() in audio
                        if args.workload_audio:assert final.read('srvros-work/workload-proof.txt')==b'Saved during workload playback.'
                        result['pcm_playback']='PASS'
                    assert final.read('srvros-work/terminal-proof.txt')==b'terminal-proof\n'
                    assert final.read('srvros-work/orphan-proof.txt')==b'lifetimeprobe: orphan setsid exec grandchild started\n'
                    sessions=list(final.entries(final.read('srvros-work/Logs')));assert len(sessions)==(2 if args.boot_session else 1)
                    if args.boot_session:
                        second='srvros-work/Logs/'+sessions[1][0]
                        assert {n for n,_ in final.entries(final.read(second))}==({'boot.log'} if args.case=='reopen-busy' else {'boot.log','exit.log'})
                        if args.case!='reopen-busy':assert b'battery-desktop-lifetime: released' in final.read(second+'/exit.log')
                    folder='srvros-work/Logs/'+sessions[0][0];names={n for n,_ in final.entries(final.read(folder))}
                    assert names=={'boot.log','session-a.log','session-b.log','exit.log'}
                    exitlog=final.read(folder+'/exit.log');(out/'exit.log').write_bytes(exitlog)
                    assert b'battery-desktop-lifetime: released' in exitlog and b'workspace-log: snapshot=exit.log' in exitlog
                    result['saved_logs']=sorted(names)
                if args.case!='runtime-fault':
                    volume.write_bytes(disk[2048*512:volume_end]);(out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result['offline_allocation']='PASS'
                result.update(stage_source=manifest['git_head'] if stage else None,status='PASS',source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
                              source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),
                              payload_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (kernel,initramfs)})
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                if not (out/'result.json').exists() and qmp and process.poll() is None:
                    try:
                        qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                        (out/'failure-registers.txt').write_text(str(qmp.call('human-monitor-command',{'command-line':'info registers'})))
                        symbols=subprocess.check_output(['nm','-a',str(kernel)],text=True)
                        uart=re.search(r'^([0-9a-f]+) [a-zA-Z] serial_ready$',symbols,re.M)
                        if uart:(out/'failure-uart.txt').write_text(str(qmp.call('human-monitor-command',{'command-line':'x /1bx 0x'+uart[1]})))
                    except Exception:pass
                (out/'serial.log').write_bytes(output);stop_process(process);(out/'last-workspace.img').write_bytes(stick.read_bytes())


if __name__=='__main__':main()
