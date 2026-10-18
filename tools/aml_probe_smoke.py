#!/usr/bin/env python3
"""Boot the isolated AML adapter in QEMU, optionally with synthetic EC resources."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture',action='store_true')
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('aml-native-fixture' if args.fixture else 'aml-native-qemu')
    out.mkdir(parents=True,exist_ok=True)
    original=boot.make_limine_conf
    boot.make_limine_conf=lambda w,h:original(w,h)+'    cmdline: srvros.aml=probe\n'
    output=b''
    with tempfile.TemporaryDirectory(prefix='srvros-aml-vm-') as directory:
        tmp=Path(directory);iso=boot.build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=boot.choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off',
            '-display','none','-monitor','none','-no-reboot']
        if args.fixture:
            subprocess.run(['iasl','-p',str(tmp/'resource'),str(root/'tools/fixtures/acpi/macbook-resources.asl')],check=True,stdout=subprocess.DEVNULL)
            command+=['-acpitable',f'file={tmp}/resource.aml']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=boot.connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<100:
                    output+=boot.read_for(serial,.5);qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output,output[-4000:].decode(errors='replace')
                result=re.search(rb'aml-probe: completed elapsed_us=(\d+) copied_bytes=(\d+) errors=(\d+) denied=(\d+) snapshot_writes=(\d+) heap_remaining=(\d+)',output)
                assert result,output[-5000:].decode(errors='replace')
                elapsed,copied,errors,denied,writes,remaining=map(int,result.groups())
                assert errors==denied==remaining==0,(result.group(0),output[-5000:].decode(errors='replace'))
                assert 0<copied<=4*1024*1024 and elapsed<5000000
                if args.fixture:
                    assert b'\\_SB.PCI0.LPCB.EC.SMB0._STA = 0x000000000000000f' in output
                    assert b'\\_SB.PCI0.LPCB.EC.SMB0._EC = 0x0000000000002010' in output
                    assert b'\\_SB.PCI0.LPCB.EC.SMB0.SBS0._HID = ACPI0002' in output
                else: assert b'MacBook EC namespace absent; no battery provider' in output
                serial.sendall(b'echo AML probe returned cleanly\n');output+=boot.read_for(serial,1)
                assert b'AML probe returned cleanly' in output
                qmp.call('quit');process.wait(timeout=10)
            finally:
                stop_process(process);(out/'serial.txt').write_bytes(output)
    print(f'aml-probe: PASS isolated namespace, resource evaluation={args.fixture}, no denied operations/leaked interpreter allocations; {out}')

if __name__=='__main__':main()
