#!/usr/bin/env python3
"""Exercise RAM compositor/battery overlap and cleanup on a disposable Q35 VM."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import struct
import tempfile
import time

from acpi_battery_smoke import check_sci_records
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso, read_for
from serial_records import complete_lines
from vm_sockets import connect_unix


def check_capture(data, text=False):
    """Require actual overlapping intervals and completed client presentations."""
    data=complete_lines(data)
    for prefix in (b'battery-desktop: started ',b'battery-desktop: PASS ',b'desktop-coexist: completed=',b'battery-service: retired '):
        assert sum(line.startswith(prefix) for line in data.splitlines())==1,'Missing or duplicate record'
    starts=re.findall(rb'battery-desktop: started pid=(\d+) epoch=(\d+) generation=(\d+) start_us=(\d+) storage=ram',data)
    ends=re.findall(rb'battery-desktop: PASS outcome=complete epoch=(\d+) first_gen=(\d+) last_gen=(\d+) start_us=(\d+) end_us=(\d+) reads=(\d+) busy_reads=(\d+) process_status=0; retired=yes',data)
    ui=re.findall(rb'desktop-coexist: completed=yes start_us=(\d+) end_us=(\d+) loops=(\d+) max_gap_us=(\d+) generated=(\d+) presented=(\d+) frames=(\d+) storage=ram',data)
    assert len(starts)==len(ends)==len(ui)==1,'Missing or repeated completion'
    pid,epoch,generation,start=map(int,starts[0])
    ep,first,last,begin,end,reads,busy=map(int,ends[0])
    ustart,uend,loops,gap,sent,shown,frames=map(int,ui[0])
    assert pid>0 and epoch==ep and generation==first and start==begin
    assert last>=first+10 and reads>=busy>0
    assert begin<=ustart<uend<=end and uend-ustart>=60000000
    assert loops>55 and 0<gap<=end-begin and sent==shown>=55 and frames>=shown
    if text:
        assert data.count(b'desktools: exited role=text failure=0')==1
        assert b'desktools: exited role=text failure=1' not in data
    else:
        assert b'deskclient: exited ' in data and b'deskclient: FAIL' not in data
    assert b'deskd: exited ' in data and b'deskd: FAIL' not in data
    assert b'battery-desktop: FAIL' not in data and b'battery: HALTED' not in data
    assert re.search(rb'battery-service: retired epoch='+str(epoch).encode()+rb' attempts=\d+ rejected=0 queries=\d+ notifications=\d+ attached=1 detached=1 heap=0 errors=0 denied=0\r?$',data,re.MULTILINE)
    phases=check_sci_records(data)
    assert any(p['snapshot']=='retired' and p['epoch']==epoch for p in phases)
    return dict(status='PASS',epoch=epoch,first_generation=first,last_generation=last,
                start_us=begin,end_us=end,desktop_start_us=ustart,desktop_end_us=uend,
                loops=loops,max_loop_gap_us=gap,generated=sent,presented=shown,frames=frames,
                physical_input='not tested',storage='RAM only')


def check_text_capture(data, injected=False):
    data=complete_lines(data)
    result=check_capture(data,text=True)
    rows=re.findall(rb'^text-coexist: checkpoint serial=(\d+) length=(\d+) caret=(\d+) anchor=(\d+) clipboard_busy=(\d+) hex=([0-9a-f-]+)\r?$',data,re.MULTILINE)
    assert len(rows)==5 and sum(line.startswith(b'text-coexist: checkpoint ') for line in data.splitlines())==5
    for i,row in enumerate(rows):
        assert tuple(map(int,row[:5]))==((i+1)*12,2,2,2,0) and row[5]==b'4141',row
    finals=re.findall(rb'^text-coexist: final serial=(\d+) length=(\d+) caret=(\d+) anchor=(\d+) clipboard_busy=(\d+) hex=([0-9a-f-]+)\r?$',data,re.MULTILINE)
    progress=re.findall(rb'^desktop-text: generated=(\d+) presented=(\d+) keyboard_sent=(\d+) keyboard_presented=(\d+) storage=ram\r?$',data,re.MULTILINE)
    assert len(finals)==len(progress)==1
    for prefix in (b'text-coexist: final ',b'desktop-text: generated=',b'text-coexist: file_worker='):
        assert sum(line.startswith(prefix) for line in data.splitlines())==1
    sent,shown,keys,acked=map(int,progress[0]);assert sent==shown==60 and keys==acked
    expected=b'5a625a62' if injected else b'4141';length=len(expected)//2
    final=finals[0];assert tuple(map(int,final[1:5]))==(length,length,length,0) and final[5]==expected,final
    assert int(final[0])==(1000+keys if keys else 60) and (keys>0)==injected
    assert data.count(b'text-coexist: file_worker=disabled checkpoints=5')==1
    assert b'desktools: file-result' not in data
    result.update(application='Text',script_checkpoints=5,final_hex=expected.decode(),keyboard_events=keys,keyboard_presented=acked)
    return result


def check_load_capture(data, injected=False):
    from hardware_desktop import battery_filesystem_result
    data=complete_lines(data)
    result=check_text_capture(data,injected)
    rows=[r for r in data.splitlines() if r.startswith(b'battery-text-load:')]
    assert len(rows)==1,'Missing or duplicate load completion'
    row=re.fullmatch(rb'battery-text-load: PASS epoch=(\d+) start_us=(\d+) end_us=(\d+) first_step_us=(\d+) last_step_us=(\d+); retired=yes revoked=yes',rows[0])
    assert row,'Incomplete load completion'
    epoch,start,end,first,last=map(int,row.groups())
    assert epoch==result['epoch'] and start==result['start_us']
    assert start<=first<last<=end and end>=result['end_us']
    assert first<=result['desktop_start_us']+1000000 and last>=result['desktop_end_us']-1000000
    overlap=min(last,result['desktop_end_us'])-max(first,result['desktop_start_us'])
    assert overlap>=59000000,'Filesystem does not span the desktop interval'
    fs=battery_filesystem_result(data.decode(),require_service=False,desktop=True)
    assert fs and fs.get('steps') and fs.get('profile') and fs['elapsed_us']==end-first,'Invalid filesystem progress/retirement'
    assert b'battery-text-load: FAIL' not in data
    result.update(storage='RAM Text; exclusive filesystem image',filesystem=fs,load_start_us=first,load_end_us=end,last_step_us=last,overlap_us=overlap)
    return result


def chord(qmp,key,*modifiers):
    for down,names in ((True,(*modifiers,key)),(False,(key,*reversed(modifiers)))):
        qmp.call('input-send-event',{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':name}}} for name in names]})
        time.sleep(.06)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',choices=('normal','missing','timeout','early-exit','refusal','runtime-fault','load-abort'),default='normal')
    parser.add_argument('--text',action='store_true',help='Exercise the real Text app and rich keyboard input in RAM')
    parser.add_argument('--no-input',action='store_true',help='Text normal case: validate the automatic script without QMP typing')
    parser.add_argument('--load',action='store_true',help='Combine Text with the exclusive disposable filesystem workload')
    args=parser.parse_args()
    if args.load:args.text=True
    if args.case=='load-abort' and not args.load:parser.error('--case load-abort requires --load')
    if args.no_input and (not args.text or args.case!='normal'):parser.error('--no-input requires --text --case normal')
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('battery-desktop-'+('load-' if args.load else '')+('text-' if args.text else '')+('script' if args.no_input else args.case));out.mkdir(parents=True,exist_ok=True)
    output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-battery-desktop-') as folder:
        tmp=Path(folder)
        (tmp/'battery.asl').write_text('#define BATTERY_FAULT '+('27' if args.case=='runtime-fault' else '15')+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='refusal' else 'vm.elf')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
                 '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
                 '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        if args.load:
            from exfat_test_image import ExfatFixture
            from battery_filesystem_image import create
            volume=tmp/'workspace.exfat';stick=tmp/'workspace.img'
            with volume.open('wb') as stream:stream.truncate(128*1024*1024)
            subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(['/usr/sbin/tune.exfat','-I','0x51525354',str(volume)],check=True,stdout=subprocess.DEVNULL)
            fixture=ExfatFixture(volume.read_bytes());inner=tmp/'inner.exfat';create(inner)
            fixture.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 51525354\n','anchor.txt':b'Unchanged battery filesystem anchor\n','battery-fs.img':inner.read_bytes()})
            mbr=bytearray(512);mbr[450]=7
            struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
            with stick.open('wb') as stream:stream.write(mbr);stream.seek(2048*512);stream.write(fixture.data)
            command+=['-device','qemu-xhci,id=xhci','-drive',f'if=none,id=stick,format=raw,file={stick}','-device','usb-storage,drive=stick,bus=xhci.0']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output,bytes(output)
                if args.load:
                    serial.sendall(b'workspace mount 51525354\n');mounted=bytearray();until=time.monotonic()+30
                    while b'workspace: ready at /work/srvros-work' not in mounted and time.monotonic()<until:mounted.extend(read_for(serial,.1))
                    output.extend(mounted);assert b'workspace: ready at /work/srvros-work' in mounted,bytes(mounted)
                    serial.sendall(b'acpiruntime battery-filesystem-prepare\n');prepared=bytearray();until=time.monotonic()+30
                    while b'workspace-image: prepared ' not in complete_lines(prepared) and time.monotonic()<until:prepared.extend(read_for(serial,.1))
                    output.extend(prepared)
                    extent=re.search(rb'workspace-image: prepared path=/work/srvros-work/battery-fs.img raw_offset=(\d+) bytes=16777216; unmounted=yes aliases=revoked',prepared)
                    assert extent,bytes(prepared)
                    image_offset=int(extent[1]);before_disk=stick.read_bytes()
                suffix='-'+args.case if args.case in ('missing','timeout') else ''
                if args.case=='load-abort':suffix='-abort'
                serial.sendall(('acpiruntime '+('battery-text-load' if args.load else 'battery-desktop'+('-text' if args.text else ''))+suffix+'\n').encode())
                part=bytearray();start=time.monotonic();injected=False
                while time.monotonic()-start<125:
                    part.extend(read_for(serial,.1));lines=complete_lines(part)
                    (out/'serial.log').write_bytes(output+part)
                    ready=b'desktop-text: script presented; keyboard ready' if args.text and args.case=='normal' else b'desktop-coexist: ready '
                    if not injected and ready in lines:
                        if args.case=='early-exit':
                            if args.text:chord(qmp,'esc','ctrl','alt')
                            else:qmp.key('esc')
                        elif args.case=='normal':
                            # Drain diagnostic output between input bursts. A paused
                            # UART reader can exhaust the guest's bounded TX budget.
                            def type_key(name,shift=False):
                                qmp.key(name,shift=shift);part.extend(read_for(serial,.08))
                            def type_chord(name,*mods):
                                chord(qmp,name,*mods);part.extend(read_for(serial,.15))
                            if not args.no_input:
                                if args.text:type_chord('a','ctrl')
                                type_key('a',shift=True);type_key('b');type_key('x');type_key('backspace')
                                if args.text:
                                    type_key('left');type_key('delete');type_key('b');type_key('home');type_key('right',shift=True);type_key('z',shift=True)
                                    type_chord('a','ctrl');type_chord('c','ctrl');type_key('end');type_chord('v','ctrl')
                                    part.extend(read_for(serial,.3))
                            qmp.mouse(40,20)
                            qmp.call('screendump',{'filename':str(out/'desktop.ppm')})
                            from PIL import Image
                            with Image.open(out/'desktop.ppm') as screenshot:screenshot.save(out/'desktop.png')
                            (out/'desktop.ppm').unlink()
                        injected=True
                    if args.case=='runtime-fault' and process.poll() is not None:break
                    if (b'battery-text-load:' in lines if args.load else b'battery-desktop: PASS' in lines or b'battery-desktop: FAIL' in lines) or b'battery: HALTED' in lines:break
                output.extend(part);data=bytes(output if args.load else part)
                if args.case=='runtime-fault':
                    assert process.wait(timeout=5)==0,data
                    assert b'desktop-coexist: ready ' in data and b'battery: FAILED operation deadline' in data,data
                    assert b'contained fault; returning to Ubuntu without live-runtime cleanup; final report unavailable' in data,data
                    assert b'battery-desktop: PASS' not in data and b'; retired=yes' not in data,data
                    if args.load:
                        from acpi_battery_smoke import verify_isolated_disk
                        assert stick.read_bytes()!=before_disk,'Runtime fault preceded all filesystem writes'
                        verify_isolated_disk(stick,before_disk,image_offset,volume,out,None)
                    result=dict(status='PASS',expected_failure='active runtime deadline',automatic_reset=True,final_report='unavailable')
                    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
                    print(json.dumps(result),flush=True)
                    return
                if args.case=='normal':
                    result=(check_load_capture if args.load else check_text_capture)(data,injected=not args.no_input) if args.text else check_capture(data)
                    if not args.text:
                        assert b'deskclient: text=Abx' in data and b'deskclient: text=Ab\r\n' in data,data
                        assert re.search(rb'deskclient: exited frames=\d+ input=Ab\r?\n',data),data
                        result['vm_keyboard']='Abx then Backspace produced Ab in client and clean exit'
                else:
                    expected={'missing':b'desktop spawn','timeout':b'desktop deadline','early-exit':b'desktop completion','refusal':b'unavailable or startup deadline','load-abort':b'filesystem abort fixture'}[args.case]
                    assert b'battery-desktop: FAIL outcome='+expected in data and b'; retired=yes' in data,data
                    assert b'battery-desktop: PASS' not in data and b'battery: HALTED' not in data,data
                    if args.case!='refusal':assert b'heap=0 errors=0 denied=0' in data,data
                    result=dict(status='PASS',expected_failure=expected.decode())
                # A second compositor must acquire the server and run after cleanup.
                serial.sendall(b'run /fat/bin/deskd --smoke\n');again=bytearray();deadline=time.monotonic()+20
                while time.monotonic()<deadline:
                    again.extend(read_for(serial,.1))
                    if b'deskd: exited ' in complete_lines(again) or b'deskd: FAIL' in complete_lines(again):break
                output.extend(again)
                assert b'deskd: exited ' in again and b'another display server' not in again,bytes(again)
                result['replacement_compositor']='PASS'
                if args.load:
                    from acpi_battery_smoke import verify_isolated_disk
                    stop_process(process)
                    verify_isolated_disk(stick,before_disk,image_offset,volume,out,result['filesystem']['cycles'] if args.case=='normal' else None)
                    result['outside_image_unchanged']=True
                    result['offline_allocation']='PASS'
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
                print(json.dumps(result),flush=True)
            finally:
                (out/'serial.log').write_bytes(output)
                stop_process(process)


if __name__=='__main__':main()
