#!/usr/bin/env python3
"""Verify the bounded load diagnostic refuses absent native sensors.

Never interprets QEMU readings as energy/frequency validation.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from serial_records import search_complete

def main():
    root=Path(__file__).resolve().parents[1]
    original=boot.make_limine_conf
    for enabled in (False,True):
        out=root/'build/review/cpu-load'/('opt-in' if enabled else 'default');out.mkdir(parents=True,exist_ok=True)
        output=b''
        with tempfile.TemporaryDirectory(prefix='srvros-power-') as directory:
            tmp=Path(directory)
            boot.make_limine_conf=lambda w,h: original(w,h)+('    cmdline: srvros.sensors=cpu srvros.smc=read-only\n' if enabled else '')
            iso=boot.build_iso(str(root),str(tmp),1280,800,os.environ.copy())
            port=boot.choose_port(30001,38000)
            command=['qemu-system-x86_64','-M','q35','-cpu','Broadwell','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
                '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off',
                '-display','none','-monitor','none','-no-reboot']
            with (out/'qemu.txt').open('w') as errors:
                process=subprocess.Popen(command,stdout=errors,stderr=errors)
                try:
                    serial=boot.connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp');start=time.monotonic()
                    while b'srv> ' not in output and time.monotonic()-start<60:
                        output+=boot.read_for(serial,.3);qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                    assert b'srv> ' in output
                    serial.sendall(b'run /fat/bin/threadstress --cpu-load-capture\n');part=b'';until=time.monotonic()+15
                    while b'srv> ' not in part and time.monotonic()<until:
                        part+=boot.read_for(serial,.2)
                    output+=part
                    assert b'cpu-load: completed=NO reason=thermal-unavailable-or-stale' in part,part
                    assert b'cpu-load: phase=load' not in part and b'cpu-load: completed=yes' not in part
                    assert b'srv> ' in part,part
                    assert not boot.has_fatal_exception(output.decode(errors='replace'))
                    qmp.call('quit');process.wait(timeout=10)
                finally:stop_process(process);(out/'serial.txt').write_bytes(output)
    boot.make_limine_conf=original
    print('cpu-load-smoke: PASS disabled and hypervisor sensors refuse load and return to monitor; native load remains pending')
if __name__=='__main__':main()
