#!/usr/bin/env python3
"""Two-boot xz guest/host interoperability on a disposable exFAT image.

Requires built boot inputs, staged /fat/bin/xz, both test probe ELFs and host xz.
Codec-progress cancellation uses a diagnostic build; signal delivery inside
an individual codec call is not established. See receipt.json and retained guest output for scope.
"""
import argparse
import hashlib
import io
import json
import lzma
import os
from pathlib import Path
import random
import re
import shutil
import socket
import subprocess
import tarfile
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from displayd_resolution_smoke import build_iso
from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from thread_smoke import has_fatal_exception

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = None


def sha(data):
    return hashlib.sha256(data).hexdigest()


def crc64_xz(data):
    crc = (1 << 64)-1
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xC96C5795D7870F42 if crc & 1 else 0)
    return crc ^ ((1 << 64)-1)


def fixtures(host_xz):
    rand = random.Random(0x585A710)
    binary = bytes(rand.randrange(256) for _ in range(262144))
    ascii_data = (b'SrvrOS xz interoperability\n0123456789\n' * 400)
    raw = {'empty': b'', 'zeros': bytes(128*1024), 'ascii': ascii_data, 'binary': binary,
           'multi': binary, 'concat': b'first concatenated stream\n'+ascii_data}
    files = {k+'.raw': v for k,v in raw.items()}
    files['cpu.raw'] = rand.randbytes(4*1024*1024)
    for name in ('empty','zeros','ascii','binary'):
        files[name+'.xz'] = lzma.compress(raw[name], format=lzma.FORMAT_XZ,
                                         check=lzma.CHECK_CRC64, preset=6)
    files['multi.xz'] = subprocess.check_output(
        [host_xz,'-6','--check=crc64','--block-size=64KiB','-c'], input=binary)
    files['concat.xz'] = (lzma.compress(b'first concatenated stream\n',preset=6) +
                          lzma.compress(ascii_data,preset=6))
    tar = io.BytesIO()
    with tarfile.open(fileobj=tar, mode='w', format=tarfile.USTAR_FORMAT) as archive:
        for name, content in [('report.txt', b'guest tar roundtrip\n'),('data.txt', b'one\ntwo\n')]:
            info = tarfile.TarInfo(name);info.size=len(content);info.mode=0o644;info.mtime=0
            archive.addfile(info,io.BytesIO(content))
    files['fixture.tar'] = tar.getvalue()
    files['fixture.tar.xz'] = lzma.compress(files['fixture.tar'], preset=6)
    files['legacy.lzma'] = lzma.compress(ascii_data, format=lzma.FORMAT_ALONE, preset=6)
    files['invalid.xz'] = b'not an xz stream\n'
    files['trunc.xz'] = files['ascii.xz'][:-8]
    crc = crc64_xz(ascii_data).to_bytes(8,'little')
    pos = files['ascii.xz'].find(crc)
    if pos < 0: raise RuntimeError('could not locate CRC64 fixture check')
    broken = bytearray(files['ascii.xz']);broken[pos] ^= 1;files['crc.xz'] = bytes(broken)
    files['large-dict.xz'] = lzma.compress(ascii_data,format=lzma.FORMAT_XZ,
                                          filters=[{'id':lzma.FILTER_LZMA2,'dict_size':64*1024*1024}])
    return files


