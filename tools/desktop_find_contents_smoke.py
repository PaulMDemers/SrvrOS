#!/usr/bin/env python3
"""Exercise Find Files contents search in a disposable matched-generated VM."""
import argparse
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture
from serial_records import COMPOSITOR_TRACE, desktop_progress_end
from test_exfat_generator import verify_image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    out = (args.output or root/'build/review/desktop-find-contents').resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    output = b''; consumed = 0; shots = []; qmp = None; process = None; serial = None
    pins = {'source': subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
            'source_status': subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True).splitlines(),
            'submodules': subprocess.check_output(['git','submodule','status','--recursive'],cwd=root,text=True).splitlines(),
            'payload_sha256': {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in
                (root/'build/kernel/srvros.elf',root/'build/initramfs.tar',root/'build/srvros.exfat')}}
    (out/'source.json').write_text(json.dumps(pins,indent=2)+'\n')
    first = b'Here is a NeEdLe/content in text.\n'
    fixture_files = {'alpha.txt': first, 'zeta.txt': b'x'*4093+b'NeEdLe/content\n',
        'needle-content.txt': b'No matching phrase here.\n', 'binary.txt': b'needle/content\x00',
        'invalid.txt': b'needle/content\xff', 'oversize.txt': b'needle/content'+b'x'*(65*1024),
        'empty.txt': b''}
    fixture_files.update({f'mass-{i:03}.txt': b'MassHit\n' for i in range(300)})
    budget_files = {f'budget-{i:03}.txt': b'BudgetHit'+b'x'*(65536-9) for i in range(257)}
    expected = {'find-contents/'+name:data for name,data in fixture_files.items()}
    expected.update({'find-contents-extra/outside.txt': b'needle/content\n'})
    expected.update({'find-budget/'+name:data for name,data in budget_files.items()})
    with tempfile.TemporaryDirectory(prefix='srvros-find-contents-') as directory:
        tmp = Path(directory); disk = tmp/'disk.exfat'
        fixture = ExfatFixture((root/'build/srvros.exfat').read_bytes())
        names = {name.casefold() for name,_ in fixture.entries(fixture.chain(fixture.root))}
        assert not names.intersection({'find-contents','find-contents-extra','find-budget'})
        fixture.add_directory_files('find-contents',fixture_files)
        fixture.add_directory_files('find-contents-extra',{'outside.txt':expected['find-contents-extra/outside.txt']})
        fixture.add_directory_files('find-budget',budget_files)
        disk.write_bytes(fixture.data)
        def allocation(label):
            check = subprocess.run([str(root/'build/tests/exfat-driver'),str(disk),'allocation-check'],capture_output=True,text=True)
            (out/(label+'.txt')).write_text(check.stdout+check.stderr)
            if check.returncode: raise RuntimeError(label+' failed')
        def generator(label):
            result = io.StringIO()
            with contextlib.redirect_stdout(result): verify_image(disk)
            (out/(label+'.txt')).write_text(result.getvalue())
        try:
            generator('before-generator-check'); allocation('before-allocation')
            iso = build_iso(str(root),str(tmp),1280,800,os.environ.copy())
            pins['iso_sha256'] = hashlib.sha256(Path(iso).read_bytes()).hexdigest()
            (out/'source.json').write_text(json.dumps(pins,indent=2)+'\n')
            port = choose_port(30001,38000)
            command = ['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
                '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
                '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
                '-qmp',f'unix:{tmp/"qmp"},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
            with (out/'qemu.log').open('wb') as errors:
                process = subprocess.Popen(command,stdout=errors,stderr=errors)
                serial = connect_serial(port,20); serial.settimeout(.2); qmp = QMP(tmp/'qmp')
                def wait(marker,timeout=45):
                    nonlocal output,consumed
                    # Serial records use CRLF; keep the original evidence bytes.
                    if marker.endswith(b'\n') and not marker.endswith(b'\r\n'):
                        marker=marker[:-1]+b'\r\n'
                    end = time.monotonic()+timeout
                    while True:
                        at = desktop_progress_end(output,marker,consumed)
                        if at is not None: consumed=at; return
                        if marker == b'srv> ' and not consumed: qmp.boot_if_menu_stalled(output,time.monotonic()-(end-timeout))
                        if time.monotonic()>end or process.poll() is not None: raise RuntimeError(f'Missing {marker!r}')
                        try:
                            chunk=serial.recv(16384)
                            if not chunk: raise RuntimeError('Serial disconnected')
                            output+=chunk; (out/'serial.log').write_bytes(output)
                        except socket.timeout: pass
                def wait_all(markers,timeout=45):
                    nonlocal consumed
                    start=consumed; end=start
                    for marker in markers:
                        consumed=start; wait(marker,timeout); end=max(end,consumed)
                    consumed=end
                def shortcut(keys):
                    qmp.call('human-monitor-command',{'command-line':'sendkey '+keys});time.sleep(.3)
                def text(value):
                    mapping={'/':('slash',False),'.':('dot',False),'-':('minus',False),' ':('spc',False)}
                    for c in value:
                        key,shift=mapping.get(c,(c.lower(),c.isupper()));qmp.key(key,shift)
                pointer=[24,56]
                def click(x,y):
                    qmp.mouse(x-pointer[0],y-pointer[1]);pointer[:]=[x,y]
                    qmp.mouse(0,0,1);qmp.mouse(0,0,0);time.sleep(.3)
                def shot(name):
                    time.sleep(.3);qmp.call('screendump',{'filename':str(out/(name+'.ppm'))});shots.append(name+'.ppm')
                def baseline():
                    serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                    return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                def focus_find():
                    shortcut('meta_l-spc');wait(b'launcher opened');text('Find Files');qmp.key('ret');wait(b'launcher window=')
                def save_text(name):
                    shortcut('ctrl-l');text('/fat/'+name);shortcut('ctrl-s');wait(b'picker ready mode=2')
                    shortcut('ctrl-l');text('/fat/home/../');qmp.key('ret');wait(b'picker ready mode=2');qmp.key('ret');wait(b'file-result role=text op=2 status=0')
                def root_query(path,query):
                    shortcut('ctrl-l');text(path);shortcut('ctrl-f');text(query)
                def run(path,query,count,matched,skipped,flags):
                    root_query(path,query);shortcut('ctrl-ret')
                    wait(f'deskfind: started root={path} query={query} kind=1 mode=1'.encode())
                    wait(f'deskfind: finished result=1 exit=0 count={count} matched={matched} skipped={skipped} mode=1 flags={flags}\n'.encode(),120)
                def throttle(rate):
                    qmp.call('block_set_io_throttle',{'device':'disk','bps':0,'bps_rd':rate,'bps_wr':0,'iops':0,'iops_rd':0,'iops_wr':0})
                wait(b'srv> ',100);before=baseline();assert before
                serial.sendall(b'run /fat/bin/deskd --apps-trace\n');wait(b'desktools: starting role=text');wait(b'deskd: commit')
                # Global launch establishes the default profile root and Name mode.
                shortcut('meta_l-shift-f');wait(b'deskfind: starting');text('nothing-matches');shortcut('ctrl-ret')
                wait(b'deskfind: started root=/fat/home query=nothing-matches kind=0 mode=0')
                wait(b'deskfind: finished result=1 exit=0 count=0 matched=0 skipped=0 mode=0 flags=0\n')
                shortcut('ctrl-shift-2')
                run('/fat/find-contents','needle/content',2,2,3,0);shot('contents-results')
                shortcut('ctrl-c');wait(b'deskfind: copy path=/fat/find-contents/alpha.txt')
                click(490,750);shortcut('ctrl-v');wait(b'clipboard role=2 action=32');save_text('find-contents-path.txt')
                focus_find();shortcut('ret');wait(b'deskfind: open application=2 path=/fat/find-contents/alpha.txt')
                wait(b'file-result role=text op=1 status=0');shot('contents-open')
                shortcut('ctrl-a');shortcut('ctrl-c');wait(b'clipboard role=2 action=8');shortcut('ctrl-w');wait(b'desktools: exited role=text failure=0')
                click(490,750);shortcut('ctrl-n');shortcut('ctrl-v');wait(b'clipboard role=2 action=32');save_text('find-contents-open.txt')
                focus_find();shortcut('ctrl-r');wait(b'deskfind: open application=1 path=/fat/find-contents');wait(b'files-location path=/fat/find-contents');shot('contents-reveal')
                shortcut('ctrl-w');wait(b'desktools: exited role=files failure=0');focus_find()
                run('/fat/find-contents','absent-phrase',0,0,3,0)
                run('/fat/find-contents','masshit',256,300,3,1);shot('contents-cap')
                run('/fat/find-budget','budgethit',256,256,0,2);shot('contents-budget')
                # A long literal near-match also keeps the real matcher busy if
                # file data is cached. Throttle reads before starting; the started/canceled ordering and
                # absence of a completion prove cancellation occurred while busy.
                root_query('/fat/find-budget','x'*90+'z');throttle(8192);start=len(output);shortcut('ctrl-ret')
                wait(('deskfind: started root=/fat/find-budget query='+'x'*90+'z kind=1 mode=1').encode());shortcut('esc');wait(b'deskfind: canceled')
                assert b'deskfind: finished' not in re.sub(COMPOSITOR_TRACE,b'',output[start:])
                # Allow the canceled worker to retire before editing/restarting;
                # the subsequent started record confirms the restart was accepted.
                throttle(0);time.sleep(2)
                run('/fat/find-contents','needle/content',2,2,3,0)
                # Return to Name explicitly, then back to Contents for close while busy.
                shortcut('ctrl-shift-1');root_query('/fat/find-contents','needle-content');shortcut('ctrl-ret')
                wait(b'deskfind: started root=/fat/find-contents query=needle-content kind=0 mode=0')
                wait(b'deskfind: finished result=1 exit=0 count=1 matched=1 skipped=0 mode=0 flags=0\n')
                shortcut('ctrl-shift-2');root_query('/fat/find-budget','x'*90+'z');throttle(8192);start=len(output);shortcut('ctrl-ret')
                wait(('deskfind: started root=/fat/find-budget query='+'x'*90+'z kind=1 mode=1').encode());shortcut('ctrl-w')
                throttle(0);wait(b'deskfind: exited failure=0',90)
                assert b'deskfind: finished' not in re.sub(COMPOSITOR_TRACE,b'',output[start:])
                shortcut('f10');shortcut('end');shortcut('ret');wait(b'deskd: exited',60);wait(b'srv> ')
                deadline=time.monotonic()+10
                while baseline()!=before:
                    if time.monotonic()>deadline: raise RuntimeError('Process baseline not restored')
                    time.sleep(.5)
                serial.sendall(b'run /fat/bin/sync\n');wait(b'run: pid=');wait(b' exited status=0');wait(b'srv> ')
                serial.sendall(b'fsck /fat\n');wait(b'errors=0 ok');wait(b'srv> ')
                assert b'failure=1' not in output and b'exception:' not in output and b'deskd: FAIL' not in output and b'unknown command:' not in output
                serial.close();serial=None;qmp.stream.close();qmp.sock.close();qmp=None
            stop_process(process);process=None
            saved=ExfatFixture(disk.read_bytes())
            for path,value in expected.items(): assert saved.read(path)==value,path
            assert saved.read('find-contents-path.txt')==b'/fat/find-contents/alpha.txt'
            assert saved.read('find-contents-open.txt')==first
            generator('generator-check');allocation('allocation')
            receipt={'status':'PASS','fixture':'matched-generated','guest_fsck':'PASS','offline_allocation':'PASS',
                'process_baseline_restored':True,'fixture_bytes_unchanged':True,'cancel_restart_close':True,
                'source':pins,'screenshots':shots,'fixture_sha256':{k:hashlib.sha256(v).hexdigest() for k,v in expected.items()}}
            (out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n')
        except Exception as exc:
            if qmp:
                try: qmp.call('screendump',{'filename':str(out/'failure.ppm')})
                except Exception: pass
            (out/'result.json').write_text(json.dumps({'status':'FAIL','error':repr(exc),'source':pins,'screenshots':shots},indent=2)+'\n')
            raise
        finally:
            if serial: serial.close()
            if qmp:
                try: qmp.stream.close();qmp.sock.close()
                except Exception: pass
            if process: stop_process(process)
            (out/'serial.log').write_bytes(output)
            shutil.copyfile(disk,out/'last-workspace.img')
    print(f'desktop-find-contents: PASS; evidence: {out}')


if __name__=='__main__': main()
