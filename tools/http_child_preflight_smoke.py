#!/usr/bin/env python3
"""Actual initramfs ELF/READY/private-pipe preflight; no disks or NIC in VM."""
import hashlib,json,os,re,subprocess,tempfile,time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from vm_sockets import connect_unix

def main():
 root=Path(__file__).resolve().parents[1]
 out=root/'build/review/http-child-preflight';out.mkdir(parents=True,exist_ok=True)
 (out/'result.json').unlink(missing_ok=True);output=bytearray()
 with tempfile.TemporaryDirectory(prefix='srvros-http-child-vm-') as folder:
  tmp=Path(folder)
  iso=build_iso(str(root),str(tmp),1024,768,os.environ.copy(),root/'build/kernel/srvros.elf',root/'build/initramfs.tar',cmdline='srvros.http-child-preflight=1 srvros.storage=ram')
  cmd=['qemu-system-x86_64','-M','q35','-m','2G','-cdrom',iso,'-boot','d','-nic','none','-serial',f'unix:{tmp}/serial,server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot']
  with (out/'qemu.log').open('w') as errors:
   p=subprocess.Popen(cmd,stdout=errors,stderr=errors)
   try:
    serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp');start=time.monotonic()
    while b'\nsrv> ' not in output and time.monotonic()-start<90:
     output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
    assert b'\nsrv> ' in output,'no monitor'
    assert b'http-child-boot: isolated_init=1 net_init_deferred=1 worker_deferred=1' in output
    assert b'init: readiness=ready' in output and b'init: startup complete' not in output
    before=len(output);serial.sendall(b'httpchildtest\n');end=time.monotonic()+180
    while b'\nsrv> ' not in output[before:] and time.monotonic()<end:output.extend(read_for(serial,.1))
    part=bytes(output[before:]);assert b'http-child-test: PASS actual ELF READY numericGO EOF exit reap lifetime cases=4; network untested' in part
    rows=re.findall(rb'http-child-case: variant=(\d+) complete=(\d+) ready=(\d+) go=(\d+) status=(\d+) expected=(\d+) retired=(\d+) sockets=0 native_wifi=0',part)
    assert rows==[(str(i).encode(),b'1',b'1',b'1',str(s).encode(),str(s).encode(),b'1') for i,s in enumerate([0,16,15,15])],rows
    assert b'FAIL' not in part and b'exception:' not in part and b'\nsrv> ' in part
    before=len(output);serial.sendall(b'httpchildtest\n');end=time.monotonic()+10
    while b'\nsrv> ' not in output[before:] and time.monotonic()<end:output.extend(read_for(serial,.1))
    assert b'http-child-test: REFUSED explicit isolated-init permit required or finite tickets exhausted' in output[before:]
    result={'status':'PASS','source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),'cases':['valid96GOEOF','wrongPID','short95EOF','extra97byte'],'actual_guest_ELF_and_SYS_PROC_READY':True,'network_tested':False,'physical_acceptance':False,'payload_sha256':{str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in [root/'build/kernel/srvros.elf',root/'build/initramfs.tar',root/'build/userspace/httpget.elf',root/'build/userspace/init.elf']}}
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
   finally:
    (out/'serial.log').write_bytes(output);stop_process(p)
if __name__=='__main__':main()
