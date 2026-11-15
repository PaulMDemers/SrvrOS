"""Durable JSON, checkout locks and content-addressed host validation.

Cache entries prove successful host commands over explicit content, never a
physical operation. Inputs are checked again after the command finishes.
"""
import contextlib
import fcntl
import glob
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2) + '\n').encode()


def atomic(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(prefix='.' + path.name, dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(value if isinstance(value, bytes) else encoded(value))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(name, path)
        fsync_dir(path.parent)
    finally:
        Path(name).unlink(missing_ok=True)


def fsync_dir(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


@contextlib.contextmanager
def lock(path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a') as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ValueError('Another workflow/harness owns ' + str(path)) from error
        yield


def files(root, patterns):
    """Hash complete glob membership as well as bytes; refuse unsafe symlinks."""
    root = Path(root).resolve()
    result = {}
    for pattern in patterns:
        if Path(pattern).is_absolute() or '..' in Path(pattern).parts:
            raise ValueError('Dependencies must be inside the checkout: ' + pattern)
        matches = glob.glob(str(root / pattern), recursive=True)
        if not matches:
            raise ValueError('Dependency pattern matched nothing: ' + pattern)
        for name in sorted(matches):
            path = Path(name)
            if (path.is_symlink() or any((root / p).is_symlink()
                    for p in path.relative_to(root).parents)
                    or not path.resolve().is_relative_to(root)):
                raise ValueError('Unsafe dependency: ' + name)
            if path.is_dir():
                continue
            if not path.is_file():
                raise ValueError('Not a regular dependency: ' + name)
            result[str(path.relative_to(root))] = dict(sha256=sha(path), mode=path.stat().st_mode & 0o777)
    if not result:
        raise ValueError('No dependency files')
    return result


def executable(argv, env):
    resolved = shutil.which(argv[0], path=env.get('PATH'))
    if resolved is None:
        raise ValueError('Executable missing: ' + argv[0])
    path = Path(resolved).resolve()
    return dict(path=str(path), sha256=sha(path))


def process_environment(overrides=None):
    env = dict(os.environ)
    # This transport token changes for every agent command. Remove it from BOTH
    # the actual child environment and its cache identity, not just the hash.
    env.pop('CODEX_TOOL_CALL_ID', None)
    env.update(overrides or {})
    return env


class Cache:
    def __init__(self, root):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)

    def run(self, job, cwd, log, *, fresh=False):
        cwd, log = Path(cwd).resolve(), Path(log)
        argv = job['argv']
        if not isinstance(argv, list) or not argv or not all(isinstance(a, str) for a in argv):
            raise ValueError('Job argv must be a nonempty string array')
        env = process_environment(job.get('env', {}))
        outputs = job.get('outputs', [])
        before = files(cwd, job['inputs'])
        identity = dict(version=1, implementation=sha(__file__), cwd=str(cwd),
                        argv=argv, executable=executable(argv, env),
                        environment_sha256=hashlib.sha256(encoded(env)).hexdigest(),
                        inputs=before, outputs=outputs, timeout=job['timeout'])
        key = hashlib.sha256(encoded(identity)).hexdigest()
        entry = self.root / key
        with lock(self.root / (key + '.lock')):
            receipt = entry / 'receipt.json'
            if receipt.is_file() and not fresh and job.get('cache', True):
                saved = json.loads(receipt.read_text())
                try:
                    current = files(cwd, outputs) if outputs else {}
                except (OSError, ValueError):
                    current = None
                if saved['identity'] == identity and current == saved['outputs'] and sha(entry / 'command.log') == saved['log_sha256']:
                    log.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(entry / 'command.log', log)
                    return dict(key=key, cached=True, seconds=0, original_seconds=saved['seconds'],
                                log_sha256=sha(log), outputs=current)
            log.parent.mkdir(parents=True, exist_ok=True)
            start = time.monotonic()
            with log.open('wb') as stream:
                subprocess.run(argv, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT,
                               timeout=job['timeout'], check=True)
            elapsed = time.monotonic() - start
            if before != files(cwd, job['inputs']):
                raise ValueError('Validation inputs changed during ' + job['name'])
            result = files(cwd, outputs) if outputs else {}
            entry.mkdir(exist_ok=True)
            shutil.copyfile(log, entry / 'command.log')
            with (entry / 'command.log').open('rb') as stream:
                os.fsync(stream.fileno())
            saved = dict(identity=identity, outputs=result, seconds=elapsed, log_sha256=sha(log))
            atomic(receipt, saved)  # written last; interrupted/failed commands never qualify
            return dict(key=key, cached=False, seconds=elapsed, log_sha256=sha(log), outputs=result)


def gc(root, *, keep, max_age_days, max_bytes, apply=False):
    """Only content-keyed cache entries. Runs/recovery/evidence are never eligible."""
    root = Path(root).resolve()
    if max_age_days < 0 or max_bytes < 0 or keep < 0:
        raise ValueError('Retention limits must be nonnegative')
    candidates = []
    for path in root.iterdir():
        if path.is_symlink() or not path.is_dir() or len(path.name) != 64 or any(c not in '0123456789abcdef' for c in path.name):
            continue
        receipt = path / 'receipt.json'
        if not receipt.is_file() or receipt.is_symlink():
            continue
        size = sum(p.stat().st_size for p in path.rglob('*') if p.is_file() and not p.is_symlink())
        candidates.append((receipt.stat().st_mtime, path, size))
    candidates.sort(reverse=True)
    total = sum(row[2] for row in candidates)
    selected = []
    for mtime, path, size in reversed(candidates[keep:]):
        if time.time() - mtime > max_age_days * 86400 or total > max_bytes:
            selected.append(dict(path=str(path), bytes=size))
            if apply:
                with lock(root / (path.name + '.lock')):
                    shutil.rmtree(path)
            total -= size
    return dict(apply=apply, removed=selected, retained_bytes=total)
