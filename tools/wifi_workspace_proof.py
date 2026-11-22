#!/usr/bin/env python3
"""Offline workspace preservation evidence; no mounting, repair or admission.

Capture after any intentional pruning. baseline_dir must be private and outside
root. Both trees require an existing exact-tree SRVROSMETA1 sidecar. Persisted
inode collisions may be remapped by vfs import_metadata: absent an independent
import proof this tool deliberately refuses those changes. vfs_read_all can
change atime; allow only explicitly named files within a supplied UTC window.
The caller must independently establish that window using the native RTC-derived
seconds used by vfs metadata_time, not Ubuntu wall time or kernel uptime. Unknown
clock provenance requires an empty read allowlist and exact metadata equality.
Directory creation does not justify changes to pre-existing directory records.
All other persisted metadata fields and all other prior bytes stay exact.
Use only on an offline/quiescent tree. The caller must independently seal the
external baseline directory with the run evidence before boot and verify that
seal before this verifier; private ownership alone does not authenticate evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat

META = '.srvros/meta'
MAX_META = 2 * 1024 * 1024
U64 = (1 << 64) - 1
SESSION = re.compile(r'session-[0-9a-f]{16}-[0-9]{2}')


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def relative(name):
    require(isinstance(name, str) and name and not name.startswith('/') and
            all(x not in ('', '.', '..') for x in name.split('/')) and
            all(32 <= ord(c) <= 126 for c in name), 'invalid relative path')
    return name


def _read(path):
    before = path.lstat()
    require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1, 'nonregular/aliased file refused')
    with path.open('rb') as f:
        require(os.fstat(f.fileno()) == before, 'file identity changed')
        data = f.read()
    after = path.lstat()
    require((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) ==
            (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns),
            'file changed during capture')
    return data


def _hash_file(path):
    before = path.lstat()
    require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1, 'nonregular/aliased file refused')
    digest = hashlib.sha256()
    with path.open('rb') as f:
        require(os.fstat(f.fileno()) == before, 'file identity changed')
        while block := f.read(1024 * 1024):
            digest.update(block)
    after = path.lstat()
    require((before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) ==
            (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns),
            'file changed during capture')
    return {'bytes': before.st_size, 'sha256': digest.hexdigest()}


def _tree(root):
    root = Path(root)
    require(root.is_dir() and not root.is_symlink(), 'invalid workspace root')
    files, dirs = {}, []
    for folder, directory_names, file_names in os.walk(root, followlinks=False):
        directory_names.sort()
        file_names.sort()
        for name in directory_names:
            p = Path(folder) / name
            require(stat.S_ISDIR(p.lstat().st_mode), 'directory link refused')
            dirs.append(relative(p.relative_to(root).as_posix()))
        for name in file_names:
            p = Path(folder) / name
            key = relative(p.relative_to(root).as_posix())
            files[key] = _hash_file(p)
    require(META in files and files[META]['bytes'] <= MAX_META, 'missing/oversized sidecar')
    require(set(k for k in files if k.startswith('.srvros/')) == {META} and
            set(k for k in dirs if k.startswith('.srvros/')) == set(), 'unexpected internal metadata path')
    return {'files': files, 'directories': sorted(dirs)}


def parse_metadata(raw, tree, mountpoint='/work'):
    require(mountpoint == '/work', 'unsupported metadata mountpoint')
    require(len(raw) <= MAX_META and raw.startswith(b'SRVROSMETA1\n') and raw.endswith(b'\n'),
            'invalid sidecar framing')
    try:
        rows = raw.decode('ascii').splitlines()[1:]
    except UnicodeError as error:
        raise ValueError('invalid metadata encoding') from error
    records, inodes = {}, set()
    paths = {'/work': True}
    paths.update(('/work/' + k, False) for k in tree['files'] if not k.startswith('.srvros/'))
    paths.update(('/work/' + k, True) for k in tree['directories'] if k != '.srvros' and not k.startswith('.srvros/'))
    for row in rows:
        fields = row.split('\t')
        require(len(fields) == 9, 'invalid metadata field count')
        path = fields[0]
        require(path in paths and path not in records, 'unexpected/duplicate metadata path')
        require(all(re.fullmatch(r'0|[1-9][0-9]*', x) for x in fields[1:]), 'invalid metadata integer')
        values = tuple(map(int, fields[1:]))
        inode, mode, links, uid, gid, atime, mtime, ctime = values
        require(all(x <= U64 for x in values) and inode > 0 and inode not in inodes,
                'invalid/duplicate metadata inode')
        require(mode & ~0o177777 == 0 and mode & 0o170000 == (0o40000 if paths[path] else 0o100000),
                'metadata type/permission bits mismatch')
        require(links == (2 if paths[path] else 1), 'metadata link count mismatch')
        records[path] = values
        inodes.add(inode)
    require(set(records) == set(paths), 'incomplete metadata tree')
    return records


def _private(path):
    st = path.lstat()
    require(not path.is_symlink() and st.st_uid == os.geteuid() and st.st_mode & 0o077 == 0,
            'evidence must be private and owned by current user')


def capture(root, baseline_dir):
    root, destination = Path(root), Path(baseline_dir)
    require(not destination.resolve().is_relative_to(root.resolve()), 'baseline must be outside workspace')
    tree = _tree(root)
    raw = _read(root / META)
    require(hashlib.sha256(raw).hexdigest() == tree['files'][META]['sha256'], 'sidecar changed during capture')
    parse_metadata(raw, tree)
    destination.mkdir(mode=0o700, exist_ok=False)
    _private(destination)
    baseline = {'version': 1, 'tree': tree, 'metadata_sha256': hashlib.sha256(raw).hexdigest()}
    for name, data in [('metadata-before.bin', raw), ('baseline.json', (json.dumps(baseline, sort_keys=True) + '\n').encode())]:
        fd = os.open(destination / name, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, 'wb') as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
    return {'prior_files': len(tree['files']), 'prior_directories': len(tree['directories']), 'metadata_saved': True}


def verify(root, baseline_dir, session, read_paths=(), time_window=None):
    require(isinstance(session, str) and SESSION.fullmatch(session), 'invalid matching session')
    root, baseline_dir = Path(root), Path(baseline_dir)
    require(not baseline_dir.resolve().is_relative_to(root.resolve()), 'baseline must be outside workspace')
    for p in (baseline_dir, baseline_dir / 'baseline.json', baseline_dir / 'metadata-before.bin'):
        _private(p)
    baseline = json.loads(_read(baseline_dir / 'baseline.json'))
    require(baseline.get('version') == 1, 'unsupported baseline')
    before = baseline['tree']
    old_raw = _read(baseline_dir / 'metadata-before.bin')
    require(hashlib.sha256(old_raw).hexdigest() == baseline['metadata_sha256'] == before['files'][META]['sha256'] and
            len(old_raw) == before['files'][META]['bytes'], 'baseline metadata hash mismatch')
    old = parse_metadata(old_raw, before)
    after = _tree(root)
    raw = _read(root / META)
    require(hashlib.sha256(raw).hexdigest() == after['files'][META]['sha256'], 'sidecar changed during verification')
    current = parse_metadata(raw, after)
    allowed = 'srvros-work/Logs/' + session
    require(allowed not in before['directories'], 'session already existed at baseline')
    require(all(after['files'].get(k) == v for k, v in before['files'].items() if k != META),
            'prior ordinary file missing or changed')
    require(set(before['directories']) <= set(after['directories']), 'prior directory missing')
    new_files = set(after['files']) - set(before['files'])
    new_dirs = set(after['directories']) - set(before['directories'])
    require(new_files and allowed in new_dirs and all(k.startswith(allowed + '/') for k in new_files) and
            all(k == allowed or k.startswith(allowed + '/') for k in new_dirs), 'unexpected/missing session paths')
    reads = set(read_paths)
    require(all(relative(k) in before['files'] and k != META for k in reads), 'invalid expected-read file')
    if reads:
        require(isinstance(time_window, (tuple, list)) and len(time_window) == 2 and
                all(type(x) is int and 0 <= x <= U64 for x in time_window) and time_window[0] <= time_window[1],
                'expected reads require bounded UTC window')
    changed_atimes = 0
    for path, previous in old.items():
        value = current[path]
        if value == previous:
            continue
        require(path.startswith('/work/') and path[6:] in reads and
                value[:5] == previous[:5] and value[6:] == previous[6:] and
                max(previous[5], time_window[0]) <= value[5] <= time_window[1],
                'unjustified prior metadata change')
        changed_atimes += 1
    return {'prior_files': len(before['files']), 'all_prior_non_sidecar_bytes_equal': True,
            'prior_metadata_preserved_except_authorized_atime': True, 'authorized_atime_changes': changed_atimes,
            'new_session_files': len(new_files), 'no_unexpected_paths': True, 'metadata_exact_current_tree': True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('capture', 'verify'):
        p = sub.add_parser(name)
        p.add_argument('root', type=Path)
        p.add_argument('baseline_dir', type=Path)
        if name == 'verify':
            p.add_argument('--session', required=True)
            p.add_argument('--read-path', action='append', default=[])
            p.add_argument('--utc-window', type=int, nargs=2)
    args = parser.parse_args()
    result = capture(args.root, args.baseline_dir) if args.command == 'capture' else verify(
        args.root, args.baseline_dir, args.session, args.read_path, args.utc_window)
    print(json.dumps(result, sort_keys=True))


if __name__ == '__main__':
    main()
