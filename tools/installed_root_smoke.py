#!/usr/bin/env python3
"""Identity-bound installed boot with two disposable GPT disks and no-write recovery."""
import argparse
import binascii
import hashlib
import json
from pathlib import Path
import platform
import shutil
import struct
import subprocess
import tarfile
import tempfile
import uuid

import installation_image as provision
import installed_disk_smoke as proof
from exfat_test_image import ExfatFixture
from uefi_usb_smoke import default_ovmf_path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return provision.digest(path)


def clone(source, destination):
    if destination.exists():
        raise RuntimeError('fixture destination already exists')
    if platform.system() == 'Darwin':
        # APFS clones keep the retained refusal cases affordable in disk space.
        subprocess.run(['cp', '-c', str(source), str(destination)], check=True)
    else:
        shutil.copyfile(source, destination)


def edit_gpt(path, change):
    """Mutate both GPT copies of an owned negative fixture, keeping valid CRCs."""
    with path.open('r+b') as stream:
        total = path.stat().st_size // 512
        for lba in (1, total - 1):
            stream.seek(lba * 512)
            header = bytearray(stream.read(512))
            table_lba, count, size = struct.unpack_from('<QII', header, 72)
            if header[:8] != b'EFI PART' or count != 128 or size != 128:
                raise RuntimeError('unexpected generated GPT')
            stream.seek(table_lba * 512)
            entries = bytearray(stream.read(count * size))
            change(entries)
            struct.pack_into('<I', header, 88, binascii.crc32(entries) & 0xffffffff)
            struct.pack_into('<I', header, 16, 0)
            struct.pack_into('<I', header, 16, binascii.crc32(header[:92]) & 0xffffffff)
            stream.seek(table_lba * 512); stream.write(entries)
            stream.seek(lba * 512); stream.write(header)


def set_part(entries, offset, value):
    entries[128 + offset:128 + offset + len(value)] = value


