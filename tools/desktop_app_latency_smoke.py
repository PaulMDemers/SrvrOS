#!/usr/bin/env python3
"""Measure actual app open/close work without changing or requiring audio."""
import argparse,hashlib,json,os,re,socket,subprocess,tempfile,time
from pathlib import Path
from displayd_frame_smoke import choose_port,connect_serial,stop_process
from displayd_resolution_smoke import build_iso
from desktop_foundation_smoke import QMP
from exfat_test_image import ExfatFixture,check_clean_image


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rounds',type=int,default=4)
    parser.add_argument('--label',default='current')
    args=parser.parse_args()
    if not 1<=args.rounds<=16 or not re.fullmatch(r'[a-z0-9-]+',args.label):parser.error('rounds 1..16; lowercase label')
    root=Path(__file__).resolve().parents[1];out=root/'build/review'/('desktop-app-latency-'+args.label);out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True);output=b'';consumed=0;cycles=[]
    with tempfile.TemporaryDirectory(prefix='srvros-app-latency-') as directory:
        tmp=Path(directory);disk_path=tmp/'disk.exfat'
        with disk_path.open('wb') as f:f.truncate(64*1024*1024)
        subprocess.run(['/usr/sbin/mkfs.exfat','-c','4K',str(disk_path)],check=True,stdout=subprocess.DEVNULL)
        fixture=ExfatFixture(disk_path.read_bytes());source=ExfatFixture((root/'build/srvros.exfat').read_bytes())
        fixture.add_directory_files('bin',{name:source.read('bin/'+name) for name in ('deskd','desktools','deskfs','deskview','imageworker')})
        fixture.add_directory_files('home',{});disk_path.write_bytes(fixture.data)
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy());port=choose_port(30001,38000)
        command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d','-drive',f'if=none,id=disk,file={disk_path},format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.log').open('w') as errors:
            process=subprocess.Popen(command,stdout=errors,stderr=errors);qmp=None
            try:
                serial=connect_serial(port,20);serial.settimeout(.1);qmp=QMP(tmp/'qmp')
                def wait(marker,timeout=60):
                    nonlocal output,consumed
                    end=time.monotonic()+timeout
                    while marker not in output[consumed:]:
                        if process.poll() is not None or time.monotonic()>end:raise RuntimeError(f'Missing {marker!r}')
                        try:
                            output+=serial.recv(65536);(out/'serial.log').write_bytes(output)
                        except socket.timeout:pass
                    consumed=output.index(marker,consumed)+len(marker)
                def shortcut(keys):qmp.call('human-monitor-command',{'command-line':'sendkey '+keys});time.sleep(.25)
                def text(value):
                    keys={' ':('spc',False),'/':('slash',False),'.':('dot',False),'-':('minus',False)}
                    for c in value:
                        key,shift=keys.get(c,(c.lower(),c.isupper()));qmp.key(key,shift)
                def processes():
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                    return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                wait(b'srv> ',100);baseline=processes()
                serial.sendall(b'latency reset\n');wait(b'latency: reset active=yes');wait(b'srv> ')
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n');wait(b'text-runtime: admitted window=1');wait(b'deskd: commit')
                text('Kept while apps cycle.');shortcut('ctrl-l');shortcut('ctrl-a');text('/fat/home/latency-proof.txt');shortcut('ctrl-s');wait(b'picker ready mode=2');qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                for round_id in range(args.rounds):
                    for name in ('Calculator','Files','Text','Preview'):
                        start=time.monotonic();shortcut('meta_l-spc');text(name+' new app');qmp.key('ret')
                        marker=b'deskview: starting' if name=='Preview' else b'desktools: starting role='+name.lower().encode()
                        wait(marker);wait(b'deskd: opened pid=');wait(b'\n')
                        pid=re.findall(rb'deskd: opened pid=(\d+)',output)[-1]
                        wait(b'deskd: commit pid='+pid);opened=time.monotonic()
                        # Exercise demand-allocated surfaces, then normal close.
                        shortcut('meta_l-alt-down');time.sleep(.1);shortcut('ctrl-q' if name in ('Text','Preview') else 'ctrl-w')
                        wait(b'deskd: child-exit pid='+pid+b' status=0');ended=time.monotonic()
                        cycles.append(dict(round=round_id,app=name,pid=int(pid),open_wall_ms=round((opened-start)*1000,2),close_wall_ms=round((ended-opened)*1000,2)))
                shortcut('meta_l-spc');text('latency-proof');qmp.key('ret');wait(b'deskd: launcher window=')
                shortcut('ctrl-end');text(' Still editable.');shortcut('ctrl-s');wait(b'file-result role=text op=2 status=0')
                qmp.call('screendump',{'filename':str(out/'survivor.ppm')})
                shortcut('ctrl-q');wait(b'desktools: exited role=text failure=0');shortcut('ctrl-alt-esc');wait(b'deskd: exited');wait(b'srv> ')
                assert processes()==baseline,'process baseline not restored'
                serial.sendall(b'latency off\n');wait(b'latency: active=no');wait(b'srv> ')
                assert b'exception:' not in output and b'deskd: FAIL' not in output
                serial.close();qmp.stream.close();qmp.sock.close()
            except Exception:
                if qmp:
                    try:qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                    except Exception:pass
                raise
            finally:stop_process(process);(out/'serial.log').write_bytes(output)
        final=ExfatFixture(disk_path.read_bytes());expected=b'Kept while apps cycle. Still editable.'
        assert final.read('home/latency-proof.txt')==expected
        (out/'fsck.txt').write_text(check_clean_image(disk_path))
        samples={m[0].decode():dict(count=int(m[1]),max_us=int(m[2]),total_us=int(m[3]),over_1ms=int(m[4]),over_10ms=int(m[5])) for m in re.findall(rb'latency: ([a-z-]+) count=(\d+) max_us=(\d+) total_us=(\d+) over_1ms=(\d+) over_10ms=(\d+)',output)}
        for name in ('process-image','process-memory','process-exit','timer-gap'):assert samples[name]['count']>0,name
        result=dict(status='PASS',physical_acceptance=False,audio_used=False,rounds=args.rounds,cycles=cycles,latency=samples,process_baseline_restored=True,saved_sha256=hashlib.sha256(expected).hexdigest(),source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),source_dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),payload_sha256={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'build/kernel/srvros.elf',root/'build/initramfs.tar')})
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))

if __name__=='__main__':main()
