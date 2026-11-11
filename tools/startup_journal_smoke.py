#!/usr/bin/env python3
"""Boot the exact startup candidate and verify its independent AHCI journal."""
import argparse
import errno
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

import hardware_boot as boot
from hardware_boot_smoke import make_disk, wait_for
from uefi_usb_smoke import default_ovmf_path, has_fatal_exception
import startup_journal as journal


def verify_sparse_changes(path, seeds, allowed):
    """Compare every allocated byte, including writes outside the journal."""
    checked = 0
    with path.open('rb', buffering=0) as stream:
        position = 0
        while True:
            try:
                first = os.lseek(stream.fileno(), position, os.SEEK_DATA)
            except OSError as error:
                if error.errno == errno.ENXIO:
                    break
                raise
            end = os.lseek(stream.fileno(), first, os.SEEK_HOLE)
            assert end-first < 2*1024*1024, 'unexpected large disk allocation'
            actual = journal.read_at(stream, first, end-first)
            expected = bytearray(len(actual))
            for offset, data in seeds:
                left, right = max(first, offset), min(end, offset+len(data))
                if left < right:
                    expected[left-first:right-first] = data[left-offset:right-offset]
            left, right = max(first, allowed[0]), min(end, allowed[1])
            if left < right:
                expected[left-first:right-first] = actual[left-first:right-first]
            assert actual == expected, 'AHCI modified bytes outside the preallocated journal'
            checked += len(actual)
            position = end
    return checked


