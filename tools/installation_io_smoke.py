#!/usr/bin/env python3
"""Qualify host publication I/O faults using generated production images.

Deterministic Python I/O injection is not a power-loss or media-fault test.
Every target and all inputs are private generated files in build/review.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import traceback

from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from profile_desktop_smoke import (ROOT, ID, BINARY, EDITED, boot, data_image,
    digest, machine_preferences, make_bundle, outside_data, partitions, save_json,
    verified_build)
from profile_installation_smoke import clone, seed_volume
from uefi_usb_smoke import default_ovmf_path
from installation_io_faults import Faults, snapshot
import installation_image
import profile_bundle
import source_provenance
MIB = 1024 ** 2


def worker(stage, operation, label, mode):
    faults = Faults(stage.resolve(), label, mode).enter()
    error = None
    try:
        getattr(installation_image, operation)(stage)
    except Exception:
        error = traceback.format_exc()
    finally:
        faults.exit()
    print(json.dumps({'fired': faults.fired, 'events': faults.events, 'error': error}))
    return 1 if error or not faults.fired else 0


class Storage:
    """Bound this run without pruning or discovering other evidence."""
    def __init__(self, out, budget, reserve):
        self.out, self.budget, self.reserve = out, budget, reserve
    def check(self, additional=0):
        seen = set()
        used = 0
        for root, dirs, files in os.walk(self.out, followlinks=False):
            dirs[:] = [name for name in dirs if not (Path(root) / name).is_symlink()]
            for name in files:
                info = (Path(root) / name).lstat()
                if not (info.st_mode & 0o170000) == 0o100000:
                    continue
                identity = (info.st_dev, info.st_ino)
                if identity not in seen:
                    seen.add(identity)
                    used += info.st_size
        if used + additional > self.budget:
            raise RuntimeError('run storage budget exceeded')
        if shutil.disk_usage(self.out).free < self.reserve + additional:
            raise RuntimeError('insufficient free space for operation')
    def call(self, allowance, function, *args, **kwargs):
        self.check(allowance)
        value = function(*args, **kwargs)
        self.check()
        return value


def run(args, storage):
    args.storage = storage
    out = storage.out
    receipt_path = out / 'receipt.json'
    receipt = {'status': 'INCOMPLETE', 'scope': 'private generated QEMU images; deterministic host Python I/O injection',
               'limits': ['not power-loss or media-fault simulation', 'profile subprocess I/O is not intercepted',
                          'no physical disk acceptance', 'external fsck.exfat not run'],
               'cwd': os.getcwd(), 'steps': [], 'boots': {}}
    save_json(receipt_path, receipt)
    print(f'Installation I/O evidence: {out}', flush=True)
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
        originals = {'kernel': ROOT / 'build/kernel/srvros.elf', 'bootx64': ROOT / 'build/limine-binary/BOOTX64.EFI',
                     'exfat': ROOT / 'build/srvros.exfat', 'init': ROOT / 'build/userspace/init.elf',
                     'driver': ROOT / 'build/tests/profile-image-driver', 'fs_driver': ROOT / 'build/tests/exfat-driver'}
        receipt['inputs'] = {n: installation_image.digest(p) for n, p in originals.items()}
        receipt['tools'] = {n: installation_image.digest(ROOT / 'tools' / n) for n in
            ('installation_io_smoke.py', 'installation_io_faults.py', 'installation_image.py',
             'profile_installation_smoke.py', 'profile_desktop_smoke.py', 'profile_image.py',
             'profile_bundle.py', 'mk_usb_image.py', 'source_provenance.py')}
        save_json(receipt_path, receipt)
        private = out / 'inputs'; private.mkdir()
        paths = {}
        for name in ('kernel', 'bootx64', 'init', 'driver'):
            paths[name] = private / name
            storage.call(originals[name].stat().st_size, clone, originals[name], paths[name])
        paths['driver'].chmod(0o700)
        paths['exfat'] = private / 'base.exfat'
        prefs = machine_preferences(4, -300)
        storage.call(2 * originals['exfat'].stat().st_size, seed_volume,
                     originals['exfat'], paths['exfat'], prefs, paths['driver'], out)
        paths['bundle'] = private / 'bundle.json'
        profile_bundle.publish_bundle(paths['bundle'], make_bundle())
        private_hashes = {n: installation_image.digest(p) for n, p in paths.items()}
        receipt['private_inputs'] = private_hashes
        save_json(receipt_path, receipt)
        home = '/fat/home/local-a'
        def prepare(stage, profile):
            options = dict(identity_bound=True, profile_bundle=paths['bundle'], profile_home=home,
                           profile_alias='local_a', profile_uid=1001, profile_gid=1001,
                           profile_driver=paths['driver']) if profile else {}
            plan = installation_image.prepare(stage, paths['kernel'], paths['bootx64'], paths['exfat'], paths['init'], **options)
            if plan['version'] != (2 if profile else 1):
                raise RuntimeError('unexpected plan version')
        def command(name, stage, *extra):
            cmd = [sys.executable, str(ROOT / 'tools/installation_image.py'), name, str(stage), *extra]
            result = subprocess.run(cmd, capture_output=True, text=True)
            return {'argv': cmd, 'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
        def faulty(stage, operation, label, mode):
            plan = json.loads((stage / 'plan.json').read_text())
            if operation == 'apply':
                storage.check(plan['layout']['size_bytes'] + 2 * paths['exfat'].stat().st_size + 64 * MIB)
            cmd = [sys.executable, str(Path(__file__).resolve()), '--worker', str(stage), operation, label, mode]
            before = snapshot(stage)
            result = subprocess.run(cmd, capture_output=True, text=True)
            call = {'argv': cmd, 'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr,
                    'before': before, 'after': snapshot(stage)}
            receipt['steps'].append(call); save_json(receipt_path, receipt)
            payload = json.loads(result.stdout)
            if result.returncode != 1 or not payload['fired'] or not payload['error']:
                raise RuntimeError(f'fault did not propagate: {call}')
            verify_sources()
            storage.check()
            return call
        def verify_sources():
            current = {n: installation_image.digest(p) for n, p in paths.items()}
            build = {n: installation_image.digest(p) for n, p in originals.items()}
            if current != private_hashes or build != receipt['inputs']:
                raise RuntimeError('source changed')
            return {'private': current, 'build': build}
        def inspect(stage, allowed, state=None):
            before = snapshot(stage); call = command('inspect', stage); after = snapshot(stage)
            record = {'call': call, 'before': before, 'after': after, 'sources': verify_sources()}
            receipt['steps'].append(record); save_json(receipt_path, receipt)
            if before != after or call['exit'] != 0:
                raise RuntimeError(f'inspect failed or modified stage: {call}')
            info = json.loads(call['stdout']); record['info'] = info
            if info.get('can_reconcile') is not allowed or (state and info.get('state') != state):
                raise RuntimeError(f'unexpected inspection: {info}')
            return info, call, before
        def reconcile(stage, allowed):
            before = snapshot(stage); call = command('reconcile', stage); after = snapshot(stage)
            record = {'call': call, 'before': before, 'after': after, 'sources': verify_sources()}
            receipt['steps'].append(record); save_json(receipt_path, receipt)
            if (call['exit'] == 0) != allowed or (not allowed and before != after):
                raise RuntimeError(f'unexpected reconcile: {call}')
            return call, before, after
        def record_inspect(stage, expected):
            return inspect(stage, expected)

        stage = out / 'publication'; prepare(stage, True)
        receipt['publication_plan'] = json.loads((stage / 'plan.json').read_text())
        receipt['publication_prepared'] = snapshot(stage); save_json(receipt_path, receipt)
        faulty(stage, 'apply', 'fsync:publication.json:1', 'before')
        inspect(stage, True, 'verified')
        candidate_hash = installation_image.digest(stage / 'image.partial')
        receipt['candidate_sha256'] = candidate_hash; save_json(receipt_path, receipt)
        for label, mode, state in (
            ('link:image.img:1', 'after', 'image-linked'),
            ('unlink:image.partial:1', 'after', 'image-published'),
            ('fsync:ready.partial:1', 'before', 'ready-written'),
            ('link:ready.json:1', 'after', 'ready-linked'),
            ('unlink:ready.partial:1', 'after', 'ready')):
            faulty(stage, 'reconcile', label, mode)
            inspect(stage, True, state)
            image = stage / ('image.img' if (stage / 'image.img').exists() else 'image.partial')
            if installation_image.digest(image) != candidate_hash:
                raise RuntimeError('candidate changed during reconciliation')
        reconcile(stage, True)
        _, before, after = reconcile(stage, True)
        if before != after or installation_image.digest(stage / 'image.img') != candidate_hash:
            raise RuntimeError('reconciliation is not byte-identical and idempotent')
        ready = json.loads((stage / 'ready.json').read_text())
        expected = {'profile_id': ID, 'home': home, 'alias': 'local_a', 'uid': 1001, 'gid': 1001,
                    'launch_command': f'run /fat/bin/deskd --profile={home}'}
        if any(ready.get('profile', {}).get(k) != v for k, v in expected.items()):
            raise RuntimeError('ready profile mismatch')
        receipt['ready'] = ready; save_json(receipt_path, receipt)
        legacy = out / 'legacy'; prepare(legacy, False)
        receipt['legacy_plan'] = json.loads((legacy / 'plan.json').read_text())
        receipt['legacy_prepared'] = snapshot(legacy); save_json(receipt_path, receipt)
        faulty(legacy, 'apply', 'write:image.partial:1', 'prefix')
        inspect(legacy, False, 'incomplete')
        reconcile(legacy, False)
        if not (legacy / 'image.partial').is_file():
            raise RuntimeError('legacy partial image missing')
        save_json(receipt_path, receipt)
        disk = stage / 'image.img'
        ready_path = stage / 'ready.json'
        ready_hash = installation_image.digest(ready_path)
        _, (offset, size) = partitions(disk)
        protected = outside_data(disk, offset, size)
        for number in ('a1', 'a2'):
            storage.call(64 * MIB, boot, disk, out, number, args, home, 4, -300, edit=(number == 'a1'))
            serial = (out / f'serial-{number}.log').read_bytes()
            identity = b'installed-root: selected device=ahci0p2 format=exfat-v1 role=system'
            namespace = b'namespace: /bin /usr /var /home active'
            launch = f'deskd: profile id={ID} home={home} credentials=unchanged'.encode()
            if not (0 <= serial.find(identity) < serial.find(namespace) < serial.find(launch)):
                raise RuntimeError('root identity, namespace and profile launch order not proven')
            if outside_data(disk, offset, size) != protected:
                raise RuntimeError('protected disk regions changed')
            payload = data_image(disk, offset, size)
            extracted = out / f'data-{number}.exfat'
            storage.call(len(payload), extracted.write_bytes, payload)
            allocation_cmd = [str(originals['fs_driver']), str(extracted), 'allocation-check']
            check = subprocess.run(allocation_cmd, capture_output=True, text=True)
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
                'environment': env, 'environment_sha256': digest(env.encode()), 'cwd': cwd, 'application_sha256': app_hashes,
                'screenshots': [str(out / f'{name}-{number}.ppm') for name in ('text', 'files', 'terminal', 'desktop')],
                'allocation_call': {'argv': allocation_cmd, 'exit': check.returncode, 'stdout': check.stdout, 'stderr': check.stderr},
                'data_sha256': digest(payload), 'allocation': check.stdout.strip(),
                'post_guest_inspect': after_boot, 'inspect_call': inspect_call, 'stage': stage_after_boot,
                'reconcile_refused': rejected, 'stage_after_refusal': after_rejection}
            verify_sources(); save_json(receipt_path, receipt)
        receipt['status'] = 'PASS'
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}', traceback=traceback.format_exc())
        raise
    finally:
        save_json(receipt_path, receipt)
    print(f'installation-io: PASS; evidence {out}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--worker', nargs=4, metavar=('STAGE', 'OPERATION', 'LABEL', 'MODE'))
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars')
    parser.add_argument('--boot-wait', type=int, default=120)
    parser.add_argument('--run-budget-gib', type=float, default=8)
    parser.add_argument('--min-free-gib', type=float, default=4)
    args = parser.parse_args()
    if args.worker:
        stage, operation, label, mode = args.worker
        candidate = Path(stage).absolute()
        base = (ROOT / 'build/review/installation-io').absolute()
        if candidate.parent.parent != base or not candidate.parent.name.startswith('proof-') or candidate.name not in ('publication', 'legacy') or any(p.is_symlink() for p in (candidate, *candidate.parents)):
            parser.error('worker stage must be a generated private proof stage')
        if operation not in ('apply', 'reconcile') or mode not in ('before', 'after', 'prefix'):
            parser.error('invalid worker operation or mode')
        return worker(Path(stage), operation, label, mode)
    if not 10 <= args.boot_wait <= 240 or args.run_budget_gib <= 0 or args.min_free_gib <= 0:
        parser.error('invalid wait or storage limit')
    for firmware in (args.ovmf, args.ovmf_vars):
        if not firmware or not Path(firmware).is_file() or ',' in firmware:
            parser.error('invalid firmware path')
    base = ROOT / 'build/review/installation-io'
    if any(p.is_symlink() for p in (base, *base.parents)):
        raise RuntimeError('linked evidence directory')
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    storage = Storage(out, int(args.run_budget_gib * 1024 ** 3), int(args.min_free_gib * 1024 ** 3))
    run(args, storage)
    return 0


if __name__ == '__main__':
    sys.exit(main())
