#!/usr/bin/env python3
"""Exercise isolated PTY sessions through real syscalls in a disposable VM."""
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
from displayd_frame_smoke import choose_port, connect_serial, stop_process


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review/pty';out.mkdir(parents=True,exist_ok=True)
    port=choose_port(30001,38000);output=b''
    with tempfile.TemporaryDirectory(prefix='srvros-pty-') as directory:
        disk=Path(directory)/'disk.exfat';shutil.copyfile(root/'build/srvros.exfat',disk)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(root/'build/srvros-x86_64.iso'),'-boot','d',
            '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_serial(port,20);serial.settimeout(.2)
                def wait(marker,timeout=90):
                    nonlocal output
                    deadline=time.monotonic()+timeout
                    while marker not in output:
                        if time.monotonic()>deadline or process.poll() is not None:
                            raise RuntimeError(f'Missing {marker!r}; see {out}/serial.log')
                        try:
                            data=serial.recv(16384)
                            if not data: raise RuntimeError('serial disconnected')
                            output+=data
                        except socket.timeout: pass
                        if b'ptyprobe: FAIL' in output or b'exception:' in output:
                            raise RuntimeError(f'Guest failure; see {out}/serial.log')
                wait(b'srv> ')
                serial.sendall(b'run /fat/bin/ptyprobe\n')
                wait(b'ptyprobe: PASS',60)
                wait(b'process: exited status=0',15)
                serial.close()
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
    print(f'pty-smoke: PASS real terminal sessions; evidence {out}')

if __name__=='__main__': main()
