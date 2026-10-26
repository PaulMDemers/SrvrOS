#!/usr/bin/env python3
"""GUI and worker wait under throttled guest writes; no physical timing claim."""
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
    out=root/'build/review/desktop-wait-load';out.mkdir(parents=True,exist_ok=True)
    output=b'';consumed=0
    with tempfile.TemporaryDirectory(prefix='srvros-wait-load-') as directory:
        tmp=Path(directory);disk=tmp/'disk.exfat'
        # The legacy boot packer omits standard exFAT boot/upcase metadata.
        # Use mkfs plus matched binaries so before/after fsck is meaningful.
        with disk.open('xb') as stream:stream.truncate(32*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','4K',str(disk)],check=True,stdout=subprocess.DEVNULL)
        source=ExfatFixture((root/'build/srvros.exfat').read_bytes())
        fixture=ExfatFixture(disk.read_bytes())
        fixture.add_directory_files('bin',{name:source.read('bin/'+name) for name in ('deskd','desktools','deskfs','sync')})
        disk.write_bytes(fixture.data)
        (out/'before-fsck.txt').write_text(check_clean_image(disk))
        iso=build_iso(str(root),str(tmp),1024,768,os.environ.copy());port=choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
            '-drive',f'if=none,id=disk,file={disk},format=raw,throttling.bps-write=262144','-device','ich9-ahci,id=ahci',
            '-device','ide-hd,drive=disk,bus=ahci.0','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
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
                            if b'wait-load: FAIL' in output or b'exception:' in output:raise RuntimeError('Guest failure')
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def processes():
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                    return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                wait(b'srv> ',100);baseline=processes()
                serial.sendall(b'run /fat/bin/deskd --file-pool-wait-smoke\n')
                wait(b'wait-load: idle ready')
                idle='abcdefghijklmnop';load='abcdefghijklmnopqrstuvwxyzabcdef'
                for char in idle:qmp.key(char)
                wait(b'wait-load: writes active')
                for char in load:qmp.key(char)
                wait(b'text='+load.encode())
                qmp.call('screendump',{'filename':str(out/'input-under-worker-load.ppm')})
                wait(b'wait-load: PASS GUI dispatch during 40 worker writes/readbacks; all keys acknowledged and workers reaped',240)
                wait(b'deskd: child-exit')
                qmp.call('human-monitor-command',{'command-line':'sendkey ctrl-alt-esc'})
                wait(b'deskd: exited');wait(b'srv> ')
                assert b'phase=idle count=16' in output and b'phase=load count=32' in output
                assert b'keys=48 acknowledged=48 workers=2/2' in output
                assert b'phase=write complete=40 keys=32' in output and b'phase=read complete=40 keys=32' in output
                assert b'text='+idle.encode() in output
                live=[int(x) for x in re.findall(rb'input phase=load count=\d+ complete=(\d+)',output)]
                assert len(live)==32 and all(x<40 for x in live),live
                deadline=time.monotonic()+10
                while True:
                    current=processes()
                    if current==baseline:break
                    if time.monotonic()>deadline:raise RuntimeError(f'Leaked processes: {current-baseline}')
                    time.sleep(.5)
                serial.sendall(b'run /fat/bin/sync\n');wait(b'exited status=0');wait(b'srv> ')
                serial.close();qmp.stream.close();qmp.sock.close()
            finally:
                stop_process(process);(out/'serial.log').write_bytes(output)
        fixture=ExfatFixture(disk.read_bytes())
        for index in range(40):
            expected=bytes((index*17+b*7)&255 for b in range(65536))
            assert fixture.read(f'wait-{index}.bin')==expected,index
        (out/'fsck.txt').write_text(check_clean_image(disk))
        result=subprocess.run([str(root/'build/tests/exfat-driver'),str(disk),'allocation-check'],capture_output=True,text=True)
        (out/'allocation.txt').write_text(result.stdout+result.stderr)
        assert result.returncode==0,result.stdout+result.stderr
    print(f'desktop-wait-load: PASS 16 idle/32 during-write keys, 40 exact 64KiB writes/readbacks, combined wait, two workers reaped, process baseline and offline integrity. Evidence: {out}')


if __name__=='__main__':main()