def verify_saved(disk, out, number):
    _, (offset, size) = proof.partitions(disk)
    data = proof.data_image(disk, offset, size)
    fs = ExfatFixture(data)
    if fs.read('home/installed-proof.txt') != proof.PHRASE:
        raise RuntimeError('document not preserved across identity-selected boots')
    prefs = fs.read('home/desktop.conf')
    if b'theme_style=1\n' not in prefs:
        raise RuntimeError('theme preference not preserved')
    extracted = out / f'data-{number}.exfat'; extracted.write_bytes(data)
    check = subprocess.run([str(ROOT / 'build/tests/exfat-driver'), str(extracted),
                            'allocation-check'], capture_output=True, text=True)
    (out / f'allocation-{number}.log').write_text(check.stdout + check.stderr)
    if check.returncode or 'leaked=0 fat_bitmap_mismatches=0 errors=0' not in check.stdout:
        raise RuntimeError('offline allocation check failed')
    return prefs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    for filename in (args.ovmf, args.ovmf_vars):
        if not Path(filename).is_file() or ',' in filename:
            parser.error('firmware templates must be regular files with no comma in path')
    if not 10 <= args.boot_wait <= 240:
        parser.error('--boot-wait must be 10..240')
    build = proof.verified_build(ROOT)
    with tarfile.open(ROOT / 'build/initramfs.tar') as archive:
        member = next(m for m in archive.getmembers() if m.name.lstrip('./') == 'init')
        if archive.extractfile(member).read() != proof.runtime_elf((ROOT / 'build/userspace/init.elf').read_bytes()):
            raise RuntimeError('Bootstrap init differs from matched build archive')
    base = ROOT / 'build/review/installed-root'; base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'Identity-bound evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'source': build['source'], 'build': build,
               'firmware_sha256': sha(Path(args.ovmf)),
               'firmware_vars_template_sha256': sha(Path(args.ovmf_vars)),
               'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0],
               'cases': [], 'scope': 'two disposable AHCI GPT disks; recovery halt, no physical installation'}
    receipt['auxiliary_sha256'] = {name: sha(ROOT / name) for name in
        ('build/limine-binary/BOOTX64.EFI', 'build/userspace/init.elf', 'build/tests/exfat-driver')}
    proof.save_json(out / 'receipt.json', receipt)
    try:
        images = []
        for name in ('target', 'decoy'):
            stage = out / name
            provision.prepare(stage, ROOT / 'build/kernel/srvros.elf',
                ROOT / 'build/limine-binary/BOOTX64.EFI', ROOT / 'build/srvros.exfat',
                ROOT / 'build/userspace/init.elf', identity_bound=True)
            ready = provision.apply(stage)
            with tarfile.open(stage / 'bootstrap.tar') as archive:
                if archive.getnames() != ['init']:
                    raise RuntimeError('Installation bootstrap contains more than /init')
            images.append(stage / 'image.img')
            receipt[name] = ready
        target, decoy = images
        _, (offset, size) = proof.partitions(target)
        fixture = ExfatFixture(proof.data_image(target, offset, size))
        for binary in ('deskd', 'desktools'):
            expected = proof.runtime_elf((ROOT / f'build/userspace/{binary}.elf').read_bytes())
            if fixture.read('bin/' + binary) != expected:
                raise RuntimeError(f'Installed {binary} differs from built application')
        receipt['bootstrap_members'] = ['init']
        protected = proof.outside_data(target, offset, size)
        decoy_hash = sha(decoy)
        old_prefs = None
        for number, reverse, selected in ((1, False, 'ahci0p2'), (2, True, 'ahci1p2')):
            proof.boot(target, out, number, args, other_disk=decoy, reverse=reverse,
                       expected_device=selected, require_identity=True)
            prefs = verify_saved(target, out, number)
            if old_prefs is not None and prefs != old_prefs:
                raise RuntimeError('preference bytes changed on reordered reboot')
            old_prefs = prefs
            if sha(decoy) != decoy_hash or proof.outside_data(target, offset, size) != protected:
                raise RuntimeError('nonselected disk or protected target regions changed')
            receipt['cases'].append({'name': f'boot-{number}', 'selected': selected,
                'reversed': reverse, 'decoy_sha256': decoy_hash, 'status': 'PASS'})
            proof.save_json(out / 'receipt.json', receipt)
        guid = uuid.UUID(receipt['target']['gpt']['exfat_partition_guid']).bytes_le
        for name, reason in (('missing-volume', 'missing'), ('wrong-id', 'missing'),
                             ('duplicate-id', 'ambiguous'), ('wrong-role', 'role'),
                             ('wrong-type', 'role'), ('wrong-format', 'format'),
                             ('invalid-decoy-map', 'invalid-map'),
                             ('filesystem-recovery', 'filesystem-recovery'),
                             ('pending-metadata', 'filesystem-recovery')):
            directory = out / name; directory.mkdir()
            candidate, other = directory / 'target.img', directory / 'decoy.img'
            clone(target, candidate); clone(decoy, other)
            if name == 'missing-volume':
                edit_gpt(candidate, lambda entries: set_part(entries, 0, bytes(128)))
            elif name == 'wrong-id':
                other_id = uuid.uuid4().bytes_le
                edit_gpt(candidate, lambda entries: set_part(entries, 16, other_id))
            elif name == 'duplicate-id':
                edit_gpt(other, lambda entries: set_part(entries, 16, guid))
            elif name == 'wrong-role':
                edit_gpt(candidate, lambda entries: set_part(entries, 56, 'srvros-data-v1'.encode('utf-16le').ljust(72, b'\0')))
            elif name == 'wrong-type':
                edit_gpt(candidate, lambda entries: set_part(entries, 0, provision.usb.ESP_TYPE.bytes_le))
            elif name == 'wrong-format':
                with candidate.open('r+b') as stream:
                    for sector in (0, 12):
                        stream.seek(offset + sector * 512 + 3); stream.write(b'UNKNOWN ')
            elif name == 'invalid-decoy-map':
                with other.open('r+b') as stream:
                    # OVMF repairs a bad primary from a valid backup before
                    # kernel entry. Damage both to reach kernel refusal without
                    # a firmware repair write; retain this distinction in proof.
                    for lba in (1, other.stat().st_size // 512 - 1):
                        stream.seek(lba * 512 + 16); stream.write(bytes(4))
            elif name == 'filesystem-recovery':
                with candidate.open('r+b') as stream:
                    stream.seek(offset + 12 * 512); stream.write(bytes(512))
            elif name == 'pending-metadata':
                extracted = directory / 'pending.exfat'
                extracted.write_bytes(proof.data_image(candidate, offset, size))
                seed = subprocess.run([str(ROOT / 'build/tests/exfat-driver'),
                                       str(extracted), 'seed-temp'],
                                      capture_output=True, text=True)
                (directory / 'seed.log').write_text(seed.stdout + seed.stderr)
                seed.check_returncode()
                seeded = extracted.read_bytes()
                if not ExfatFixture(seeded).read('.srvros/meta.tmp').startswith(b'SRVROSMETA1\n'):
                    raise RuntimeError('pending metadata fixture was not seeded')
                with candidate.open('r+b') as stream:
                    stream.seek(offset); stream.write(seeded)
            before = {'target': sha(candidate), 'decoy': sha(other)}
            proof.boot(candidate, directory, name, args, other_disk=other, reverse=True,
                       recovery_reason=reason)
            after = {'target': sha(candidate), 'decoy': sha(other)}
            if before != after:
                raise RuntimeError(f'{name}: recovery wrote to a candidate disk')
            receipt['cases'].append({'name': name, 'reason': reason, 'before': before,
                                     'after': after, 'status': 'PASS'})
            proof.save_json(out / 'receipt.json', receipt)
        receipt.update(status='PASS', final_target_sha256=sha(target), decoy_sha256=decoy_hash,
                       document_sha256=hashlib.sha256(proof.PHRASE).hexdigest(),
                       preferences_sha256=hashlib.sha256(old_prefs).hexdigest(),
                       protected_regions=protected, limits=[
                           'missing system volume retains a bootable ESP; removal of sole boot disk is firmware failure',
                           'all attached raw maps must validate; unrelated unknown media conservatively refuses',
                           'GUIDs are identifiers, not authentication',
                           'primary GPT validation only; no disk repair or profile activation',
                           'invalid-map VM fixture damages both GPT copies to prevent OVMF primary-map repair',
                           'not physical power-loss or arbitrary mount-I/O-failure qualification'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        proof.save_json(out / 'receipt.json', receipt)
    print(f'installed-root: PASS {len(receipt["cases"])} cases; evidence {out}', flush=True)


if __name__ == '__main__':
    main()