def script():
    lines=['#!/fat/bin/sh', 'chmod 755 /fat/xz-test/xz-probe', 'chmod 755 /fat/xz-test/xz-alloc-probe', 'chmod 755 /fat/xz-test/xz-diagnostic']
    def run(name, command):
        lines.extend([command, f'echo $? > /fat/xz-test/{name}.status'])
    opt='--memlimit-decompress=32MiB'
    for name in ('empty','zeros','ascii','binary','multi','concat'):
        run('dec-'+name,f'xz -dc {opt} /fat/xz-test/{name}.xz > /fat/xz-test/{name}.out')
        run('test-'+name,f'xz -t {opt} /fat/xz-test/{name}.xz')
    run('pipe','cat /fat/xz-test/binary.xz | xz -dc --memlimit-decompress=32MiB > /fat/xz-test/pipe.out')
    run('compress','xz -0 -c --memlimit-compress=8MiB --no-adjust /fat/xz-test/binary.raw > /fat/xz-test/guest.xz')
    run('roundtrip','xz -dc --memlimit-decompress=32MiB /fat/xz-test/guest.xz > /fat/xz-test/roundtrip.out')
    run('tar-decode','xz -dc --memlimit-decompress=32MiB /fat/xz-test/fixture.tar.xz > /fat/xz-test/guest.tar')
    run('tar-list','tar -tf /fat/xz-test/guest.tar > /fat/xz-test/tar.list')
    for name in ('invalid','trunc','crc','large-dict'):
        run('fail-'+name,f'xz -dc {opt} /fat/xz-test/{name}.xz > /fat/xz-test/{name}.partial 2> /fat/xz-test/{name}.err')
    run('fail-legacy','xz -dc --memlimit-decompress=32MiB /fat/xz-test/legacy.lzma > /fat/xz-test/legacy.partial 2> /fat/xz-test/legacy.err')
    run('fail-cap-override','xz -dc --memlimit-decompress=128MiB /fat/xz-test/large-dict.xz > /fat/xz-test/override.partial 2> /fat/xz-test/override.err')
    run('fail-cap-env','XZ_DEFAULTS=--memlimit-decompress=128MiB xz -dc /fat/xz-test/large-dict.xz > /fat/xz-test/env.partial 2> /fat/xz-test/env.err')
    run('fail-no-adjust','xz -0 -c --memlimit-compress=1MiB --no-adjust /fat/xz-test/binary.raw > /fat/xz-test/no-adjust.partial 2> /fat/xz-test/no-adjust.err')
    run('fail-inplace','xz -0 /fat/xz-test/ascii.raw > /fat/xz-test/inplace.out 2> /fat/xz-test/inplace.err')
    run('probe','/fat/xz-test/xz-probe > /fat/xz-test/probe.log 2> /fat/xz-test/probe.err')
    run('alloc-probe','/fat/xz-test/xz-alloc-probe > /fat/xz-test/alloc.log 2> /fat/xz-test/alloc.err')
    lines.append('echo XZ_WORKFLOW_DONE')
    return ('\n'.join(lines)+'\n').encode()