def run(args):
    source = boot.STAGE / args.release
    manifest = boot.verify(source)
    assert manifest['config_version'].startswith('ram-usb-wifi-fw-v141-')
    descriptor = journal.verify_initramfs(source/'initramfs.tar')
    host = json.loads((args.host_record/'journal-host.json').read_text())
    assert descriptor == (args.host_record/'descriptor.bin').read_bytes()
    sectors, offset, start, size = struct.unpack_from('<QQQQ', descriptor, 16)
    assert sectors == host['disk_sectors'] and offset == host['journal_offset']
    result_dir = args.output / args.mode
    result_dir.mkdir(parents=True, exist_ok=False)
    serial = result_dir/'serial.log'
    with tempfile.TemporaryDirectory(prefix='startup-vm-', dir=args.output) as directory:
        temp = Path(directory)
        cfg = temp/'grub.cfg'
        cfg.write_text('serial --unit=0 --speed=115200\nterminal_output console serial\nset default=srvros-current\n'
                       + boot.grub_config('5352-5652', args.release))
        subprocess.run(['grub-script-check', str(cfg)], check=True)
        loader = temp/'BOOTX64.EFI'
        subprocess.run(['grub-mkstandalone', '-O', 'x86_64-efi', '--locales=', '--fonts=',
                        '--modules=part_gpt fat chain search search_fs_uuid normal serial terminal',
                        '-o', str(loader), f'boot/grub/grub.cfg={cfg}'], check=True)
        esp = temp/'boot.img'
        make_disk(esp, loader, source, args.release)
        internal = temp/'internal.img'
        seeds = [(512, (args.host_record/'disk-gpt-sector.bin').read_bytes()),
                 (start, (args.host_record/'esp-bpb-sector.bin').read_bytes()),
                 (offset, journal.initial(descriptor[48:64]))]
        with internal.open('wb') as stream:
            stream.truncate(sectors*512)
            for at, data in seeds:
                stream.seek(at)
                stream.write(data)
        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '2G',
                   '-drive', f'if=pflash,format=raw,readonly=on,file={default_ovmf_path()}',
                   '-drive', f'format=raw,file={esp},index=0',
                   '-drive', f'format=raw,file={internal},index=1',
                   '-device', 'qemu-xhci,id=xhci', '-device', 'usb-kbd,bus=xhci.0',
                   '-serial', f'file:{serial}', '-monitor', 'none', '-display', 'none', '-no-reboot']
        if args.mode != 'missing':
            from exfat_test_image import ExfatFixture, check_clean_image
            volume = temp/'workspace.exfat'
            with volume.open('wb') as stream:
                stream.truncate(128*1024*1024)
            subprocess.run(['/usr/sbin/mkfs.exfat', '-c', '128K', str(volume)], check=True, stdout=subprocess.DEVNULL)
            subprocess.run(['/usr/sbin/tune.exfat', '-I', '0x0e535900', str(volume)], check=True, stdout=subprocess.DEVNULL)
            fixture = ExfatFixture(volume.read_bytes())
            files = {'.workspace': b'SRVROS-WORKSPACE-V1 0e535900\n' if args.mode != 'bad-marker' else b'bad marker\n'}
            if args.mode == 'full':
                from hardware_log_archive import limits
                files.update({f'fill{i:04d}': b'' for i in range(limits()[0]-3)})
                fixture.add_directory_files('.srvros', {'meta': b'SRVROSMETA1\n'})
            fixture.add_directory_files('srvros-work', files)
            volume.write_bytes(fixture.data)
            check_clean_image(volume)
            stick = temp/'workspace.img'
            mbr = bytearray(512)
            mbr[450] = 7
            struct.pack_into('<II', mbr, 454, 2048, len(fixture.data)//512)
            mbr[510:] = b'\x55\xaa'
            with stick.open('wb') as stream:
                stream.write(mbr)
                stream.seek(2048*512)
                stream.write(fixture.data)
            command += ['-drive', f'if=none,id=stick,format=raw,file={stick}',
                        '-device', 'usb-storage,drive=stick,bus=xhci.0']
        with (result_dir/'qemu.log').open('wb') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                text = wait_for(serial, lambda s: 'Startup diagnostic complete.' in s, process, 100)
                assert process.wait(timeout=30) == 0
                assert not has_fatal_exception(text)
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait()
        with internal.open('rb') as stream:
            raw = journal.read_at(stream, offset, journal.FILE_BYTES)
        (result_dir/'journal.bin').write_bytes(raw)
        parsed = journal.decode(raw, descriptor)
        (result_dir/'journal.json').write_text(json.dumps(parsed, indent=2)+'\n')
        assert parsed['complete'], parsed
        assert [x['phase'] for x in parsed['records']] == list(range(1,8))
        assert not any(x in text for x in ('wifi-firmware-progress:', 'wifi-cal5-command:', 'wifi-reconnect5-begin:'))
        if args.mode == 'present':
            assert parsed['workspace_ready'] and parsed['logger_started'] and parsed['saved'] and parsed['unmounted']
        elif args.mode == 'full':
            assert parsed['workspace_ready'] and not parsed['logger_started'] and not parsed['saved'] and parsed['unmounted']
            assert 'REFUSED insufficient file/directory slots' in text
        else:
            assert not any(parsed[x] for x in ('workspace_ready','logger_started','saved','unmounted'))
        checked = verify_sparse_changes(internal, seeds, (offset+512, offset+journal.FILE_BYTES))
        if args.mode != 'missing':
            with stick.open('rb') as stream:
                saved = journal.read_at(stream, 1048576, len(fixture.data))
            clean = result_dir/'workspace-after.exfat'
            clean.write_bytes(saved)
            check_clean_image(clean)
            # Existing guest checker validates allocation as well as fsck.
        report = dict(status='PASS', source=manifest['git_head'], release=args.release, mode=args.mode,
                      exact_stage=True, exact_descriptor=True, independent_ahci=True,
                      native_accepted=False, phases=7, allocated_bytes_compared=checked,
                      changed_bytes_confined_to_journal=True, workspace_ready=parsed['workspace_ready'],
                      logger_started=parsed['logger_started'])
        (result_dir/'result.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(report), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', required=True)
    parser.add_argument('--host-record', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=('missing','present','bad-marker','full'), required=True)
    run(parser.parse_args())
