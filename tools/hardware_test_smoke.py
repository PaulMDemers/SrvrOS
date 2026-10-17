#!/usr/bin/env python3
"""Real GRUB one-shot -> SrvrOS USB test/report -> reset -> default-entry sentinel."""
import argparse
import contextlib
import io
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
from unittest.mock import patch

import hardware_boot as boot
import hardware_test as test
import hardware_shell_workload
from hardware_workload import verify_tree
import inspect_hardware_test as recovery
import repair_hardware_test as repair
from hardware_boot_smoke import make_disk, wait_for
from exfat_test_image import ExfatFixture, check_clean_image
from uefi_usb_smoke import default_ovmf_path

RUN = '0123456789abcdef'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--stress', action='store_true')
    args = parser.parse_args()
    members = test.batch_runs(RUN, 5 if args.stress else 1)
    root = Path(__file__).resolve().parents[1]
    logs = root / 'build/review'
    logs.mkdir(exist_ok=True)
    serial = logs / 'autotest-cycle-serial.log'
    serial.unlink(missing_ok=True)
    with tempfile.TemporaryDirectory(prefix='srvros-auto-cycle-') as name:
        temp = Path(name)
        # Use an independently formatted filesystem, including a real up-case
        # table and boot checksums. Our boot RAM image is not an interoperability oracle.
        volume_image = temp / 'volume.img'
        with volume_image.open('wb') as out:
            out.truncate(128 * 1024 * 1024)
        subprocess.run(['/usr/sbin/mkfs.exfat', '-c', '128K', str(volume_image)], check=True)
        fixture = ExfatFixture(volume_image.read_bytes())
        volume = f'{fixture.u32(100):08x}'
        fixture.add_directory_files('srvros-tests', {f'{member}.req': f'SRVROS-AUTOTEST-V3 {member}\n'.encode() for member in members})
        volume_image.write_bytes(fixture.data)
        print(check_clean_image(volume_image), end='', flush=True)
        with patch.object(boot, 'STAGE', temp / 'stage'), contextlib.redirect_stdout(io.StringIO()):
            release = boot.stage(autotest_volume=volume, run_id=RUN, repeat=len(members))
        source = temp / 'stage' / release
        assert fixture.read(f'srvros-tests/{RUN}.req') == f'SRVROS-AUTOTEST-V3 {RUN}\n'.encode()
        disk = temp / 'scratch.img'
        mbr = bytearray(512)
        mbr[450] = 7
        struct.pack_into('<II', mbr, 454, 2048, len(fixture.data)//512)
        mbr[510:] = b'\x55\xaa'
        with disk.open('wb') as out:
            out.write(mbr); out.seek(2048*512); out.write(fixture.data)
        env = temp / 'grubenv'
        subprocess.run(['grub-editenv', str(env), 'create'], check=True)
        subprocess.run(['grub-editenv', str(env), 'set', 'next_entry=srvros-test'], check=True)
        cfg = temp / 'grub.cfg'
        cfg.write_text(f'''serial --unit=0 --speed=115200
terminal_output console serial
search --no-floppy --fs-uuid --set=root 5352-5652
load_env -f ($root)/grubenv next_entry
if [ "${{next_entry}}" ]; then
    set default="${{next_entry}}"
    set next_entry=
    save_env -f ($root)/grubenv next_entry
else
    set default=host
fi
set timeout=0
menuentry 'Host default sentinel' --id host {{
    echo SRVROS_RETURNED_TO_HOST_DEFAULT
    halt
}}
menuentry 'SrvrOS automated test' --id srvros-test {{
    chainloader /EFI/srvros/{release}/loader/BOOTX64.EFI
}}
''')
        subprocess.run(['grub-script-check', str(cfg)], check=True)
        loader = temp / 'BOOTX64.EFI'
        subprocess.run(['grub-mkstandalone', '-O', 'x86_64-efi', '--locales=', '--fonts=',
                        '--modules=part_gpt fat chain search search_fs_uuid normal serial terminal loadenv halt',
                        '-o', str(loader), f'boot/grub/grub.cfg={cfg}'], check=True)
        esp = temp / 'esp.img'
        make_disk(esp, loader, source, release, {'grubenv': env.read_bytes()})
        cmd = [args.qemu, '-M', 'q35', '-m', '2G',
               '-drive', f'if=pflash,format=raw,readonly=on,file={args.ovmf}',
               '-drive', f'format=raw,file={esp}',
               '-drive', f'if=none,id=stick,format=raw,file={disk}',
               '-device', 'qemu-xhci,id=xhci', '-device', 'usb-storage,drive=stick,bus=xhci.0',
               '-device', 'usb-kbd,bus=xhci.0', '-serial', f'file:{serial}',
               '-monitor', 'none', '-display', 'none']
        def boot_guest(label, readonly=False, replay=False):
            log = logs / f'autotest-{label}-serial.log'
            log.unlink(missing_ok=True)
            # Re-arm only the disposable GRUB environment for negative tests.
            make_disk(esp, loader, source, release, {'grubenv': env.read_bytes()})
            command = list(cmd)
            command[command.index('-serial') + 1] = f'file:{log}'
            if readonly:
                drive = f'if=none,id=stick,format=raw,file={disk}'
                command[command.index(drive)] = drive + ',readonly=on'
            with (logs / f'autotest-{label}-qemu.log').open('wb') as errors:
                process = subprocess.Popen(command, stdout=errors, stderr=errors)
                try:
                    marker = ('autotest: HALTED - photograph this screen; restart the laptop manually\n'
                              if replay or readonly else 'SRVROS_RETURNED_TO_HOST_DEFAULT')
                    output = wait_for(log, lambda s: marker in s, process, 900 if args.stress else 180)
                    if replay or readonly:
                        required = 'request absent, invalid or already consumed' if replay else 'request-consumption write/flush failed'
                        if required not in output:
                            raise RuntimeError('Missing failure diagnostic')
                        # Observe beyond the removed 15-second delay. Failure
                        # must retain the screen and never reach the host entry.
                        if readonly:
                            time.sleep(17)
                            settled = log.read_text(errors='replace')
                            if settled != output or process.poll() is not None:
                                raise RuntimeError('Failure did not remain halted')
                        if 'reboot: ACPI reset' in output or 'SRVROS_RETURNED_TO_HOST_DEFAULT' in output:
                            raise RuntimeError('Failure unexpectedly rebooted')
                    else:
                        for text in ('autotest: PASS report=flushed', 'reboot: ACPI reset', 'storage: RAM-only mode; AHCI disabled'):
                            if text not in output: raise RuntimeError(f'Missing {text}')
                    if args.stress and not readonly and not replay:
                        if output.count('autotest: PASS report=flushed') != 5 or 'stress 5/5 PASS' not in output:
                            raise RuntimeError('Not all five rounds completed')
                    if output.count('srvros: entering kernel') != 1:
                        raise RuntimeError('One-shot test did not boot exactly once')
                finally:
                    process.terminate()
                    try: process.wait(10)
                    except subprocess.TimeoutExpired:
                        process.kill(); process.wait()
            print(f'hardware-test-smoke: {label} PASS', flush=True)

        boot_guest('cycle')
        with disk.open('rb') as stream:
            stream.seek(2048*512)
            result = ExfatFixture(stream.read())
        volume_image.write_bytes(result.data)
        print(check_clean_image(volume_image), end='', flush=True)
        data = result.read(f'srvros-tests/{RUN}.txt')
        done = result.read(f'srvros-tests/{RUN}.done')
        metadata = {'run': RUN, 'release': release, 'volume': volume, 'suite': test.SUITE}
        fields = test.validate_result(metadata, data, done)
        assert fields['status'] == 'PASS'
        assert result.read(f'srvros-tests/{RUN}.probe') == test.PROBE_BYTES
        assert result.read(f'srvros-tests/{RUN}.req') == f'STARTED {RUN}\n'.encode()
        (logs / 'autotest-cycle-report.txt').write_bytes(data)
        (logs / 'autotest-cycle-completion.txt').write_bytes(done)
        base = f'srvros-tests/{RUN}.files'
        def listing(relative):
            path = base + ('/' + relative if relative else '')
            return {name: 'directory' if entry[4] & 0x10 else 'file'
                    for name, entry in result.entries(result.read(path))}
        verified = verify_tree(lambda name, size: result.read(base + '/' + name), listing)
        assert len(verified) == 6
        print('hardware-test-smoke: persisted file-operation tree PASS', flush=True)
        shell_base = f'srvros-tests/{RUN}.shell'
        hardware_shell_workload.verify(lambda name: result.read(shell_base+'/'+name))
        print('hardware-test-smoke: persisted shell/editor/history/settings PASS',flush=True)
        for member in members[1:]:
            child = dict(metadata, run=member)
            payload = result.read(f'srvros-tests/{member}.txt')
            completion = result.read(f'srvros-tests/{member}.done')
            assert test.validate_result(child, payload, completion)['status'] == 'PASS'
            assert result.read(f'srvros-tests/{member}.probe') == test.PROBE_BYTES
            assert result.read(f'srvros-tests/{member}.req') == f'STARTED {member}\n'.encode()
            base = f'srvros-tests/{member}.files'
            assert len(verify_tree(lambda name, size: result.read(base+'/'+name), listing)) == 6
            shell_base = f'srvros-tests/{member}.shell'
            assert len(hardware_shell_workload.verify(lambda name: result.read(shell_base+'/'+name))) == 9
            (logs / f'autotest-round-{member}-report.txt').write_bytes(payload)
        print(f'hardware-test-smoke: all {len(members)} rounds independently verified', flush=True)
        with volume_image.open('rb') as stream:
            recovered = recovery.inspect(stream, metadata)
        assert recovered['recovered_payload_checksum_valid']
        assert recovered['recovered_report'] == fields
        for entry in recovered['entries'].values():
            assert entry['flags'] & 1
            assert entry['name_hash'] == entry['expected_name_hash']
            assert entry['set_checksum'] == entry['expected_set_checksum']
        # Reproduce the original defects in a COPY, requiring the independent
        # checker to reject each. Recovery must still extract/check payload CRC.
        defects = ('name-hash', 'stale-checksum')
        raw = bytes(result.data)
        for defect in defects:
            broken = bytearray(raw)
            item = recovered['entries'][f'srvros-tests/{RUN}.txt']
            off = item['offset']
            length = len(bytes.fromhex(item['raw_hex']))
            if defect == 'name-hash': struct.pack_into('<H', broken, off+36, 0)
            else: broken[off+2] ^= 1
            if defect != 'stale-checksum':
                struct.pack_into('<H', broken, off+2, recovery.checksum(broken[off:off+length], (2,3)))
            volume_image.write_bytes(broken)
            check = subprocess.run(['/usr/sbin/fsck.exfat', '-n', str(volume_image)], capture_output=True, text=True)
            (logs / f'autotest-invalid-{defect}-fsck.log').write_text(check.stdout + check.stderr)
            if check.returncode == 0: raise RuntimeError(f'Independent checker accepted {defect}')
            with volume_image.open('rb') as stream:
                recovered_bad = recovery.inspect(stream, metadata)
            assert recovered_bad['recovered_payload_checksum_valid']
            assert recovered_bad['filesystem_validation'] == 'NOT_PASSED'
            proposed = repair.plan(recovered_bad, metadata)
            with volume_image.open('r+b') as stream:
                repair.apply(stream, proposed)
            # The limited repair restores EXACT original bytes, not just a
            # filesystem accepted by our own parser.
            assert volume_image.read_bytes() == raw
            print(check_clean_image(volume_image), end='', flush=True)
        print('hardware-test-smoke: independent exFAT checks and read-only recovery PASS', flush=True)
        before = hashlib.sha256(disk.read_bytes()).hexdigest()
        boot_guest('replay', replay=True)
        if hashlib.sha256(disk.read_bytes()).hexdigest() != before:
            raise RuntimeError('Consumed request replay changed the disk')
        with disk.open('wb') as out:
            out.write(mbr); out.seek(2048*512); out.write(fixture.data)
        before = hashlib.sha256(disk.read_bytes()).hexdigest()
        boot_guest('write-failure', readonly=True)
        if hashlib.sha256(disk.read_bytes()).hexdigest() != before:
            raise RuntimeError('Read-only failure changed the disk')
        print(data.decode(), end='')
        print('hardware-test-smoke: PASS (one-shot GRUB, persisted report CRC, successful default return, failures remain halted)')


if __name__ == '__main__':
    main()
