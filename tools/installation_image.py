#!/usr/bin/env python3
"""Build a disposable installed-disk fixture, never a physical-device installer.

prepare creates a new private directory; apply consumes its plan exactly once.
Publication recovery requires an offline, controlled, single-writer staging directory.
The publication record is an integrity record, not authentication.
"""
import argparse
import binascii
import fcntl
import hashlib
import io
import json
import mmap
import os
from pathlib import Path
from contextlib import contextmanager
import stat
import subprocess
import struct
import tarfile
import uuid
from types import SimpleNamespace

import mk_usb_image as usb
import profile_bundle as profiles
import profile_image

ESP_MIB = 384
CONFIG = (b'timeout: 0\nserial: yes\n/srvros\n    protocol: limine\n'
          b'    path: boot():/boot/srvros.elf\n'
          b'    resolution: 1280x800\n'
          b'    cmdline: srvros.virtio=on\n'
          b'    module_path: boot():/boot/initramfs.tar\n'
          b'    module_string: initramfs\n')


def checked_path(path):
    path = Path(os.path.abspath(path))
    for component in (path, *path.parents):
        if component.is_symlink():
            raise ValueError(f'symlink path refused: {component}')
    return path


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def source(path):
    path = checked_path(path)
    st = path.stat()
    if not stat.S_ISREG(st.st_mode) or not st.st_size:
        raise ValueError(f'not a nonempty regular source: {path}')
    result = {'path': str(path), 'device': st.st_dev, 'inode': st.st_ino,
              'size': st.st_size, 'sha256': digest(path)}
    after = path.stat()
    if (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns) != (st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns, st.st_ctime_ns):
        raise ValueError(f'source changed: {path}')
    return result


def validate_exfat(path, size):
    with open(path, 'rb') as f:
        region = f.read(24 * 512)
    if len(region) != 24 * 512 or region[3:11] != b'EXFAT   ' or region[510:512] != b'\x55\xaa':
        raise ValueError('invalid exFAT boot region')
    length = struct.unpack_from('<Q', region, 72)[0]
    fat, fat_len, heap, clusters, root = struct.unpack_from('<IIIII', region, 80)
    shift, cluster_shift, fats = region[108:111]
    if (shift != 9 or cluster_shift > 16 or fats != 1 or length * 512 != size or
        fat < 24 or fat_len == 0 or fat + fat_len > heap or clusters == 0 or
        heap + (clusters << cluster_shift) > length or root < 2 or root >= clusters + 2 or
        fat_len * 512 < (clusters + 2) * 4):
        raise ValueError('invalid exFAT geometry')
    checksum = 0
    for i, b in enumerate(region[:11 * 512]):
        if i not in (106, 107, 112):
            checksum = (((checksum >> 1) | ((checksum & 1) << 31)) + b) & 0xffffffff
    if region[11*512:12*512] != struct.pack('<I', checksum) * 128 or region[:12*512] != region[12*512:]:
        raise ValueError('invalid exFAT boot checksum or backup')


def layout(exfat_size):
    if exfat_size <= 0 or exfat_size % 512:
        raise ValueError('invalid exFAT size')
    esp_first, esp_sectors = 2048, ESP_MIB * 1024 * 1024 // 512
    data_first = usb.align_up(esp_first + esp_sectors, 2048)
    data_sectors = exfat_size // 512
    backup = usb.align_up(data_first + data_sectors - 1 + 2048, 2048) - 1
    if backup > 0xffffffff or data_first + data_sectors - 1 > backup - 33:
        raise ValueError('partition outside supported disk bounds')
    return {'size_bytes': (backup + 1) * 512,
            'esp': {'offset_bytes': esp_first * 512, 'size_bytes': esp_sectors * 512},
            'exfat': {'offset_bytes': data_first * 512, 'size_bytes': exfat_size}}


def write_json(path, obj):
    payload = json.dumps(obj, indent=2, sort_keys=True) + '\n'
    with open(path, 'x') as f:
        if f.write(payload) != len(payload):
            raise OSError('short metadata write')
        f.flush()
        os.fsync(f.fileno())