def main():
    global EVIDENCE
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--qemu',default=os.environ.get('QEMU','qemu-system-x86_64'))
    ap.add_argument('--disk',type=Path,default=ROOT/'build/srvros.exfat')
    ap.add_argument('--probe',type=Path,default=ROOT/'build/userspace/test_xz_probe.elf')
    ap.add_argument('--alloc-probe',type=Path,default=ROOT/'build/userspace/test_xz_alloc_probe.elf')
    ap.add_argument('--diagnostic',type=Path,default=ROOT/'build/userspace/xz-diagnostic.elf')
    ap.add_argument('--host-xz',default=shutil.which('xz'))
    args=ap.parse_args()
    if not args.host_xz: raise RuntimeError('host xz required for true multi-block fixture')
    base=ROOT/'build/review/xz';base.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'),dir=base));EVIDENCE=out
    disk=out/'disk.exfat';shutil.copyfile(args.disk,disk)
    image=ExfatFixture(disk.read_bytes())
    binary=(ROOT/'build/userspace/xz.elf').read_bytes();staged=image.read('bin/xz')
    if staged!=runtime_elf(binary): raise RuntimeError('staged xz differs from built ELF')
    files=fixtures(args.host_xz);files['workflow.sh']=script();files['xz-probe']=runtime_elf(args.probe.read_bytes());files['xz-alloc-probe']=runtime_elf(args.alloc_probe.read_bytes());files['xz-diagnostic']=runtime_elf(args.diagnostic.read_bytes())
    image.add_directory_files('xz-test',files);disk.write_bytes(image.data)
    source_files=[p for p in sorted((ROOT/'ports/srvros/xz').rglob('*')) if p.is_file()]
    receipt={'status':'INCOMPLETE','source_disk_sha256':sha(args.disk.read_bytes()),
             'port_source_sha256':{str(p.relative_to(ROOT)):sha(p.read_bytes()) for p in source_files},
             'kernel_sha256':sha((ROOT/'build/iso_root/boot/srvros.elf').read_bytes()),
             'initramfs_sha256':sha((ROOT/'build/iso_root/boot/initramfs.tar').read_bytes()),
             'source_diff_sha256':sha(subprocess.check_output(['git','diff','--binary','HEAD'],cwd=ROOT)),
             'xz_sha256':sha(binary),'staged_xz_sha256':sha(staged),'probe_sha256':sha(files['xz-probe']),'alloc_probe_sha256':sha(files['xz-alloc-probe']),
             'diagnostic_sha256':sha(files['xz-diagnostic']),
             'fixtures_sha256':{k:sha(v) for k,v in files.items()},
             'host_xz_version':subprocess.check_output([args.host_xz,'--version'],text=True).splitlines()[0],
             'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
             'source_status':subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True),
             'scope':'guest interoperability, failure, allocator and cancellation after codec progress; GUI and upstream tests separate; no in-call signal timing claim'}
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    print('xz evidence:',out,flush=True)
    with tempfile.TemporaryDirectory(prefix='xz-vm-') as temp:
        temp=Path(temp);iso=build_iso(str(ROOT),str(temp),1280,800,os.environ.copy())
        for boot in (1,2):
            output=bytearray();consumed=0;port=choose_port(30001,38000);qmp_path=temp/f'qmp{boot}'
            cmd=[args.qemu,'-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
                 '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci',
                 '-device','ide-hd,drive=disk,bus=ahci.0','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
                 '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off','-qmp',f'unix:{qmp_path},server=on,wait=off',
                 '-display','none','-monitor','none','-no-reboot']
            serial=qmp=None
            with (out/f'qemu-{boot}.log').open('wb') as log:
                vm=subprocess.Popen(cmd,stdout=log,stderr=log)
                try:
                    serial=connect_serial(port,20);serial.settimeout(.2);qmp=QMP(qmp_path)
                    def wait(marker,timeout):
                        nonlocal consumed
                        start=time.monotonic()
                        while marker not in output[consumed:]:
                            if marker==b'srv> ' and not consumed: qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                            if time.monotonic()-start>timeout or vm.poll() is not None: raise RuntimeError(f'boot {boot}: missing {marker!r}')
                            try:
                                chunk=serial.recv(16384)
                                if not chunk: raise RuntimeError('serial disconnected')
                                output.extend(chunk)
                            except socket.timeout: pass
                        consumed=output.index(marker,consumed)+len(marker)
                    wait(b'srv> ',100)
                    if boot==1:
                        serial.sendall(b'run /fat/bin/sh /fat/xz-test/workflow.sh\n');wait(b'XZ_WORKFLOW_DONE',300);wait(b'srv> ',40)
                    else:
                        serial.sendall(b'run /fat/bin/sh\n');wait(b' $ ',40)
                        serial.sendall(b'xz -t --memlimit-decompress=32MiB /fat/xz-test/guest.xz\n')
                        wait(b' $ ',60)
                        serial.sendall(b'echo $? > /fat/xz-test/reboot.status\n');wait(b' $ ',40)
                        serial.sendall(b'xz -dc --memlimit-decompress=32MiB /fat/xz-test/guest.xz > /fat/xz-test/reboot.out\n');wait(b' $ ',60)
                        serial.sendall(b'echo $? > /fat/xz-test/reboot-decode.status\n');wait(b' $ ',40)
                        serial.sendall(b'exit\n');wait(b'srv> ',40)
                    serial.sendall(b'fsck /fat\n');wait(b'exfat-check:',40);wait(b'srv> ',40)
                    if not re.search(rb'exfat-check: mounts=1 files=\d+ clusters=\d+ leaks=0 fat=0 errors=0 ok',output): raise RuntimeError('guest fsck failed')
                    if has_fatal_exception(output.decode(errors='replace')): raise RuntimeError('guest fatal exception')
                finally:
                    if serial:serial.close()
                    if qmp:qmp.stream.close();qmp.sock.close()
                    stop_process(vm);(out/f'serial-{boot}.log').write_bytes(output)
            img=ExfatFixture(disk.read_bytes())
            for name,data in files.items():
                if img.read('xz-test/'+name)!=data: raise RuntimeError('fixture modified: '+name)
    img=ExfatFixture(disk.read_bytes())
    def get(name):
        data=img.read('xz-test/'+name);(out/name).write_bytes(data);return data
    # Save failure evidence before asserting any status, including partial
    # redirected outputs. Missing artifacts remain visible as missing.
    artifacts=['probe.log','probe.err','alloc.log','alloc.err','guest.xz','guest.tar','tar.list',
               'pipe.out','roundtrip.out','reboot.out','override.partial','override.err',
               'env.partial','env.err','no-adjust.partial','no-adjust.err','inplace.out','inplace.err','cpu-partial.xz',
               'legacy.partial','legacy.err']
    for name in ('empty','zeros','ascii','binary','multi','concat'):
        artifacts.append(name+'.out')
    for name in ('invalid','trunc','crc','large-dict'):
        artifacts += [name+'.partial',name+'.err']
    for name in artifacts:
        try: get(name)
        except StopIteration: pass
    successes=[f'{p}-{n}' for p in ('dec','test') for n in ('empty','zeros','ascii','binary','multi','concat')]
    successes+=['pipe','compress','roundtrip','tar-decode','tar-list','probe','alloc-probe','reboot','reboot-decode']
    for name in successes:
        result=get(name+'.status')
        if result!=b'0\n': raise RuntimeError(f'{name} status {result!r}')
    for name in ('invalid','trunc','crc','large-dict','legacy','cap-override','cap-env','no-adjust','inplace'):
        status=get('fail-'+name+'.status').strip()
        if not status.isdigit() or int(status)==0: raise RuntimeError(f'{name} unexpectedly succeeded: {status!r}')
    try:
        img.read('xz-test/ascii.raw.xz')
    except StopIteration:
        pass
    else:
        raise RuntimeError('in-place mode created an output file')
    for name in ('empty','zeros','ascii','binary','multi','concat'):
        if get(name+'.out')!=files[name+'.raw']:raise RuntimeError(name+' decode mismatch')
    for name in ('pipe.out','roundtrip.out','reboot.out'):
        if get(name)!=files['binary.raw']:raise RuntimeError(name+' mismatch')
    if get('guest.tar')!=files['fixture.tar']:raise RuntimeError('tar bytes differ')
    listing=get('tar.list')
    if b'report.txt' not in listing or b'data.txt' not in listing:raise RuntimeError('tar listing mismatch')
    if lzma.decompress(get('guest.xz'))!=files['binary.raw']:raise RuntimeError('host decode mismatch')
    if b'xz-probe: PASS' not in get('probe.log'):raise RuntimeError('probe missing PASS')
    if b'xz-alloc: PASS' not in get('alloc.log'):raise RuntimeError('allocator probe missing PASS')
    receipt['status']='PASS';(out/'receipt.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
    print('xz smoke: PASS interoperability and probe; GUI and upstream gates separate; no in-call timing claim:',out,flush=True)


if __name__=='__main__':
    try:main()
    except Exception as error:
        if EVIDENCE:
            path=EVIDENCE/'receipt.json';receipt=json.loads(path.read_text()) if path.exists() else {}
            receipt.update(status='FAIL',error=f'{type(error).__name__}: {error}');path.write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
        raise
