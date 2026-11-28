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
    refusal = args.mode in ('journal-refused', 'both-refused', 'dirty-state')
    missing = args.mode in ('missing', 'both-refused')
    refusal_reason = 8 if args.mode == 'dirty-state' else 6
    source = boot.STAGE / args.release
    manifest = boot.verify(source)
    assert manifest['config_version'].startswith(('ram-usb-rtl8822bu-v1-','ram-usb-rtl8822bu-v2-','ram-usb-rtl8822bu-v3-','ram-usb-rtl8822bu-v4-','ram-usb-rtl8822bu-v5-','ram-usb-rtl8822bu-v6-','ram-usb-rtl8188cu-v1-','ram-usb-rtl8188cu-v2-','ram-usb-rtl8188cu-v3-','ram-usb-rtl8188cu-v4-','ram-usb-rtl8188cu-v5-','ram-usb-rtl8188cu-v6-','ram-usb-wifi-fw-v141-','ram-usb-wifi-fw-v142-','ram-usb-wifi-fw-v143-','ram-usb-wifi-fw-v147-','ram-usb-wifi-fw-v148-','ram-usb-wifi-fw-v149-','ram-usb-wifi-fw-v144-','ram-usb-wifi-fw-v145-','ram-usb-wifi-fw-v146-','ram-usb-wifi-fw-v151-','ram-usb-wifi-fw-v152-','ram-usb-wifi-fw-v153-','ram-usb-wifi-fw-v154-','ram-usb-wifi-fw-v155-'))
    rtl8822bu = manifest['config_version'].startswith("ram-usb-rtl8822bu-")
    rtl8188cu = manifest['config_version'].startswith(("ram-usb-rtl8188cu-v1-","ram-usb-rtl8188cu-v2-","ram-usb-rtl8188cu-v3-","ram-usb-rtl8188cu-v4-","ram-usb-rtl8188cu-v5-","ram-usb-rtl8188cu-v6-"))
    desktop_owner = manifest['config_version'].startswith(('ram-usb-wifi-fw-v151-','ram-usb-wifi-fw-v152-','ram-usb-wifi-fw-v153-','ram-usb-wifi-fw-v154-','ram-usb-wifi-fw-v155-'))
    early_compare = manifest['config_version'].startswith(('ram-usb-wifi-fw-v144-','ram-usb-wifi-fw-v145-','ram-usb-wifi-fw-v146-'))
    http_reconnect = manifest['config_version'].startswith(('ram-usb-wifi-fw-v143-','ram-usb-wifi-fw-v147-','ram-usb-wifi-fw-v148-','ram-usb-wifi-fw-v149-'))
    full_reconnect = manifest['config_version'].startswith(('ram-usb-wifi-fw-v142-','ram-usb-wifi-fw-v143-','ram-usb-wifi-fw-v147-','ram-usb-wifi-fw-v148-','ram-usb-wifi-fw-v149-'))
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
        import hashlib
        clean_sector = seeds[1][1]
        mounted_sector = (args.host_record/'esp-bpb-mounted.bin').read_bytes()
        assert clean_sector == journal.clean_boot_sector(mounted_sector)
        assert clean_sector[65]&1 == 0 and hashlib.sha256(clean_sector).digest() == descriptor[96:]
        if args.mode == 'dirty-state':
            changed = bytearray(clean_sector);changed[65] |= 1
            seeds[1] = (start, bytes(changed))
        elif refusal:
            changed = bytearray(seeds[0][1]);changed[16] ^= 1
            seeds[0] = (512, bytes(changed))
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
        if not missing:
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
                marker = ('neither persistent destination admitted' if args.mode == 'both-refused' else
                          'laptop-capture: returning to Ubuntu' if full_reconnect or early_compare or desktop_owner or rtl8188cu or rtl8822bu else 'Startup diagnostic complete.')
                text = wait_for(serial, lambda s: marker in s, process, 100)
                if args.mode == 'both-refused':
                    assert process.poll() is None
                else:
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
        if refusal:
            assert not parsed['complete'] and not parsed['records']
            assert raw == journal.initial(descriptor[48:64])
            assert f'startup-journal-refused: version=1 reason={refusal_reason}' in text
        else:
            assert parsed['complete'], parsed
            assert [x['phase'] for x in parsed['records']] == list(range(1,8))
        assert not any(x in text for x in ('wifi-firmware-progress:', 'wifi-cal5-command:', 'wifi-reconnect5-begin:'))
        if args.mode == 'present':
            assert parsed['workspace_ready'] and parsed['logger_started'] and parsed['saved'] and parsed['unmounted']
        elif args.mode == 'full':
            assert parsed['workspace_ready'] and not parsed['logger_started'] and not parsed['saved'] and parsed['unmounted']
            assert 'REFUSED insufficient file/directory slots' in text
        elif not refusal:
            assert not any(parsed[x] for x in ('workspace_ready','logger_started','saved','unmounted'))
        if (full_reconnect or early_compare) and args.mode != 'both-refused':
            from hardware_boot_smoke import verify_reconnect5_absent_radio,verify_http_reconnect5_absent_radio,verify_early5_absent_radio
            if early_compare:verify_early5_absent_radio(text,require_logger_parked=args.mode in ('present','journal-refused','dirty-state'))
            elif http_reconnect:verify_http_reconnect5_absent_radio(text,require_prepare=args.mode in ('present','journal-refused','dirty-state'))
            else:verify_reconnect5_absent_radio(text)
            assert 'wifi-journal-summary: version=1 sampled=0' in text
        if rtl8822bu:
            receipt = "rtl8822bu-probe: result=absent register_writes=0 firmware=0 rf=0"
            assert text.count(receipt) == 1 and "rtl8822bu-identity:" not in text
            if not refusal:
                assert receipt in parsed['records'][1]['tail']
            station = manifest['config_version'].startswith("ram-usb-rtl8822bu-v6-")
            skipped = station and refusal
            if not skipped and manifest['config_version'].startswith(("ram-usb-rtl8822bu-v2-", "ram-usb-rtl8822bu-v3-", "ram-usb-rtl8822bu-v4-", "ram-usb-rtl8822bu-v5-", "ram-usb-rtl8822bu-v6-")):
                firmware_receipt = "rtl8822bu-firmware: result=identity bytes=0 packets=0"
                assert firmware_receipt in text
                if not refusal:
                    assert firmware_receipt in parsed['records'][1]['tail']
                assert "rtl8822bu-firmware-progress:" not in text
            if not skipped and manifest['config_version'].startswith(("ram-usb-rtl8822bu-v3-", "ram-usb-rtl8822bu-v4-", "ram-usb-rtl8822bu-v5-", "ram-usb-rtl8822bu-v6-")):
                board_receipt = "rtl8822bu-board: result=not-run"
                assert board_receipt in text and "rtl8822bu-board-entry:" not in text
                if not refusal:
                    assert board_receipt in parsed['records'][1]['tail']
            if not skipped and manifest['config_version'].startswith(("ram-usb-rtl8822bu-v4-", "ram-usb-rtl8822bu-v5-", "ram-usb-rtl8822bu-v6-")):
                phy_receipt = "rtl8822bu-phy: result=not-run writes=0"
                assert phy_receipt in text
                if not refusal:
                    assert phy_receipt in parsed['records'][1]['tail']
            if not skipped and manifest['config_version'].startswith(("ram-usb-rtl8822bu-v5-", "ram-usb-rtl8822bu-v6-")):
                rx_receipt = "rtl8822bu-passive: result=not-run"
                assert rx_receipt in text
                if not refusal:
                    assert rx_receipt in parsed['records'][1]['tail']
            if station:
                assert "srvros.capture=wifi-rtl8822bu-wpa2" in text
                assert "rtl8822bu-station-tx:" not in text and "rtl8822bu-wpa2: result=credential" not in text
                assert ("rtl8822bu-station-skipped: reason=journal-unavailable" in text) == skipped
                if skipped:
                    assert "rtl8822bu-firmware: result=" not in text
        if rtl8188cu:
            receipt = 'rtl8188cu-probe: result=absent register_writes=0 firmware=0 rf=0'
            assert text.count(receipt) == 1 and 'rtl8188cu-identity:' not in text
            if manifest["config_version"].startswith(("ram-usb-rtl8188cu-v2-","ram-usb-rtl8188cu-v3-","ram-usb-rtl8188cu-v4-","ram-usb-rtl8188cu-v5-")):
                assert "rtl8188cu-firmware: result=identity bytes=0" in text
                assert "rtl8188cu-firmware-progress:" not in text
            if manifest["config_version"].startswith(("ram-usb-rtl8188cu-v4-","ram-usb-rtl8188cu-v5-")):
                assert "rtl8188cu-wpa2: phase=admission" in text
                assert "auth_tx=0 auth=0 assoc_tx=0 assoc=0" in text
            if manifest["config_version"].startswith("ram-usb-rtl8188cu-v5-"):
                assert "rtl8188cu-ip: opened=0 tx=0 completed=0" in text
            if manifest["config_version"].startswith(("ram-usb-rtl8188cu-v3-","ram-usb-rtl8188cu-v4-","ram-usb-rtl8188cu-v5-")):
                assert "rtl8188cu-passive: result=admission writes=0" in text
                assert "rtl8188cu-passive-progress:" not in text
            if manifest["config_version"].startswith("ram-usb-rtl8188cu-v6-") and args.mode != 'both-refused':
                assert "rtl8188cu-desktop-final: connections=0 disconnects=0 return_safe=1 complete=0" in text
                assert "rtl8188cu-firmware-progress:" not in text
            if not refusal:
                assert receipt in parsed['records'][1]['tail']
            if args.mode != 'both-refused':
                assert 'wifi-journal-summary: version=1 sampled=0' in text
        if desktop_owner and args.mode != 'both-refused':
            assert 'wifi-journal-summary: version=1 sampled=0' in text
            assert 'wifi-desktop5-capture: complete=0 launched=0' in text
            assert 'wifi-http-reconnect5-prepare:' not in text and 'deskd:' not in text
        checked = verify_sparse_changes(internal, seeds, (offset+512, offset+journal.FILE_BYTES))
        if not missing:
            with stick.open('rb') as stream:
                saved = journal.read_at(stream, 1048576, len(fixture.data))
            clean = result_dir/'workspace-after.exfat'
            clean.write_bytes(saved)
            check_clean_image(clean)
            if args.mode in ('journal-refused', 'dirty-state'):
                final = ExfatFixture(saved)
                entries = dict(final.entries(final.read('srvros-work/Logs')))
                assert len(entries) == 1
                session = next(iter(entries))
                log = final.read('srvros-work/Logs/'+session+'/exit.log')
                (result_dir/'fallback-exit.log').write_bytes(log)
                assert f'startup-journal-refused: version=1 reason={refusal_reason}'.encode() in log
                assert f'startup-journal-hash: offset={start if args.mode == "dirty-state" else 512}'.encode() in log
                assert b'srvros.desktop-release='+args.release.encode() in log
                assert b'workspace-log: snapshot=exit.log ' in log
                if manifest['config_version'].startswith("ram-usb-rtl8822bu-v6-"):
                    assert b"rtl8822bu-station-skipped: reason=journal-unavailable" in log
        report = dict(status='PASS', source=manifest['git_head'], release=args.release, mode=args.mode,
                      exact_stage=True, exact_descriptor=True, independent_ahci=True, full_reconnect_profile=full_reconnect, early_compare_profile=early_compare,
                      native_accepted=False, phases=len(parsed['records']), refusal_exercised=refusal, fallback_saved=args.mode in ('journal-refused','dirty-state'), clean_boot_sector_pinned=True, expected_hold=args.mode=='both-refused', allocated_bytes_compared=checked,
                      changed_bytes_confined_to_journal=True, workspace_ready=parsed['workspace_ready'],
                      logger_started=parsed['logger_started'])
        (result_dir/'result.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps(report), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', required=True)
    parser.add_argument('--host-record', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--mode', choices=('missing','present','bad-marker','full','journal-refused','both-refused','dirty-state'), required=True)
    run(parser.parse_args())
