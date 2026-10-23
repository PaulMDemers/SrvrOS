#!/usr/bin/env python3
"""Ordinary Home apps, periodic logs and battery ownership on disposable Q35 USB."""
import argparse, hashlib, json, os, re, struct, subprocess, tempfile, time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso, read_for
from vm_sockets import connect_unix
from exfat_test_image import ExfatFixture, check_clean_image
from battery_workspace_smoke import ANCHOR


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=('normal','cancel','profile-refusal','runtime-fault','refusal','missing-workspace','runtime-missing','opt-out'),default='normal')
    parser.add_argument('--boot-session',action='store_true',help='exercise ordinary opt-in boot, then reopen from monitor')
    parser.add_argument('--stage',help='exact v54 native stage; requires --boot-session --case profile-refusal on Q35')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    entry_refusal=args.case in ('missing-workspace','runtime-missing','opt-out')
    if entry_refusal and not args.boot_session:parser.error('entry refusal cases require --boot-session')
    if args.stage and (not args.boot_session or args.case!='profile-refusal'):parser.error('stage requires --boot-session --case profile-refusal')
    out=root/'build/review'/('battery-home-desktop-'+('boot-' if args.boot_session else '')+(args.stage+'-' if args.stage else '')+args.case);out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True);output=bytearray();consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-home-apps-') as directory:
        tmp=Path(directory);volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
        fault={'normal':83,'cancel':83,'profile-refusal':15,'runtime-fault':79,'refusal':83}.get(args.case,83)
        (tmp/'battery.asl').write_text(f'#define BATTERY_FAULT {fault}\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        uuid='51525354';stage=None;manifest=None
        if args.stage:
            import hardware_boot as boot
            assert boot.RELEASE_RE.fullmatch(args.stage),'invalid staged release'
            stage=boot.STAGE/args.stage;manifest=boot.verify(stage)
            assert manifest['config_version'].startswith('ram-usb-v54-desktop-')
            uuid=boot.DESKTOP_RE.fullmatch(manifest['config_version'])[1]
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x'+uuid,str(volume)],check=True,stdout=subprocess.DEVNULL)
        marker=f'SRVROS-WORKSPACE-V1 {uuid}\n'.encode()
        fixture=ExfatFixture(volume.read_bytes());fixture.add_directory_files('srvros-work',{'.workspace':marker,'anchor.txt':ANCHOR})
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as f:f.write(mbr);f.seek(2048*512);f.write(fixture.data)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='refusal' else 'vm.elf')
        if args.case=='runtime-missing':kernel=root/'build/kernel/srvros.elf'
        initramfs=root/'build/initramfs.tar'
        cmdline=f'srvros.storage=ram srvros.workspace-volume={uuid} srvros.desktop=session srvros.battery=on' if args.boot_session else None
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
        cmd=['qemu-system-x86_64','-M','q35','-m','512M',*boot_args,'-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
             '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
             '-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors);qmp=None
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=60):
                    nonlocal consumed
                    start=time.monotonic()
                    while marker not in output[consumed:]:
                        assert process.poll() is None and time.monotonic()-start<timeout,f'missing {marker!r}'
                        output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                        if not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':'sendkey '+keys});time.sleep(.3)
                def type_text(text):
                    mapping={' ':('spc',False),'/':('slash',False),'.':('dot',False),'-':('minus',False),'\n':('ret',False),'>':('dot',True)}
                    for c in text:
                        key,shift=mapping.get(c,(c.lower(),c.isupper()));qmp.key(key,shift)
                        output.extend(read_for(serial,.001))
                def launch(name,marker):
                    shortcut('meta_l-spc');type_text(name);qmp.key('ret');wait(marker)
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
                result={'case':args.case,'boot_session':args.boot_session,'stage':args.stage,'physical_acceptance':False}
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
                    wait(b'desktools: starting role=text');wait(b'deskd: opened pid=')
                    time.sleep(1);type_text('Ordinary Home desktop.');shortcut('ctrl-l');shortcut('ctrl-a')
                    type_text('/work/srvros-work/desktop-proof.txt');shortcut('ctrl-s');wait(b'picker ready mode=2')
                    qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                    launch('Files',b'desktools: starting role=files');wait(b'file-result role=files op=3 status=0')
                    launch('Terminal',b'deskterm: ready');time.sleep(1)
                    type_text('echo terminal-proof > /work/srvros-work/terminal-proof.txt\n')
                    time.sleep(2)
                    # A real shell job changes session and execs, then leaves two
                    # descendants alive after its original parent returns.
                    type_text('/fat/bin/lifetimeprobe --orphan > /work/srvros-work/orphan-proof.txt\n');time.sleep(2)
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
                    shortcut('ctrl-alt-esc');wait(b'battery-workspace-controls: complete=yes',90);wait(b'\nsrv> ')
                    data=bytes(output)
                    acquired=re.search(rb'battery-desktop-lifetime: acquired token=(\d+) root=(\d+)',data)
                    drained=re.search(rb'battery-desktop-lifetime: drained token=(\d+) members=0 peak=(\d+) revoked=yes',data)
                    released=re.search(rb'battery-desktop-lifetime: released token=(\d+) runtime_retired=yes',data)
                    assert acquired and drained and released and acquired[1]==drained[1]==released[1] and int(drained[2])>=8
                    assert acquired.end()<drained.start()<released.start()<data.index(b'battery-home-desktop: logger_retired=yes final_saved=yes')<data.index(b'battery-workspace-hold: released')
                    assert b'battery-controls: PASS' in data and b'logger_retired=yes final_saved=NO' not in data
                    result['peak_members']=int(drained[2])
                    if args.case not in ('profile-refusal','refusal'):
                        events=re.findall(rb'battery-monitor-events: epoch=1 .* events=(\d+).* final=1',data)
                        assert len(events)==1 and int(events[0])>=64
                        result['battery_events']=int(events[0])
                if args.boot_session and not entry_refusal and args.case!='runtime-fault':
                    # Restart using the real monitor entry with the same mount.
                    # The new owner must not reuse a live lifetime or logger.
                    serial.sendall(b'desktop battery\n')
                    wait(b'desktop-session: requested battery=on')
                    wait(b'desktools: starting role=text');wait(b'deskd: opened pid=')
                    time.sleep(4);shortcut('ctrl-alt-esc')
                    wait(b'desktop-session: returned complete=yes');wait(b'\nsrv> ')
                    tokens=re.findall(rb'battery-desktop-lifetime: acquired token=(\d+) root=',bytes(output))
                    assert len(tokens)==2 and tokens[0]!=tokens[1]
                    assert bytes(output).count(b'battery-workspace-hold: released')==2
                    assert bytes(output).count(b'battery-home-desktop: logger_retired=yes final_saved=yes')==2
                    result['restarted']=True
                if args.case not in ('runtime-fault','missing-workspace'):
                    serial.sendall(b'workspace unmount\n');wait(b'workspace: unmounted; USB writes disabled');wait(b'\nsrv> ')
                if stage:
                    serial.sendall(b'reboot\n');assert process.wait(timeout=30)==0
                    output.extend(read_for(serial,.1));result['monitor_reboot']=True
                    result['manifest_sha256']=hashlib.sha256((stage/'manifest.json').read_bytes()).hexdigest()
                    result['boot_path']='GRUB -> staged Limine -> native kernel'
                stop_process(process);disk=stick.read_bytes();assert disk[:512]==mbr and not any(disk[512:2048*512])
                if entry_refusal or (args.case=='refusal' and not args.boot_session):assert disk[2048*512:]==bytes(fixture.data)
                elif args.case!='runtime-fault':
                    final=ExfatFixture(disk[2048*512:]);assert final.read('srvros-work/.workspace')==marker and final.read('srvros-work/anchor.txt')==ANCHOR
                    assert final.read('srvros-work/desktop-proof.txt')==(b'Ordinary Home desktop. Pending save.' if args.case=='cancel' else b'Ordinary Home desktop.')
                    assert {n for n,_ in final.entries(final.chain(final.root))}=={'srvros-work','.srvros'}
                    assert {n for n,_ in final.entries(final.read('srvros-work'))}=={'.workspace','anchor.txt','desktop-proof.txt','terminal-proof.txt','orphan-proof.txt','Logs'}
                    assert final.read('srvros-work/terminal-proof.txt')==b'terminal-proof\n'
                    assert final.read('srvros-work/orphan-proof.txt')==b'lifetimeprobe: orphan setsid exec grandchild started\n'
                    sessions=list(final.entries(final.read('srvros-work/Logs')));assert len(sessions)==(2 if args.boot_session else 1)
                    if args.boot_session:
                        second='srvros-work/Logs/'+sessions[1][0]
                        assert {n for n,_ in final.entries(final.read(second))}=={'boot.log','exit.log'}
                        assert b'battery-desktop-lifetime: released' in final.read(second+'/exit.log')
                    folder='srvros-work/Logs/'+sessions[0][0];names={n for n,_ in final.entries(final.read(folder))}
                    assert names=={'boot.log','session-a.log','session-b.log','exit.log'}
                    exitlog=final.read(folder+'/exit.log');(out/'exit.log').write_bytes(exitlog)
                    assert b'battery-desktop-lifetime: released' in exitlog and b'workspace-log: snapshot=exit.log' in exitlog
                    result['saved_logs']=sorted(names)
                if args.case!='runtime-fault':
                    volume.write_bytes(disk[2048*512:]);(out/'fsck.txt').write_text(check_clean_image(volume))
                    allocation=subprocess.run([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],capture_output=True,text=True)
                    (out/'allocation.txt').write_text(allocation.stdout+allocation.stderr);assert allocation.returncode==0
                    result['offline_allocation']='PASS'
                result.update(status='PASS',source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
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
