#!/usr/bin/env python3
"""Exercise the AWK port in Terminal on a disposable exFAT image.

Runs three unmodified pinned upstream regression programs, not the full suite.
Artifacts, including the disk and failure logs, remain under build/review/awk.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture
from thread_smoke import has_fatal_exception
from pack_runtime_elf import runtime_elf

ROOT = Path(__file__).resolve().parents[1]
REVISION = '5739fd79bcfc75ba7526773d0cf634521f8aca3c'
CASES = ('concat-assign-same', 'decr-NF', 'a-format')
EVIDENCE = None

WORKFLOW = r'''#!/fat/bin/sh
awk --version > /fat/awk-test/version.txt
echo $? > /fat/awk-test/version-status.txt
awk --csv -f /fat/awk-test/report.awk /fat/awk-test/input.csv > /fat/awk-test/report.txt
echo $? > /fat/awk-test/report-status.txt
awk -f /fat/awk-test/features.awk > /fat/awk-test/features.txt
echo $? > /fat/awk-test/features-status.txt
cat /fat/awk-test/pipe-input.txt | awk '{print $2}' > /fat/awk-test/pipe.txt
echo $? > /fat/awk-test/pipe-status.txt
awk -f /fat/awk-test/process.awk > /fat/awk-test/process.txt
echo $? > /fat/awk-test/process-status.txt
awk -f /fat/awk-test/concat-assign-same.awk > /fat/awk-test/concat-assign-same.txt
echo $? > /fat/awk-test/concat-status.txt
awk -f /fat/awk-test/decr-NF.awk > /fat/awk-test/decr-NF.txt
echo $? > /fat/awk-test/decr-status.txt
awk -f /fat/awk-test/a-format.awk > /fat/awk-test/a-format.txt
echo $? > /fat/awk-test/format-status.txt
awk -f /fat/awk-test/bad.awk > /fat/awk-test/bad-out.txt
echo $? > /fat/awk-test/bad-status.txt
awk '{print}' /fat/awk-test/missing.csv > /fat/awk-test/missing-out.txt
echo $? > /fat/awk-test/missing-status.txt
awk 'BEGIN { print "bad" > "/fat/awk-test/no-such-dir/out"; close("/fat/awk-test/no-such-dir/out") }' > /fat/awk-test/write-out.txt
echo $? > /fat/awk-test/write-status.txt
awk 'BEGIN {print "recovered"}' > /fat/awk-test/recovery.txt
echo $? > /fat/awk-test/recovery-status.txt
'''
FEATURES = r'''BEGIN {
 $0="alpha 12 beta"; a[$1]=$2; print NF, a["alpha"], ($3 ~ /^be.a$/)
 s="red red"; sub(/red/,"blue",s); gsub(/red/,"green",s); print s
 printf "%.1f %.1f %.1f\n", sqrt(9), int(3.9), 2^3
 srand(17); x=rand(); srand(17); y=rand(); print (x==y && x>=0 && x<1 ? "rand-ok" : "rand-fail")
}'''
PROCESS = r'''BEGIN {
 cmd="printf child-output"; cmd | getline x; s=close(cmd); print x, s
 s=system("exit 7"); print "system", s
}'''
REPORT = r'''NR==1 {next}
{gsub(/\n/, " / ", $2); print $1 ": " $2 " = " $3; total += $3}
END {print "total: " total}
'''


def digest(data): return hashlib.sha256(data).hexdigest()


def image_file_hashes(fixture):
    result={};seen=set()
    def walk(first,size=None,contiguous=False,parent=''):
        if first in seen:raise RuntimeError('Directory alias in fixture')
        seen.add(first)
        for name,entry in fixture.entries(fixture.chain(first,size,contiguous)):
            child=int.from_bytes(entry[52:56],'little');length=int.from_bytes(entry[56:64],'little');flag=bool(entry[33]&2);path=parent+'/'+name
            if int.from_bytes(entry[4:6],'little')&16:walk(child,length,flag,path)
            else:result[path]=digest(fixture.chain(child,length,flag))
    walk(fixture.root);return result


def main():
    global EVIDENCE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    parser.add_argument('--disk', type=Path, default=ROOT/'build/srvros.exfat')
    parser.add_argument('--fsck', default=shutil.which('fsck.exfat'), help='path to fsck.exfat (optional; missing is reported as SKIP)')
    args = parser.parse_args()
    upstream = ROOT/'ports/upstream/awk'
    revision = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REVISION: raise RuntimeError('Unexpected upstream revision: '+revision)
    subprocess.run(['git', '-C', str(upstream), 'diff', '--quiet', 'HEAD', '--', 'bugs-fixed'], check=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=ensure_dir(ROOT/'build/review/awk')))
    EVIDENCE = out
    disk = out/'disk.exfat'
    shutil.copyfile(args.disk, disk)
    files = {'workflow.sh':WORKFLOW.encode(), 'features.awk':FEATURES.encode(), 'process.awk':PROCESS.encode(),
             'report.awk':REPORT.encode(), 'bad.awk':b'BEGIN { if (\n',
             'input.csv':b'name,description,amount\nA,"red, large",3\nB,"two\nlines",4\n',
             'pipe-input.txt':b'one two\nthree four\n'}
    expected = {'report.txt':b'A: red, large = 3\nB: two / lines = 4\ntotal: 7\n',
                'features.txt':b'3 12 1\nblue green\n3.0 3.0 8.0\nrand-ok\n',
                'pipe.txt':b'two\nfour\n', 'process.txt':b'child-output 0\nsystem 7\n', 'recovery.txt':b'recovered\n'}
    for case in CASES:
        files[case+'.awk'] = (upstream/'bugs-fixed'/f'{case}.awk').read_bytes()
        expected[case+'.txt'] = (upstream/'bugs-fixed'/f'{case}.ok').read_bytes()
    for name in ('version','report','features','pipe','process','concat','decr','format','recovery'):
        expected[name+'-status.txt'] = b'0\n'
    fixture = ExfatFixture(disk.read_bytes())
    source_files = image_file_hashes(fixture)
    copied_aliases = fixture.copy_file_aliases()
    if image_file_hashes(fixture)!=source_files:raise RuntimeError('Independent fixture changed file names or bytes')
    staged_binary = fixture.read('bin/awk')
    fixture.add_directory_files('awk-test', files); disk.write_bytes(fixture.data)
    binary = ROOT/'build/userspace/awk.elf'
    receipt = {'upstream_revision':revision, 'upstream_cases':list(CASES),
               'fixtures_sha256':{k:digest(v) for k,v in files.items()},
               'expected_sha256':{k:digest(v) for k,v in expected.items()},
               'binary_sha256':digest(binary.read_bytes()), 'runtime_binary_sha256':digest(runtime_elf(binary.read_bytes())), 'staged_binary_sha256':digest(staged_binary), 'source_disk_sha256':digest(args.disk.read_bytes()), 'fixture_independent_file_copies':copied_aliases, 'fixture_source_files':len(source_files), 'fixture_source_file_hashes_sha256':digest(json.dumps(source_files,sort_keys=True).encode()), 'fixture_initial_sha256':digest(fixture.data),
               'port_source_sha256':{str(p.relative_to(ROOT)):digest(p.read_bytes()) for p in sorted((ROOT/'ports/srvros/awk').rglob('*')) if p.is_file()},
               'source_diff_sha256':digest(subprocess.check_output(['git','diff','--binary','HEAD'],cwd=ROOT)),
               'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
               'source_status':subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True),
               'kernel_sha256':digest((ROOT/'build/iso_root/boot/srvros.elf').read_bytes()),
               'initramfs_sha256':digest((ROOT/'build/iso_root/boot/initramfs.tar').read_bytes()),
               'status':'INCOMPLETE', 'scope':'selected regression cases only; no complete upstream suite claim'}
    if staged_binary != runtime_elf(binary.read_bytes()): raise RuntimeError('Staged awk differs from build/userspace/awk.elf')
    (out/'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True)+'\n')
    if args.fsck:
        initial=subprocess.run([args.fsck,'-n',str(disk)],capture_output=True,text=True)
        (out/'fsck-initial.log').write_text(initial.stdout+initial.stderr)
        if initial.returncode or 'ERROR' in initial.stdout+initial.stderr or 'corrupted' in initial.stdout+initial.stderr:
            raise RuntimeError('Initial independent-extent fixture is not clean')

    print('awk smoke evidence:',out,flush=True)
    with tempfile.TemporaryDirectory(prefix='awk-vm-') as temp:
        temp=Path(temp)
        iso=build_iso(str(ROOT),str(temp),1280,800,os.environ.copy())
        for boot in (1,2):
            port=choose_port(30001,38000); qmp_path=temp/f'qmp{boot}'
            cmd=[args.qemu,'-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
                 '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci',
                 '-device','ide-hd,drive=disk,bus=ahci.0','-device','qemu-xhci,id=xhci',
                 '-device','usb-kbd,bus=xhci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
                 '-qmp',f'unix:{qmp_path},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
            output=bytearray(); consumed=0; qmp=None; serial=None
            with (out/f'qemu-{boot}.log').open('wb') as log:
                vm=subprocess.Popen(cmd,stdout=log,stderr=log)
                try:
                    serial=connect_serial(port,20); serial.settimeout(.2); qmp=QMP(qmp_path)
                    def wait(marker, timeout=40):
                        nonlocal consumed
                        start=time.monotonic()
                        while marker not in output[consumed:]:
                            if marker==b'srv> ' and not consumed: qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                            if time.monotonic()-start>timeout or vm.poll() is not None: raise RuntimeError(f'boot {boot}: missing {marker!r}')
                            try:
                                data=serial.recv(16384)
                                if not data: raise RuntimeError('Serial disconnected')
                                output.extend(data)
                            except socket.timeout: pass
                        consumed=output.index(marker,consumed)+len(marker)
                    def keytext(s):
                        mapping={' ': 'spc', '/':'slash', '.':'dot', '-':'minus', '\n':'ret'}
                        for c in s: qmp.key(mapping.get(c,c))
                    def shortcut(key):
                        qmp.call('human-monitor-command',{'command-line':'sendkey '+key});time.sleep(.3)
                    wait(b'srv> ',100)
                    def processes():
                        serial.sendall(b'ps\n');wait(b'pid name status');wait(b'srv> ')
                        return set(re.findall(rb'^(\d+) (\S+) (background|exited|foreground)',output.split(b'pid name status')[-1].split(b'srv> ')[0],re.M))
                    baseline=processes()
                    if boot==1:
                        serial.sendall(b'run /fat/bin/deskd --apps-trace\n');wait(b'desktools: starting role=text');wait(b'deskd: commit')
                        qmp.mouse(866,694);qmp.mouse(0,0,1);qmp.mouse(0,0,0);wait(b'deskterm: ready');time.sleep(1)
                        keytext('sh /fat/awk-test/workflow.sh\n');time.sleep(3)
                        keytext('clear\ncat /fat/awk-test/report.txt\n');time.sleep(1)
                        qmp.call('screendump',{'filename':str(out/'terminal-workflow.ppm')})
                        keytext('exit\n')
                        wait(b'deskterm: shell exited status=0',90);shortcut('ctrl-shift-w');wait(b'deskterm: exited failure=0')
                        shortcut('f10');shortcut('end');shortcut('ret');wait(b'deskd: exited',60);wait(b'srv> ')
                    else:
                        serial.sendall(b'cat /fat/awk-test/report.txt\n');wait(b'total: 7\r\n');wait(b'srv> ')
                    deadline=time.monotonic()+5
                    after=processes()
                    while after!=baseline and time.monotonic()<deadline:
                        time.sleep(.5);after=processes()
                    if after!=baseline: raise RuntimeError(f'Processes did not retire: baseline={baseline!r} after={after!r}')
                    serial.sendall(b'fsck /fat\n');wait(b'exfat-check:');wait(b'srv> ')
                    if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok', output):
                        raise RuntimeError('Guest exFAT check failed')
                    if has_fatal_exception(output.decode(errors='replace')) or b'deskd: FAIL' in output or b'failure=1' in output:
                        raise RuntimeError('Guest reported failure')
                except Exception:
                    if qmp:
                        try: qmp.call('screendump',{'filename':str(out/f'failure-{boot}.ppm')})
                        except Exception: pass
                    raise
                finally:
                    if serial: serial.close()
                    if qmp: qmp.stream.close();qmp.sock.close()
                    stop_process(vm);(out/f'serial-{boot}.log').write_bytes(output)
            image=ExfatFixture(disk.read_bytes())
            for name,value in expected.items():
                actual=image.read('awk-test/'+name)
                if actual!=value: raise RuntimeError(f'{name}: expected {value!r}, got {actual!r}; see {out}')
            version=image.read('awk-test/version.txt')
            if not version.startswith(b'awk version '): raise RuntimeError(f'Unexpected version: {version!r}')
            for name in ('bad','missing','write'):
                status=image.read('awk-test/'+name+'-status.txt').strip()
                if not status.isdigit() or int(status)==0: raise RuntimeError(f'{name} unexpectedly succeeded: {status!r}')
            driver=ROOT/'build/tests/exfat-driver'
            if not driver.is_file(): raise RuntimeError('Build build/tests/exfat-driver before running')
            if driver.is_file():
                check=subprocess.run([str(driver),str(disk),'allocation-check'],capture_output=True,text=True)
                (out/f'allocation-{boot}.log').write_text(check.stdout+check.stderr)
                if check.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in check.stdout: raise RuntimeError('Allocation check failed')
            if args.fsck:
                result=subprocess.run([args.fsck,'-n',str(disk)],capture_output=True,text=True)
                (out/f'fsck-{boot}.log').write_text(result.stdout+result.stderr)
                if result.returncode or 'ERROR' in result.stdout+result.stderr or 'corrupted' in result.stdout+result.stderr:
                    raise RuntimeError('fsck.exfat failed; see evidence')
            else: print('fsck.exfat: SKIP (not available on host)',flush=True)
    receipt['status']='PASS'
    receipt['external_fsck']='PASS' if args.fsck else 'SKIP: unavailable'
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    print('awk smoke: PASS selected regressions, Terminal workflow, errors, process commands, offline verification and reboot persistence; full suite not run',flush=True)


def ensure_dir(path):
    path.mkdir(parents=True,exist_ok=True)
    return path


if __name__=='__main__':
    try:
        main()
    except Exception as error:
        if EVIDENCE:
            path=EVIDENCE/'receipt.json'
            receipt=json.loads(path.read_text()) if path.exists() else {}
            receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
            path.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
        raise
