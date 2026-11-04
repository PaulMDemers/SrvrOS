#!/usr/bin/env python3
"""Qualify less's guest PTY/model and non-TTY paths on a disposable image.

Prerequisites: built ISO inputs, staged less in --disk, and a separately built
build/userspace/test_less_probe.elf (see tools/test_less_probe.c). This does
not drive the desktop Terminal; its GUI checks are separate.
"""
import argparse
import hashlib
import json
import os
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
from pack_runtime_elf import runtime_elf
from thread_smoke import has_fatal_exception

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = None
COPY = b'plain ASCII first\nsecond line\n\nlast line without newline'
WORKFLOW = b'''#!/fat/bin/sh
chmod 755 /fat/less-test/less-probe
/fat/less-test/less-probe > /fat/less-test/probe.log
 echo $? > /fat/less-test/probe-status.txt
less /fat/less-test/copy-input.txt > /fat/less-test/copy-out.txt
 echo $? > /fat/less-test/copy-status.txt
cat /fat/less-test/copy-input.txt | less > /fat/less-test/pipe-out.txt
 echo $? > /fat/less-test/pipe-status.txt
less /fat/less-test/absent.txt > /fat/less-test/missing-out.txt 2> /fat/less-test/missing-err.txt
 echo $? > /fat/less-test/missing-status.txt
echo LESS_SMOKE_WORKFLOW_DONE
'''
SNAPSHOTS = {
    '01-initial.txt': 'line-001', '02-forward.txt': 'line-010',
    '03-back.txt': 'line-001', '04-line-forward.txt': 'line-010',
    '05-line-back.txt': 'line-001', '06-end.txt': 'line-100',
    '07-begin.txt': 'line-001', '08-search.txt': 'line-035',
    '09-next.txt': 'line-070', '10-previous.txt': 'line-035',
    '11-resize.txt': 'line-045', '13-pipe-initial.txt': 'pipe-001',
    '14-pipe-forward.txt': 'pipe-012', '15-pipe-back.txt': 'pipe-001',
    '17-held-initial.txt': 'held-001',
    '23-unknown-term.txt': 'less: unsupported TERM:',
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    global EVIDENCE
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    parser.add_argument('--disk', type=Path, default=ROOT/'build/srvros.exfat')
    parser.add_argument('--probe', type=Path, default=ROOT/'build/userspace/test_less_probe.elf')
    args = parser.parse_args()
    evidence_root = ROOT/'build/review/less'
    evidence_root.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'), dir=evidence_root))
    EVIDENCE = out
    disk = out/'disk.exfat'
    shutil.copyfile(args.disk, disk)
    image = ExfatFixture(disk.read_bytes())
    less = (ROOT/'build/userspace/less.elf').read_bytes()
    staged = image.read('bin/less')
    if staged != runtime_elf(less):
        raise RuntimeError('staged less differs from build/userspace/less.elf')
    probe = runtime_elf(args.probe.read_bytes())
    fixtures = {'less-probe': probe, 'workflow.sh': WORKFLOW, 'copy-input.txt': COPY}
    image.add_directory_files('less-test', fixtures)
    disk.write_bytes(image.data)
    receipt = {'status': 'INCOMPLETE', 'source_disk_sha256': sha(args.disk.read_bytes()),
               'less_runtime_sha256': sha(staged), 'probe_runtime_sha256': sha(probe),
               'fixtures_sha256': {k: sha(v) for k, v in fixtures.items()},
               'source_commit': subprocess.check_output(['git','rev-parse','HEAD'], cwd=ROOT, text=True).strip(),
               'source_status': subprocess.check_output(['git','status','--porcelain'], cwd=ROOT, text=True),
               'scope': 'PTY model and non-TTY checks; GUI and full upstream suite excluded'}
    (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('less smoke evidence:', out, flush=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix='less-vm-') as tempdir:
        tmp = Path(tempdir)
        iso = build_iso(str(ROOT), str(tmp), 1280, 800, os.environ.copy())
        port = choose_port(30001, 38000)
        qmp_path = tmp/'qmp'
        cmd = [args.qemu, '-M','q35','-m','512M','-cdrom',str(iso),'-boot','d',
               '-drive',f'if=none,id=disk,file={disk},format=raw','-device','ich9-ahci,id=ahci',
               '-device','ide-hd,drive=disk,bus=ahci.0','-device','qemu-xhci,id=xhci',
               '-device','usb-kbd,bus=xhci.0','-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
               '-qmp',f'unix:{qmp_path},server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        serial = qmp = None
        with (out/'qemu.log').open('wb') as log:
            vm = subprocess.Popen(cmd, stdout=log, stderr=log)
            try:
                serial = connect_serial(port, 20); serial.settimeout(.2); qmp = QMP(qmp_path)
                consumed = 0
                def wait(marker, timeout):
                    nonlocal consumed
                    start = time.monotonic()
                    while marker not in output[consumed:]:
                        if marker == b'srv> ' and not consumed:
                            qmp.boot_if_menu_stalled(output, time.monotonic()-start)
                        if time.monotonic()-start > timeout or vm.poll() is not None:
                            raise RuntimeError(f'missing {marker!r}; see {out}')
                        try:
                            chunk = serial.recv(16384)
                            if not chunk: raise RuntimeError('serial disconnected')
                            output.extend(chunk)
                        except socket.timeout: pass
                    consumed = output.index(marker, consumed)+len(marker)
                wait(b'srv> ', 100)
                serial.sendall(b'run /fat/bin/sh /fat/less-test/workflow.sh\n')
                wait(b'LESS_SMOKE_WORKFLOW_DONE', 300)
                wait(b'srv> ', 40)
                serial.sendall(b'fsck /fat\n'); wait(b'exfat-check:', 40); wait(b'srv> ', 40)
                if b'leaks=0 fat=0 errors=0 ok' not in output:
                    raise RuntimeError('guest exFAT check failed')
                if has_fatal_exception(output.decode(errors='replace')):
                    raise RuntimeError('guest fatal exception')
            finally:
                if serial: serial.close()
                if qmp: qmp.stream.close(); qmp.sock.close()
                stop_process(vm)
                (out/'serial.log').write_bytes(output)
    image = ExfatFixture(disk.read_bytes())
    def read(name):
        data = image.read('less-test/'+name)
        (out/name).write_bytes(data)
        return data
    for name in ('probe', 'copy', 'pipe'):
        status = read(name+'-status.txt')
        if status != b'0\n': raise RuntimeError(f'{name} status: {status!r}')
    log = read('probe.log')
    if b'less-probe: PASS file and pipe navigation search resize interrupts quit termios alternate screen and unknown TERM' not in log:
        raise RuntimeError(f'probe did not report PASS: {log!r}')
    for name in ('copy-out.txt', 'pipe-out.txt'):
        if read(name) != COPY: raise RuntimeError(f'{name} is not byte-identical to input')
    missing_status = read('missing-status.txt').strip()
    if not missing_status.isdigit() or int(missing_status) == 0:
        raise RuntimeError(f'missing file unexpectedly succeeded: {missing_status!r}')
    if read('missing-out.txt') != b'' or not read('missing-err.txt'):
        raise RuntimeError('missing file did not report an error on stderr')
    for name, marker in SNAPSHOTS.items():
        data = read(name)
        if marker.encode() not in data: raise RuntimeError(f'{name} missing {marker}')
    for name in ('18-held-first-block.txt', '19-held-first-interrupt.txt',
                 '20-held-second-block.txt', '21-held-second-interrupt.txt'):
        if not read(name).startswith(b'alt=1 '):
            raise RuntimeError(f'{name}: pager left alternate screen prematurely')
    for name in ('12-quit.txt', '16-pipe-quit.txt', '22-held-quit.txt'):
        data = read(name)
        if not data.startswith(b'alt=0 '): raise RuntimeError(f'{name}: alternate screen left active')
        if name == '12-quit.txt' and b'original-shell' not in data:
            raise RuntimeError('original screen was not restored')
        if name == '22-held-quit.txt' and b'signal-shell' not in data:
            raise RuntimeError('screen was not restored after interrupts')
    receipt['status'] = 'PASS'
    (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(f'less smoke: PASS PTY model, pipe, interrupts, unknown TERM, and non-TTY checks; evidence: {out}', flush=True)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        if EVIDENCE:
            path = EVIDENCE/'receipt.json'
            receipt = json.loads(path.read_text()) if path.exists() else {}
            receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
            path.write_text(json.dumps(receipt, indent=2)+'\n')
        raise
