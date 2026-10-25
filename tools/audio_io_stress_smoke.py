#!/usr/bin/env python3
"""Automatic PCM/verified filesystem stress, battery retirement and reboot on Q35."""
import argparse,binascii,json,os,re,struct,subprocess,tempfile,time,uuid
from pathlib import Path
import hardware_boot as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from exfat_test_image import ExfatFixture,check_clean_image
from mk_usb_image import gpt_entry,gpt_header
from vm_sockets import connect_unix


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage');args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=root/('build/review/audio-io-exact' if args.stage else 'build/review/audio-io-auto');out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True);(out/'tone.wav').unlink(missing_ok=True)
    stage=boot.STAGE/args.stage if args.stage else None
    manifest=boot.verify(stage) if stage else None
    volume_id=boot.DESKTOP_RE.fullmatch(manifest['config_version'])[1] if stage else '51525354'
    if stage:assert manifest['config_version'].startswith('ram-usb-v74-')
    output=bytearray();consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-combined-') as directory:
        tmp=Path(directory);volume=tmp/'volume';stick=tmp/'stick'
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x'+volume_id,str(volume)],check=True,stdout=subprocess.DEVNULL)
        f=ExfatFixture(volume.read_bytes());marker=f'SRVROS-WORKSPACE-V1 {volume_id}\n'.encode()
        f.add_directory_files('srvros-work',{'.workspace':marker,'anchor.txt':b'unchanged\n'})
        last=2048+len(f.data)//512-1;backup=last+33;entries=bytearray(128*128)
        entries[:128]=gpt_entry(uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7'),uuid.uuid4(),2048,last,'workspace')
        crc=binascii.crc32(entries)&0xffffffff;guid=uuid.uuid4();mbr=bytearray(512);mbr[450]=0xee;struct.pack_into('<II',mbr,454,1,backup);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as disk:
            disk.write(mbr);disk.write(gpt_header(1,backup,34,last,2,128,crc,guid));disk.write(entries);disk.seek(2048*512);disk.write(f.data);disk.write(entries);disk.write(gpt_header(backup,1,34,last,backup-32,128,crc,guid))
        original=stick.read_bytes();end=2048*512+len(f.data)
        (tmp/'battery.asl').write_text('#define BATTERY_AIR61 1\n#define BATTERY_FAULT 83\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-air61-battery/vm.elf';initramfs=root/'build/initramfs.tar'
        cmdline=f'srvros.storage=ram srvros.workspace-volume={volume_id} srvros.desktop=session srvros.battery=on srvros.desktop-audio-io=smoke srvros.laptop-services=brightness srvros.audio=hda srvros.sensors=cpu srvros.smc=read-only'
        if stage:
            from hardware_boot_smoke import make_disk
            from uefi_usb_smoke import default_ovmf_path
            cfg=tmp/'grub.cfg'
            cfg.write_text('serial --unit=0 --speed=115200\nterminal_output console serial\necho SRVROS_GRUB_CHAINLOAD_TEST\nset default=srvros-current\n'+boot.grub_config('5352-5652',args.stage))
            subprocess.run(['grub-script-check',str(cfg)],check=True)
            loader=tmp/'BOOTX64.EFI'
            subprocess.run(['grub-mkstandalone','-O','x86_64-efi','--locales=','--fonts=','--modules=part_gpt fat chain search search_fs_uuid normal serial terminal','-o',str(loader),'boot/grub/grub.cfg='+str(cfg)],check=True)
            esp=tmp/'esp.img';make_disk(esp,loader,stage,args.stage)
            boot_args=['-drive',f'if=pflash,format=raw,readonly=on,file={default_ovmf_path()}','-drive',f'format=raw,file={esp}']
        else:
            iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,initramfs,cmdline)
            boot_args=['-cdrom',iso,'-boot','d']
        cmd=['qemu-system-x86_64','-M','q35','-m','2G' if stage else '1G',*boot_args,'-nic','none',*([] if stage else ['-acpitable',f'file={tmp}/battery.aml']),'-serial',f'unix:{tmp}/serial,server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0','-device','ich9-intel-hda','-device','hda-duplex,audiodev=silent,use-timer=off','-audiodev',f'wav,id=silent,path={out}/tone.wav,out.frequency=48000']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                overlap=[];previous=None;sample_at=0
                def pump(seconds):
                    nonlocal previous,sample_at
                    output.extend(read_for(serial,seconds));(out/'serial.log').write_bytes(output)
                    now=time.monotonic()
                    if now-sample_at>=.2 and b'desktop-audio-io: result=' not in output:
                        sample_at=now
                        written=next(x['stats']['wr_bytes'] for x in qmp.call('query-blockstats') if x.get('device')=='stick')
                        pcm=(out/'tone.wav').stat().st_size if (out/'tone.wav').exists() else 0
                        if previous and pcm>previous[1] and written-previous[0]>=65536:
                            with (out/'tone.wav').open('rb') as capture:
                                capture.seek(max(44,previous[1]));audible=any(capture.read(pcm-max(44,previous[1])))
                            if audible:overlap.append(dict(usb_written=written-previous[0],pcm_bytes=pcm-previous[1]))
                        previous=(written,pcm)
                def wait(marker,timeout=90):
                    nonlocal consumed
                    start=time.monotonic()
                    while marker not in output[consumed:]:
                        assert process.poll() is None and time.monotonic()-start<timeout,f'missing {marker!r}'
                        pump(.1)
                        if not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                    consumed=output.index(marker,consumed)+len(marker)
                wait(b'deskd: ready;',120)
                started=time.monotonic()
                while b'desktop-audio-io: result=' not in output:
                    assert time.monotonic()-started<180 and process.poll() is None
                    pump(.2)
                    assert b'battery: HALTED' not in output
                expected=b'desktop-audio-io: result=FAIL unmounted=yes' if stage else b'desktop-audio-io: result=PASS unmounted=yes'
                assert expected in output,output[-2000:]
                if stage:
                    assert b'SRVROS_GRUB_CHAINLOAD_TEST' in output
                    assert b'audio-io: begin' not in output # exact native profile refuses Q35
                else:
                    assert re.search(rb'audio-io: result=PASS seconds=60 .*underruns=0 worker_status=0',output)
                    lifecycle=re.search(rb'desktop-life: result=PASS cycles=(\d+) max_loop_us=(\d+) max_ready_us=(\d+) max_close_us=(\d+) errors=0 limit_us=500000 first_us=(\d+) last_us=(\d+)',output)
                    assert lifecycle and int(lifecycle[1])>=6 and 0<int(lifecycle[2])<=500000
                    assert int(lifecycle[6])-int(lifecycle[5])>=40000000
                    pcm_window=re.search(rb'audio-io: result=PASS seconds=60 start_us=(\d+) end_us=(\d+)',output)
                    assert pcm_window and int(pcm_window[1])<=int(lifecycle[5])<int(lifecycle[6])<=int(pcm_window[2])
                    ready=re.findall(rb'desktop-life: ready cycle=(\d+) pid=(\d+) elapsed_us=(\d+)',output)
                    closed=re.findall(rb'desktop-life: closed cycle=(\d+) pid=(\d+) elapsed_us=(\d+) errors=0',output)
                    assert len(ready)==len(closed)==int(lifecycle[1])
                    assert [(a,b) for a,b,_ in ready]==[(a,b) for a,b,_ in closed]
                    assert len(set(pid for _,pid,_ in ready))==len(ready)
                    assert b'async=yes' in output
                    (out/'lifecycle.json').write_text(json.dumps(dict(cycles=int(lifecycle[1]),max_loop_us=int(lifecycle[2]),max_ready_us=int(lifecycle[3]),max_close_us=int(lifecycle[4]),errors=0,limit_us=500000),indent=2)+'\n')
                    assert re.search(rb'battery-service: retired epoch=1 .*heap=0 errors=0 denied=0',output)
                    assert b'runtime_retired=yes' in output and overlap
                process.wait(timeout=30)
                data=stick.read_bytes();assert data[:2048*512]==original[:2048*512] and data[end:]==original[end:]
                final=ExfatFixture(data[2048*512:end]);assert final.read('srvros-work/anchor.txt')==b'unchanged\n'
                if not stage:
                    directory=re.search(rb'audio-io: begin seconds=60 directory=/work/([^ ]+) automatic=yes',output)[1].decode()
                    receipt=final.read(directory+'/result.txt');assert b'result=PASS' in receipt
                    (out/'receipt.txt').write_bytes(receipt)
                volume.write_bytes(final.data);check_clean_image(volume)
                allocation=subprocess.check_output([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],text=True)
                assert 'leaked=0 fat_bitmap_mismatches=0 errors=0' in allocation
                (out/'allocation.log').write_text(allocation);(out/'overlap.json').write_text(json.dumps(overlap,indent=2)+'\n')
                result=dict(status='PASS',stage=args.stage,manifest_sha256=boot.digest(stage/'manifest.json') if stage else None,observed_overlap_windows=len(overlap),input_injected=False,automatic_return=True,source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],text=True)),native_acceptance=False)
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)

if __name__=='__main__':main()
