#!/usr/bin/env python3
"""Shorten pacing only: real shell workload, PCM, Text/Files and battery on Q35."""
import argparse,binascii,json,os,re,struct,subprocess,tempfile,time,uuid
from pathlib import Path
from desktop_combined_workload import files,verify
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from exfat_test_image import ExfatFixture,check_clean_image
from mk_usb_image import gpt_entry,gpt_header
from vm_sockets import connect_unix


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--full-round',action='store_true');args=parser.parse_args()
    rounds,seconds,bursts=(1,60,40) if args.full_round else (2,6,2)
    root=Path(__file__).resolve().parents[1];out=root/('build/review/desktop-combined-workload-full' if args.full_round else 'build/review/desktop-combined-workload');out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True);(out/'tone.wav').unlink(missing_ok=True);run='0123456789abcdef';stem='combined-'+run
    inputs=files(run,rounds,1,seconds,bursts)
    inputs['combined-proof.sh']=(f'cd /work/srvros-work || exit 1\nsh {stem}.sh > trial-output.txt 2>&1\necho $? > first-status.txt\nsh {stem}.sh > refusal.txt\necho $? > refusal-status.txt\nsync\n').encode()
    output=bytearray();consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-combined-') as directory:
        tmp=Path(directory);volume=tmp/'volume';stick=tmp/'stick'
        with volume.open('wb') as f:f.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
        f=ExfatFixture(volume.read_bytes());marker=b'SRVROS-WORKSPACE-V1 51525354\n'
        f.add_directory_files('srvros-work',{'.workspace':marker,'anchor.txt':b'unchanged\n',**inputs})
        last=2048+len(f.data)//512-1;backup=last+33;entries=bytearray(128*128)
        entries[:128]=gpt_entry(uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7'),uuid.uuid4(),2048,last,'workspace')
        crc=binascii.crc32(entries)&0xffffffff;guid=uuid.uuid4();mbr=bytearray(512);mbr[450]=0xee;struct.pack_into('<II',mbr,454,1,backup);mbr[510:]=b'\x55\xaa'
        with stick.open('wb') as disk:
            disk.write(mbr);disk.write(gpt_header(1,backup,34,last,2,128,crc,guid));disk.write(entries);disk.seek(2048*512);disk.write(f.data);disk.write(entries);disk.write(gpt_header(backup,1,34,last,backup-32,128,crc,guid))
        original=stick.read_bytes();end=2048*512+len(f.data)
        (tmp/'battery.asl').write_text('#define BATTERY_AIR61 1\n#define BATTERY_FAULT 83\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-air61-battery/vm.elf';initramfs=root/'build/initramfs.tar'
        cmdline='srvros.storage=ram srvros.workspace-volume=51525354 srvros.desktop=session srvros.battery=on srvros.laptop-services=brightness srvros.audio=hda srvros.sensors=cpu srvros.smc=read-only'
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,initramfs,cmdline)
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none','-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0','-device','ich9-intel-hda','-device','hda-duplex,audiodev=silent,use-timer=off','-audiodev',f'wav,id=silent,path={out}/tone.wav,out.frequency=48000']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                overlap=[];previous=None;sample_at=0
                def pump(seconds):
                    nonlocal previous,sample_at
                    output.extend(read_for(serial,seconds));(out/'serial.log').write_bytes(output)
                    now=time.monotonic()
                    if now-sample_at>=.2:
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
                def key(keys):qmp.call('human-monitor-command',{'command-line':'sendkey '+keys});pump(.3)
                def text(s):
                    mapping={' ':'spc','/':'slash','.':'dot','-':'minus','\n':'ret'}
                    for c in s:qmp.key(mapping.get(c,c.lower()),c.isupper());pump(.001)
                def launch(name,marker):key('meta_l-spc');text(name+' new app');key('ret');wait(marker)
                wait(b'desktools: starting role=text');wait(b'deskd: opened pid=')
                launch('Terminal',b'deskterm: ready');pump(1);text('sh /work/srvros-work/combined-proof.sh\n');started=time.monotonic()
                launch('Text',b'desktools: starting role=text');pump(1);text('Concurrent combined trial.');key('ctrl-l');key('ctrl-a');text('/work/srvros-work/combined-note.txt');key('ctrl-s');wait(b'picker ready mode=2');key('ret');wait(b'file-result role=text op=2 status=0')
                launch('Files',b'desktools: starting role=files');wait(b'file-result role=files op=3 status=0')
                while time.monotonic()-started<rounds*(seconds+1)+30:
                    pump(.2);assert b'battery: HALTED' not in output
                qmp.call('screendump',{'filename':str(out/'desktop.ppm')})
                key('ctrl-alt-esc');wait(b'desktop-session: returned complete=yes');wait(b'\nsrv> ')
                serial.sendall(b'workspace unmount\n');wait(b'workspace: unmounted; USB writes disabled');wait(b'\nsrv> ')
                assert re.search(rb'battery-service: retired epoch=1 .*heap=0 errors=0 denied=0',output)
                assert b'runtime_retired=yes' in output
                stop_process(process);data=stick.read_bytes();assert data[:2048*512]==original[:2048*512] and data[end:]==original[end:]
                final=ExfatFixture(data[2048*512:end]);read=lambda name:final.read('srvros-work/'+name)
                evidence={}
                for name in ['trial-output.txt','first-status.txt','refusal-status.txt','refusal.txt',stem+'/io.txt',stem+'/rounds.txt',stem+'/audio-1.txt',stem+'/audio-2.txt',stem+'/complete.txt']:
                    try:evidence[name]=read(name).decode(errors='replace')
                    except Exception as e:evidence[name]=repr(e)
                (out/'guest-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
                proof=verify(read,run,rounds,1,seconds,bursts)
                assert read('first-status.txt')==b'0\n' and read('refusal-status.txt')==b'1\n'
                assert read('refusal.txt')==b'REFUSED existing combined trial\n'
                assert read('combined-note.txt')==b'Concurrent combined trial.' and read('anchor.txt')==b'unchanged\n' and read('.workspace')==marker
                volume.write_bytes(final.data);check_clean_image(volume)
                allocation=subprocess.check_output([str(root/'build/tests/exfat-driver'),str(volume),'allocation-check'],text=True)
                assert 'leaked=0 fat_bitmap_mismatches=0 errors=0' in allocation
                (out/'allocation.log').write_text(allocation)
                assert overlap,'No observed PCM-output/USB-write overlap'
                (out/'overlap.json').write_text(json.dumps(overlap,indent=2)+'\n')
                result=dict(status='PASS',observed_overlap_windows=len(overlap),workload=proof,rerun_refusal=True,concurrent_text_files=True,clean_retirement=True,source=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],text=True)),native_acceptance=False)
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)

if __name__=='__main__':main()
