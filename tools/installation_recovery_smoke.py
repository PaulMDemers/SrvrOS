#!/usr/bin/env python3
"""Exercise interrupted profile-install publication on disposable QEMU images.

This is prototype VM evidence, not a physical installer or power-loss proof.
All targets and mutations are private generated files under build/review.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tarfile
import tempfile

from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from profile_desktop_smoke import (ROOT, ID, BINARY, EDITED, boot, data_image,
    digest, machine_preferences, make_bundle, outside_data, partitions, save_json,
    verified_build)
from profile_installation_smoke import clone, seed_volume
from uefi_usb_smoke import default_ovmf_path
import installation_image
import profile_bundle
import source_provenance


POINTS = ('provisioning-started', 'provisioned', 'assembled', 'verified',
          'image-linked', 'image-published', 'ready-written', 'ready-linked', 'ready-published')
EARLY = set(POINTS[:3])


def snapshot(path):
    """Capture every file's bytes, type, permissions and link identity, without following links."""
    result = {}
    def walk(directory, prefix=''):
        for child in sorted(directory.iterdir(), key=lambda p: p.name):
            name = prefix + child.name
            info = child.lstat()
            entry = {'mode': stat.S_IMODE(info.st_mode), 'device': info.st_dev,
                     'inode': info.st_ino, 'links': info.st_nlink, 'size': info.st_size}
            if stat.S_ISLNK(info.st_mode):
                entry.update(type='symlink', target=os.readlink(child))
            elif stat.S_ISDIR(info.st_mode):
                entry['type'] = 'directory'
                walk(child, name + '/')
            elif stat.S_ISREG(info.st_mode):
                entry.update(type='file', sha256=installation_image.digest(child))
            else:
                raise RuntimeError(f'unexpected object in stage: {child}')
            result[name] = entry
    walk(path)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 240:
        parser.error('invalid boot wait')
    for firmware in (args.ovmf, args.ovmf_vars):
        if not Path(firmware).is_file() or ',' in firmware:
            parser.error('invalid firmware path')
    base = ROOT / 'build/review/installation-recovery'
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base)).resolve()
    receipt_path = out / 'receipt.json'
    receipt = {'status': 'INCOMPLETE', 'scope': 'disposable QEMU prototype only',
               'boundaries': {}, 'negative_cases': {}, 'boots': {}}
    save_json(receipt_path, receipt)
    print(f'Installation recovery evidence: {out}', flush=True)
    try:
        receipt['source'] = source_provenance.capture(ROOT)
        receipt['build'] = verified_build(ROOT)
        with tarfile.open(ROOT / 'build/initramfs.tar') as archive:
            member = next(m for m in archive.getmembers() if m.name.lstrip('./') == 'init')
            if archive.extractfile(member).read() != runtime_elf((ROOT / 'build/userspace/init.elf').read_bytes()):
                raise RuntimeError('Bootstrap init differs from matched build archive')
        receipt['qemu_version'] = subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]
        receipt['firmware_sha256'] = installation_image.digest(args.ovmf)
        receipt['firmware_vars_sha256'] = installation_image.digest(args.ovmf_vars)
        originals = {'kernel': ROOT / 'build/kernel/srvros.elf',
                     'bootx64': ROOT / 'build/limine-binary/BOOTX64.EFI',
                     'exfat': ROOT / 'build/srvros.exfat',
                     'init': ROOT / 'build/userspace/init.elf',
                     'driver': ROOT / 'build/tests/profile-image-driver',
                     'fs_driver': ROOT / 'build/tests/exfat-driver'}
        receipt['inputs'] = {name: installation_image.digest(path) for name, path in originals.items()}
        receipt['tools'] = {name: installation_image.digest(ROOT / 'tools' / name) for name in
                            ('installation_recovery_smoke.py', 'installation_image.py',
                             'profile_installation_smoke.py', 'profile_desktop_smoke.py',
                             'profile_image.py', 'profile_bundle.py')}
        save_json(receipt_path, receipt)
        private = out / 'inputs'; private.mkdir()
        paths = {}
        for name in ('kernel', 'bootx64', 'init', 'driver'):
            paths[name] = private / name
            clone(originals[name], paths[name])
        paths['driver'].chmod(0o700)
        paths['exfat'] = private / 'base.exfat'
        prefs = machine_preferences(4, -300)
        seed_volume(originals['exfat'], paths['exfat'], prefs, paths['driver'], out)
        paths['bundle'] = private / 'bundle.json'
        profile_bundle.publish_bundle(paths['bundle'], make_bundle())
        private_hashes = {name: installation_image.digest(path) for name, path in paths.items()}
        receipt['private_inputs'] = private_hashes
        save_json(receipt_path, receipt)
        home = '/fat/home/local-a'
        def prepare(stage):
            plan = installation_image.prepare(stage, paths['kernel'], paths['bootx64'], paths['exfat'],
                paths['init'], identity_bound=True, profile_bundle=paths['bundle'], profile_home=home,
                profile_alias='local_a', profile_uid=1001, profile_gid=1001, profile_driver=paths['driver'])
            if plan['version'] != 2:
                raise RuntimeError('profile plan is not version 2')
        def command(name, stage, *extra):
            cmd = [sys.executable, str(ROOT / 'tools/installation_image.py'), name, str(stage), *extra]
            result = subprocess.run(cmd, capture_output=True, text=True)
            return {'argv': cmd, 'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
        def inspect(stage):
            before = snapshot(stage)
            call = command('inspect', stage)
            after = snapshot(stage)
            if before != after:
                raise RuntimeError('inspect modified the stage')
            try:
                result = json.loads(call['stdout'])
            except ValueError as error:
                raise RuntimeError(f'inspect did not return JSON: {call}') from error
            return result, call, before
        def reconcile(stage, allowed):
            before = snapshot(stage)
            call = command('reconcile', stage)
            after = snapshot(stage)
            if (call['exit'] == 0) != allowed:
                raise RuntimeError(f'unexpected reconcile result: {call}')
            if not allowed and before != after:
                raise RuntimeError('refused reconcile modified the stage')
            return call, before, after
        def verify_sources():
            for name, path in paths.items():
                if installation_image.digest(path) != private_hashes[name]:
                    raise RuntimeError(f'private source changed: {name}')
            for name, path in originals.items():
                if installation_image.digest(path) != receipt['inputs'][name]:
                    raise RuntimeError(f'build source changed: {name}')
        def record_inspect(stage, expected):
            info, call, state = inspect(stage)
            if info.get('can_reconcile') is not expected:
                raise RuntimeError(f'unexpected inspect eligibility: {info}')
            return info, call, state

        for point in POINTS:
            stage = out / ('case-' + point)
            prepare(stage)
            interrupted = command('apply', stage, '--interrupt-after', point)
            if interrupted['exit'] != 75:
                raise RuntimeError(f'{point}: did not interrupt at real checkpoint: {interrupted}')
            allowed = point not in EARLY
            info, inspection, before = record_inspect(stage, allowed)
            record = {'interrupted': interrupted, 'inspect': info, 'inspect_call': inspection,
                      'stage_before': before}
            receipt['boundaries'][point] = record
            save_json(receipt_path, receipt)
            # Probe corruption before publication, restoring exact bytes and
            # inode identities between variants. Never forge a journal identity.
            if point in ('verified', 'ready-written'):
                candidate = stage / ('image.partial' if point == 'verified' else 'image.img')
                baseline = snapshot(stage)
                plan = json.loads((stage / 'plan.json').read_text())
                def replace_bytes(path, replacement):
                    previous = path.read_bytes()
                    path.write_bytes(replacement)
                    return lambda: path.write_bytes(previous)
                def flip(path, offset):
                    with path.open('r+b') as stream:
                        stream.seek(offset); old = stream.read(1)
                        if len(old) != 1:
                            raise RuntimeError(f'cannot flip byte in {path}')
                        stream.seek(offset); stream.write(bytes([old[0] ^ 0x80])); stream.flush(); os.fsync(stream.fileno())
                    def restore():
                        with path.open('r+b') as stream:
                            stream.seek(offset); stream.write(old); stream.flush(); os.fsync(stream.fileno())
                    return restore
                def created(path, data):
                    path.write_bytes(data)
                    return path.unlink
                def link(path, destination):
                    os.link(path, destination)
                    return destination.unlink
                def symlink(destination):
                    destination.symlink_to('plan.json')
                    return destination.unlink
                def tamper_plan():
                    plan_path = stage / 'plan.json'; checksum = stage / 'plan.sha256'
                    old_plan, old_checksum = plan_path.read_bytes(), checksum.read_bytes()
                    obj = json.loads(old_plan); obj['target_uuid'] = '00000000-0000-4000-8000-000000000001'
                    plan_path.write_text(json.dumps(obj, sort_keys=True) + '\n')
                    checksum.write_text(installation_image.digest(plan_path) + '\n')
                    def restore():
                        plan_path.write_bytes(old_plan); checksum.write_bytes(old_checksum)
                    return restore
                def wrong_target_inode():
                    replacement = out / 'wrong-target-copy'
                    saved = out / 'wrong-target-original'
                    shutil.copytree(stage, replacement, copy_function=lambda src, dst: clone(Path(src), Path(dst)))
                    stage.rename(saved)
                    replacement.rename(stage)
                    def restore():
                        shutil.rmtree(stage)
                        saved.rename(stage)
                    return restore
                if point == 'verified':
                    tests = [
                        ('changed-plan-and-checksum', tamper_plan),
                        ('wrong-target-inode', wrong_target_inode),
                        ('changed-bundle', lambda: flip(paths['bundle'], 0)),
                        ('changed-driver', lambda: flip(paths['driver'], 0)),
                        ('changed-kernel', lambda: flip(paths['kernel'], 0)),
                        ('changed-base', lambda: flip(paths['exfat'], 0)),
                        ('corrupt-gpt', lambda: flip(candidate, 512)),
                        ('corrupt-esp', lambda: flip(candidate, plan['layout']['esp']['offset_bytes'])),
                        ('corrupt-data', lambda: flip(candidate, plan['layout']['exfat']['offset_bytes'] + 3)),
                        ('altered-profile', lambda: flip(stage / 'profile.exfat', 3)),
                        ('torn-journal', lambda: replace_bytes(stage / 'publication.json', b'{')),
                        ('unknown-file', lambda: created(stage / 'unexpected', b'unknown')),
                        ('symlink', lambda: symlink(stage / 'unexpected-link')),
                        ('outside-hardlink', lambda: link(candidate, out / 'outside-link')),
                    ]
                else:
                    ready_path = stage / 'ready.partial'
                    tests = [('torn-ready', lambda: replace_bytes(ready_path, b'{')),
                             ('mismatched-ready', lambda: replace_bytes(ready_path, b'{}\n'))]
                for label, corrupt in tests:
                    restore = corrupt()
                    try:
                        changed_sources = {name: installation_image.digest(path) for name, path in paths.items()}
                        negative_info, negative_call, negative_before = record_inspect(stage, False)
                        rec_call, _, negative_after = reconcile(stage, False)
                        if changed_sources != {name: installation_image.digest(path) for name, path in paths.items()}:
                            raise RuntimeError(f'{label}: refusal changed private sources')
                        receipt['negative_cases'][label] = {'inspect': negative_info,
                            'inspect_call': negative_call, 'reconcile': rec_call,
                            'stage_before': negative_before, 'stage_after': negative_after,
                            'private_inputs': {name: installation_image.digest(path) for name, path in paths.items()}}
                        save_json(receipt_path, receipt)
                    finally:
                        restore()
                    if snapshot(stage) != baseline:
                        raise RuntimeError(f'{label}: fixture restoration failed')
                    verify_sources()
                    record_inspect(stage, True)
            expected_state = ('incomplete' if point in EARLY else 'ready' if point == 'ready-published' else point)
            if info.get('state') != expected_state:
                raise RuntimeError(f'{point}: unexpected state {info}')
            candidate_hash = next((entry['sha256'] for name, entry in before.items()
                                   if name in ('image.partial', 'image.img')), None)
            rec_call, _, after = reconcile(stage, allowed)
            record.update(reconcile=rec_call, stage_after=after)
            if allowed:
                if not candidate_hash or installation_image.digest(stage / 'image.img') != candidate_hash:
                    raise RuntimeError(f'{point}: reconciliation changed candidate bytes')
                ready = json.loads((stage / 'ready.json').read_text())
                expected = {'profile_id': ID, 'home': home, 'alias': 'local_a', 'uid': 1001,
                            'gid': 1001, 'launch_command': f'run /fat/bin/deskd --profile={home}'}
                if any(ready.get('profile', {}).get(k) != value for k, value in expected.items()):
                    raise RuntimeError('reconciled ready profile does not match plan')
                second, second_before, second_after = reconcile(stage, True)
                if second_before != second_after:
                    raise RuntimeError('idempotent reconcile changed ready stage')
                record['repeat_reconcile'] = second
                record['ready'] = ready
            verify_sources()
            save_json(receipt_path, receipt)
        # Interrupt reconciliation itself twice, then finish in another fresh
        # process. The verified candidate is never regenerated or provisioned.
        stage = out / 'reconcile-interruptions'
        prepare(stage)
        call = command('apply', stage, '--interrupt-after', 'verified')
        if call['exit'] != 75:
            raise RuntimeError(f'could not create verified reconciliation fixture: {call}')
        info, inspection, before = record_inspect(stage, True)
        if info.get('state') != 'verified':
            raise RuntimeError('fresh reconciliation fixture is not verified')
        candidate_hash = installation_image.digest(stage / 'image.partial')
        receipt['reconcile_interruptions'] = {'initial_apply': call, 'initial_inspect': inspection,
                                               'initial_stage': before, 'steps': []}
        save_json(receipt_path, receipt)
        for boundary in ('image-linked', 'ready-linked'):
            interrupted = command('reconcile', stage, '--interrupt-after', boundary)
            if interrupted['exit'] != 75:
                raise RuntimeError(f'reconcile did not interrupt at {boundary}: {interrupted}')
            info, inspection, state = record_inspect(stage, True)
            if info.get('state') != boundary:
                raise RuntimeError(f'wrong reconciliation checkpoint state: {info}')
            image = stage / ('image.img' if (stage / 'image.img').exists() else 'image.partial')
            if installation_image.digest(image) != candidate_hash:
                raise RuntimeError('interrupted reconciliation changed candidate content')
            receipt['reconcile_interruptions']['steps'].append(
                {'boundary': boundary, 'call': interrupted, 'inspect': info,
                 'inspect_call': inspection, 'stage': state})
            verify_sources(); save_json(receipt_path, receipt)
        completed, _, completed_state = reconcile(stage, True)
        if installation_image.digest(stage / 'image.img') != candidate_hash:
            raise RuntimeError('resumed reconciliation changed candidate')
        repeat, repeat_before, repeat_after = reconcile(stage, True)
        if repeat_before != repeat_after:
            raise RuntimeError('completed reconciliation not idempotent')
        receipt['reconcile_interruptions'].update(completed=completed, stage_after=completed_state,
                                                   repeat_reconcile=repeat)
        save_json(receipt_path, receipt)
        disk = stage / 'image.img'
        ready_path = stage / 'ready.json'
        ready_hash = installation_image.digest(ready_path)
        _, (offset, size) = partitions(disk)
        protected = outside_data(disk, offset, size)
        for number in ('a1', 'a2'):
            boot(disk, out, number, args, home, 4, -300, edit=(number == 'a1'))
            serial = (out / f'serial-{number}.log').read_bytes()
            identity = b'installed-root: selected device=ahci0p2 format=exfat-v1 role=system'
            namespace = b'namespace: /bin /usr /var /home active'
            launch = f'deskd: profile id={ID} home={home} credentials=unchanged'.encode()
            if not (0 <= serial.find(identity) < serial.find(namespace) < serial.find(launch)):
                raise RuntimeError('root identity, namespace and profile launch order not proven')
            if outside_data(disk, offset, size) != protected:
                raise RuntimeError('protected disk regions changed')
            payload = data_image(disk, offset, size)
            extracted = out / f'data-{number}.exfat'; extracted.write_bytes(payload)
            check = subprocess.run([str(originals['fs_driver']), str(extracted), 'allocation-check'],
                                   capture_output=True, text=True)
            (out / f'allocation-{number}.log').write_text(check.stdout + check.stderr)
            if check.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in check.stdout:
                raise RuntimeError('offline allocation check failed')
            fs = ExfatFixture(payload); prefix = home.removeprefix('/fat/') + '/'
            if fs.read(prefix + 'profile.id') != b'srvros-profile-v1\n' + ID.encode() + b'\n':
                raise RuntimeError('profile marker mismatch')
            local = profile_bundle.decode_json(fs.read(prefix + 'profile-local.json'))
            if local != {'alias': 'local_a', 'uid': 1001, 'gid': 1001}:
                raise RuntimeError('profile local binding mismatch')
            document = fs.read(prefix + 'Documents/migration.txt')
            binary = fs.read(prefix + 'Documents/binary.dat')
            portable = fs.read(prefix + profile_bundle.PREF_PATH)
            if (document != EDITED or binary != BINARY or
                profile_bundle.decode_json(portable) != dict(make_bundle()['preferences'], theme_style=1)):
                raise RuntimeError('profile documents or portable settings mismatch')
            machine = fs.read('home/desktop.conf')
            if machine != prefs:
                raise RuntimeError('machine settings changed')
            app_hashes = {}
            for app in ('deskd', 'desktools', 'deskterm'):
                data = fs.read('bin/' + app)
                if data != runtime_elf((ROOT / f'build/userspace/{app}.elf').read_bytes()):
                    raise RuntimeError(f'installed {app} differs from matched build')
                app_hashes[app] = digest(data)
            env = fs.read(prefix + 'Documents/environment.txt').decode()
            cwd = fs.read(prefix + 'Documents/cwd.txt').decode().strip()
            expected = {'HOME': home, 'SRVROS_DESKTOP_HOME': home, 'SRVROS_PROFILE_ID': ID,
                        'XDG_CONFIG_HOME': home + '/.config', 'XDG_DATA_HOME': home + '/.local/share',
                        'XDG_CACHE_HOME': home + '/.cache'}
            if cwd != home or any(f'{k}={v}' not in env.splitlines() for k, v in expected.items()):
                raise RuntimeError('terminal environment or cwd mismatch')
            after_boot, inspect_call, stage_after_boot = record_inspect(stage, False)
            rejected, _, after_rejection = reconcile(stage, False)
            if installation_image.digest(ready_path) != ready_hash:
                raise RuntimeError('recovery changed or removed ready receipt after guest writes')
            receipt['boots'][number] = {'profile_id': ID, 'home': home, 'local': local,
                'document_sha256': digest(document), 'binary_sha256': digest(binary),
                'portable_preferences_sha256': digest(portable), 'machine_preferences_sha256': digest(machine),
                'environment_sha256': digest(env.encode()), 'cwd': cwd, 'application_sha256': app_hashes,
                'data_sha256': digest(payload), 'allocation': check.stdout.strip(),
                'post_guest_inspect': after_boot, 'inspect_call': inspect_call, 'stage': stage_after_boot,
                'reconcile_refused': rejected, 'stage_after_refusal': after_rejection}
            verify_sources(); save_json(receipt_path, receipt)
        receipt.update(status='PASS', limits=['disposable QEMU prototype only',
            'process interruption after successful I/O, not media failure or fsync fault qualification',
            'journal is an integrity record, not authentication',
            'no physical disk or power-loss acceptance', 'external fsck.exfat not run'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        save_json(receipt_path, receipt)
    print(f'installation-recovery: PASS; evidence {out}', flush=True)


if __name__ == '__main__':
    main()
