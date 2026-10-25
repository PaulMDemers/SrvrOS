#!/usr/bin/env python3
"""Bounded worker pool over actual pipes/processes and a disposable filesystem."""
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time
from displayd_frame_smoke import choose_port,connect_serial,stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP
from exfat_test_image import ExfatFixture,check_clean_image


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review/desktop-file-pool';out.mkdir(parents=True,exist_ok=True)
    output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-file-pool-') as directory:
        tmp=Path(directory);disk=tmp/'disk.exfat'
        # The legacy boot packer omits standard exFAT boot/upcase metadata.
        # Use mkfs plus matched binaries so before/after fsck is meaningful.
        with disk.open('xb') as stream:stream.truncate(32*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','4K',str(disk)],check=True,stdout=subprocess.DEVNULL)
        source=ExfatFixture((root/'build/srvros.exfat').read_bytes())
        fixture=ExfatFixture(disk.read_bytes())
        fixture.add_directory_files('bin',{name:source.read('bin/'+name) for name in ('desktools','deskfs','sync')})
        disk.write_bytes(fixture.data)
        (out/'before-fsck.txt').write_text(check_clean_image(disk))
        iso=build_iso(str(root),str(tmp),800,600,os.environ.copy());port=choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
            '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci',
            '-device','ide-hd,drive=disk,bus=ahci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
            '-qmp',f'unix:{tmp/"qmp"},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.log').open('wb') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors)
            try:
                serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=60):
                    nonlocal output,consumed
                    start=time.monotonic()
                    while marker not in output[consumed:]:
                        if marker==b'srv> ' and not consumed:qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                        if time.monotonic()-start>timeout or process.poll() is not None:raise RuntimeError(f'Missing {marker!r}')
                        try:
                            chunk=serial.recv(16384)
                            if not chunk:raise RuntimeError('Serial disconnected')
                            output+=chunk;(out/'serial.log').write_bytes(output)
                            if b'file-pool-capture: FAIL' in output or b'exception:' in output:raise RuntimeError('Guest failure')
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def processes():
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                    return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                wait(b'srv> ',100);baseline=processes()
                serial.sendall(b'run /fat/bin/desktools text --file-pool-capture\n')
                wait(b'file-pool-capture: PASS 40 queued 64KiB writes/readbacks, two workers reaped',240);wait(b'srv> ')
                for phase in ('write','read'):assert f'file-pool-capture: phase={phase} jobs=40 spawned=2 peak=2'.encode() in output
                assert processes()==baseline
                serial.sendall(b'run /fat/bin/sync\n');wait(b'exited status=0');wait(b'srv> ')
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
        fixture=ExfatFixture(disk.read_bytes())
        for index in range(40):
            expected=bytes((index*17+b*7)&255 for b in range(65536))
            assert fixture.read(f'pool-{index}.bin')==expected,index
        (out/'fsck.txt').write_text(check_clean_image(disk))
        result=subprocess.run([str(root/'build/tests/exfat-driver'),str(disk),'allocation-check'],capture_output=True,text=True)
        (out/'allocation.txt').write_text(result.stdout+result.stderr)
        assert result.returncode==0,result.stdout+result.stderr
    print(f'desktop-file-pool: PASS 40 queued 64KiB writes/readbacks, two worker lifetimes, process baseline, exact offline contents and clean allocation. Evidence: {out}')


if __name__=='__main__':main()
