#!/usr/bin/env python3
"""Exercise deferred work from synthetic IRQs, self-rescheduling and teardown."""
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
    root=Path(__file__).resolve().parents[1];out=root/'build/review/acpi-work';out.mkdir(parents=True,exist_ok=True)
    output=b''
    with tempfile.TemporaryDirectory(prefix='srvros-acpi-work-') as directory:
        tmp=Path(directory);iso=boot.build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=boot.choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
                 '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off',
                 '-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=boot.connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp');start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<60:
                    output+=boot.read_for(serial,.5);qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output
                identities=[]
                for _ in range(4):
                    serial.sendall(b'acpiworktest\n');part=b'';until=time.monotonic()+8
                    while not re.search(rb'acpi-work-test: [^\r\n]*\r?\n',part) and time.monotonic()<until:part+=boot.read_for(serial,.2)
                    output+=part
                    match=re.search(rb'acpi-work-test: PASS owner=(\d+) worker=(\d+) calls=1000 interrupts=8 queued=1 stopped=yes',part)
                    assert match,part
                    owner,worker=map(int,match.groups());assert owner and worker and owner!=worker
                    assert worker not in identities;identities.append(worker)
                qmp.call('quit');process.wait(timeout=10)
            finally:stop_process(process);(out/'serial.txt').write_bytes(output)
    print('acpi-work-smoke: PASS four worker lifetimes, synthetic IRQ enqueue, 1000 self-reschedules, self-free, timeout and stopped teardown')
if __name__=='__main__':main()
