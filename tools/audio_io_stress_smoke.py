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
from app_timing import RECORD,validate_app_timing,saved_app_timing,lifecycle_receipts
from loop_timing import validate_loop_timing
from operation_timing import operation_snapshots


def checkpoint_durable(output, after, name, at):
    # New writers acknowledge only the exact completed log/device barrier.
    # Older immutable stages used the global workspace-flushed receipt.
    if b'workspace-log: durable snapshot=' in output:
        marker=b'workspace-log: durable snapshot='+name+b' monotonic_us='+at
        return re.search(re.escape(marker)+rb'\r?\n',output[after:]) is not None
    return b'workspace: flushed' in output[after:]


def completion_line(output):
    # UART/socket reads may end anywhere, including in PASS or unmounted=yes.
    match=re.search(rb'desktop-audio-io: result=[^\r\n]*\r?\n',output)
    return match.group(0) if match else None


def cache_retirement(output):
    rows=re.findall(rb'^desktop-cache-retired: version=1 bytes=([0-9]+) retained=([0-9]+) reuses=([0-9]+)\r?\n',output,re.M)
    if len(rows)!=1:return None
    used,retained,reuses=map(int,rows[0])
    if used or retained or not reuses:return None
    return dict(bytes=used,retained=retained,reuses=reuses)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage')
    parser.add_argument('--accel',choices=('tcg','kvm'),default='tcg')
    parser.add_argument('--air72',action='store_true',help='use Air7,2 synthetic ownership fixture; stages infer their own profile')
    parser.add_argument('--populated-home',action='store_true',help='Exercise Files startup with 64 existing Home files')
    parser.add_argument('--write-iops',type=int,default=0,help='Limit disposable VM USB write IOPS without pacing the guest workload')
    args=parser.parse_args()
    if args.write_iops<0 or (args.stage and (args.populated_home or args.write_iops)):parser.error('Load options apply only to the synthetic positive VM')
    root=Path(__file__).resolve().parents[1];out=root/('build/review/audio-io-exact' if args.stage else 'build/review/audio-io-populated' if args.populated_home else 'build/review/audio-io-auto');out=out.with_name(out.name+'-air72') if args.air72 else out;out.mkdir(parents=True,exist_ok=True)
    for name in ('result.json','tone.wav','lifecycle.json','app-timing.json','operations.json','loop-timing.json','automatic-screenshot.png','receipt.txt','allocation.log','overlap.json','cache-retirement.json'):
        (out/name).unlink(missing_ok=True)
    for pattern in ('saved-*.log','checkpoint-*.log'):
        for old in out.glob(pattern):old.unlink()
    stage=boot.STAGE/args.stage if args.stage else None
    manifest=boot.verify(stage) if stage else None
    volume_id=boot.DESKTOP_RE.fullmatch(manifest['config_version'])[1] if stage else '51525354'
    if stage:assert manifest['config_version'].startswith(('ram-usb-v74-','ram-usb-v80-'))
    air72=manifest['config_version'].startswith('ram-usb-v80-') if stage else args.air72
    if stage and args.air72 and not air72:parser.error('--air72 conflicts with staged profile')
    output=bytearray();consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-combined-') as directory:
        tmp=Path(directory);volume=tmp/'volume';stick=tmp/'stick'
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x'+volume_id,str(volume)],check=True,stdout=subprocess.DEVNULL)
        f=ExfatFixture(volume.read_bytes());marker=f'SRVROS-WORKSPACE-V1 {volume_id}\n'.encode()
        prior={'.workspace':marker,'anchor.txt':b'unchanged\n','audio-io-volume.conf':f'1 {int(time.time())+86400}\n'.encode()}
        if args.populated_home:prior.update({f'Existing-{n:03d}.txt':(f'Preserved file {n}\n'*64).encode() for n in range(64)})
        f.add_directory_files('srvros-work',prior)
        last=2048+len(f.data)//512-1;backup=last+33;entries=bytearray(128*128)
        entries[:128]=gpt_entry(uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7'),uuid.uuid4(),2048,last,'workspace')
        crc=binascii.crc32(entries)&0xffffffff;guid=uuid.uuid4();mbr=bytearray(512);mbr[450]=0xee;struct.pack_into('<II',mbr,454,1,backup);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as disk:
            disk.write(mbr);disk.write(gpt_header(1,backup,34,last,2,128,crc,guid));disk.write(entries);disk.seek(2048*512);disk.write(f.data);disk.write(entries);disk.write(gpt_header(backup,1,34,last,backup-32,128,crc,guid))
        original=stick.read_bytes();end=2048*512+len(f.data)
        (tmp/'battery.asl').write_text(('' if air72 else '#define BATTERY_AIR61 1\n')+'#define BATTERY_FAULT 83\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/('build/acpi-battery/vm.elf' if air72 else 'build/acpi-air61-battery/vm.elf');initramfs=root/'build/initramfs.tar'
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
        cmd=['qemu-system-x86_64','-accel',args.accel,'-M','q35','-m','2G' if stage else '1G',*boot_args,'-nic','none',*([] if stage else ['-acpitable',f'file={tmp}/battery.aml']),'-serial',f'unix:{tmp}/serial,server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0','-device','ich9-intel-hda','-device','hda-duplex,audiodev=silent,use-timer=off','-audiodev',f'wav,id=silent,path={out}/tone.wav,out.frequency=48000']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                overlap=[];previous=None;sample_at=0
                checkpoints=[];collected=set()
                def retain_checkpoint():
                    # Read only the disposable backing file, after the guest's
                    # existing flush. Never stop QEMU, inject input, add guest
                    # writes or change its two-file rotation/limits. Require
                    # identical bytes from two reads and the exact generation;
                    # all records still face the complete-phase validator.
                    if stage:return
                    directories=re.findall(rb'workspace-log: directory=/work/([^ ]+) periodic_seconds=10',output)
                    marks=list(re.finditer(rb'workspace-log: snapshot=(session-[ab]\.log) monotonic_us=(\d+)[^\r\n]*[\r\n]',output))
                    if len(directories)!=1 or not marks:return
                    mark=marks[-1];key=mark[1],mark[2]
                    if key in collected or not checkpoint_durable(output,mark.end(),mark[1],mark[2]):return
                    path=directories[0].decode()+'/'+mark[1].decode()
                    def read_snapshot():return ExfatFixture(stick.read_bytes()[2048*512:end]).read(path)
                    raw=read_snapshot()
                    assert raw==read_snapshot(),'live checkpoint changed during host collection'
                    receipt=b'workspace-log: snapshot='+mark[1]+b' monotonic_us='+mark[2]+b' '
                    assert receipt in raw,'live checkpoint generation mismatch'
                    checkpoints.append(raw);collected.add(key)
                    (out/f'checkpoint-{len(checkpoints):03d}.log').write_bytes(raw)
                def pump(seconds):
                    nonlocal previous,sample_at
                    output.extend(read_for(serial,seconds));(out/'serial.log').write_bytes(output)
                    retain_checkpoint()
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
                if args.write_iops:qmp.call('block_set_io_throttle',{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':args.write_iops})
                started=time.monotonic()
                while completion_line(output) is None:
                    assert time.monotonic()-started<180 and process.poll() is None
                    pump(.2)
                    assert b'battery: HALTED' not in output
                # Keep atomic failure diagnostics too: a clean automatic return
                # is not a workload pass. Read only after QEMU exits, before any
                # PASS assertion can discard the disposable backing image.
                process.wait(timeout=30)
                data=stick.read_bytes();assert data[:2048*512]==original[:2048*512] and data[end:]==original[end:]
                final=ExfatFixture(data[2048*512:end]);snapshots=list(checkpoints)
                if not stage:
                    session=re.findall(rb'workspace-log: directory=/work/([^ ]+) periodic_seconds=10',output)
                    assert len(session)==1
                    logdir=session[0].decode()
                    for name,_ in final.entries(final.read(logdir)):
                        if name.endswith('.log'):
                            raw=final.read(logdir+'/'+name);snapshots.append(raw)
                            (out/('saved-'+name)).write_bytes(raw)
                expected=b'desktop-audio-io: result=FAIL unmounted=yes' if stage else b'desktop-audio-io: result=PASS unmounted=yes'
                assert expected in completion_line(output),output[-2000:]
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
                    assert b'async=yes' in output
                    (out/'lifecycle.json').write_text(json.dumps(dict(cycles=int(lifecycle[1]),max_loop_us=int(lifecycle[2]),max_ready_us=int(lifecycle[3]),max_close_us=int(lifecycle[4]),errors=0,limit_us=500000),indent=2)+'\n')
                    assert re.search(rb'battery-service: retired epoch=1 .*heap=0 errors=0 denied=0',output)
                    assert b'runtime_retired=yes' in output and overlap
                for name,contents in prior.items():assert final.read('srvros-work/'+name)==contents
                if not stage:
                    directory=re.search(rb'audio-io: begin seconds=60 directory=/work/([^ ]+) automatic=yes',output)[1].decode()
                    receipt=final.read(directory+'/result.txt');assert b'result=PASS' in receipt
                    assert b' volume=1\n' in receipt
                    (out/'receipt.txt').write_bytes(receipt)
                    # UART writers may interleave; saved bootlog records are atomic.
                    ready,closed=lifecycle_receipts([bytes(output),*snapshots],int(lifecycle[1]))
                    assert max(row[2] for row in ready)==int(lifecycle[3])
                    assert max(row[2] for row in closed)==int(lifecycle[4])
                    # This VM retains the complete UART capture as well as rotated disk
                    # snapshots. Accept only whole console records, never repair
                    # interleaved text; the existing complete-phase validator
                    # still rejects every missing/conflicting lifecycle record.
                    console_records=b'\n'.join(line for line in output.splitlines() if RECORD.fullmatch(line))
                    timing=validate_app_timing(saved_app_timing([console_records,*snapshots]),[(int(cycle),int(pid)) for cycle,pid,_ in closed],files_window_runtime=True)
                    (out/'app-timing.json').write_text(json.dumps(timing,indent=2)+'\n')
                    worst=validate_loop_timing(snapshots,int(lifecycle[2]))
                    assert worst['version']==2, 'Missing wait/scheduler diagnostic companions'
                    (out/'loop-timing.json').write_text(json.dumps(worst,indent=2)+'\n')
                    # UART writers can interleave. Use the saved atomic exit
                    # snapshot for the single final ownership receipt.
                    retired=cache_retirement((out/'saved-exit.log').read_bytes())
                    assert retired is not None, 'Missing complete zero-reservation cache retirement/reuse receipt'
                    (out/'cache-retirement.json').write_text(json.dumps(retired,indent=2)+'\n')
                    operations=operation_snapshots(snapshots,require_final=True)
                    assert b'desktop-stress-exit: followup=PASS ' in output
                    assert operations['operations']['exiting']==1 and operations['operations']['queued']==0
                    assert b'desktop-stress-capture: result=PASS requested=1 saved=1 busy=0 normal_exit=1' in output
                    assert re.search(rb'audio-io: volume_override=1 ',output)
                    for kind in ('current','slowest'):
                        cleanup=operations['cleanup'][kind];assert cleanup['final']==1 and cleanup['pending']==0 and cleanup['total_us']>0
                    capture=operations['captures']['current']
                    assert capture['id']==1 and capture['busy']==0 and capture['frames']>0 and capture['phase']==12
                    assert operations['phases']['current'][7]['elapsed_us']>0 and operations['phases']['current'][10]['elapsed_us']>0
                    (out/'operations.json').write_text(json.dumps(operations,indent=2)+'\n')
                    images=[name for name,_ in final.entries(final.read('srvros-work/Screenshots')) if name.endswith('.bmp')]
                    assert len(images)==1
                    from PIL import Image
                    import io
                    image_bytes=final.read('srvros-work/Screenshots/'+images[0])
                    with Image.open(io.BytesIO(image_bytes)) as image:
                        image.load();assert image.size==(1280,800);image.save(out/'automatic-screenshot.png')

                volume.write_bytes(final.data);check_clean_image(volume)
                allocation=subprocess.check_output([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],text=True)
                assert 'leaked=0 fat_bitmap_mismatches=0 errors=0' in allocation
                (out/'allocation.log').write_text(allocation);(out/'overlap.json').write_text(json.dumps(overlap,indent=2)+'\n')
                result=dict(status='PASS',accelerator=args.accel,stage=args.stage,manifest_sha256=boot.digest(stage/'manifest.json') if stage else None,observed_overlap_windows=len(overlap),input_injected=False,automatic_return=True,source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],text=True)),native_acceptance=False,model='MacBookAir7,2' if air72 else 'MacBookAir6,1')
                result.update(app_timing_evidence='complete_console_records_and_retained_atomic_checkpoints',retained_checkpoints=len(checkpoints),populated_home=args.populated_home,write_iops=args.write_iops,prior_files_preserved=len(prior))
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)

if __name__=='__main__':main()
