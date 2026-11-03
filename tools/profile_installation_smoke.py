#!/usr/bin/env python3
"""Prove profile-aware installation on disposable QEMU images only.

Requires a source-verified build. Retains generated images and evidence under a
private build/review directory; never accepts or discovers a physical disk.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from profile_desktop_smoke import (ROOT, ID, BINARY, EDITED, boot, data_image,
    digest, machine_preferences, make_bundle, outside_data, partitions, save_json,
    verified_build)
from uefi_usb_smoke import default_ovmf_path
import installation_image
import profile_bundle
import source_provenance


def clone(source, target):
    """Copy only our own regular fixture, requesting APFS clone where possible."""
    if target.exists():
        raise ValueError('fixture already exists')
    if os.uname().sysname == 'Darwin':
        result = subprocess.run(['cp', '-c', str(source), str(target)], capture_output=True)
        if result.returncode == 0:
            return
        if target.exists():
            target.unlink()
    shutil.copyfile(source, target)


def seed_volume(base, target, prefs, driver, out):
    # The driver requires a private single-link image inside this staging prefix.
    with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=out) as tmp:
        stage = Path(tmp) / 'image'
        clone(base, stage)
        content = Path(tmp) / 'machine.conf'
        content.write_bytes(prefs)
        for op, path, extra in (('ensure-home', '/fat/home', []),
                                ('write-desktop', '/fat/home/desktop.conf', [str(content)]),
                                ('check-clean', '/fat/home', [])):
            subprocess.run([str(driver), str(stage), op, path, *extra], check=True)
        os.link(stage, target)


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
    base = ROOT / 'build/review/profile-installation'
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base)).resolve()
    print(f'Profile installation evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'scope': 'disposable QEMU images only',
               'boots': {}, 'installations': {}, 'refusals': []}
    receipt_path = out / 'receipt.json'
    save_json(receipt_path, receipt)
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
        inputs = {'kernel': ROOT / 'build/kernel/srvros.elf',
                  'bootx64': ROOT / 'build/limine-binary/BOOTX64.EFI',
                  'exfat': ROOT / 'build/srvros.exfat',
                  'init': ROOT / 'build/userspace/init.elf',
                  'driver': ROOT / 'build/tests/profile-image-driver',
                  'fs_driver': ROOT / 'build/tests/exfat-driver'}
        receipt['inputs'] = {name: {'path': str(path), 'sha256': installation_image.digest(path)}
                             for name, path in inputs.items()}
        receipt['tools'] = {name: installation_image.digest(ROOT / 'tools' / name) for name in
                            ('profile_installation_smoke.py', 'profile_desktop_smoke.py',
                             'installation_image.py', 'profile_bundle.py', 'profile_image.py')}
        save_json(receipt_path, receipt)
        original = make_bundle()
        bundle_path = out / 'bundle-a.json'
        profile_bundle.publish_bundle(bundle_path, original)
        for label, home, alias, uid, gid, speed, offset in (
                ('a', '/fat/home/local-a', 'local_a', 1001, 1001, 4, -300),
                ('b', '/fat/home/local-b', 'local_b', 2050, 2051, 2, 60)):
            prefs = machine_preferences(speed, offset)
            volume = out / f'base-{label}.exfat'
            seed_volume(inputs['exfat'], volume, prefs, inputs['driver'], out)
            base_hash = installation_image.digest(volume)
            bundle = profile_bundle.decode_json(bundle_path.read_bytes())
            store = out / f'store-{label}'
            profile_bundle.init_store(store)
            local = profile_bundle.import_bundle(store, bundle, alias, uid, gid)
            stage = out / f'target-{label}'
            plan = installation_image.prepare(stage, inputs['kernel'], inputs['bootx64'], volume,
                inputs['init'], identity_bound=True, profile_bundle=bundle_path,
                profile_home=home, profile_alias=alias, profile_uid=uid, profile_gid=gid,
                profile_driver=inputs['driver'])
            ready = installation_image.apply(stage)
            expected_profile = {'profile_id': ID, 'home': home, 'alias': alias, 'uid': uid,
                                'gid': gid, 'launch_command': f'run /fat/bin/deskd --profile={home}'}
            if any(ready.get('profile', {}).get(k) != v for k, v in expected_profile.items()):
                raise RuntimeError('ready profile binding or explicit launch command mismatch')
            disk = stage / 'image.img'
            _, (offset_bytes, size) = partitions(disk)
            bounds = outside_data(disk, offset_bytes, size)
            if installation_image.digest(volume) != base_hash:
                raise RuntimeError('bound base volume changed during installation')
            if label == 'b':
                first = receipt['installations']['a']['gpt']
                if set(first.values()) & set(ready['gpt'].values()):
                    raise RuntimeError('installation GPT identities are not distinct')
            receipt['installations'][label] = {'plan': plan, 'ready': ready, 'gpt': ready['gpt'],
                'base_sha256': base_hash, 'bundle_sha256': installation_image.digest(bundle_path),
                'protected_regions': bounds, 'data_offset': offset_bytes, 'data_bytes': size,
                'initial_disk_sha256': installation_image.digest(disk), 'binding': local}
            save_json(receipt_path, receipt)
            for iteration in (1, 2):
                number = f'{label}{iteration}'
                boot(disk, out, number, args, home, speed, offset, edit=(number == 'a1'))
                serial = (out / f'serial-{number}.log').read_bytes()
                identity = b'installed-root: selected device=ahci0p2 format=exfat-v1 role=system'
                namespace = b'namespace: /bin /usr /var /home active'
                launch = f'deskd: profile id={ID} home={home} credentials=unchanged'.encode()
                if not (0 <= serial.find(identity) < serial.find(namespace) < serial.find(launch)):
                    raise RuntimeError('root identity, namespace and profile launch order not proven')
                if outside_data(disk, offset_bytes, size) != bounds:
                    raise RuntimeError('protected disk regions changed')
                payload = data_image(disk, offset_bytes, size)
                extracted = out / f'data-{number}.exfat'
                extracted.write_bytes(payload)
                result = subprocess.run([str(inputs['fs_driver']), str(extracted), 'allocation-check'],
                                        capture_output=True, text=True)
                (out / f'allocation-{number}.log').write_text(result.stdout + result.stderr)
                if result.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in result.stdout:
                    raise RuntimeError('offline allocation check failed')
                fs = ExfatFixture(payload)
                prefix = home.removeprefix('/fat/') + '/'
                marker = fs.read(prefix + 'profile.id')
                binding = profile_bundle.decode_json(fs.read(prefix + 'profile-local.json'))
                if marker != b'srvros-profile-v1\n' + ID.encode() + b'\n' or binding != {'alias': alias, 'uid': uid, 'gid': gid}:
                    raise RuntimeError('profile identity or local binding mismatch')
                document = fs.read(prefix + 'Documents/migration.txt')
                binary = fs.read(prefix + 'Documents/binary.dat')
                portable = fs.read(prefix + profile_bundle.PREF_PATH)
                if document != EDITED or binary != BINARY or profile_bundle.decode_json(portable) != dict(original['preferences'], theme_style=1):
                    raise RuntimeError('portable documents or settings mismatch')
                app_hashes = {}
                for name in ('deskd', 'desktools', 'deskterm'):
                    app = fs.read('bin/' + name)
                    if app != runtime_elf((ROOT / f'build/userspace/{name}.elf').read_bytes()):
                        raise RuntimeError(f'installed {name} differs from built binary')
                    app_hashes[name] = digest(app)
                machine = fs.read('home/desktop.conf')
                if machine != prefs:
                    raise RuntimeError('machine preferences changed')
                env = fs.read(prefix + 'Documents/environment.txt').decode()
                cwd = fs.read(prefix + 'Documents/cwd.txt').decode().strip()
                expected = {'HOME': home, 'SRVROS_DESKTOP_HOME': home, 'SRVROS_PROFILE_ID': ID,
                            'XDG_CONFIG_HOME': home + '/.config', 'XDG_DATA_HOME': home + '/.local/share',
                            'XDG_CACHE_HOME': home + '/.cache'}
                if cwd != home or any(f'{k}={v}' not in env.splitlines() for k, v in expected.items()):
                    raise RuntimeError('terminal environment or cwd mismatch')
                receipt['boots'][number] = {'profile_id': ID, 'home': home, 'local': binding,
                    'document_sha256': digest(document), 'binary_sha256': digest(binary),
                    'portable_preferences_sha256': digest(portable), 'machine_preferences_sha256': digest(machine),
                    'environment_sha256': digest(env.encode()), 'cwd': cwd, 'application_sha256': app_hashes,
                    'data_sha256': digest(payload), 'allocation': result.stdout.strip()}
                save_json(receipt_path, receipt)
                if number == 'a2':
                    host_home = Path(local['home'])
                    for relative, data in (('Documents/migration.txt', document), ('Documents/binary.dat', binary),
                                           (profile_bundle.PREF_PATH, portable)):
                        (host_home / relative).write_bytes(data)
                    exported = profile_bundle.export_bundle(store, ID, ['Documents/migration.txt', 'Documents/binary.dat'])
                    bundle_path = out / 'export-a.json'
                    profile_bundle.publish_bundle(bundle_path, exported)
                    receipt['export_sha256'] = installation_image.digest(bundle_path)
                    save_json(receipt_path, receipt)
            if installation_image.digest(volume) != base_hash or installation_image.digest(inputs['exfat']) != receipt['inputs']['exfat']['sha256']:
                raise RuntimeError('base or build template changed during proof')
            receipt['installations'][label]['final_disk_sha256'] = installation_image.digest(disk)
            save_json(receipt_path, receipt)
        # Every refusal uses private copies and invokes the actual prepare/apply
        # path. No failing case may publish a usable image or ready receipt.
        cases = ('malformed-bundle', 'truncated-bundle', 'partial-binding', 'invalid-alias',
                 'invalid-uid', 'traversal-home', 'changed-bundle', 'changed-plan', 'changed-driver',
                 'occupied-profile-home', 'provision-driver-failure')
        bound_base = out / 'base-a.exfat'
        bound_hash = installation_image.digest(bound_base)
        for case in cases:
            case_dir = out / ('refusal-' + case)
            case_dir.mkdir()
            case_bundle = case_dir / 'bundle.json'
            shutil.copyfile(out / 'bundle-a.json', case_bundle)
            case_driver = case_dir / 'profile-image-driver'
            shutil.copyfile(inputs['driver'], case_driver)
            case_driver.chmod(0o700)
            if case == 'provision-driver-failure':
                case_driver.write_bytes(b'#!/bin/sh\nexit 1\n')
                case_driver.chmod(0o700)
            case_base = out / 'target-a/profile.exfat' if case == 'occupied-profile-home' else bound_base
            case_base_hash = installation_image.digest(case_base)
            if case == 'malformed-bundle':
                case_bundle.write_text('{"format":"invalid"}\n')
            if case == 'truncated-bundle':
                case_bundle.write_bytes(case_bundle.read_bytes()[:40])
            kwargs = {'identity_bound': True, 'profile_bundle': case_bundle,
                      'profile_home': '/fat/home/refusal', 'profile_alias': 'refusal',
                      'profile_uid': 1001, 'profile_gid': 1001, 'profile_driver': case_driver}
            if case == 'occupied-profile-home':
                kwargs['profile_home'] = '/fat/home/local-a'
            if case == 'partial-binding':
                kwargs['profile_gid'] = None
            if case == 'invalid-alias':
                kwargs['profile_alias'] = '../bad'
            if case == 'invalid-uid':
                kwargs['profile_uid'] = -1
            if case == 'traversal-home':
                kwargs['profile_home'] = '/fat/home/../bad'
            target = case_dir / 'stage'
            record = {'case': case, 'input_sha256': {name: installation_image.digest(path)
                       for name, path in inputs.items()}, 'bundle_sha256': installation_image.digest(case_bundle),
                      'driver_sha256': installation_image.digest(case_driver), 'base_path': str(case_base),
                      'base_sha256': case_base_hash}
            receipt['refusals'].append(record)
            save_json(receipt_path, receipt)
            expected_bundle = installation_image.digest(case_bundle)
            expected_driver = installation_image.digest(case_driver)
            prepared = False
            try:
                installation_image.prepare(target, inputs['kernel'], inputs['bootx64'], case_base,
                                           inputs['init'], **kwargs)
                prepared = True
                record['plan_sha256'] = installation_image.digest(target / 'plan.json')
                save_json(receipt_path, receipt)
                if case == 'changed-bundle':
                    with case_bundle.open('ab') as stream:
                        stream.write(b' ')
                elif case == 'changed-plan':
                    with (target / 'plan.json').open('ab') as stream:
                        stream.write(b' ')
                elif case == 'changed-driver':
                    with case_driver.open('ab') as stream:
                        stream.write(b'changed')
                expected_bundle = installation_image.digest(case_bundle)
                expected_driver = installation_image.digest(case_driver)
                installation_image.apply(target)
            except (ValueError, OSError, KeyError, TypeError, subprocess.CalledProcessError) as error:
                record['error'] = f'{type(error).__name__}: {error}'
                if case in ('occupied-profile-home', 'provision-driver-failure') and (
                        not prepared or not isinstance(error, subprocess.CalledProcessError)):
                    raise RuntimeError(f'{case}: did not reach backend provisioning refusal') from error
            else:
                raise RuntimeError(f'{case}: expected refusal did not occur')
            if (target / 'image.img').exists() or (target / 'ready.json').exists():
                raise RuntimeError(f'{case}: refused installation published output')
            if installation_image.digest(case_base) != case_base_hash or installation_image.digest(bound_base) != bound_hash:
                raise RuntimeError(f'{case}: refusal changed bound base')
            if installation_image.digest(case_bundle) != expected_bundle or installation_image.digest(case_driver) != expected_driver:
                raise RuntimeError(f'{case}: refusal changed private bundle or driver')
            if case == 'provision-driver-failure':
                try:
                    installation_image.apply(target)
                except (ValueError, OSError, KeyError) as error:
                    record['retry_error'] = f'{type(error).__name__}: {error}'
                else:
                    raise RuntimeError('provision-driver-failure: retry unexpectedly accepted')
                if ((target / 'image.img').exists() or (target / 'ready.json').exists() or
                    installation_image.digest(case_base) != case_base_hash or
                    installation_image.digest(case_bundle) != expected_bundle or
                    installation_image.digest(case_driver) != expected_driver):
                    raise RuntimeError('provision-driver-failure: retry changed inputs or published output')
            for name, path in inputs.items():
                if installation_image.digest(path) != receipt['inputs'][name]['sha256']:
                    raise RuntimeError(f'{case}: refusal changed input {name}')
            if (target / 'plan.json').is_file():
                record['plan_after_sha256'] = installation_image.digest(target / 'plan.json')
            record.update(status='REFUSED', base_after_sha256=installation_image.digest(case_base),
                          bundle_after_sha256=installation_image.digest(case_bundle),
                          driver_after_sha256=installation_image.digest(case_driver),
                          published_image=False, published_ready=False)
            save_json(receipt_path, receipt)
        receipt.update(status='PASS', limits=['disposable QEMU images only',
            'no authentication or access-control claim', 'no physical disk or power-loss acceptance',
            'external fsck.exfat not run'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        save_json(receipt_path, receipt)
    print(f'profile-installation: PASS; evidence {out}', flush=True)


if __name__ == '__main__':
    main()
