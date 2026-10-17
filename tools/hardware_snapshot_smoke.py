#!/usr/bin/env python3
"""Run five guest rounds on a private sparse copy of a captured exFAT volume."""
import argparse
import hashlib
import json
from pathlib import Path
import secrets
import subprocess
import tempfile

import hardware_test as tests
from hardware_workload import verify_tree
from hardware_shell_workload import verify as verify_shell
from inspect_hardware_test import Reader
from usb_storage_smoke import make_iso
from uefi_usb_smoke import default_ovmf_path


def inventory(path, volume):
    result = {}
    with path.open('rb') as stream:
        reader = Reader(stream, volume)
        def walk(first, size=None, contiguous=False, prefix='', depth=0):
            if depth > 16: raise ValueError('Snapshot directory depth exceeded')
            for name, _, entry in reader.directory(first, size, contiguous):
                key = prefix + name
                if key == '.srvros': continue  # SrvrOS rewrites its metadata sidecar.
                info = reader.stream_info(entry)
                if entry[4] & 0x10:
                    walk(*info, prefix=key+'/', depth=depth+1)
                else:
                    digest = hashlib.sha256()
                    for _, block in reader.chunks(*info): digest.update(block)
                    result[key] = {'bytes': info[1], 'sha256': digest.hexdigest()}
        walk(reader.root)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--volume', required=True)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    args = parser.parse_args()
    source = args.snapshot.resolve()
    if not source.is_file(): raise ValueError('A regular snapshot file is required')
    volume = tests.volume_id(args.volume)
    baseline = inventory(source, volume)
    root = Path(__file__).resolve().parents[1]
    logs = root/'build/review'
    run = secrets.token_hex(8)
    release = hashlib.sha256((root/'build/kernel/srvros.elf').read_bytes()).hexdigest()[:16]
    serial = logs/'snapshot-cycle-serial.log'
    serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='srvros-snapshot-') as directory:
        temp = Path(directory)
        disk = temp/'volume.img'
        subprocess.run(['cp', '--reflink=auto', '--sparse=always', str(source), str(disk)], check=True)
        # Only the disposable copy is recovered; the captured evidence is read-only.
        subprocess.run(['/usr/sbin/fsck.exfat', '-p', str(disk)], check=True)
        subprocess.run([str(root/'build/tests/exfat-driver'), str(disk), 'snapshot-seed', run], check=True)
        subprocess.run(['/usr/sbin/fsck.exfat', '-n', str(disk)], check=True)
        iso = make_iso(root, temp, False, f' srvros.autotest-volume={volume}'
            f' srvros.autotest-run={run} srvros.autotest-release={release} srvros.autotest-repeat=5')
        cmd = [args.qemu, '-M', 'q35', '-m', '2G',
            '-drive', f'if=pflash,format=raw,readonly=on,file={args.ovmf}',
            '-cdrom', str(iso), '-boot', 'd', '-device', 'qemu-xhci,id=xhci',
            '-device', 'usb-kbd,bus=xhci.0', '-drive', f'if=none,id=stick,format=raw,file={disk}',
            '-device', 'usb-storage,drive=stick,bus=xhci.0', '-serial', f'file:{serial}',
            '-display', 'none', '-monitor', 'none', '-no-reboot']
        with (logs/'snapshot-cycle-qemu.log').open('wb') as err:
            process = subprocess.Popen(cmd, stdout=err, stderr=err)
            try: process.wait(timeout=900)
            finally:
                if process.poll() is None: process.kill(); process.wait()
        output = serial.read_text(errors='replace')
        assert process.returncode == 0 and 'stress 5/5 PASS' in output, 'See snapshot-cycle-serial.log'
        assert output.count('autotest: PASS report=flushed') == 5
        reports = []
        with disk.open('rb') as stream:
            reader = Reader(stream, volume)
            assert int.from_bytes(reader.read(106, 2), 'little') == 0
            def entry_at(path):
                info = (reader.root, None, False)
                for name in path.split('/'):
                    entry = next(e for n, _, e in reader.directory(*info) if n == name)
                    info = reader.stream_info(entry)
                return entry
            def read(path):
                return b''.join(data for _, data in reader.chunks(*reader.stream_info(entry_at(path))))
            def listing(path):
                return {n: 'directory' if e[4]&0x10 else 'file'
                        for n, _, e in reader.directory(*reader.stream_info(entry_at(path)))}
            for member in tests.batch_runs(run, 5):
                prefix = 'srvros-tests/'+member
                meta = {'run': member, 'release': release, 'volume': volume, 'suite': tests.SUITE}
                fields = tests.validate_result(meta, read(prefix+'.txt'), read(prefix+'.done'))
                assert fields['status'] == 'PASS' and read(prefix+'.probe') == tests.PROBE_BYTES
                files = verify_tree(lambda name, size: read(prefix+'.files/'+name),
                    lambda name: listing(prefix+'.files'+('/'+name if name else '')))
                shell = verify_shell(lambda name: read(prefix+'.shell/'+name))
                reports.append({'run': member, 'report': fields, 'files': files, 'shell': shell})
        after = inventory(disk, volume)
        assert all(after.get(k) == v for k, v in baseline.items()), 'An existing file changed'
        subprocess.run(['/usr/sbin/fsck.exfat', '-n', str(disk)], check=True)
        (logs/'snapshot-cycle-result.json').write_text(json.dumps({
            'source': str(source), 'run': run, 'release': release, 'reports': reports,
            'existing_files_unchanged': len(baseline), 'flags': 0, 'status': 'PASS'}, indent=2)+'\n')
        print(f'snapshot-cycle: PASS (5 reports/CRCs, 75 outputs, {len(baseline)} existing files unchanged, clean filesystem)', flush=True)


if __name__ == '__main__': main()