def sync_dir(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def identity_config(partition_guid):
    return CONFIG.replace(b"    cmdline: srvros.virtio=on\n",
        f"    cmdline: srvros.virtio=on srvros.root={partition_guid} srvros.root-format=exfat-v1 srvros.root-role=system\n".encode("ascii"))


def load_profile(path, home, alias, uid, gid):
    bundle = profiles.decode_json(profiles.read_regular(path))
    metadata, _, _ = profile_image.validate_request(bundle, home, alias, uid, gid)
    return bundle, metadata


def prepare(staging_dir, kernel, bootx64, exfat, init, *, identity_bound=False,
            profile_bundle=None, profile_home=None, profile_alias=None, profile_uid=None,
            profile_gid=None, profile_driver=None):
    target = checked_path(staging_dir)
    options = (profile_bundle, profile_home, profile_alias, profile_uid, profile_gid, profile_driver)
    if any(value is not None for value in options) and (not identity_bound or any(value is None for value in options)):
        raise ValueError('profile requires identity-bound mode and all profile options')
    profile = None
    if profile_bundle is not None:
        _, profile = load_profile(profile_bundle, profile_home, profile_alias, profile_uid, profile_gid)
        if Path(checked_path(exfat)).stat().st_nlink != 1:
            raise ValueError('profile source exFAT must have a single link')
    sources = {name: source(value) for name, value in
               [('kernel', kernel), ('bootx64', bootx64), ('exfat', exfat), ('init', init)]}
    if profile is not None:
        sources['profile_bundle'] = source(profile_bundle)
        sources['profile_driver'] = source(profile_driver)
        if not os.access(sources['profile_driver']['path'], os.X_OK):
            raise ValueError('profile driver is not executable')
        _, current = load_profile(sources['profile_bundle']['path'], profile_home, profile_alias, profile_uid, profile_gid)
        if current != profile or source(sources['profile_bundle']['path']) != sources['profile_bundle']:
            raise ValueError('profile bundle changed during preparation')
    validate_exfat(sources['exfat']['path'], sources['exfat']['size'])
    for name in ('kernel', 'init'):
        with open(sources[name]['path'], 'rb') as f:
            if f.read(4) != b'\x7fELF':
                raise ValueError(f'{name} is not ELF')
    with open(sources['bootx64']['path'], 'rb') as f:
        if f.read(2) != b'MZ':
            raise ValueError('bootx64 is not a PE executable')
    spec = layout(sources['exfat']['size'])
    os.mkdir(target, 0o700)  # exclusive: existing files, directories and devices are refused
    st = target.stat()
    plan = {'version': 2 if profile is not None else 1, 'target_uuid': str(uuid.uuid4()), 'target_directory': str(target),
            'target_device': st.st_dev, 'target_inode': st.st_ino,
            'sources': sources, 'layout': spec}
    if profile is not None:
        plan['profile'] = profile
    if identity_bound:
        gpt = {'disk_guid': str(uuid.uuid4()), 'esp_partition_guid': str(uuid.uuid4()),
               'exfat_partition_guid': str(uuid.uuid4()), 'exfat_partition_name': 'srvros-system-v1'}
        plan['selection'] = {'mode': 'identity-bound', 'format': 'exfat-v1', 'role': 'system'}
        plan['gpt'] = gpt
        plan['config'] = identity_config(gpt['exfat_partition_guid']).decode('ascii')
    write_json(target / 'plan.json', plan)
    with open(target / 'plan.sha256', 'x') as f:
        f.write(digest(target / 'plan.json') + '\n')
        f.flush()
        os.fsync(f.fileno())
    sync_dir(target)
    sync_dir(target.parent)
    return plan



def verify_image(path, plan, bootstrap):
    """Read back both GPT copies and every boot payload from the finished disk."""
    spec = plan['layout']
    if path.stat().st_size != spec['size_bytes']:
        raise ValueError('built image size mismatch')
    total = spec['size_bytes'] // 512
    with open(path, 'rb') as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as disk:
        if disk[510:512] != b'\x55\xaa' or disk[450] != 0xee:
            raise ValueError('invalid protective MBR')
        def header(lba, backup, entries_lba):
            raw = bytearray(disk[lba * 512:(lba + 1) * 512])
            if raw[:8] != b'EFI PART' or struct.unpack_from('<II', raw, 8) != (0x10000, 92):
                raise ValueError('invalid GPT header')
            stored = struct.unpack_from('<I', raw, 16)[0]
            struct.pack_into('<I', raw, 16, 0)
            if binascii.crc32(raw[:92]) & 0xffffffff != stored:
                raise ValueError('invalid GPT header checksum')
            current, other, first, last = struct.unpack_from('<QQQQ', raw, 24)
            entries_at, count, size, crc = struct.unpack_from('<QIII', raw, 72)
            if (current, other, first, last, entries_at, count, size) != (lba, backup, 34, total - 34, entries_lba, 128, 128):
                raise ValueError('invalid GPT header bounds')
            entries = disk[entries_at * 512:entries_at * 512 + count * size]
            if binascii.crc32(entries) & 0xffffffff != crc:
                raise ValueError('invalid GPT entries checksum')
            return bytes(raw[56:72]), entries
        disk_guid, entries = header(1, total - 1, 2)
        backup_guid, backup_entries = header(total - 1, 1, total - 33)
        if disk_guid != backup_guid or entries != backup_entries:
            raise ValueError('GPT copies differ')
        expected_gpt = plan.get('gpt')
        guids = []
        for index, (kind, key) in enumerate(((usb.ESP_TYPE, 'esp'), (usb.BASIC_DATA_TYPE, 'exfat'))):
            entry = entries[index * 128:(index + 1) * 128]
            part_type, part_id, first, last = struct.unpack_from('<16s16sQQ', entry)
            expected_first = spec[key]['offset_bytes'] // 512
            expected_last = expected_first + spec[key]['size_bytes'] // 512 - 1
            if part_type != kind.bytes_le or (first, last) != (expected_first, expected_last) or not any(part_id):
                raise ValueError('GPT partition does not match plan')
            guid = str(uuid.UUID(bytes_le=part_id))
            name = entry[56:128].decode('utf-16le').rstrip('\x00')
            expected_name = 'srvros-esp' if key == 'esp' else (expected_gpt['exfat_partition_name'] if expected_gpt else 'srvros-fat')
            if name != expected_name or (expected_gpt and guid != expected_gpt[key + '_partition_guid']):
                raise ValueError('GPT partition identity does not match plan')
            guids.append(guid)
        if expected_gpt and str(uuid.UUID(bytes_le=disk_guid)) != expected_gpt['disk_guid']:
            raise ValueError('GPT disk identity does not match plan')
        if any(entries[256:]) or len(set(guids + [str(uuid.UUID(bytes_le=disk_guid))])) != 3:
            raise ValueError('unexpected or duplicate GPT entries')
        fat = usb.Fat32Builder(spec['esp']['size_bytes'], spec['esp']['offset_bytes'] // 512)
        base = spec['esp']['offset_bytes']
        # The existing FAT readers use byte slices; this view avoids loading the
        # entire 384 MiB ESP into memory while keeping their interface intact.
        class EspView:
            def __getitem__(self, key):
                if not isinstance(key, slice) or key.step not in (None, 1):
                    raise ValueError('invalid ESP read')
                start, stop, _ = key.indices(spec['esp']['size_bytes'])
                return disk[base + start:base + stop]
        esp = EspView()
        if esp[:512] != fat.boot_sector():
            raise ValueError('invalid ESP boot sector')
        fat_offset = fat.reserved_sectors * 512
        fat_bytes = fat.fat_sectors * 512
        if esp[fat_offset:fat_offset + fat_bytes] != esp[fat_offset + fat_bytes:fat_offset + 2 * fat_bytes]:
            raise ValueError('FAT copies differ')
        fat.fat = [value[0] for value in struct.iter_unpack('<I', esp[fat_offset:fat_offset + fat_bytes])]
        usb.validate_fat32_image(esp, fat, expected_config=plan.get('config', CONFIG.decode('ascii')).encode('ascii'))
        for filename, expected in (('EFI/BOOT/BOOTX64.EFI', plan['sources']['bootx64']['sha256']),
                                   ('BOOT/SRVROS.ELF', plan['sources']['kernel']['sha256']),
                                   ('BOOT/INITRAMFS.TAR', hashlib.sha256(bootstrap).hexdigest())):
            cluster = fat.root_cluster
            for part in filename.split('/'):
                entry = usb.fat32_dir_entries(esp, fat, cluster)[part]
                cluster = entry['cluster']
            data = usb.fat32_file_data(esp, fat, cluster, entry['size'])
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError(f'ESP payload mismatch: {filename}')
    return {'disk_guid': str(uuid.UUID(bytes_le=disk_guid)),
            'esp_partition_guid': guids[0], 'exfat_partition_guid': guids[1]}


@contextmanager
def plan_lock(target):
    # Cooperative lock on the stable plan inode; not a hostile-process defense.
    path = checked_path(target / 'plan.json')
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_nlink != 1:
            raise ValueError('invalid plan file')
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield
    finally:
        os.close(fd)


def validate_plan(target):
    st = target.stat()
    if (not stat.S_ISDIR(st.st_mode) or st.st_uid != os.geteuid() or
        stat.S_IMODE(st.st_mode) & 0o077):
        raise ValueError('staging directory must be private and owned by current user')
    plan_path = checked_path(target / 'plan.json')
    if not stat.S_ISREG(plan_path.stat().st_mode) or plan_path.stat().st_nlink != 1 or plan_path.stat().st_size > 1024 * 1024:
        raise ValueError('invalid plan')
    checksum_path = checked_path(target / 'plan.sha256')
    if not stat.S_ISREG(checksum_path.stat().st_mode) or checksum_path.stat().st_nlink != 1 or checksum_path.stat().st_size > 128:
        raise ValueError('invalid plan checksum')
    with open(checksum_path) as f:
        checksum = f.read()
    if checksum != digest(plan_path) + '\n':
        raise ValueError('plan changed since prepare')
    with open(plan_path) as f:
        plan = json.load(f)
    if (type(plan['version']) is not int or plan['version'] not in (1, 2) or plan['target_directory'] != str(target) or
        (st.st_dev, st.st_ino) != (plan['target_device'], plan['target_inode']) or
        plan['layout'] != layout(plan['sources']['exfat']['size'])):
        raise ValueError('target or plan identity mismatch')
    uuid.UUID(plan['target_uuid'])
    selection = plan.get('selection')
    if selection is not None:
        if selection != {'mode': 'identity-bound', 'format': 'exfat-v1', 'role': 'system'}:
            raise ValueError('invalid root selection')
        gpt = plan['gpt']
        identifiers = [gpt[k] for k in ('disk_guid', 'esp_partition_guid', 'exfat_partition_guid')]
        if (any(str(uuid.UUID(value)) != value or uuid.UUID(value).int == 0 for value in identifiers) or
            len(set(identifiers)) != 3 or gpt['exfat_partition_name'] != 'srvros-system-v1' or
            plan['config'] != identity_config(gpt['exfat_partition_guid']).decode('ascii')):
            raise ValueError('invalid identity-bound plan')
    elif 'gpt' in plan or 'config' in plan:
        raise ValueError('unexpected root selection fields')
    for item in plan['sources'].values():
        if source(item['path']) != item:
            raise ValueError('source changed since prepare')
    profile = plan.get('profile')
    if (plan['version'] == 2) != (profile is not None):
        raise ValueError('profile plan requires version 2')
    bundle = None
    if profile is not None:
        if not selection or set(profile) != {'profile_id', 'home', 'alias', 'uid', 'gid'}:
            raise ValueError('invalid profile plan')
        bundle, current = load_profile(plan['sources']['profile_bundle']['path'], profile['home'],
                                       profile['alias'], profile['uid'], profile['gid'])
        if current != profile or not os.access(plan['sources']['profile_driver']['path'], os.X_OK):
            raise ValueError('profile plan or driver changed')
    elif 'profile_bundle' in plan['sources'] or 'profile_driver' in plan['sources']:
        raise ValueError('unexpected profile sources')
    validate_exfat(plan['sources']['exfat']['path'], plan['sources']['exfat']['size'])
    return plan, selection, profile, bundle


def bootstrap_bytes(init_data):
    data = io.BytesIO()
    with tarfile.open(fileobj=data, mode='w', format=tarfile.USTAR_FORMAT) as tar:
        entry = tarfile.TarInfo('init')
        entry.mode, entry.size = 0o755, len(init_data)
        tar.addfile(entry, io.BytesIO(init_data))
    return data.getvalue()


def ready_record(target, plan, gpt, image_hash, data_hash):
    ready = {'version': 1, 'gpt': gpt, 'target_uuid': plan['target_uuid'],
             'image': str(target / 'image.img'), 'image_sha256': image_hash,
             'layout': plan['layout'], 'sources': plan['sources']}
    profile, selection = plan.get('profile'), plan.get('selection')
    if profile is not None:
        ready['profile'] = dict(profile, launch_command='run /fat/bin/deskd --profile=' + profile['home'])
        ready['profile_exfat_sha256'] = data_hash
    if selection:
        ready['selection'] = dict(selection, partition_guid=plan['gpt']['exfat_partition_guid'],
                                  partition_name=plan['gpt']['exfat_partition_name'])
    return ready


def data_digest(path, spec):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        f.seek(spec['exfat']['offset_bytes'])
        remaining = spec['exfat']['size_bytes']
        while remaining:
            chunk = f.read(min(1024 * 1024, remaining))
            if not chunk:
                raise ValueError('truncated image')
            h.update(chunk)
            remaining -= len(chunk)
    return h.hexdigest()


def apply(staging_dir, checkpoint=None):
    checkpoint = checkpoint or (lambda name: None)
    target = checked_path(staging_dir)
    with plan_lock(target):
        return _apply(target, checkpoint)


def _apply(target, checkpoint):
    if set(os.listdir(target)) != {'plan.json', 'plan.sha256'}:
        raise ValueError('unknown, already used, or interrupted staging target')
    plan, selection, profile, bundle = validate_plan(target)
    write_json(target / 'in-progress.json', {'target_uuid': plan['target_uuid']})
    sync_dir(target)
    # Private temporary files are retained on failure, preventing ambiguous reuse.
    init_data = Path(plan['sources']['init']['path']).read_bytes()
    bootstrap = bootstrap_bytes(init_data)
    with open(target / 'bootstrap.tar', 'xb') as stream:
        if stream.write(bootstrap) != len(bootstrap):
            raise OSError('short bootstrap write')
        stream.flush()
        os.fsync(stream.fileno())
    config = plan.get('config', CONFIG.decode('ascii')).encode('ascii')
    with open(target / 'limine.conf', 'xb') as stream:
        if stream.write(config) != len(config):
            raise OSError('short configuration write')
        stream.flush()
        os.fsync(stream.fileno())
    sync_dir(target)
    exfat_path = plan['sources']['exfat']['path']
    checkpoint('provisioning-started')
    if profile is not None:
        provisioned = target / 'profile.exfat'
        result = profile_image.provision(exfat_path, provisioned, bundle, profile['home'],
            profile['alias'], profile['uid'], profile['gid'], plan['sources']['profile_driver']['path'])
        if (result['profile_id'] != profile['profile_id'] or result['home'] != profile['home'] or
            any(result[key] != profile[key] for key in ('alias', 'uid', 'gid'))):
            raise ValueError('profile provision result mismatch')
        if provisioned.stat().st_size != plan['sources']['exfat']['size']:
            raise ValueError('profile exFAT size changed')
        validate_exfat(provisioned, plan['sources']['exfat']['size'])
        with open(provisioned, 'rb') as stream:
            os.fsync(stream.fileno())
        exfat_path = str(provisioned)
    checkpoint('provisioned')
    exfat_hash = digest(exfat_path) if profile is not None else plan['sources']['exfat']['sha256']
    temp = target / 'image.partial'
    usb.build_image(SimpleNamespace(esp_size_mib=ESP_MIB, exfat=exfat_path,
        bootx64=plan['sources']['bootx64']['path'], bootia32=None,
        kernel=plan['sources']['kernel']['path'], initramfs=str(target / 'bootstrap.tar'),
        config=str(target / 'limine.conf'), output=str(temp),
        disk_guid=plan['gpt']['disk_guid'] if selection else None,
        esp_guid=plan['gpt']['esp_partition_guid'] if selection else None,
        data_guid=plan['gpt']['exfat_partition_guid'] if selection else None,
        data_name=plan['gpt']['exfat_partition_name'] if selection else None))
    with open(temp, 'rb') as f:
        os.fsync(f.fileno())
    sync_dir(target)
    checkpoint('assembled')
    gpt = verify_image(temp, plan, (target / 'bootstrap.tar').read_bytes())
    for item in plan['sources'].values():
        if source(item['path']) != item:
            raise ValueError('source changed during build')
    if data_digest(temp, plan['layout']) != exfat_hash or digest(exfat_path) != exfat_hash:
        raise ValueError('image exFAT verification failed')
    image_hash = digest(temp)
    ready = ready_record(target, plan, gpt, image_hash, exfat_hash)
    candidate = temp.stat()
    journal = {'version': 1, 'target_uuid': plan['target_uuid'],
               'target_device': plan['target_device'], 'target_inode': plan['target_inode'],
               'plan_sha256': digest(target / 'plan.json'), 'sources': plan['sources'],
               'candidate': {'device': candidate.st_dev, 'inode': candidate.st_ino,
                             'size': candidate.st_size, 'sha256': image_hash},
               'data_sha256': exfat_hash, 'ready': ready}
    write_json(target / 'publication.json', journal)
    sync_dir(target)
    checkpoint('verified')
    return publish(target, ready, checkpoint)


def regular(path, links=1):
    st = path.lstat()
    if not stat.S_ISREG(st.st_mode) or st.st_nlink != links:
        raise ValueError(f'invalid file or unexpected hard links: {path.name}')
    return st


def inspect(staging_dir):
    """Read-only diagnosis; journal is integrity metadata, not authentication."""
    try:
        target = checked_path(staging_dir)
        return _inspect(target)
    except (OSError, ValueError, KeyError, TypeError, IndexError, RuntimeError, UnicodeError) as error:
        return {'state': 'refused', 'can_reconcile': False, 'reason': str(error)}


def _inspect(target):
    names = set(os.listdir(target))
    allowed = {'plan.json', 'plan.sha256', 'in-progress.json', 'bootstrap.tar', 'limine.conf',
               'profile.exfat', 'image.partial', 'image.img', 'publication.json',
               'ready.partial', 'ready.json'}
    if names - allowed:
        raise ValueError('unknown staging entries')
    # All fixed names are regular files, with only two known publication alias pairs.
    for name in names - {'image.partial', 'image.img', 'ready.partial', 'ready.json'}:
        info = regular(target / name)
        if name in {'in-progress.json', 'publication.json'} and info.st_size > 1024 * 1024:
            raise ValueError('metadata exceeds size limit')
    for pair in (('image.partial', 'image.img'), ('ready.partial', 'ready.json')):
        paths = [target / name for name in pair if name in names]
        stats = [regular(path, len(paths)) for path in paths]
        if len(stats) == 2 and (stats[0].st_dev, stats[0].st_ino) != (stats[1].st_dev, stats[1].st_ino):
            raise ValueError('publication aliases differ')
        if pair[0] == 'ready.partial' and any(info.st_size > 1024 * 1024 for info in stats):
            raise ValueError('ready metadata exceeds size limit')
    plan, selection, profile, _ = validate_plan(target)
    if names == {'plan.json', 'plan.sha256'}:
        return {'state': 'prepared', 'can_reconcile': False, 'reason': 'apply has not started'}
    if 'publication.json' not in names:
        return {'state': 'incomplete', 'can_reconcile': False, 'reason': 'no verified publication record'}
    required = {'plan.json', 'plan.sha256', 'in-progress.json', 'bootstrap.tar', 'limine.conf', 'publication.json'}
    if profile is not None:
        required.add('profile.exfat')
    elif 'profile.exfat' in names:
        raise ValueError('unexpected profile image')
    if not required <= names:
        raise ValueError('publication files missing')
    with open(target / 'in-progress.json') as stream:
        if json.load(stream) != {'target_uuid': plan['target_uuid']}:
            raise ValueError('invalid in-progress record')
    with open(target / 'publication.json') as stream:
        journal = json.load(stream)
    if (set(journal) != {'version', 'target_uuid', 'target_device', 'target_inode', 'plan_sha256',
                         'sources', 'candidate', 'data_sha256', 'ready'} or type(journal['version']) is not int or
        journal['version'] != 1 or set(journal['candidate']) != {'device', 'inode', 'size', 'sha256'} or
        any(type(journal['candidate'][key]) is not int for key in ('device', 'inode', 'size')) or
        journal['target_uuid'] != plan['target_uuid'] or journal['target_device'] != plan['target_device'] or
        journal['target_inode'] != plan['target_inode'] or journal['plan_sha256'] != digest(target / 'plan.json') or
        journal['sources'] != plan['sources']):
        raise ValueError('publication record does not match plan')
    images = [target / n for n in ('image.partial', 'image.img') if n in names]
    if not images:
        raise ValueError('verified image missing')
    for path in images:
        st = regular(path, len(images))
        if (st.st_dev, st.st_ino, st.st_size) != (journal['candidate']['device'],
            journal['candidate']['inode'], journal['candidate']['size']):
            raise ValueError('image identity mismatch')
    if len(images) == 2 and images[0].stat().st_ino != images[1].stat().st_ino:
        raise ValueError('image aliases differ')
    image = images[0]
    if digest(image) != journal['candidate']['sha256']:
        raise ValueError('verified image content changed')
    expected_bootstrap = bootstrap_bytes(Path(plan['sources']['init']['path']).read_bytes())
    if ((target / 'bootstrap.tar').stat().st_size != len(expected_bootstrap) or
        (target / 'bootstrap.tar').read_bytes() != expected_bootstrap):
        raise ValueError('bootstrap changed')
    expected_config = plan.get('config', CONFIG.decode('ascii')).encode('ascii')
    if ((target / 'limine.conf').stat().st_size != len(expected_config) or
        (target / 'limine.conf').read_bytes() != expected_config):
        raise ValueError('configuration changed')
    if profile is not None:
        data_path = target / 'profile.exfat'
        validate_exfat(data_path, plan['sources']['exfat']['size'])
        if data_path.stat().st_size != plan['sources']['exfat']['size']:
            raise ValueError('profile image size changed')
    else:
        data_path = Path(plan['sources']['exfat']['path'])
    if digest(data_path) != journal['data_sha256'] or data_digest(image, plan['layout']) != journal['data_sha256']:
        raise ValueError('partition content changed')
    gpt = verify_image(image, plan, expected_bootstrap)
    expected = ready_record(target, plan, gpt, journal['candidate']['sha256'], journal['data_sha256'])
    if journal['ready'] != expected:
        raise ValueError('publication ready record mismatch')
    ready_paths = [target / n for n in ('ready.partial', 'ready.json') if n in names]
    if ready_paths and 'image.img' not in names:
        raise ValueError('ready marker without published image')
    expected_ready_bytes = (json.dumps(expected, indent=2, sort_keys=True) + '\n').encode('utf-8')
    for path in ready_paths:
        # Two names may coexist only as the very same publication hardlink.
        regular(path, len(ready_paths))
        if path.read_bytes() != expected_ready_bytes:
            raise ValueError('ready marker mismatch')
    if len(ready_paths) == 2 and ready_paths[0].stat().st_ino != ready_paths[1].stat().st_ino:
        raise ValueError('ready aliases differ')
    if 'ready.json' in names:
        state = 'ready-linked' if 'ready.partial' in names else 'ready'
    elif 'ready.partial' in names:
        state = 'ready-written'
    elif len(images) == 2:
        state = 'image-linked'
    elif 'image.img' in names:
        state = 'image-published'
    else:
        state = 'verified'
    return {'state': state, 'can_reconcile': True, 'reason': 'verified publication', 'ready': expected}


def publish(target, ready, checkpoint):
    if not (target / 'image.img').exists():
        os.link(target / 'image.partial', target / 'image.img')
        sync_dir(target)
        checkpoint('image-linked')
    if (target / 'image.partial').exists():
        os.unlink(target / 'image.partial')
        sync_dir(target)
        checkpoint('image-published')
    if not (target / 'ready.json').exists():
        if not (target / 'ready.partial').exists():
            write_json(target / 'ready.partial', ready)
            sync_dir(target)
            checkpoint('ready-written')
        os.link(target / 'ready.partial', target / 'ready.json')
        sync_dir(target)
        checkpoint('ready-linked')
    if (target / 'ready.partial').exists():
        os.unlink(target / 'ready.partial')
        sync_dir(target)
        checkpoint('ready-published')
    return ready


def reconcile(staging_dir, checkpoint=None):
    """Publish verified bytes only; never provision, repair or rewrite an image."""
    checkpoint = checkpoint or (lambda name: None)
    target = checked_path(staging_dir)
    with plan_lock(target):
        result = inspect(target)  # Verify every existing item before mutation.
        if not result['can_reconcile']:
            raise ValueError(result['reason'])
        # An earlier fsync may have failed after complete bytes became visible.
        # Hash equality alone cannot establish that those bytes or names reached
        # the filesystem's durability boundary. Re-establish every stage-file
        # barrier, including an already visible ready receipt, before success.
        synced = set()
        for path in sorted(target.iterdir()):
            fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
            try:
                info = os.fstat(fd)
                if not stat.S_ISREG(info.st_mode):
                    raise ValueError('nonregular verified stage entry')
                identity = (info.st_dev, info.st_ino)
                if identity not in synced:
                    os.fsync(fd)
                    synced.add(identity)
            finally:
                os.close(fd)
        sync_dir(target)
        if result['state'] == 'ready':
            return result['ready']
        return publish(target, result['ready'], checkpoint)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('staging_dir')
    for name in ('kernel', 'bootx64', 'exfat', 'init'):
        prep.add_argument('--' + name, required=True)
    prep.add_argument('--identity-bound', action='store_true')
    for name in ('bundle', 'home', 'alias', 'driver'):
        prep.add_argument('--profile-' + name)
    for name in ('uid', 'gid'):
        prep.add_argument('--profile-' + name, type=int)
    run = sub.add_parser('apply')
    run.add_argument('staging_dir')
    run.add_argument('--interrupt-after', choices=('provisioning-started', 'provisioned', 'assembled', 'verified',
                                                   'image-linked', 'image-published', 'ready-written',
                                                   'ready-linked', 'ready-published'))
    sub.add_parser('inspect').add_argument('staging_dir')
    installed = sub.add_parser('inspect-installed')
    installed.add_argument('staging_dir')
    installed.add_argument('--driver', required=True)
    recovery = sub.add_parser('reconcile')
    recovery.add_argument('staging_dir')
    recovery.add_argument('--interrupt-after', choices=('image-linked', 'image-published', 'ready-written',
                                                        'ready-linked', 'ready-published'))
    args = parser.parse_args()
    try:
        if args.action == 'prepare':
            result = prepare(args.staging_dir, args.kernel, args.bootx64, args.exfat, args.init,
                identity_bound=args.identity_bound, profile_bundle=args.profile_bundle,
                profile_home=args.profile_home, profile_alias=args.profile_alias, profile_uid=args.profile_uid,
                profile_gid=args.profile_gid, profile_driver=args.profile_driver)
        elif args.action == 'apply':
            def checkpoint(name):
                if name == args.interrupt_after:
                    os._exit(75)
            result = apply(args.staging_dir, checkpoint=checkpoint)
        elif args.action == 'inspect':
            result = inspect(args.staging_dir)
        elif args.action == 'inspect-installed':
            from installation_inspection import inspect_installed
            result = inspect_installed(args.staging_dir, args.driver)
        else:
            def checkpoint(name):
                if name == args.interrupt_after:
                    os._exit(75)
            result = reconcile(args.staging_dir, checkpoint=checkpoint)
        print(json.dumps(result, indent=2, sort_keys=True))
        if args.action == 'inspect-installed' and not result['consistent']:
            raise SystemExit(1)
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'error: {error}\n')


if __name__ == '__main__':
    main()
