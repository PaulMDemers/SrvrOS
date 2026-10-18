#!/usr/bin/env python3
"""Verify bulk pipes, filesystem tee and early-reader exit in a disposable VM."""
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review/pipe';out.mkdir(parents=True,exist_ok=True)
    expected=b''.join(f'{n}\n'.encode() for n in range(1,100001))
    checksum=subprocess.run(['cksum'],input=expected,capture_output=True,check=True).stdout.strip()
    port=choose_port(30001,38000);output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-pipe-') as directory:
        tmp=Path(directory);disk=tmp/'disk.exfat';shutil.copyfile(root/'build/srvros.exfat',disk)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(root/'build/srvros-x86_64.iso'),'-boot','d',
            '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
            '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off',
            '-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=60):
                    nonlocal output,consumed
                    started=time.monotonic()
                    while marker not in output[consumed:]:
                        if marker==b'srv> ' and not consumed: qmp.boot_if_menu_stalled(output,time.monotonic()-started)
                        if time.monotonic()-started>timeout or process.poll() is not None: raise RuntimeError(f'Missing {marker!r}; see {out}/serial.log')
                        try:
                            data=serial.recv(16384)
                            if not data: raise RuntimeError('serial disconnected')
                            output+=data
                        except socket.timeout: pass
                    consumed=output.index(marker,consumed)+len(marker)
                wait(b'srv> ',100);serial.sendall(b'run /fat/bin/sh\n');wait(b' $ ')
                serial.sendall(b'seq 1 100000 | tee /fat/pipe-proof.txt | wc -l\n');wait(b'\r\n100000\r\n');wait(b' $ ')
                serial.sendall(b'cat /fat/pipe-proof.txt | cksum\n');wait(checksum);wait(b' $ ')
                serial.sendall(b'yes x | head -n 4096 | wc -l\n');wait(b'\r\n4096\r\n');wait(b' $ ')
                serial.sendall(b'exit\n');wait(b'srv> ')
                if b'exception:' in output or b'process: exited status=139' in output: raise RuntimeError('Guest fault')
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
    print(f'pipe-smoke: PASS 100000-line three-process pipeline, exact persisted checksum ({checksum.decode()}), early-reader exit and shell return. Evidence: {out}')


if __name__=='__main__': main()
