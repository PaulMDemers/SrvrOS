#!/usr/bin/env python3
"""Prove synthetic v2 offline installation and two boots on disposable QEMU disks.

Requires existing source-matched artifacts. Does not build or access an original
installation. Evidence and owned disks are retained in a new proof directory.
"""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path

import installation_image as image
import profile_bundle as bundle
import profile_chooser_smoke as chooser
import profile_create_smoke as create
import profile_desktop_smoke as desktop
import profile_installation_smoke as install
import profile_stream as stream
import profile_stream_fixture
import profile_transfer_smoke as transfer
import source_provenance
from installation_export_recovery_smoke import readback, stamp
from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path

ROOT = Path(__file__).resolve().parents[1]
HOME = '/fat/home/recovered_b'
CHUNK = stream.BUFFER
GIB = 1024 ** 3


def require(value, message):
    if not value:
        raise RuntimeError(message)


def make_stream(path):
    """Write frozen v2 wire format using at most one 64 KiB payload block."""
    portable = desktop.make_bundle()
    docs = [('Documents/migration.txt', create.TEXT),
            ('Documents/binary.profile-import.dat', create.BINARY),
            ('Documents/large.bin', bytes(range(256)) * (stream.MAX_FILE // 256))]
    manifest = profile_stream_fixture.write_stream(
        path, portable['profile'], portable['preferences'], docs)
    require(path.stat().st_size <= stream.MAX_BUNDLE, 'bundle too large')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence-parent', type=Path, required=True)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    require(10 <= args.boot_wait <= 240, 'invalid boot wait')
    for path in (args.ovmf, args.ovmf_vars):
        require(Path(path).is_file() and ',' not in str(path), 'invalid firmware path')
    parent = image.checked_path(args.evidence_parent)
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=parent)).resolve()
    receipt = {'status': 'INCOMPLETE', 'scope': 'synthetic v2 offline restore, two owned EFI QEMU boots',
               'limits': ['no physical media or power-loss claim', '16 MiB selected document; not a 64 MiB aggregate test',
                          'offline ExfatFixture allocation audit loads the data partition into evidence-tool RAM'],
               'steps': [], 'boots': []}
    def save(): desktop.save_json(out / 'receipt.json', receipt)
    print('Installation stream restore evidence: ' + str(out), flush=True)
    save()
    try:
        receipt['source'] = source_provenance.capture(ROOT)
        require(not any(v.get('dirty') for v in receipt['source'].values()), 'source must be clean')
        receipt['build'] = desktop.verified_build(ROOT)
        require(shutil.disk_usage(out).free >= 8 * GIB, 'need 8 GiB free for proof')
        inputs = {name: ROOT / path for name, path in {
            'kernel': 'build/kernel/srvros.elf', 'bootx64': 'build/limine-binary/BOOTX64.EFI',
            'init': 'build/userspace/init.elf', 'exfat': 'build/srvros.exfat',
            'driver': 'build/tests/profile-image-driver', 'fs_driver': 'build/tests/exfat-driver',
            'inspect_driver': 'build/tests/installation-inspect-driver'}.items()}
        originals = {name: stamp(path) for name, path in inputs.items()}
        receipt['inputs'] = originals
        firmware = {str(path): image.digest(path) for path in map(Path, (args.ovmf, args.ovmf_vars))}
        receipt['firmware'] = firmware
        receipt['qemu_version'] = subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]
        tool_names = ('installation_stream_restore_smoke.py', 'installation_image.py', 'profile_stream.py',
                      'profile_image.py', 'profile_image_driver.c', 'installation_inspection.py',
                      'profile_bundle.py', 'profile_chooser_smoke.py', 'profile_create_smoke.py',
                      'profile_transfer_smoke.py', 'installation_export_recovery_smoke.py', 'exfat_test_image.py')
        receipt['tools'] = {name: image.digest(ROOT / 'tools' / name) for name in tool_names}
        def guard():
            require({name: stamp(path) for name, path in inputs.items()} == originals, 'matched input changed')
            require(source_provenance.capture(ROOT) == receipt['source'], 'source checkout changed')
            require({str(path): image.digest(path) for path in map(Path, (args.ovmf, args.ovmf_vars))} == firmware, 'firmware changed')
            require(shutil.disk_usage(out).free >= 4 * GIB, 'evidence reserve exhausted')
        def call(argv):
            guard()
            proc = subprocess.run(argv, capture_output=True, text=True)
            receipt['steps'].append({'argv': argv, 'exit': proc.returncode, 'stdout': proc.stdout, 'stderr': proc.stderr})
            save(); guard()
            require(proc.returncode == 0, 'command failed: ' + str(argv))
            return json.loads(proc.stdout)
        source = out / 'synthetic.srvrprofile'
        manifest = make_stream(source)
        source_stamp = stamp(source)
        receipt['synthetic_source'] = {'file': str(source), 'stamp': source_stamp, 'manifest': manifest,
                                       'writer_chunk_bytes': CHUNK}
        identity = manifest['profile']
        machine = desktop.machine_preferences(2, 60)
        volume = create.make_volume(inputs['exfat'], out, 'b-base', machine)
        volume_stamp = stamp(volume)
        stage = out / 'stage-b'
        command = [sys.executable, str(ROOT / 'tools/installation_image.py')]
        plan = call(command + ['prepare', str(stage), '--kernel', str(inputs['kernel']), '--bootx64', str(inputs['bootx64']),
            '--exfat', str(volume), '--init', str(inputs['init']), '--identity-bound', '--profile-bundle', str(source),
            '--profile-expected-id', identity['id'], '--profile-home', HOME, '--profile-alias', 'recovered_b',
            '--profile-uid', '3050', '--profile-gid', '3051', '--profile-driver', str(inputs['driver'])])
        require(plan['version'] == 2, 'unexpected plan version')
        ready = call(command + ['apply', str(stage)])
        expected = {'profile_id': identity['id'], 'home': HOME, 'alias': 'recovered_b', 'uid': 3050, 'gid': 3051}
        require(all(ready['profile'].get(k) == v for k, v in expected.items()), 'ready binding differs')
        disk = stage / 'image.img'
        transport, transport_record = create.install(inputs['exfat'], out / 'transport')
        offset, size = desktop.partitions(disk)[1]
        protected = desktop.outside_data(disk, offset, size)
        require(len(set(ready['gpt'].values())) == len(ready['gpt']), 'GPT identifiers not distinct')
        fields = ('disk_guid', 'esp_partition_guid', 'exfat_partition_guid')
        require(not {ready['gpt'][k] for k in fields} & {transport_record['gpt'][k] for k in fields},
                'B reused transport GPT identity')
        receipt['fresh_b'] = {'plan': plan, 'ready': ready, 'base': volume_stamp, 'transport': transport_record,
                              'protected_regions': protected, 'initial_disk_sha256': image.digest(disk)}
        prefix = HOME.removeprefix('/fat/') + '/'
        preboot = transfer.check_allocation(disk, out, 'b-preboot')
        for directory in ('.cache', '.local/share'):
            require(not {name for name, _ in preboot.entries(preboot.read(prefix + directory)) if name not in ('.', '..')},
                    'fresh B inherited runtime state')
        for name in ('Documents/unselected.txt', '.config/credential-sentinel.fixture',
                     '.local/share/app-state.fixture', '.cache/runtime-cache.fixture'):
            create.absent(preboot, prefix + name)
        del preboot
        for iteration in (1, 2):
            label = 'b' + str(iteration)
            with chooser.VM(disk, transport, out, label, args) as vm:
                if iteration == 1:
                    start, index = chooser.chooser(vm, HOME)
                    chooser.select(vm, index); chooser.opened(vm, HOME, start, identity['id'])
                else:
                    create.start_restored(vm, HOME, identity)
                readback(vm)
            fs = transfer.check_allocation(disk, out, label)
            require(json.loads(fs.read(prefix + 'identity.json')) == identity, 'identity differs')
            require(json.loads(fs.read(prefix + 'profile-local.json')) == {'alias': 'recovered_b', 'uid': 3050, 'gid': 3051}, 'local binding differs')
            require(json.loads(fs.read(prefix + bundle.PREF_PATH)) == manifest['preferences'], 'portable preferences differ')
            hashes = {}
            for item in manifest['documents']:
                data = fs.read(prefix + item['path'])
                digest = hashlib.sha256(data).hexdigest()
                require(len(data) == item['size'] and digest == item['sha256'], 'document differs: ' + item['path'])
                hashes[item['path']] = {'size': len(data), 'sha256': digest}
            require({name for name, _ in fs.entries(fs.read(prefix + 'Documents')) if name not in ('.', '..')} ==
                    {'migration.txt', 'binary.profile-import.dat', 'large.bin'}, 'unexpected Documents entries')
            for name in ('Documents/unselected.txt', '.config/credential-sentinel.fixture',
                         '.local/share/app-state.fixture', '.cache/runtime-cache.fixture', '.profile-import-pending'):
                create.absent(fs, prefix + name)
            create.absent(fs, HOME.removeprefix('/fat/') + '.profile-import.json')
            create.legacy_and_binaries(fs, machine); create.check_choice(fs, HOME, identity)
            for name in ('env', 'pwd'):
                require(fs.read('bin/' + name) == runtime_elf((ROOT / f'build/userspace/{name}.elf').read_bytes()),
                        'installed context binary differs: ' + name)
            env = fs.read(prefix + '.cache/proof-env').decode().splitlines()
            expected_env = {'HOME': HOME, 'SRVROS_DESKTOP_HOME': HOME, 'SRVROS_PROFILE_ID': identity['id'],
                            'XDG_CONFIG_HOME': HOME + '/.config', 'XDG_DATA_HOME': HOME + '/.local/share',
                            'XDG_CACHE_HOME': HOME + '/.cache'}
            require(all([line for line in env if line.startswith(k + '=')] == [k + '=' + v] for k, v in expected_env.items()) and
                    fs.read(prefix + '.cache/proof-cwd').decode().strip() == HOME, 'environment/cwd differs')
            require(desktop.outside_data(disk, offset, size) == protected, 'protected regions changed')
            require(stamp(source) == source_stamp and stamp(volume) == volume_stamp, 'source or base changed')
            receipt['boots'].append({'label': label, 'documents': hashes, 'disk_sha256': image.digest(disk),
                                     'protected_regions': protected, 'allocation': 'clean', 'binding': expected})
            guard(); save()
        inspected = call(command + ['inspect-installed', str(stage), '--driver', str(inputs['inspect_driver'])])
        require(inspected['consistent'] and inspected['used'] and inspected['filesystem']['writes'] ==
                inspected['filesystem']['flushes'] == 0, 'used installation inspection failed')
        receipt['inspection'] = inspected
        artifacts = out / 'matched-artifacts'; artifacts.mkdir()
        for name, src in {**inputs, 'build.json': ROOT / 'build/macos-build.json', 'initramfs': ROOT / 'build/initramfs.tar',
                          **{name: ROOT / f'build/userspace/{name}.elf' for name in
                             ('deskd', 'desktools', 'deskterm', 'profile', 'deskprofiles', 'env', 'pwd')}}.items():
            target = artifacts / name; install.clone(src, target)
            require(image.digest(target) == image.digest(src), 'artifact preservation mismatch')
        guard()
        require({name: image.digest(ROOT / 'tools' / name) for name in tool_names} == receipt['tools'], 'proof tool changed')
        require(stamp(source) == source_stamp and stamp(volume) == volume_stamp, 'source or base changed')
        processes = subprocess.check_output(['ps', '-axo', 'comm=,command='], text=True).splitlines()
        require(not [line for line in processes if line.split() and line.split()[0].endswith('qemu-system-x86_64') and str(out) in line], 'owned QEMU still active')
        receipt.update(status='PASS', owned_qemu_remaining=0, final_disk=stamp(disk))
        save(); print('PASS: v2 offline restore and two boots; evidence ' + str(out), flush=True)
    except BaseException as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        save(); traceback.print_exc(); raise


if __name__ == '__main__':
    main()
