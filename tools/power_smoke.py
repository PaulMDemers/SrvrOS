#!/usr/bin/env python3
"""Verify power telemetry stays disabled by default and refuses an emulated CPU.

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
        out=root/'build/review/power'/('opt-in' if enabled else 'default');out.mkdir(parents=True,exist_ok=True)
        output=b''
        with tempfile.TemporaryDirectory(prefix='srvros-power-') as directory:
            tmp=Path(directory)
            boot.make_limine_conf=lambda w,h: original(w,h)+('    cmdline: srvros.power=read-only\n' if enabled else '')
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
                    if enabled:assert b'srvros.power=read-only' in output
                    else:assert b'power: state=' not in output
                    serial.sendall(b'power\n');part=b'';until=time.monotonic()+8
                    while not search_complete(rb'power: units=[^\r\n]*native acceptance pending',part) and time.monotonic()<until:
                        part+=boot.read_for(serial,.2)
                    output+=part
                    expected=1 if enabled else 0
                    assert search_complete(fr'power: state={expected} flags=0 sample_us=0 interval_us=0 samples=0 rejected=0 package_uj=0 package_mw=0 active_mhz=0 nominal_mhz=0'.encode(),part),part
                    assert not boot.has_fatal_exception(output.decode(errors='replace'))
                    qmp.call('quit');process.wait(timeout=10)
                finally:stop_process(process);(out/'serial.txt').write_bytes(output)
    boot.make_limine_conf=original
    print('power-smoke: PASS default disabled, explicit opt-in refuses Broadwell hypervisor, no samples; native measurements remain pending')
if __name__=='__main__':main()
