#!/usr/bin/env python3
"""Prepare, launch and collect one USB-backed laptop test. No daemon is installed."""
import argparse
import binascii
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import secrets
import shlex
import subprocess

import hardware_boot as boot
import hardware_workload

RUNS = boot.ROOT / 'build/hardware-tests'
UUID_RE = re.compile(r'[0-9a-fA-F]{4}-?[0-9a-fA-F]{4}')
RUN_RE = re.compile(r'[0-9a-f]{16}')
PROBE_BYTES = b'SrvrOS USB log probe: AaZz 0123456789\n'
SUITE = 'srvros-autotest-v2'


def volume_id(value):
    if not UUID_RE.fullmatch(value):
        raise ValueError('Expected an exFAT volume UUID, such as 37D8-C5D0')
    return value.replace('-', '').lower()


def volume_mount(volume, inventory=None):
    """Resolve by UUID and USB ancestry, never by a /dev/sdX name."""
    if inventory is None:
        inventory = json.loads(subprocess.check_output([
            'lsblk', '--json', '--paths', '--output', 'NAME,TYPE,TRAN,FSTYPE,UUID,MOUNTPOINTS'], text=True))
    matches = []

    def visit(node, transport=None):
        transport = node.get('tran') or transport
        uuid = (node.get('uuid') or '').replace('-', '').lower()
        if uuid == volume:
            matches.append((node, transport))
        for child in node.get('children', []):
            visit(child, transport)
    for node in inventory['blockdevices']:
        visit(node)
    if len(matches) != 1:
        raise RuntimeError('USB volume is absent or its UUID is ambiguous')
    node, transport = matches[0]
    if transport != 'usb' or node.get('fstype') != 'exfat':
        raise RuntimeError('Refusing a volume that is not USB exFAT')
    mounts = [Path(p) for p in node.get('mountpoints', []) if p]
    if len(mounts) != 1 or not mounts[0].is_absolute():
        raise RuntimeError('Mount the flash drive in Ubuntu first')
    return mounts[0]


def sync_file(path, content, exclusive=False):
    with path.open('xb' if exclusive else 'wb') as out:
        out.write(content)
        out.flush()
        os.fsync(out.fileno())


def prepare(uuid):
    volume = volume_id(uuid)
    mount = volume_mount(volume)
    folder = mount / 'srvros-tests'
    if folder.is_symlink():
        raise RuntimeError('Refusing a symlink at the USB result directory')
    folder.mkdir(exist_ok=True)
    run = secrets.token_hex(8)
    release = boot.stage(autotest_volume=volume, run_id=run)
    manifest = boot.verify(boot.STAGE / release)
    directory = RUNS / run
    directory.mkdir(parents=True)
    metadata = {'format': 'srvros-host-test-v1', 'run': run, 'volume': volume,
                'suite': SUITE,
                'release': release, 'created_utc': datetime.now(timezone.utc).isoformat(),
                'boot_manifest': manifest}
    sync_file(directory / 'manifest.json', (json.dumps(metadata, indent=2) + '\n').encode(), exclusive=True)
    sync_file(folder / f'{run}.req', f'SRVROS-AUTOTEST-V2 {run}\n'.encode(), exclusive=True)
    # Commit the directory entry as well as the request contents before reboot.
    os.sync()
    RUNS.mkdir(parents=True, exist_ok=True)
    boot.atomic_write(RUNS / 'latest', (run + '\n').encode(), 0o644)
    print(f'Prepared run {run}; release {release}; USB UUID {volume}')
    print(f'Launch when ready: python3 tools/hardware_test.py run --run {run}')
    print(f'Results on the flash drive: srvros-tests/{run}.txt and .done')
    return run


def load_run(value):
    run = (RUNS / 'latest').read_text().strip() if value == 'latest' else value
    if not RUN_RE.fullmatch(run):
        raise ValueError('Invalid run identifier')
    metadata = json.loads((RUNS / run / 'manifest.json').read_text())
    if metadata.get('run') != run or not RUN_RE.fullmatch(metadata.get('release', '')):
        raise ValueError('Invalid run manifest')
    metadata['volume'] = volume_id(metadata['volume'])
    return metadata


def check_ubuntu_default(defaults, config):
    """Deliberately require the host's existing, simple Ubuntu default."""
    settings = {}
    for line in defaults.splitlines():
        words = shlex.split(line, comments=True)
        if len(words) == 1 and '=' in words[0]:
            key, value = words[0].split('=', 1)
            settings[key] = value
    if settings.get('GRUB_DEFAULT') != '0' or settings.get('GRUB_SAVEDEFAULT', 'false') != 'false':
        raise RuntimeError('Automatic return requires GRUB_DEFAULT=0 and no saved-default mode; settings were not changed')
    first = re.search(r"^(menuentry|submenu)\s+(['\"])(.*?)\2", config, re.M)
    if not first or first[1] != "menuentry" or not first[3].lower().startswith('ubuntu'):
        raise RuntimeError('The first GRUB entry is not Ubuntu; refusing to arm a reboot loop')
    selections = re.findall(r'^\s*set\s+default=(.+)$', config, re.M)
    if not selections or any(shlex.split(value) not in (["0"], ["${next_entry}"]) for value in selections):
        raise RuntimeError('Generated GRUB configuration has an unexpected default selection')


