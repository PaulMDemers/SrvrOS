"""Read-only, raw-first USB collection. Invoked explicitly through the unit helper."""
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess

from check_exfat_allocation import select_partition
from hardware_desktop import collect, SESSION
from inspect_hardware_test import Reader
from workflow.store import atomic, sha, fsync_dir


def inventory():
    return json.loads(subprocess.check_output(['lsblk', '--json', '--tree', '--output',
                      'PATH,TYPE,TRAN,UUID,FSTYPE,MOUNTPOINTS'], text=True))


def tree(path):
    result = {}
    for item in sorted(Path(path).rglob('*')):
        if item.is_symlink():
            raise ValueError('Symlink in USB workspace: ' + str(item))
        if item.is_file():
            result[str(item.relative_to(path))] = dict(bytes=item.stat().st_size, sha256=sha(item))
    return result


def durable_copy(source, destination):
    source, destination = Path(source), Path(destination)
    expected = tree(source)
    if destination.exists():
        if tree(destination) != expected:
            raise ValueError('Existing raw bank differs; preserve it and investigate')
    else:
        temporary = destination.with_name(destination.name + '.copying')
        if temporary.exists():
            shutil.rmtree(temporary)  # only our incomplete copy, never the source/raw bank
        shutil.copytree(source, temporary)
        if tree(temporary) != expected:
            raise ValueError('Raw copy verification failed')
        for item in temporary.rglob('*'):
            if item.is_file():
                with item.open('rb') as stream:
                    os.fsync(stream.fileno())
        fsync_dir(temporary)
        temporary.rename(destination)
        fsync_dir(destination.parent)
    return expected


def snapshot(config, destination, *, before=None):
    """The caller owns the checkout lock. No filesystem repair or USB writes."""
    destination = Path(destination)
    if destination.is_symlink():
        raise ValueError('Unsafe output directory')
    destination.mkdir(parents=True, exist_ok=True)
    device = select_partition(inventory(), config['volume'])
    with open(device, 'rb', buffering=0) as stream:
        if not stat.S_ISBLK(os.fstat(stream.fileno()).st_mode):
            raise ValueError('Not a block device')
        Reader(stream, config['volume'])
        stream.seek(0)
        boot = stream.read(24 * 512)
    if len(boot) != 24*512 or int.from_bytes(boot[106:108], 'little') != 0:
        raise ValueError('Invalid or dirty exFAT boot region')
    import hashlib
    boot_sha = hashlib.sha256(boot).hexdigest()
    if before and boot_sha != before['boot_sha256']:
        raise ValueError('Boot region changed')
    mount = destination / 'volume-ro'
    mount.mkdir(exist_ok=True)
    if mount.is_mount():
        raise ValueError('Output mountpoint already mounted')
    subprocess.run(['mount', '-o', 'ro,nodev,nosuid,noexec', device, str(mount)], check=True)
    try:
        marker = mount / 'srvros-work/.workspace'
        if marker.is_symlink() or marker.read_bytes() != f"SRVROS-WORKSPACE-V1 {config['volume']}\n".encode():
            raise ValueError('Prepared workspace marker mismatch')
        logs = mount / 'srvros-work/Logs'
        if logs.is_symlink() or not logs.is_dir():
            raise ValueError('Missing workspace Logs directory')
        sessions = sorted(path.name for path in logs.iterdir() if SESSION.fullmatch(path.name))
        if len(sessions) > 4096:
            raise ValueError('Session count exceeds collection limit')
        fresh = sorted(set(sessions) - set(before['sessions'])) if before else []
        raw = {}
        if before:
            # Preserve every new session before parsing or deciding whether it is the right one.
            total = 0
            for name in fresh:
                source = logs / name
                if source.is_symlink() or not source.is_dir():
                    raise ValueError('Unsafe session directory')
                for item in source.rglob('*'):
                    if item.is_symlink() or not item.is_file() or item.stat().st_size > 65536:
                        raise ValueError('Unsafe/oversized raw session file')
                    total += item.stat().st_size
                if total > 64*1024*1024:
                    raise ValueError('Raw collection exceeds 64 MiB')
                raw[name] = durable_copy(source, destination / ('raw-' + name))
            atomic(destination / 'raw.json', raw)
        files = tree(mount)
        if before:
            changed = sorted(name for name, value in before['files'].items()
                             if files.get(name) != value and name != '.srvros/meta')
            if changed:
                raise ValueError('Prior USB files changed: ' + ', '.join(changed[:10]))
            # Parse only after the durable raw copies exist, even for a failed trial.
            collection, report = collect(mount, config['volume'], destination / 'collection')
            atomic(destination / 'parsed.json', dict(collection=str(collection), report=report))
        result = dict(device=device, volume=config['volume'], boot_sha256=boot_sha,
                      files=files, sessions=sessions, new_sessions=fresh, raw=raw)
        atomic(destination / 'snapshot.json', result)
    finally:
        subprocess.run(['umount', str(mount)], check=True)
    # Offline checks are repeated after collection; no remembered fsck pass is reused.
    root = Path(config['repo'])
    for name, argv in (
        ('fsck', ['fsck.exfat', '-n', device]),
        ('allocation', ['python3', 'tools/check_exfat_allocation.py', '--uuid', config['volume']]),
        ('writable', ['python3', 'tools/check_exfat_allocation.py', '--uuid', config['volume'], '--require-writable']),
        ('headroom', ['python3', 'tools/check_exfat_allocation.py', '--uuid', config['volume'], '--require-headroom'])):
        with (destination / (name + '.log')).open('wb') as stream:
            subprocess.run(argv, cwd=root, stdout=stream, stderr=subprocess.STDOUT, check=True, timeout=180)
    atomic(destination / 'offline.json', dict(status='PASS', device=device, unmounted=True))
    return result


def as_owner(path, uid, gid):
    if (Path(path) / 'volume-ro').is_mount():
        raise ValueError('Unmount failed; do not traverse a live USB mount')
    for item in [Path(path), *Path(path).rglob('*')]:
        if item.is_symlink():
            raise ValueError('Unsafe output symlink')
        os.chown(item, uid, gid)
