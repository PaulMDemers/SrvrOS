#!/usr/bin/env python3
"""Prove offline v2 export/reconciliation feeds a fresh two-boot EFI install.

The source A and destination B are generated under a new evidence directory.
No original installation, source tree, or existing guest disk is opened for
writing. The guest payload root must contain the recorded matched build and
native fixture drivers; this script does not build anything.
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
import profile_stream as stream
import profile_stream_fixture
import profile_transfer_smoke as transfer
import source_provenance
from installation_export_recovery_smoke import readback, stamp, tree
from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path

ROOT = Path(__file__).resolve().parents[1]
HOME_A = '/fat/home/offline_a'
HOME_B = '/fat/home/recovered_b'
ALIAS_A = 'offline_a'
ALIAS_B = 'recovered_b'
UID_A, GID_A = 3040, 3041
UID_B, GID_B = 3050, 3051
GIB = 1024 ** 3
CHUNK = stream.BUFFER
DOCS = ('Documents/migration.txt', 'Documents/binary.profile-import.dat', 'Documents/large.bin')
UNSELECTED = 'Documents/unselected.txt'


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest_bytes(data):
    return hashlib.sha256(data).hexdigest()


def make_stream(path):
    """Create the exact synthetic v2 source used to provision offline A."""
    portable = desktop.make_bundle()
    docs = [
        (DOCS[0], create.TEXT),
        (DOCS[1], create.BINARY),
        (DOCS[2], bytes(range(256)) * (stream.MAX_FILE // 256)),
        (UNSELECTED, b'synthetic unselected source document\n'),
    ]
    manifest = profile_stream_fixture.write_stream(
        path, portable['profile'], portable['preferences'], docs)
    require(path.stat().st_size <= stream.MAX_BUNDLE, 'bundle too large')
    return manifest, {name: data for name, data in docs}


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def source_equivalence(payload_root):
    """Verify the historical build payloads and current runtime source match."""
    build = desktop.verified_build(payload_root)
    payload_source = source_provenance.capture(payload_root)
    require(build.get('schema') == 1 and type(build.get('source')) is str,
            'invalid historical build record')
    historical = build['source']
    payload_head = payload_source['.']['head']
    current = source_provenance.capture(ROOT)
    require(not any(row.get('dirty') for row in current.values()), 'proof checkout must be clean')
    is_ancestor = subprocess.run(['git', '-C', str(ROOT), 'merge-base', '--is-ancestor',
                                   historical, 'HEAD'], capture_output=True).returncode == 0
    payload_is_descendant = subprocess.run(['git', '-C', str(payload_root), 'merge-base', '--is-ancestor',
                                             historical, 'HEAD'], capture_output=True).returncode == 0
    require(is_ancestor and payload_is_descendant,
            'historical payload source is not an ancestor of the proof source')
    paths = ('kernel', 'shared', 'userspace', 'ports', 'Makefile')
    identities = {}
    for path in paths:
        build_id = git(payload_root, 'rev-parse', f'{historical}:{path}')
        payload_id = git(payload_root, 'rev-parse', f'HEAD:{path}')
        current_id = git(ROOT, 'rev-parse', f'HEAD:{path}')
        require(build_id == payload_id == current_id,
                'source dependency differs from matched build: ' + path)
        identities[path] = build_id
    payload_repos = build.get('repositories', {})
    for name, record in current.items():
        if name == '.':
            continue
        require(name in payload_repos and record.get('head') == payload_repos[name].get('head'),
                'submodule source differs from matched payload: ' + name)
    required_runtime = ('build/kernel/srvros.elf', 'build/initramfs.tar', 'build/srvros.exfat')
    runtime_hashes = {}
    for relative in required_runtime:
        expected = build.get('hashes', {}).get(relative)
        actual = image.digest(payload_root / relative)
        require(type(expected) is str and actual == expected,
                'historical runtime payload hash mismatch: ' + relative)
        runtime_hashes[relative] = actual
    return {'build_record': build, 'payload_source': source_provenance.capture(payload_root),
            'checkout_head': git(ROOT, 'rev-parse', 'HEAD'),
            'payload_head': payload_head, 'historical_build_source': historical,
            'runtime_payload_hashes': runtime_hashes,
            'dependency_tree_ids': identities, 'submodule_heads_match': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload-root', required=True, type=Path,
                        help='clean checkout holding the earlier matched build payloads')
    parser.add_argument('--evidence-parent', required=True, type=Path)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    require(10 <= args.boot_wait <= 240, 'invalid boot wait')
    payload_root = image.checked_path(args.payload_root)
    for path in (args.ovmf, args.ovmf_vars):
        require(Path(path).is_file() and ',' not in str(path), 'invalid firmware path')

    # Helpers that historically resolve fixture binaries from their own ROOT
    # constants must use the explicitly pinned, read-only payload checkout.
    create.ROOT = payload_root
    transfer.guest.ROOT = payload_root

    parent = image.checked_path(args.evidence_parent)
    parent.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=parent)).resolve()
    receipt = {'status': 'INCOMPLETE',
               'scope': 'synthetic offline v2 export, explicit reconciliation, fresh B two EFI boots',
               'limits': ['no physical media or power-loss claim', 'one 16 MiB selected document',
                          'no source A boot required; A is a newly provisioned offline installation'],
               'steps': [], 'boots': []}

    def save():
        desktop.save_json(out / 'receipt.json', receipt)

    print('Installation stream export evidence: ' + str(out), flush=True)
    save()
    try:
        receipt['source_equivalence'] = source_equivalence(payload_root)
        require(shutil.disk_usage(out).free >= 8 * GIB, 'need 8 GiB free for proof')
        inputs = {name: payload_root / path for name, path in {
            'kernel': 'build/kernel/srvros.elf',
            'bootx64': 'build/limine-binary/BOOTX64.EFI',
            'init': 'build/userspace/init.elf',
            'base': 'build/srvros.exfat',
            'profile_driver': 'build/tests/profile-image-driver',
            'fs_driver': 'build/tests/exfat-driver',
            'inspect_driver': 'build/tests/installation-inspect-driver',
        }.items()}
        # This driver implements the new chunked host stream contract. The
        # other guest/runtime payloads remain pinned to the earlier build.
        inputs['export_driver'] = ROOT / 'build/tests/installation-export-driver'
        originals = {name: stamp(path) for name, path in inputs.items()}
        receipt['payloads'] = originals
        receipt['payload_source_sha256'] = {
            'installation_export_driver.c': image.digest(ROOT / 'tools/installation_export_driver.c'),
            'installation_export.py': image.digest(ROOT / 'tools/installation_export.py'),
            'installation_export_publication.py': image.digest(ROOT / 'tools/installation_export_publication.py'),
            'installation_image.py': image.digest(ROOT / 'tools/installation_image.py'),
            'profile_stream.py': image.digest(ROOT / 'tools/profile_stream.py'),
        }
        firmware = {str(path): image.digest(path) for path in map(Path, (args.ovmf, args.ovmf_vars))}
        receipt['firmware'] = firmware
        receipt['qemu_version'] = subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]
        tool_names = ('installation_stream_export_smoke.py', 'installation_image.py',
                      'installation_export.py', 'installation_export_publication.py',
                      'installation_inspection.py', 'installation_export_driver.c',
                      'profile_image.py', 'profile_image_driver.c', 'profile_stream.py',
                      'profile_bundle.py', 'profile_chooser_smoke.py', 'profile_create_smoke.py',
                      'profile_desktop_smoke.py', 'profile_transfer_smoke.py',
                      'profile_guest_smoke.py', 'installation_export_recovery_smoke.py',
                      'exfat_test_image.py')
        receipt['tools'] = {name: image.digest(ROOT / 'tools' / name) for name in tool_names}
        source = source_provenance.capture(ROOT)
        receipt['tool_checkout'] = source

        def guard(source_stage=None, stage_tree=None, bundle_file=None, bundle_stamp=None):
            require({name: stamp(path) for name, path in inputs.items()} == originals,
                    'matched payload changed')
            require(source_provenance.capture(ROOT) == source, 'tool checkout changed during proof')
            require({str(path): image.digest(path) for path in map(Path, (args.ovmf, args.ovmf_vars))} == firmware,
                    'firmware changed')
            require(shutil.disk_usage(out).free >= 4 * GIB, 'evidence reserve exhausted')
            if source_stage is not None:
                require(tree(source_stage) == stage_tree, 'offline source A changed')
            if bundle_file is not None:
                require(stamp(bundle_file) == bundle_stamp, 'portable input bundle changed')

        def call(argv, expected=0):
            guard()
            proc = subprocess.run(argv, capture_output=True, text=True)
            row = {'argv': argv, 'exit': proc.returncode, 'stdout': proc.stdout, 'stderr': proc.stderr}
            receipt['steps'].append(row)
            save()
            guard()
            require(proc.returncode == expected, 'unexpected command exit: ' + str(row))
            return json.loads(proc.stdout) if proc.stdout.strip() else None

        command = [sys.executable, str(ROOT / 'tools/installation_image.py')]
        source_bundle = out / 'synthetic-source-v2.srvrprofile'
        manifest_a, source_docs = make_stream(source_bundle)
        source_stamp = stamp(source_bundle)
        receipt['synthetic_source'] = {'file': str(source_bundle), 'stamp': source_stamp,
                                       'manifest': manifest_a, 'writer_chunk_bytes': CHUNK}
        identity = manifest_a['profile']
        machine_a = desktop.machine_preferences(2, 60)
        base_a = create.make_volume(inputs['base'], out, 'a-base', machine_a)
        stage_a = out / 'stage-a'
        plan_a = call(command + ['prepare', str(stage_a), '--kernel', str(inputs['kernel']),
            '--bootx64', str(inputs['bootx64']), '--exfat', str(base_a), '--init', str(inputs['init']),
            '--identity-bound', '--profile-bundle', str(source_bundle), '--profile-expected-id', identity['id'],
            '--profile-home', HOME_A, '--profile-alias', ALIAS_A, '--profile-uid', str(UID_A),
            '--profile-gid', str(GID_A), '--profile-driver', str(inputs['profile_driver'])])
        require(plan_a['version'] == 2, 'A is not a fresh identity-bound v2 installation')
        ready_a = call(command + ['apply', str(stage_a)])
        source_after_a = tree(stage_a)
        source_a_image = stamp(stage_a / 'image.img')
        receipt['offline_a'] = {'plan': plan_a, 'ready': ready_a,
                                'image': source_a_image, 'tree': source_after_a,
                                'base_sha256': image.digest(base_a)}
        a_fs = transfer.check_allocation(stage_a / 'image.img', out, 'a-before-export')
        prefix_a = HOME_A.removeprefix('/fat/') + '/'
        require(json.loads(a_fs.read(prefix_a + 'identity.json')) == identity,
                'A profile identity differs')
        require(json.loads(a_fs.read(prefix_a + bundle.PREF_PATH)) == manifest_a['preferences'],
                'A portable preferences differ')
        for item in manifest_a['documents']:
            actual = a_fs.read(prefix_a + item['path'])
            require(len(actual) == item['size'] and digest_bytes(actual) == item['sha256'],
                    'A source document differs: ' + item['path'])
        del a_fs

        output = out / 'offline-export-v2.srvrprofile'
        attempt = out / 'offline-export-attempt'
        export_args = command + ['export-profile', str(stage_a), str(output),
            '--publication-dir', str(attempt), '--interrupt-after', 'written',
            '--profile-id', identity['id'], '--home', HOME_A,
            '--inspect-driver', str(inputs['inspect_driver']), '--export-driver', str(inputs['export_driver'])]
        for document in DOCS:
            export_args += ['--document', document]
        call(export_args, expected=75)
        require(tree(stage_a) == source_after_a, 'interrupted v2 export changed source A')
        inspected = call(command + ['inspect-export', str(attempt)])
        require(inspected and inspected.get('state') == 'prepared' and inspected.get('can_reconcile') and
                inspected.get('visible') is False and not output.exists(),
                'written v2 bundle is not explicitly reconcilable')
        receipt['export_prepared'] = {'inspection': inspected, 'output_visible': output.exists()}
        reconciled = call(command + ['reconcile-export', str(attempt)])
        require(reconciled and reconciled.get('state') == 'complete', 'v2 export reconciliation failed')
        require(output.is_file() and tree(stage_a) == source_after_a, 'reconciled export missing or A changed')
        exported_stamp = stamp(output)
        exported_hash = image.digest(output)
        with stream.open_bundle(output, require_canonical=True) as exported:
            require(exported.sha256 == exported_hash, 'output is not canonical streaming v2')
            metadata = exported.metadata
            require(metadata['profile'] == identity and metadata['preferences'] == manifest_a['preferences'],
                    'exported profile metadata differs')
            expected_entries = [entry for entry in manifest_a['documents'] if entry['path'] in DOCS]
            require(metadata['documents'] == expected_entries, 'exported selection/manifest differs')
        receipt['export'] = {'path': str(output), 'stamp': exported_stamp, 'sha256': exported_hash,
                             'manifest': metadata, 'attempt_inspection': inspected,
                             'reconciliation': reconciled,
                             'selected_document_hashes': {entry['path']: entry['sha256'] for entry in expected_entries},
                             'source_unselected_document': UNSELECTED}
        output_stamp = stamp(output)
        guard(stage_a, source_after_a, output, output_stamp)

        machine_b = desktop.machine_preferences(4, -300)
        require(machine_b != desktop.machine_preferences(2, 60), 'B machine settings are not distinct')
        base_b = create.make_volume(inputs['base'], out, 'b-base', machine_b)
        stage_b = out / 'stage-b'
        plan_b = call(command + ['prepare', str(stage_b), '--kernel', str(inputs['kernel']),
            '--bootx64', str(inputs['bootx64']), '--exfat', str(base_b), '--init', str(inputs['init']),
            '--identity-bound', '--profile-bundle', str(output), '--profile-expected-id', identity['id'],
            '--profile-home', HOME_B, '--profile-alias', ALIAS_B, '--profile-uid', str(UID_B),
            '--profile-gid', str(GID_B), '--profile-driver', str(inputs['profile_driver'])])
        require(plan_b['version'] == 2, 'fresh B plan is not streaming v2')
        ready_b = call(command + ['apply', str(stage_b)])
        expected_binding = {'profile_id': identity['id'], 'home': HOME_B, 'alias': ALIAS_B,
                            'uid': UID_B, 'gid': GID_B}
        require(all(ready_b['profile'].get(k) == v for k, v in expected_binding.items()),
                'B local binding differs')
        disk = stage_b / 'image.img'
        transport, transport_record = create.install(inputs['base'], out / 'transport')
        offset, size = desktop.partitions(disk)[1]
        protected = desktop.outside_data(disk, offset, size)
        require(len(set(ready_b['gpt'].values())) == len(ready_b['gpt']), 'B GPT identifiers are not distinct')
        receipt['fresh_b'] = {'plan': plan_b, 'ready': ready_b, 'transport': transport_record,
                              'protected_regions': protected, 'initial_disk_sha256': image.digest(disk)}
        prefix_b = HOME_B.removeprefix('/fat/') + '/'
        preboot = transfer.check_allocation(disk, out, 'b-preboot')
        for directory in ('.cache', '.local/share'):
            require(not {name for name, _ in preboot.entries(preboot.read(prefix_b + directory))
                         if name not in ('.', '..')}, 'B inherited runtime state')
        del preboot

        for iteration in (1, 2):
            label = 'b' + str(iteration)
            with chooser.VM(disk, transport, out, label, args) as vm:
                if iteration == 1:
                    start, index = chooser.chooser(vm, HOME_B)
                    chooser.select(vm, index)
                    chooser.opened(vm, HOME_B, start, identity['id'])
                else:
                    create.start_restored(vm, HOME_B, identity)
                readback(vm)
            fs = transfer.check_allocation(disk, out, label)
            require(json.loads(fs.read(prefix_b + 'identity.json')) == identity, 'B identity differs')
            require(json.loads(fs.read(prefix_b + 'profile-local.json')) ==
                    {'alias': ALIAS_B, 'uid': UID_B, 'gid': GID_B}, 'B local account binding differs')
            require(json.loads(fs.read(prefix_b + bundle.PREF_PATH)) == manifest_a['preferences'],
                    'B preferences differ')
            hashes = {}
            for item in expected_entries:
                data = fs.read(prefix_b + item['path'])
                actual = {'size': len(data), 'sha256': digest_bytes(data)}
                require(actual == {'size': item['size'], 'sha256': item['sha256']},
                        'B document differs: ' + item['path'])
                hashes[item['path']] = actual
            names = {name for name, _ in fs.entries(fs.read(prefix_b + 'Documents')) if name not in ('.', '..')}
            require(names == {Path(path).name for path in DOCS},
                    'B Documents include missing or unselected files: ' + repr(names))
            create.absent(fs, prefix_b + UNSELECTED)
            for name in ('.profile-import-pending', '.cache/runtime-cache.fixture',
                         '.local/share/app-state.fixture', '.config/credential-sentinel.fixture'):
                create.absent(fs, prefix_b + name)
            create.absent(fs, HOME_B.removeprefix('/fat/') + '.profile-import.json')
            create.legacy_and_binaries(fs, machine_b)
            create.check_choice(fs, HOME_B, identity)
            for name in ('env', 'pwd'):
                require(fs.read('bin/' + name) == runtime_elf(
                    (payload_root / f'build/userspace/{name}.elf').read_bytes()),
                    'installed context binary differs: ' + name)
            env = fs.read(prefix_b + '.cache/proof-env').decode().splitlines()
            expected_env = {'HOME': HOME_B, 'SRVROS_DESKTOP_HOME': HOME_B,
                            'SRVROS_PROFILE_ID': identity['id'], 'XDG_CONFIG_HOME': HOME_B + '/.config',
                            'XDG_DATA_HOME': HOME_B + '/.local/share', 'XDG_CACHE_HOME': HOME_B + '/.cache'}
            require(all([line for line in env if line.startswith(key + '=')] == [key + '=' + value]
                        for key, value in expected_env.items()) and
                    fs.read(prefix_b + '.cache/proof-cwd').decode().strip() == HOME_B,
                    'B environment/cwd differs')
            require(desktop.outside_data(disk, offset, size) == protected,
                    'B protected regions changed')
            require(stamp(output) == output_stamp, 'portable output changed during B boot')
            receipt['boots'].append({'label': label, 'documents': hashes,
                                    'disk_sha256': image.digest(disk),
                                    'protected_regions': protected, 'binding': expected_binding})
            guard(bundle_file=output, bundle_stamp=output_stamp)
            save()

        inspection = call(command + ['inspect-installed', str(stage_b), '--driver', str(inputs['inspect_driver'])])
        require(inspection['consistent'] and inspection['used'] and
                inspection['filesystem']['writes'] == inspection['filesystem']['flushes'] == 0,
                'used-B native read-only inspection failed')
        receipt['inspection'] = inspection
        require(tree(stage_a) == source_after_a and stamp(output) == output_stamp,
                'A or v2 export changed before closeout')
        require({name: stamp(path) for name, path in inputs.items()} == originals,
                'matched payload changed during proof')
        require({name: image.digest(ROOT / 'tools' / name) for name in tool_names} == receipt['tools'],
                'proof tools changed during run')
        require({str(path): image.digest(path) for path in map(Path, (args.ovmf, args.ovmf_vars))} == firmware,
                'firmware changed during run')
        processes = subprocess.check_output(['ps', '-axo', 'comm=,command='], text=True).splitlines()
        require(not [line for line in processes if line.split() and
                     line.split()[0].endswith('qemu-system-x86_64') and str(out) in line],
                'owned QEMU remains active')
        receipt.update(status='PASS', owned_qemu_remaining=0,
                       final_b_disk=stamp(disk), final_source_a=tree(stage_a),
                       final_export=stamp(output))
        save()
        print('PASS: offline streaming-v2 export/reconcile and fresh-B two-boot restore; evidence ' + str(out),
              flush=True)
    except BaseException as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        save()
        traceback.print_exc()
        raise


if __name__ == '__main__':
    main()
