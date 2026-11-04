#!/usr/bin/env python3
"""Prepare, or explicitly run once, the experimental public-signal guest.

The generated receipt describes only this private diskless QEMU test. The
public production capability remains a separate decision.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tarfile
import tempfile
import time

from browser_storage import StorageGuard
from pack_runtime_elf import runtime_elf

ROOT = Path(__file__).resolve().parents[1]
BROWSER = ROOT.parent / 'SrvrOS-browser'
EXIT = re.compile(rb'(?m)^run: pid=(\d+) exited status=(\d+)\r?\n')
MARKERS = (b'signal-public: native main and worker context ok',
           b'signal-public: nested suspend resume handshake ok')
BOOT_NETWORK_LINES = (b'net: no supported network device, stack idle',
                      b'net: worker not started')


def boot_network_state(data):
    counts = {line.decode():len(re.findall(rb'(?m)^'+re.escape(line)+rb'\r?\n',data))
              for line in BOOT_NETWORK_LINES}
    provider = bool(re.search(rb'(?m)^net: (?:provider=|static ipv4=)',data))
    return {'required_line_counts':counts,'unexpected_provider':provider,
            'allowed':all(count==1 for count in counts.values()) and not provider}


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda: f.read(1024 * 1024), b''):
            h.update(data)
    return h.hexdigest()


def sdk_headers(sdk):
    h = hashlib.sha256()
    files = sorted((sdk / 'include').rglob('*'))
    for p in files:
        if p.is_symlink():
            raise ValueError(f'symlink in SDK headers: {p}')
        if p.is_file():
            h.update(str(p.relative_to(sdk)).encode() + b'\0' + bytes.fromhex(sha(p)))
    return h.hexdigest()


def parsed(data):
    """Return an exit only after its full LF line and a subsequent prompt."""
    matches = list(EXIT.finditer(data))
    if len(matches) != 1:
        return None
    m = matches[0]
    if b'srv> ' not in data[m.end():]:
        return None
    return int(m.group(1)), int(m.group(2))


def parser_selftest():
    assert parsed(b'srv> run /signal-public\r\nrun: pid=4 exited status=0') is None
    assert parsed(b'run: pid=4 exited status=0\n') is None
    assert parsed(b'srv> \nrun: pid=4 exited status=0\r\nsrv> ') == (4, 0)
    assert parsed(b'run: pid=4 exited status=0\nsrv> \nrun: pid=5 exited status=0\nsrv> ') is None
    boot=b'\r\n'.join(BOOT_NETWORK_LINES)+b'\r\nsrv> '
    assert boot_network_state(boot)['allowed']
    assert not boot_network_state(BOOT_NETWORK_LINES[0]+b'\n')['allowed']
    assert not boot_network_state(boot.replace(b'\r\n',b'',1))['allowed']
    assert not boot_network_state(boot+b'\nnet: provider=e1000\n')['allowed']
    assert not boot_network_state(boot+b'\n'+BOOT_NETWORK_LINES[0]+b'\n')['allowed']
    print('signal-public-parser: ok')


def required_file(path):
    path = Path(path).resolve()
    if not path.is_file():
        raise ValueError(f'missing input: {path}')
    return path


def prepare(args):
    parent = Path(args.output_parent)
    guard = StorageGuard(parent)
    guard.preflight()
    run = Path(tempfile.mkdtemp(prefix='signal-public-', dir=parent))
    receipt = {'schema': 1, 'result': 'preparing', 'created_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
               'scope': 'experimental diskless Q35 public-signal probe; production capability not qualified',
               'storage': guard.snapshot, 'inputs': {}, 'commands': []}
    try:
        sdk = Path(args.sdk).resolve()
        inputs = {
            'kernel': required_file(args.kernel), 'init': required_file(args.init),
            'probe': required_file(args.probe), 'probe_source': required_file(ROOT/'tools/test_signal_public_guest.c'),
            'probe_asm': required_file(ROOT/'tools/test_signal_delivery_guest.S'),
            'probe_recipe': required_file(ROOT/'tools/signal_public_guest.mk'),
            'runtime_packer': required_file(ROOT/'tools/pack_runtime_elf.py'),
            'storage_guard': required_file(ROOT/'tools/browser_storage.py'),
            'harness': required_file(__file__), 'sdk_archive': required_file(sdk/'lib/libsrvros.a'),
            'sdk_crt': required_file(sdk/'lib/crt0.o'), 'sdk_linker': required_file(sdk/'lib/app.ld'),
            'limine_sys': required_file(BROWSER/'build/limine-binary/limine-bios.sys'),
            'limine_cd': required_file(BROWSER/'build/limine-binary/limine-bios-cd.bin'),
            'limine_tool': required_file(BROWSER/'build/limine-binary/limine'),
        }
        for name in ('kernel/src/process.c','kernel/src/process_signal_public.inc',
                     'kernel/src/arch/x86_64/idt.c','kernel/src/arch/x86_64/syscall.c',
                     'userspace/lib/src/posix_signal.c','userspace/lib/src/signal_runtime.c',
                     'userspace/lib/src/posix_signal_context.c'):
            inputs[name] = required_file(ROOT/name)
        xorriso = required_file(shutil.which('xorriso') or '')
        qemu = required_file(shutil.which('qemu-system-x86_64') or '')
        inputs.update(xorriso=xorriso, qemu=qemu)
        receipt['git_head'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
        receipt['git_dirty'] = subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)
        receipt['sdk'] = str(sdk)
        receipt['sdk_headers_sha256'] = sdk_headers(sdk)
        receipt['inputs'] = {k:{'path':str(p),'sha256':sha(p)} for k,p in inputs.items()}
        iso_root = run/'iso-root'; boot = iso_root/'boot/limine'; boot.mkdir(parents=True)
        shutil.copyfile(inputs['kernel'],iso_root/'boot/srvros.elf')
        shutil.copyfile(inputs['limine_sys'],boot/'limine-bios.sys')
        shutil.copyfile(inputs['limine_cd'],boot/'limine-bios-cd.bin')
        with tarfile.open(iso_root/'boot/initramfs.tar','w',format=tarfile.USTAR_FORMAT) as ar:
            for name in ('init','probe'):
                payload=runtime_elf(inputs[name].read_bytes())
                ti=tarfile.TarInfo('signal-public' if name=='probe' else name)
                ti.size=len(payload);ti.mode=0o755;ti.mtime=0
                ar.addfile(ti,io.BytesIO(payload))
                receipt.setdefault('packed',{})[name]={'sha256':hashlib.sha256(payload).hexdigest(),'bytes':len(payload)}
        config=('timeout: 0\nserial: yes\n\n/signal-public\n    protocol: limine\n'
                '    path: boot():/boot/srvros.elf\n    cmdline: srvros.log=quiet\n'
                '    module_path: boot():/boot/initramfs.tar\n    module_string: initramfs\n')
        (boot/'limine.conf').write_text(config)
        iso=run/'guest.iso'
        commands=[[str(xorriso),'-as','mkisofs','-R','-r','-J','-b','boot/limine/limine-bios-cd.bin',
                   '-no-emul-boot','-boot-load-size','4','-boot-info-table',str(iso_root),'-o',str(iso)],
                  [str(inputs['limine_tool']),'bios-install',str(iso)]]
        with (run/'prepare.log').open('wb') as log:
            for cmd in commands:
                guard.preflight()
                started=time.monotonic()
                result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=60,check=False)
                receipt['commands'].append({'argv':cmd,'status':result.returncode,'seconds':round(time.monotonic()-started,3)})
                if result.returncode: raise RuntimeError(f'prepare command failed: {cmd[0]}')
        receipt['qemu_argv_template']=[str(qemu),'-name','SrvrOS experimental public signal',
            '-machine','q35','-accel','tcg,thread=single','-cpu','qemu64','-smp','1','-m','1536M',
            '-cdrom',str(iso),'-boot','d','-display','none','-nic','none',
            '-serial','unix:{private_uart},server=on,wait=off','-monitor','none','-no-reboot']
        receipt['boot_network_requirements']=[line.decode() for line in BOOT_NETWORK_LINES]
        receipt['config_sha256']=sha(boot/'limine.conf')
        receipt['initramfs_sha256']=sha(iso_root/'boot/initramfs.tar')
        receipt['iso_sha256']=sha(iso)
        receipt['result']='prepared'
        raw=(json.dumps(receipt,indent=2,sort_keys=True)+'\n').encode()
        (run/'prepare-receipt.json').write_bytes(raw)
        (run/'prepare-receipt.sha256').write_text(hashlib.sha256(raw).hexdigest()+'\n')
        print(run)
    except Exception as e:
        receipt['result']='failed';receipt['error']=f'{type(e).__name__}: {e}'
        (run/'prepare-failure.json').write_text(json.dumps(receipt,indent=2)+'\n')
        raise


def run_guest(args):
    run = Path(args.run).resolve()
    result_path=run/'run-result.json'
    with result_path.open('x') as f: json.dump({'result':'started'},f)
    result={'result':'failed','guest_stopped':False,'payload_send_attempted':False}
    serial=bytearray();proc=None
    try:
        receipt_file=run/'prepare-receipt.json'; raw=receipt_file.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=(run/'prepare-receipt.sha256').read_text().strip():
            raise ValueError('prepared receipt hash mismatch')
        receipt=json.loads(raw)
        if receipt.get('schema')!=1 or receipt.get('result')!='prepared': raise ValueError('not a prepared run')
        if receipt.get('boot_network_requirements')!=[line.decode() for line in BOOT_NETWORK_LINES]:
            raise ValueError('prepared boot requirements mismatch')
        result['prepare_sha256']=hashlib.sha256(raw).hexdigest()
        for name,item in receipt['inputs'].items():
            if sha(required_file(item['path']))!=item['sha256']: raise ValueError(f'changed input: {name}')
        if sdk_headers(Path(receipt['sdk']))!=receipt['sdk_headers_sha256']: raise ValueError('SDK headers changed')
        iso=required_file(run/'guest.iso')
        if sha(iso)!=receipt['iso_sha256']: raise ValueError('ISO hash mismatch')
        if sha(run/'iso-root/boot/limine/limine.conf')!=receipt['config_sha256']: raise ValueError('config hash mismatch')
        guard=StorageGuard(run.parent);guard.preflight()
        with tempfile.TemporaryDirectory(prefix='srv-public-',dir='/tmp') as temp:
            uart=str(Path(temp)/'uart')
            cmd=[arg.replace('{private_uart}',uart) for arg in receipt['qemu_argv_template']]
            result['argv']=cmd
            with (run/'qemu.log').open('xb') as log:
                proc=subprocess.Popen(cmd,stdout=log,stderr=log)
                result['owned_qemu_pid']=proc.pid
                with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as sock:
                    end=time.monotonic()+30
                    while True:
                        try: sock.connect(uart);break
                        except (FileNotFoundError,ConnectionRefusedError):
                            if proc.poll() is not None or time.monotonic()>=end: raise RuntimeError('serial connection failed')
                            time.sleep(0.05)
                    sock.settimeout(0.2)
                    def receive_until(predicate,seconds):
                        data=bytearray();end=time.monotonic()+seconds
                        while time.monotonic()<end:
                            try:
                                part=sock.recv(65536)
                                if not part: raise RuntimeError('serial closed')
                                data.extend(part);serial.extend(part)
                                if len(serial)>16*1024*1024: raise RuntimeError('serial output exceeded 16 MiB')
                                if predicate(data): return bytes(data)
                            except socket.timeout: pass
                        raise TimeoutError('guest serial deadline expired')
                    boot=receive_until(lambda b:b'srv> ' in b,90)
                    (run/'boot-serial.log').write_bytes(boot)
                    result['boot_serial_sha256']=sha(run/'boot-serial.log')
                    result['boot_network_state']=boot_network_state(boot)
                    if not result['boot_network_state']['allowed']:
                        raise RuntimeError('boot network precondition refused; payload not sent')
                    result['payload_send_attempted']=True
                    sock.sendall(b'run /signal-public\n')
                    data=receive_until(lambda b:parsed(b) is not None,150)
                    pid,status=parsed(data)
                    result.update(pid=pid,exit_status=status,markers={m.decode():data.count(m) for m in MARKERS})
                    if status!=0 or any(data.count(m)!=1 for m in MARKERS): raise RuntimeError('probe exit or marker mismatch')
                    result['result']='passed'
    except Exception as e:
        result['error']=f'{type(e).__name__}: {e}'
    finally:
        if proc is not None:
            if proc.poll() is None:
                proc.terminate()
                try:proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    try:proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:result['stop_error']='owned QEMU did not terminate after kill'
            result['guest_stopped']=proc.poll() is not None
        (run/'serial.log').write_bytes(serial)
        result['serial_sha256']=sha(run/'serial.log')
        result_path.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    if result['result']!='passed' or not result['guest_stopped']: raise SystemExit(1)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    action=p.add_mutually_exclusive_group(required=True)
    action.add_argument('--prepare',action='store_true')
    action.add_argument('--run',type=Path,metavar='PREPARED_DIR')
    action.add_argument('--self-test',action='store_true')
    p.add_argument('--output-parent',type=Path,help='existing absolute task-owned APFS directory')
    p.add_argument('--kernel',type=Path,default=ROOT/'build/signal-context/public-experimental/kernel.elf')
    p.add_argument('--probe',type=Path,default=ROOT/'build/signal-context/public-experimental/probe.elf')
    p.add_argument('--init',type=Path,default=ROOT/'build/userspace/init.elf')
    p.add_argument('--sdk',type=Path,default=ROOT/'build/sysroot/srvros')
    a=p.parse_args()
    if a.self_test: parser_selftest()
    elif a.prepare:
        if not a.output_parent or not a.output_parent.is_absolute() or not a.output_parent.is_dir():p.error('--output-parent must be an existing absolute task-owned directory')
        prepare(a)
    else:run_guest(a)


if __name__=='__main__':main()
