#!/usr/bin/env python3
"""Measure QEMU USB-key event to serial echo, including active I/O/output.
These are host wall-clock diagnostic samples, not physical display latency.
"""
import argparse
import json
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
            '-serial',f'file:{serial}','-qmp',f'unix:{qmp_path},server=on,wait=off','-monitor','none','-display','none','-no-reboot']
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
                    def cmd(text,marker,prompt=' $ '):
                        offset=len(read());usb.send_qmp_text(qmp,text+'\n',0.025)
                        return wait_for(serial,lambda s:marker in s[offset:] and s.endswith(prompt),process,90)
                    def echo_sample():
                        offset=len(read());start=time.monotonic_ns()
                        usb.send_qmp_key(qmp,'z')
                        deadline=time.monotonic()+90
                        while True:
                            tail=read()[offset:]
                            if 'z' in tail:break
                            if time.monotonic()>deadline or process.poll() is not None:raise RuntimeError('USB key echo timed out')
                            time.sleep(0.002)
                        elapsed=(time.monotonic_ns()-start)/1e6
                        usb.send_qmp_key(qmp,'backspace');time.sleep(0.03)
                        return elapsed
                    cmd('workspace shell','srvsh ready.')
                    measurements['idle_ms']=[echo_sample() for _ in range(10)]
                    # Slow only the disposable USB disk to expose service/ownership stalls.
                    call(qmp,{'execute':'block_set_io_throttle','arguments':{'device':'stick','bps':0,'bps_rd':0,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':200}})
                    measurements['during_usb_write_ms']=[]
                    for _ in range(args.write_rounds):
                        offset=len(read())
                        usb.send_qmp_text(qmp,"sh -c 'dd if=/dev/zero of=load.bin bs=4096 count=128; echo disk-finished' &\n",0.025)
                        wait_for(serial,lambda s:'/fat/bin/dd entry=' in s[offset:],process,30)
                        assert '\ndisk-finished\n' not in read()[offset:]
                        measurements['during_usb_write_ms'].append(echo_sample())
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
                    cmd('sync && echo flushed','\nflushed\n')
                    cmd('exit','srv> ',prompt='srv> ')
                    cmd('workspace unmount','USB writes disabled',prompt='srv> ')
                    assert not usb.has_fatal_exception(read())
            finally:
                process.terminate()
                try:process.wait(10)
                except subprocess.TimeoutExpired:process.kill();process.wait()
        with disk.open('rb') as source:source.seek(2048*512);data=source.read()
        volume.write_bytes(data);check=check_clean_image(volume)
        result=ExfatFixture(data);assert result.read('srvros-work/load.bin')==bytes(512*1024)
        record={'measurement':'QEMU USB QMP key event to serial prompt echo','clock':'host monotonic wall clock','physical_display_latency':'NOT_TESTED','usb_write_throttle_iops':200,'samples':measurements,'filesystem':check}
        (logs/'workspace-latency.json').write_text(json.dumps(record,indent=2)+'\n')
        print(json.dumps(record,indent=2));print('workspace-latency: PASS (keys retained, exact disk bytes, clean filesystem)')


if __name__=='__main__':main()
