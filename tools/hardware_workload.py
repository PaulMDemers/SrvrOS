"""Independent expected tree for the v2 laptop storage workload."""
import hashlib
from pathlib import Path

LARGE_BYTES = 384 * 1024 + 37
DIRECTORIES = {'nested', 'nested/deep', 'after'}


def expected_files():
    pattern = bytes((i * 37 + (i >> 8) + 11) & 255 for i in range(LARGE_BYTES))
    return {
        'anchor.txt': b'Unchanged anchor: AaZz 0123456789\n',
        'Renamed-MixedCase.txt': b'Overwritten: AaZz\nAppended: 0123456789\n',
        'empty.txt': b'',
        'truncated.txt': b'',
        'nested/deep/large.bin': pattern,
        'shrunk.bin': pattern[:17],
    }


def verify_tree(read_file, list_directory):
    """Callbacks use relative paths; directory listings map exact names to types."""
    expected = expected_files()
    for parent in ('', *sorted(DIRECTORIES)):
        prefix = parent + '/' if parent else ''
        entries = {}
        for path in expected.keys() | DIRECTORIES:
            if path.startswith(prefix) and '/' not in path[len(prefix):]:
                entries[path[len(prefix):]] = 'directory' if path in DIRECTORIES else 'file'
        if list_directory(parent) != entries:
            raise ValueError(f'USB workload directory mismatch: {parent or "/"}')
    result = {}
    for name, payload in expected.items():
        if read_file(name, len(payload)) != payload:
            raise ValueError(f'USB workload contents mismatch: {name}')
        result[name] = {'bytes': len(payload), 'sha256': hashlib.sha256(payload).hexdigest()}
    return result


def verify_mounted(root: Path):
    if root.is_symlink() or not root.is_dir():
        raise ValueError('USB workload directory is missing or is a symlink')

    def listing(relative):
        entries = {}
        for child in (root / relative).iterdir():
            if child.is_symlink(): raise ValueError('Symlink in USB workload')
            entries[child.name] = 'directory' if child.is_dir() else 'file' if child.is_file() else 'other'
            if len(entries) > 16: raise ValueError('Too many entries in USB workload')
        return entries

    def read(relative, size):
        path = root / relative
        if path.stat().st_size != size: raise ValueError(f'USB workload size mismatch: {relative}')
        return path.read_bytes()

    return verify_tree(read, listing)