def launch(run):
    metadata = load_run(run)
    release = metadata['release']
    manifest = boot.verify(boot.STAGE / release)
    expanded = metadata.get('suite') == SUITE
    expected = f"ram-usb-v{6 if expanded else 5}-autotest-{metadata['volume']}-{metadata['run']}"
    if manifest['config_version'] != expected or manifest['sha256'] != metadata['boot_manifest']['sha256']:
        raise RuntimeError('Staged payload does not match this test request')
    mount = volume_mount(metadata['volume'])
    request = mount / 'srvros-tests' / f"{metadata['run']}.req"
    if request.read_bytes() != f"SRVROS-AUTOTEST-V{2 if expanded else 1} {metadata['run']}\n".encode():
        raise RuntimeError('Request is missing or already consumed; prepare a new run')
    subprocess.run(['sudo', '-v'], check=True)
    defaults = Path('/etc/default/grub').read_text()
    config = subprocess.check_output(['sudo', 'cat', '/boot/grub/grub.cfg'], text=True)
    check_ubuntu_default(defaults, config)
    subprocess.run(['sudo', 'python3', str(boot.ROOT / 'tools/hardware_boot.py'),
                    'install', '--release', release], check=True)
    # Recheck generated configuration before changing GRUB's one-shot state.
    config = subprocess.check_output(['sudo', 'cat', '/boot/grub/grub.cfg'], text=True)
    check_ubuntu_default(defaults, config)
    try:
        subprocess.run(['sudo', 'grub-reboot', 'srvros-current'], check=True)
        os.sync()
        print('One-shot test armed. Rebooting; SrvrOS will write its report and request a normal reboot.', flush=True)
        subprocess.run(['sudo', 'systemctl', 'reboot', '-i'], check=True)
    except BaseException:
        subprocess.run(['sudo', 'grub-editenv', '/boot/grub/grubenv', 'unset', 'next_entry'], check=False)
        raise


def validate_result(metadata, data, completion):
    if len(data) > 2048 or len(completion) > 128:
        raise ValueError('Oversized result')
    run = metadata['run']
    crc = binascii.crc32(data) & 0xffffffff
    if completion != f'COMPLETE {run} {crc:08x}\n'.encode():
        raise ValueError('Missing/invalid completion marker or report checksum')
    fields = {}
    for line in data.decode('ascii').splitlines():
        key, value = line.split('=', 1)
        if key in fields:
            raise ValueError('Duplicate result field')
        fields[key] = value
    suite = metadata.get('suite', 'srvros-autotest-v1')
    if suite not in ('srvros-autotest-v1', SUITE): raise ValueError('Unknown test suite')
    for key, expected in {'format': suite, 'run': run,
                          'release': metadata['release'], 'volume': metadata['volume']}.items():
        if fields.get(key) != expected:
            raise ValueError(f'Result identity mismatch: {key}')
    checks = ('boot_ready', 'timer', 'memory', 'fpu_traps', 'render',
              'usb_keyboard_enumeration', 'usb_write_flush_read')
    if suite == SUITE: checks += ('usb_file_operations',)
    if any(fields.get(key) not in ('PASS', 'FAIL') for key in checks):
        raise ValueError('Missing or invalid test status')
    status = 'PASS' if all(fields[key] == 'PASS' for key in checks) else 'FAIL'
    if fields.get('status') != status or fields.get('physical_keypress') != 'NOT_TESTED':
        raise ValueError('Inconsistent test summary')
    return fields


def collect(run):
    metadata = load_run(run)
    folder = volume_mount(metadata['volume']) / 'srvros-tests'
    prefix = metadata['run']
    report_path, done_path = folder / f'{prefix}.txt', folder / f'{prefix}.done'
    if not report_path.is_file() or not done_path.is_file():
        listed = {entry.name for entry in folder.iterdir()}
        inaccessible = [p.name for p in (report_path, done_path)
                        if p.name in listed and not p.is_file()]
        if inaccessible:
            raise RuntimeError('Result names are listed but Ubuntu cannot open their directory entries: '
                               + ', '.join(inaccessible)
                               + '; possible invalid exFAT metadata, not a missing test run')
        request = folder / f'{prefix}.req'
        state = request.read_text()[:80].strip() if request.exists() else 'request missing'
        raise RuntimeError(f'No complete report for {prefix} ({state}); this is not a pass')
    if report_path.stat().st_size > 2048 or done_path.stat().st_size > 128:
        raise ValueError('Oversized result')
    data, completion = report_path.read_bytes(), done_path.read_bytes()
    fields = validate_result(metadata, data, completion)
    if fields['usb_write_flush_read'] == 'PASS':
        probe = folder / f'{prefix}.probe'
        if probe.stat().st_size != len(PROBE_BYTES) or probe.read_bytes() != PROBE_BYTES:
            raise ValueError('USB probe did not survive the reboot intact')
    workload = None
    if fields.get('usb_file_operations') == 'PASS':
        workload = hardware_workload.verify_mounted(folder / f'{prefix}.files')
    directory = RUNS / prefix
    sync_file(directory / 'report.txt', data)
    sync_file(directory / 'completion.txt', completion)
    sync_file(directory / 'result.json', (json.dumps(fields, indent=2) + '\n').encode())
    if workload is not None:
        sync_file(directory / 'storage-files.json', (json.dumps(workload, indent=2) + '\n').encode())
    print(data.decode('ascii'), end='')
    print(f'Validated report copied to {directory}')
    return fields['status'] == 'PASS'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('prepare', 'run', 'collect'))
    parser.add_argument('--uuid', default='37D8-C5D0', help='Target USB exFAT UUID for prepare')
    parser.add_argument('--run', default='latest', help='Exact prepared run ID (default: latest)')
    args = parser.parse_args()
    try:
        if args.action == 'prepare': prepare(args.uuid)
        elif args.action == 'run': launch(args.run)
        elif not collect(args.run): parser.exit(2, 'Hardware test reported FAIL\n')
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'hardware-test: {error}\n')


if __name__ == '__main__':
    main()
