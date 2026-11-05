"""Bound disposable proof output; never discover or prune recovery storage.

Only runs created by this helper, finished successfully, and older than the
retained successes are eligible. Receipts/logs stay; only inventoried images go.
Legacy, failed, interrupted, pinned and externally opened runs are fail-closed.
"""
import argparse
import fcntl
import json
import math
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
import time

GIB = 1024 ** 3
MIB = 1024 ** 2
MARKER = 'proof-storage.json'


class StorageLimit(RuntimeError):
    pass


def positive_gib(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError('must be a finite positive GiB value')
    return number


def keep_count(value):
    number = int(value)
    if number < 2:
        raise argparse.ArgumentTypeError('keep at least two successful proofs')
    return number


def add_arguments(parser):
    parser.add_argument('--min-free-gib', type=positive_gib, default=4,
                        help='free-space reserve in addition to remaining run budget (default: 4)')
    parser.add_argument('--run-budget-gib', type=positive_gib, default=8,
                        help='maximum logical size of this proof (default: 8)')
    parser.add_argument('--proof-budget-gib', type=positive_gib, default=24,
                        help='maximum logical size of this harness proof directory (default: 24)')
    parser.add_argument('--keep-successful-runs', type=keep_count, default=2,
                        help='successful proofs whose images remain intact (minimum/default: 2)')


def files(directory):
    """Walk regular files without traversing links or other mounted filesystems."""
    device = directory.stat().st_dev
    for root, dirs, names in os.walk(directory, followlinks=False):
        dirs[:] = [name for name in dirs if not (Path(root) / name).is_symlink()
                   and (Path(root) / name).stat().st_dev == device]
        for name in names:
            path = Path(root) / name
            info = path.lstat()
            if stat.S_ISREG(info.st_mode) and info.st_dev == device:
                yield path, info


def tree_bytes(directory):
    seen = set()
    total = 0
    for _, info in files(directory):
        key = (info.st_dev, info.st_ino)
        if key not in seen:
            total += info.st_size
            seen.add(key)
    return total


def save(path, value):
    # A partial completion marker must never make an interrupted proof eligible.
    temporary = path.with_name(path.name + '.tmp')
    with temporary.open('x') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def lock(path):
    descriptor = os.open(path, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    stream = os.fdopen(descriptor, 'r+')
    try:
        fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BaseException:
        stream.close()
        raise
    return stream


def identity(info):
    return [info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns]


class ProofStorage:
    def __init__(self, root, harness, args):
        if harness not in ('profile-chooser', 'installation-recovery'):
            raise ValueError('unknown disposable proof harness')
        self.root = Path(os.path.abspath(root))
        self.base = self.root / 'build' / 'review' / harness
        self.harness = harness
        self.reserve = int(args.min_free_gib * GIB)
        self.run_budget = int(args.run_budget_gib * GIB)
        self.total_budget = int(args.proof_budget_gib * GIB)
        self.keep = args.keep_successful_runs
        self.out = None
        self.base_lock = self.run_lock = None
        self.last_check = 0

    def __enter__(self):
        # Do not turn a linked build/review tree into cleanup authority elsewhere.
        if any(p.is_symlink() for p in (self.base, *self.base.parents)):
            raise StorageLimit('proof directory contains a symlink; refusing storage management')
        self.base.mkdir(parents=True, exist_ok=True)
        root_device = self.root.stat().st_dev
        if any(p.stat().st_dev != root_device for p in (self.root / 'build', self.root / 'build/review', self.base)):
            raise StorageLimit('proof directory crosses a filesystem mount; refusing storage management')
        try:
            try:
                self.base_lock = lock(self.base / '.proof-storage.lock')
            except BlockingIOError as error:
                raise StorageLimit(f'another {self.harness} proof is active; retry after it exits') from error
            self.prune()
            self.check(self.run_budget)
            self.out = Path(tempfile.mkdtemp(prefix='proof-', dir=self.base))
            self.run_lock = lock(self.out / '.proof-active.lock')
            self.record = {'version': 1, 'harness': self.harness, 'pid': os.getpid(),
                           'state': 'INCOMPLETE', 'created_ns': time.time_ns(),
                           'reserve_bytes': self.reserve, 'run_budget_bytes': self.run_budget,
                           'proof_budget_bytes': self.total_budget, 'keep_successful_runs': self.keep}
            save(self.out / MARKER, self.record)
            return self
        except BaseException:
            self.close()
            raise

    def check(self, additional=0):
        used = tree_bytes(self.out) if self.out else 0
        total = tree_bytes(self.base)
        remaining = max(0, self.run_budget - used)
        free = shutil.disk_usage(self.base).free
        reason = None
        if used + additional > self.run_budget:
            reason = f'run budget {self.run_budget / GIB:g} GiB would be exceeded'
        elif total + (remaining if self.out else self.run_budget) > self.total_budget:
            reason = f'proof directory budget {self.total_budget / GIB:g} GiB would be exceeded'
        elif free < self.reserve + remaining:
            reason = (f'{free / GIB:.2f} GiB free; need {(self.reserve + remaining) / GIB:.2f} GiB '
                      f'including {self.reserve / GIB:g} GiB reserve')
        if reason:
            raise StorageLimit(f'{self.harness}: {reason}. Protected evidence was kept. '
                               f'Free space or explicitly archive reviewed proofs in {self.base} before retrying.')
        self.last_check = time.monotonic()

    def periodic_check(self):
        if time.monotonic() - self.last_check >= 5:
            self.check()

    def call(self, additional, function, *args, **kwargs):
        self.check(additional)
        result = function(*args, **kwargs)
        self.check()
        return result

    def prune(self):
        candidates = []
        for path in self.base.glob('proof-*'):
            if path.is_symlink() or not path.is_dir() or path.stat().st_dev != self.base.stat().st_dev:
                continue
            try:
                if (path / MARKER).is_symlink() or (path / 'receipt.json').is_symlink():
                    continue
                record = json.loads((path / MARKER).read_text())
                receipt = json.loads((path / 'receipt.json').read_text())
                if (record.get('version') == 1 and record.get('harness') == self.harness
                        and record.get('state') == 'PASS' and receipt.get('status') == 'PASS'
                        and isinstance(record.get('completed_ns'), int)
                        and isinstance(record.get('images'), dict)):
                    candidates.append((record['completed_ns'], path, record))
            except (OSError, ValueError, TypeError, AttributeError):
                continue
        for _, path, record in sorted(candidates, reverse=True)[self.keep:]:
            if (path / '.keep').exists() or (path / '.keep').is_symlink() or path == self.out:
                continue
            try:
                active = lock(path / '.proof-active.lock')
            except OSError:
                continue
            try:
                if not self._unused_images(path):
                    continue
                removed = []
                for relative, expected in record['images'].items():
                    if (path / '.keep').exists() or (path / '.keep').is_symlink():
                        break
                    if not isinstance(relative, str):
                        continue
                    name = Path(relative)
                    if name.is_absolute() or '..' in name.parts:
                        continue
                    image = path / name
                    if not (image.suffix in ('.img', '.exfat') or image.name == 'image.partial'):
                        continue
                    if any(p.is_symlink() for p in (image, *image.parents)):
                        continue
                    try:
                        info = image.lstat()
                        if (stat.S_ISREG(info.st_mode) and info.st_nlink == 1
                                and info.st_dev == path.stat().st_dev and identity(info) == expected
                                and self._unlink_confined(path, name, expected)):
                            removed.append(relative)
                    except FileNotFoundError:
                        continue
                if removed:
                    record.setdefault('removed_images', []).extend(removed)
                    record['pruned_ns'] = time.time_ns()
                    save(path / MARKER, record)
                    print(f'proof-storage: removed {len(removed)} obsolete fixture images from {path}', flush=True)
            finally:
                active.close()

    @staticmethod
    def _unlink_confined(run, relative, expected):
        # Anchor each traversal step to an open directory; reject symlinks and
        # mounted subtrees even if a path is replaced after inventory.
        flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
        fds = []
        try:
            fd = os.open(run, flags)
            fds.append(fd)
            device = os.fstat(fd).st_dev
            for part in relative.parts[:-1]:
                fd = os.open(part, flags, dir_fd=fd)
                fds.append(fd)
                if os.fstat(fd).st_dev != device:
                    return False
            info = os.stat(relative.name, dir_fd=fd, follow_symlinks=False)
            if (not stat.S_ISREG(info.st_mode) or info.st_dev != device or
                    info.st_nlink != 1 or identity(info) != expected):
                return False
            os.unlink(relative.name, dir_fd=fd)
            return True
        except OSError:
            return False
        finally:
            for fd in reversed(fds):
                os.close(fd)

    @staticmethod
    def _unused_images(path):
        # lsof sees our own per-run lock. Keep every other open file/process,
        # including a reviewer using this run as cwd, conservative and protected.
        try:
            result = subprocess.run(['lsof', '-nP', '-F', 'pn', '+D', str(path)],
                                    capture_output=True, text=True, timeout=30)
        except (OSError, subprocess.TimeoutExpired):
            return False
        if result.stderr.strip() or result.returncode not in (0, 1):
            return False
        pid = None
        saw_lock = False
        for line in result.stdout.splitlines():
            if line.startswith('p'):
                pid = line[1:]
            elif line.startswith('f'):
                # lsof includes the file descriptor field even with -F pn.
                continue
            elif line.startswith('n'):
                if pid != str(os.getpid()) or line[1:] != str(path / '.proof-active.lock'):
                    return False
                saw_lock = True
            else:
                return False
        return saw_lock

    def close(self):
        for stream in (self.run_lock, self.base_lock):
            if stream:
                stream.close()

    def __exit__(self, typ, value, traceback):
        try:
            if typ is None and json.loads((self.out / 'receipt.json').read_text()).get('status') == 'PASS':
                try:
                    self.check()
                except StorageLimit as error:
                    receipt_path = self.out / 'receipt.json'
                    receipt = json.loads(receipt_path.read_text())
                    receipt.update(status='FAIL', error=f'StorageLimit: {error}')
                    save(receipt_path, receipt)
                    raise
                images = {str(path.relative_to(self.out)): identity(info)
                          for path, info in files(self.out)
                          if path.suffix in ('.img', '.exfat') or path.name == 'image.partial'}
                self.record.update(state='PASS', completed_ns=time.time_ns(), images=images)
                save(self.out / MARKER, self.record)
                self.prune()
        finally:
            self.close()
