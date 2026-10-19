#!/usr/bin/env python3
"""Measure QEMU USB-key event to serial echo, including active I/O/output.
These are host wall-clock diagnostic samples, not physical display latency.
"""
import argparse
import json
import re
import shutil
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import uefi_usb_smoke as usb
from hardware_boot_smoke import wait_for
from usb_storage_smoke import make_iso
from exfat_test_image import ExfatFixture, check_clean_image


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu',default='qemu-system-x86_64')
    parser.add_argument('--ovmf',default=usb.default_ovmf_path())
    parser.add_argument('--write-rounds',type=int,default=3,
        help='Independent active-write echo samples (default: 3)')
    args=parser.parse_args()
    if not 1<=args.write_rounds<=10: parser.error('--write-rounds must be between 1 and 10')
    root=Path(__file__).resolve().parents[1]; logs=root/'build/review'
    logs.mkdir(parents=True,exist_ok=True)
    serial=logs/'workspace-latency-serial.log';serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='srvros-key-latency-') as name:
        temp=Path(name);volume=temp/'volume.img';disk=temp/'usb.img';qmp_path=temp/'qmp'
        with volume.open('wb') as out:out.truncate(128*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(volume)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(volume.read_bytes());uuid=f'{fixture.u32(100):08x}'
        fixture.add_directory_file('srvros-work','.workspace',f'SRVROS-WORKSPACE-V1 {uuid}\n'.encode())
        mbr=bytearray(512);mbr[450]=7;struct.pack_into('<II',mbr,454,2048,len(fixture.data)//512);mbr[510:]=b'\x55\xaa'
        with disk.open('wb') as out:out.write(mbr);out.seek(2048*512);out.write(fixture.data)
        iso=make_iso(root,temp,False,f' srvros.workspace-volume={uuid}')
        command=[args.qemu,'-M','q35','-m','512M','-drive',f'if=pflash,format=raw,readonly=on,file={args.ovmf}',
            '-cdrom',str(iso),'-boot','d','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
            '-drive',f'if=none,id=stick,format=raw,file={disk}','-device','usb-storage,drive=stick,bus=xhci.0',
            '-serial',f'file:{serial}','-qmp',f'unix:{qmp_path},server=on,wait=off','-monitor','none','-display','none','-no-reboot',
            '-trace','enable=hid_kbd_queue_full','-gdb',f'unix:{temp}/gdb,server=on,wait=off']
        measurements={}
        with (logs/'workspace-latency-qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                wait_for(serial,lambda s:s.endswith('srv> '),process,180)
                with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as qmp:
                    qmp.settimeout(10);qmp.connect(str(qmp_path));stream=qmp.makefile('rb');json.loads(stream.readline());sequence=0
                    def call(_sock,request):
                        nonlocal sequence
                        sequence+=1;qmp.sendall(json.dumps(dict(request,id=sequence)).encode()+b'\n')
                        while True:
                            response=json.loads(stream.readline())
                            if response.get('id')==sequence:
                                if 'error' in response:raise RuntimeError(response)
                                return response.get('return')
                    usb.qmp_command=call;call(qmp,{'execute':'qmp_capabilities'})
                    def read():return serial.read_text(errors='replace')
                    def written_bytes():
                        stats=call(qmp,{'execute':'query-blockstats'})
                        return next(s['stats']['wr_bytes'] for s in stats if s.get('device')=='stick')
                    def wait_written(before,minimum):
                        deadline=time.monotonic()+60
                        while written_bytes()-before<minimum:
                            if time.monotonic()>deadline or process.poll() is not None:
                                raise RuntimeError('USB write overlap did not start')
                            time.sleep(0.01)
                    def cmd(text,marker,prompt=' $ '):
                        offset=len(read());usb.send_qmp_text(qmp,text+'\n',0.025)
                        return wait_for(serial,lambda s:marker in s[offset:] and s.endswith(prompt),process,90)
                    def echo_sample(pending_marker=None,work_offset=0):
                        offset=len(read());start=time.monotonic_ns()
                        usb.send_qmp_key(qmp,'z')
                        deadline=time.monotonic()+90
                        while True:
                            tail=read()[offset:]
                            if 'z' in tail:break
                            if time.monotonic()>deadline or process.poll() is not None:raise RuntimeError('USB key echo timed out')
                            time.sleep(0.002)
                        elapsed=(time.monotonic_ns()-start)/1e6
                        if pending_marker is not None:
                            assert pending_marker not in read()[work_offset:], 'Writer finished before key echo'
                            assert elapsed<100, f'Key echo stalled for {elapsed:.1f} ms'
                        usb.send_qmp_key(qmp,'backspace');time.sleep(0.03)
                        return elapsed
                    cmd('workspace shell','srvsh ready.')
                    measurements['idle_ms']=[echo_sample() for _ in range(10)]
                    # Slow only the disposable USB disk to expose service/ownership stalls.
                    call(qmp,{'execute':'block_set_io_throttle','arguments':{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':200}})
                    measurements['during_usb_write_ms']=[]
                    for _ in range(args.write_rounds):
                        before=written_bytes();offset=len(read())
                        usb.send_qmp_text(qmp,"sh -c 'dd if=/dev/zero of=load.bin bs=4096 count=128; echo disk-finished' &\n",0.025)
                        wait_for(serial,lambda s:'/fat/bin/dd entry=' in s[offset:],process,30)
                        wait_written(before,512*1024)
                        assert '\ndisk-finished\n' not in read()[offset:]
                        measurements['during_usb_write_ms'].append(echo_sample('\ndisk-finished\n',offset))
                        wait_for(serial,lambda s:'\ndisk-finished\n' in s[offset:],process,90)
                        cmd('echo disk-keys-retained','\ndisk-keys-retained\n')
                    call(qmp,{'execute':'block_set_io_throttle','arguments':{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':0}})
                    offset=len(read())
                    usb.send_qmp_text(qmp,"sh -c 'seq 1 20000; echo output-finished' &\n",0.025)
                    wait_for(serial,lambda s:'\n100\n' in s[offset:],process,30)
                    assert '\noutput-finished\n' not in read()[offset:]
                    measurements['during_output_ms']=[echo_sample()]
                    wait_for(serial,lambda s:'\noutput-finished\n' in s[offset:],process,90)
                    cmd('echo output-keys-retained','\noutput-keys-retained\n')
                    # Exercise actual USB modifiers, editing, rapid keys and
                    # hold/release while a slower background write is active.
                    call(qmp,{'execute':'block_set_io_throttle','arguments':{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':40}})
                    offset=len(read())
                    usb.send_qmp_text(qmp,"sh -c 'dd if=/dev/zero of=typing-load.bin bs=4096 count=128; echo typing-finished' &\n",0.025)
                    wait_for(serial,lambda s:'/fat/bin/dd entry=' in s[offset:],process,30)
                    def type_burst(text):
                        # QEMU queues modifier transitions as separate reports;
                        # do not outrun its 8/10 ms HID polling interval.
                        shifted='!@#$%^&*()_+{}|:"<>?~'
                        for char in text:
                            usb.send_qmp_text(qmp,char,0.05 if char.isupper() or char in shifted else 0.025)
                    def tap(key):
                        usb.send_qmp_key(qmp,key);time.sleep(0.05)
                    type_burst("echo 'aA")
                    tap('caps_lock');type_burst('bC')
                    tap('caps_lock');type_burst('z')
                    def hold(key,down):
                        call(qmp,{'execute':'input-send-event','arguments':{'events':[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':key}}}]}})
                    hold('shift_r',True);tap('z');hold('shift_r',False)
                    punctuation=' !@#$%^&*()_+[]{}|;:,.<>?/~ 0123456789'
                    type_burst(punctuation+' edti')
                    tap('backspace');tap('backspace')
                    type_burst('it ac')
                    tap('left');tap('b');tap('right')
                    type_burst(' ')
                    hold('r',True);time.sleep(0.66);hold('r',False)
                    type_burst(' end')
                    # Echo must be visible before the background write finishes.
                    wait_for(serial,lambda s:' end' in s[offset:],process,5)
                    assert '\ntyping-finished\n' not in read()[offset:]
                    submission_offset=len(read())
                    type_burst("' > typed.txt\n")
                    # Both jobs must finish before sending a long new command.
                    # Background output may arrive after the fresh prompt, so
                    # looking only at the log suffix is insufficient.
                    wait_for(serial,lambda s:'\ntyping-finished\n' in s[offset:] and
                        '\n/work/srvros-work $ ' in s[submission_offset:],process,90)
                    # Concurrent mutations/readback enter through the wait gate.
                    cmd("sh -c 'dd if=/dev/zero of=concurrent-a.bin bs=4096 count=32 & dd if=/dev/zero of=concurrent-b.bin bs=4096 count=32 & cp load.bin readback.bin & wait; echo contention-finished'",'\ncontention-finished\n')
                    # Interrupt a foreground writer only after USB has accepted
                    # >1 MiB, so cancellation overlaps its final file flush
                    # rather than only the initial empty-file metadata update.
                    before=written_bytes();offset=len(read())
                    usb.send_qmp_text(qmp,'dd if=/dev/zero of=cancel.bin bs=4096 count=256\n',0.025)
                    wait_written(before,1024*1024)
                    assert not read().endswith(' $ '),'Missed cancellation overlap'
                    hold('ctrl',True);usb.send_qmp_key(qmp,'c');hold('ctrl',False)
                    wait_for(serial,lambda s:'status 130' in s[offset:] and s.endswith(' $ '),process,90)
                    cmd('echo cancel-cleanup-ok','\ncancel-cleanup-ok\n')
                    call(qmp,{'execute':'block_set_io_throttle','arguments':{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':0}})
                    cmd('sync && echo flushed','\nflushed\n')
                    cmd('exit','srv> ',prompt='srv> ')
                    cmd('workspace unmount','USB writes disabled',prompt='srv> ')
                    stats_offset=len(read())
                    captured=cmd('latency','latency: active=',prompt='srv> ')[stats_offset:]
                    drops=re.findall(r'input-ps2:.* dropped=(\d+)',captured)
                    assert drops and int(drops[-1])==0, 'Kernel keyboard queue dropped input'
                    assert not usb.has_fatal_exception(read())
            except Exception:
                # Preserve scheduler/device state before tearing down a stalled
                # guest. Optional GDB is diagnostic only, never a pass criterion.
                if shutil.which('gdb') and process.poll() is None:
                    script=temp/'failure.gdb'
                    script.write_text(f"set pagination off\ntarget remote {temp}/gdb\nbt\n"+
                        "p io_depth\np io_owner\np current_thread\np io_waiters\n"+
                        "python\n"+
                        "ts=gdb.parse_and_eval('threads')\n"+
                        "for i in range(int(gdb.parse_and_eval('sizeof(threads)/sizeof(threads[0])'))):\n"+
                        " t=ts[i]\n"+
                        " if int(t['state']): print(i, {k:str(t[k]) for k in ['state','name','user_context','io_user_safe','context','blocked_queue','switches']})\n"+
                        "end\npython\n"+
                        "regs=['rbx','rbp','r12','r13','r14','r15','rsp','rip']\n"+
                        "saved={r:int(gdb.parse_and_eval('$'+r)) for r in regs}\n"+
                        "t=gdb.parse_and_eval('threads[0]')\n"+
                        "try:\n"+
                        " for r in regs: gdb.execute('set $'+r+' = '+str(int(t['context'][r])))\n"+
                        " gdb.execute('bt 8')\n"+
                        " f=gdb.newest_frame()\n"+
                        " for i in range(8):\n"+
                        "  if f is None: break\n"+
                        "  if f.name()=='dispatch_syscall_with_cancellation':\n"+
                        "   frame=f.read_var('frame').dereference();print('blocked syscall', {r:int(frame[r]) for r in ['rax','rdi','rsi','rdx']});break\n"+
                        "  f=f.older()\n"+
                        "finally:\n"+
                        " for r in regs: gdb.execute('set $'+r+' = '+str(saved[r]))\n"+
                        "end\np xhci.devices[0]\np xhci.event_dequeue\np xhci.event_cycle\n"+
                        "p 'keyboard.c'::read_index\np 'keyboard.c'::write_index\np 'keyboard.c'::dropped_count\n"+
                        "detach\nquit\n")
                    with (logs/'workspace-latency-failure-gdb.log').open('w') as diagnostic:
                        try: subprocess.run(['gdb','-batch',str(root/'build/kernel/srvros.elf'),'-x',str(script)],stdout=diagnostic,stderr=subprocess.STDOUT,timeout=15)
                        except (OSError,subprocess.TimeoutExpired) as error: diagnostic.write(str(error))
                raise
            finally:
                process.terminate()
                try:process.wait(10)
                except subprocess.TimeoutExpired:process.kill();process.wait()
        with disk.open('rb') as source:source.seek(2048*512);data=source.read()
        assert 'hid_kbd_queue_full' not in (logs/'workspace-latency-qemu.log').read_text(), 'QEMU input queue overflowed'
        volume.write_bytes(data);check=check_clean_image(volume)
        result=ExfatFixture(data);assert result.read('srvros-work/load.bin')==bytes(512*1024)
        for name,size in [('typing-load.bin',512*1024),('concurrent-a.bin',128*1024),('concurrent-b.bin',128*1024),('readback.bin',512*1024),('cancel.bin',1024*1024)]:
            assert result.read('srvros-work/'+name)==bytes(size),name
        typed=result.read('srvros-work/typed.txt').decode('ascii')
        expected='aABczZ'+punctuation+' edit abc '
        assert re.fullmatch(re.escape(expected)+r'r{2,12} end\n',typed),repr(typed)
        record={'measurement':'QEMU USB QMP key event to serial prompt echo','clock':'host monotonic wall clock','physical_display_latency':'NOT_TESTED','usb_write_throttle_iops':200,'typing_throttle_iops':40,'keyboard_drops':0,'samples':measurements,'typed_text':typed,'filesystem':check}
        (logs/'workspace-latency.json').write_text(json.dumps(record,indent=2)+'\n')
        print(json.dumps(record,indent=2));print('workspace-latency: PASS (keys retained, exact disk bytes, clean filesystem)')


if __name__=='__main__':main()
