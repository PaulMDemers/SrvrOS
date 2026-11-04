#!/usr/bin/env python3
"""Observe one WTF utility and one JSC startup in a private diskless guest."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import tarfile
import tempfile
import time
from browser_evidence import verify_runtime, verify_icu
from browser_runtime_smoke import receive, receive_run
from pack_runtime_elf import runtime_elf

ROOT = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jsc-receipt', type=Path, required=True)
parser.add_argument('--wtf-elf', type=Path, required=True)
parser.add_argument('--fixture-receipt', type=Path, required=True)
parser.add_argument('--build-helper-snapshot', type=Path, help='exact archived recipe for replaying a historical ELF after helper-only changes')
parser.add_argument('--qemu-interrupt-log', action='store_true', help='record target exceptions/interrupt registers for diagnosis')
parser.add_argument('--require-success', action='store_true', help='require status zero and exact fixture/JavaScript success markers')
parser.add_argument('--wtf-only', action='store_true', help='run only the utility fixture')
args=parser.parse_args()
base = ROOT/'build/browser-jsc'
run = args.jsc_receipt.resolve().parent
if run.parent != base.resolve() or not run.name.startswith('run-') or not run.is_dir():
    parser.error('JSC receipt must belong to a retained run-* directory under build/browser-jsc')
out = Path(tempfile.mkdtemp(prefix='guest-', dir=base))
receipt = {'result':'failed', 'scope':'private diskless guest observation; no browser or GC acceptance', 'tests':[]}
serial_data = bytearray()

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''): h.update(block)
    return h.hexdigest()

def check_hashes(mapping, historical_recipe=False):
    for name, expected in mapping.items():
        path = Path(name)
        if historical_recipe and name == 'tools/browser_jsc_build.py' and args.build_helper_snapshot:
            snapshot = args.build_helper_snapshot.resolve()
            if not snapshot.is_relative_to(base.resolve()) or sha(snapshot) != expected:
                raise ValueError('historical build-helper snapshot mismatch')
            receipt['historical_build_helper'] = {'path': str(snapshot.relative_to(ROOT)), 'sha256': expected}
            continue
        if path.is_absolute() or '..' in path.parts or not (ROOT/path).is_file() or sha(ROOT/path) != expected:
            raise ValueError('input hash mismatch: '+name)

try:
    if shutil.disk_usage(base).free < 2*1024**3: raise ValueError('2 GiB disk reserve')
    runtime=verify_runtime();icu=verify_icu()
    jsc_receipt=args.jsc_receipt.resolve(); jsc=json.loads(jsc_receipt.read_text())
    fixture_receipt=args.fixture_receipt.resolve(); fixture=json.loads(fixture_receipt.read_text())
    if jsc_receipt.parent != run.resolve() or fixture_receipt.parent != run.resolve(): raise ValueError('receipts must belong to retained JSC run')
    if jsc.get('result')!='built; not executed' or fixture.get('result')!='linked; not run': raise ValueError('build receipts are not successful')
    if jsc['runtime']['sha256']!=runtime['sha256'] or jsc['icu']['sha256']!=icu['sha256']:
        raise ValueError('JSC runtime or ICU proof changed')
    check_hashes(jsc['inputs'], historical_recipe=True);check_hashes(jsc['outputs']);check_hashes(fixture['sha256'])
    if fixture['jsc_source_sha256']!=jsc['verified_final_source_sha256'] or fixture['jsc_build_identifier']!=jsc['build_identifier']['sha256']:
        raise ValueError('fixture and JSC build identity differ')
    math_guest=ROOT/jsc['math_provider']['guest_receipt']; math=json.loads(math_guest.read_text())
    if sha(math_guest)!=jsc['math_provider']['guest_receipt_sha256'] or math.get('result')!='passed': raise ValueError('runtime guest proof changed')
    items={'kernel':ROOT/'build/kernel/srvros.elf','init':ROOT/'build/userspace/init.elf',
           'jsc':run/'cmake/bin/jsc','wtfprobe':args.wtf_elf.resolve(),
           'limine_bios':ROOT/'build/limine-binary/limine-bios.sys',
           'limine_cd':ROOT/'build/limine-binary/limine-bios-cd.bin',
           'limine_tool':ROOT/'build/limine-binary/limine'}
    if items['wtfprobe'].parent != run.resolve(): raise ValueError('fixture ELF must belong to retained JSC run')
    fixture_key = str(items['wtfprobe'].relative_to(ROOT))
    if fixture['sha256'].get(fixture_key) != sha(items['wtfprobe']): raise ValueError('fixture receipt does not bind requested ELF')
    for name,path in items.items():
        if not path.is_file(): raise ValueError('missing '+str(path))
        if name in ('kernel','init') and sha(path)!=math['artifacts'][name]['sha256']:
            raise ValueError('kernel/init differs from matched runtime guest: '+name)
    receipt['artifacts']={name:{'path':str(path),'sha256':sha(path)} for name,path in items.items()}
    receipt['jsc_receipt_sha256']=sha(jsc_receipt);receipt['fixture_receipt_sha256']=sha(fixture_receipt)
    receipt['harness_sha256']=sha(Path(__file__).resolve());receipt['runtime_provenance']=runtime;receipt['icu_provenance']=icu
    iso_root=out/'iso-root';boot=iso_root/'boot/limine';boot.mkdir(parents=True)
    shutil.copyfile(items['kernel'],iso_root/'boot/srvros.elf')
    shutil.copyfile(items['limine_bios'],boot/'limine-bios.sys');shutil.copyfile(items['limine_cd'],boot/'limine-bios-cd.bin')
    with tarfile.open(iso_root/'boot/initramfs.tar','w',format=tarfile.USTAR_FORMAT) as archive:
        for name in ('init','wtfprobe','jsc'):
            data=runtime_elf(items[name].read_bytes());info=tarfile.TarInfo(name);info.size=len(data);info.mode=0o755;info.mtime=0
            archive.addfile(info,io.BytesIO(data));receipt.setdefault('packed_bytes',{})[name]=len(data)
    (boot/'limine.conf').write_text('timeout: 0\nserial: yes\n\n/jsc-observation\n    protocol: limine\n    path: boot():/boot/srvros.elf\n    cmdline: srvros.log=quiet\n    module_path: boot():/boot/initramfs.tar\n    module_string: initramfs\n')
    iso=out/'jsc.iso'
    with (out/'image.log').open('wb') as log:
        subprocess.run(['xorriso','-as','mkisofs','-R','-r','-J','-b','boot/limine/limine-bios-cd.bin','-no-emul-boot','-boot-load-size','4','-boot-info-table',str(iso_root),'-o',str(iso)],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=90)
        subprocess.run([str(items['limine_tool']),'bios-install',str(iso)],stdout=log,stderr=subprocess.STDOUT,check=True,timeout=30)
    receipt['iso_sha256']=sha(iso)
    with tempfile.TemporaryDirectory(prefix='srv-jsc-',dir='/tmp') as tmp:
        serial=str(Path(tmp)/'serial');cmd=['qemu-system-x86_64','-name','SrvrOS JSC bounded observation','-machine','q35','-accel','tcg,thread=single','-cpu','qemu64','-smp','1','-m','1024M','-cdrom',str(iso),'-boot','d','-display','none','-nic','none','-serial',f'unix:{serial},server=on,wait=off','-monitor','none','-no-reboot']
        if args.qemu_interrupt_log:
            cmd += ['-d', 'int', '-D', str(out/'interrupts.log')]
        receipt['qemu_command']=cmd
        with (out/'qemu.log').open('wb') as log:
            proc=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT);receipt['pid']=proc.pid
            try:
                with socket.socket(socket.AF_UNIX) as sock:
                    deadline=time.monotonic()+15
                    while True:
                        try:sock.connect(serial);break
                        except (FileNotFoundError,ConnectionRefusedError):
                            if proc.poll() is not None or time.monotonic()>=deadline: raise RuntimeError('serial connection failed')
                            time.sleep(.1)
                    sock.settimeout(.2);bootlog=receive(sock,b'srv> ',90);serial_data.extend(bootlog)
                    if b'srv> ' not in bootlog: raise RuntimeError('guest did not reach monitor')
                    commands = [('wtfprobe', 'run /wtfprobe')]
                    if not args.wtf_only:
                        commands.append(('jsc', 'run /jsc -e print(42)'))
                    for name,command in commands:
                        start=time.monotonic();sock.sendall((command+'\n').encode());result=receive_run(sock,45);serial_data.extend(result)
                        exits=re.findall(rb'run: pid=\d+ exited status=(\d+)',result)
                        marker=b'wtfprobe: ok' if name=='wtfprobe' else b'42'
                        marker_found=(marker in result if name=='wtfprobe' else bool(re.search(rb'(?:^|\r?\n)42\r?\n',result)))
                        completed=len(exits)==1 and b'srv> ' in result
                        passed = (completed and exits == [b'0'] and marker_found and b'exception:' not in result)
                        if name=='wtfprobe': passed = passed and result.count(marker)==1
                        else: passed = passed and len(re.findall(rb'(?:^|\r?\n)42\r?\n',result))==1
                        receipt['tests'].append({'name':name,'exit_status':int(exits[0]) if len(exits)==1 else None,'marker_found':marker_found,'completed':completed,'passed':passed,'seconds':round(time.monotonic()-start,3)})
                        if not completed: raise RuntimeError(name+' did not return a monitor exit receipt')
            finally:
                if proc.poll() is None: proc.terminate()
                try:proc.wait(timeout=5)
                except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=5)
                receipt['guest_stopped']=proc.poll() is not None
    if args.require_success and not all(test['passed'] for test in receipt['tests']):
        raise RuntimeError('guest smoke did not produce every required successful result')
    receipt['result']='passed' if args.require_success else 'observed'
except (OSError,ValueError,KeyError,RuntimeError,subprocess.SubprocessError,tarfile.TarError) as error:
    receipt['result']='failed';receipt['error']=str(error)
finally:
    if (out/'interrupts.log').is_file():
        receipt['interrupt_log'] = {'path': str((out/'interrupts.log').relative_to(ROOT)), 'sha256': sha(out/'interrupts.log'), 'bytes': (out/'interrupts.log').stat().st_size}
    (out/'serial.log').write_bytes(serial_data)
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('Guest receipt:',out/'receipt.json','result:',receipt['result'])
if receipt['result'] not in ('observed','passed'):sys.exit(1)
