#!/usr/bin/env python3
"""Full-boot inherited process lifetime regression; disposable VM, no disks."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso, read_for
from vm_sockets import connect_unix


def main():
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review/process-lifetime';out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-lifetime-') as folder:
        tmp=Path(folder)
        iso=build_iso(str(root),str(tmp),1024,768,os.environ.copy(),root/'build/kernel/srvros.elf',root/'build/initramfs.tar')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-serial',f'unix:{tmp}/serial,server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off',
             '-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            qmp=None
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp')
                start=time.monotonic()
                while b'\nsrv> ' not in output and time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'\nsrv> ' in output,'boot did not reach monitor'
                for iteration in range(2):
                    before=len(output);serial.sendall(b'lifetimetest\n');end=time.monotonic()+120
                    while b'\nsrv> ' not in output[before:] and time.monotonic()<end:
                        output.extend(read_for(serial,.2))
                    part=bytes(output[before:])
                    assert b'lifetime-test: PASS orphan exec setsid group-change descendants=2 unrelated=preserved drained=yes' in part
                    assert b'lifetime-test: failed root retired' in part and b'lifetime-test: churn retired' in part
                    assert b'lifetimeprobe: churn=128 failed-spawn=refused synchronous=2' in part
                    assert b'lifetimeprobe: competing-waiters=16 one-winner=yes' in part
                    assert b'FAIL' not in part and b'exception:' not in part and b'\nsrv> ' in part
                # Existing PTY/job-control and init reaping behavior stays intact.
                before=len(output);serial.sendall(b'run /fat/bin/ptyprobe\n');end=time.monotonic()+60
                while b'\nsrv> ' not in output[before:] and time.monotonic()<end:
                    output.extend(read_for(serial,.2))
                part=bytes(output[before:]);assert b'ptyprobe: PASS' in part and b'FAIL' not in part and b'exited status=0' in part
                qmp.call('screendump',{'filename':str(out/'screen.ppm')})
                result={'status':'PASS','source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
                        'source_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),
                        'iterations':2,'sequential_apps':256,'competing_waiter_pairs':32,'physical_acceptance':False,
                        'payload_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
                                          for p in (root/'build/kernel/srvros.elf',root/'build/initramfs.tar')}}
                (out/'result.json').write_text(json.dumps(result,indent=2)+'\n')
                print(json.dumps(result))
            except Exception:
                if qmp:
                    try:
                        (out/'failure-registers.json').write_text(json.dumps(qmp.call('human-monitor-command',{'command-line':'info registers'})))
                        qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                    except Exception:pass
                raise
            finally:
                (out/'serial.log').write_bytes(output);stop_process(process)


if __name__=='__main__':main()
