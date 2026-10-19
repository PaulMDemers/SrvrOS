#!/usr/bin/env python3
"""Boot the isolated AML adapter in QEMU, optionally with synthetic EC resources."""
import argparse
import os
import hashlib
import json
from pathlib import Path
import re
import subprocess
import shutil
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    mode=parser.add_mutually_exclusive_group()
    mode.add_argument('--fixture',action='store_true')
    mode.add_argument('--tables',type=Path,help='replay captured DSDT/SSDT files in the native snapshot-only adapter; denied hardware regions are expected')
    parser.add_argument('--boot-wait',type=int,default=100)
    args=parser.parse_args()
    if not 10<=args.boot_wait<=180:parser.error('--boot-wait must be between 10 and 180 seconds')
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('aml-native-replay' if args.tables else 'aml-native-fixture' if args.fixture else 'aml-native-qemu')
    out.mkdir(parents=True,exist_ok=True)
    original=boot.make_limine_conf
    boot.make_limine_conf=lambda w,h:original(w,h)+'    cmdline: srvros.aml=probe\n'
    output=b''
    tables=[]
    if args.tables:
        tables=[args.tables/'DSDT.dat',*sorted(args.tables.glob('SSDT*.dat'))]
        for table in tables:
            data=table.read_bytes()
            assert len(data)>=36 and data[:4] in (b'DSDT',b'SSDT') and int.from_bytes(data[4:8],'little')==len(data) and sum(data)%256==0,table
        assert tables[0].read_bytes()[:4]==b'DSDT'
    (out/'result.json').unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='srvros-aml-vm-') as directory:
        tmp=Path(directory);iso=boot.build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=boot.choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off',
            '-display','none','-monitor','none','-no-reboot','-gdb',f'unix:{tmp}/gdb,server=on,wait=off']
        for table in tables:command+=['-acpitable',f'file={table.resolve()}']
        if args.fixture:
            subprocess.run(['iasl','-p',str(tmp/'resource'),str(root/'tools/fixtures/acpi/macbook-resources.asl')],check=True,stdout=subprocess.DEVNULL)
            command+=['-acpitable',f'file={tmp}/resource.aml']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=boot.connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<args.boot_wait:
                    output+=boot.read_for(serial,.5);qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                    (out/'serial.txt').write_bytes(output)
                assert b'srv> ' in output,output[-4000:].decode(errors='replace')
                result=re.search(rb'aml-probe: completed elapsed_us=(\d+) copied_bytes=(\d+) errors=(\d+) denied=(\d+) snapshot_writes=(\d+) heap_remaining=(\d+)',output)
                assert result,output[-5000:].decode(errors='replace')
                elapsed,copied,errors,denied,writes,remaining=map(int,result.groups())
                assert remaining==0,(result.group(0),output[-5000:].decode(errors='replace'))
                if not args.tables: assert errors==denied==0,(result.group(0),output[-5000:].decode(errors='replace'))
                assert 0<copied<=4*1024*1024 and elapsed<5000000
                if args.fixture:
                    assert b'\\_SB.PCI0.LPCB.EC.SMB0._STA = 0x000000000000000f' in output
                    assert b'\\_SB.PCI0.LPCB.EC.SMB0._EC = 0x0000000000002010' in output
                    assert b'\\_SB.PCI0.LPCB.EC.SMB0.SBS0._HID = ACPI0002' in output
                elif not args.tables: assert b'MacBook EC namespace absent; no battery provider' in output
                else: assert b'aml-probe: stage=initialized' in output and b'aml-probe: stage=namespace-loaded' in output
                serial.sendall(b'echo AML probe returned cleanly\n');output+=boot.read_for(serial,1)
                assert b'AML probe returned cleanly' in output
                qmp.call('quit');process.wait(timeout=10)
            except BaseException:
                if shutil.which('gdb') and process.poll() is None:
                    script=tmp/'failure.gdb'
                    script.write_text(f"set pagination off\ntarget remote {tmp}/gdb\nbt\ninfo registers\np current_thread\np threads[0]\nx/12i $pc-16\ndetach\nquit\n")
                    with (out/'failure-gdb.txt').open('w') as diagnostic:
                        try:subprocess.run(['gdb','-batch',str(root/'build/kernel/srvros.elf'),'-x',str(script)],stdout=diagnostic,stderr=subprocess.STDOUT,timeout=15)
                        except (OSError,subprocess.TimeoutExpired) as error:diagnostic.write(str(error))
                raise
            finally:
                stop_process(process);(out/'serial.txt').write_bytes(output)
    (out/'result.json').write_text(json.dumps({'status':'RETURNED' if args.tables else 'PASS','elapsed_us':elapsed,'errors':errors,'denied':denied,'snapshot_writes':writes,'heap_remaining':remaining,'tables':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in tables},'hardware_acceptance':'not established'},indent=2)+'\n')
    if args.tables:
        print(f'aml-probe: RETURNED captured-table replay; errors={errors} denied={denied}; no physical hardware acceptance; {out}');return
    print(f'aml-probe: PASS isolated namespace, resource evaluation={args.fixture}, no denied operations/leaked interpreter allocations; {out}')

if __name__=='__main__':main()
