"""Storage preflight for bounded, task-owned browser builds."""
import os
from pathlib import Path
import shutil


class ResourceError(RuntimeError):
    """A required storage location is invalid or below its free-space guard."""


class StorageGuard:
    """Check each storage role independently; no filesystem identity is inferred."""

    def __init__(self, output_path, host_path=None, backing_path=None, reserve=2 * 1024**3):
        if host_path is None:
            host_path = Path.home()
        if backing_path is None:
            backing_path = os.environ.get('SRVROS_BUILD_BACKING_PATH') or None
        self.reserve = int(reserve)
        if self.reserve < 0:
            raise ValueError('reserve must be nonnegative')
        self.paths = {'host': self._directory('host', host_path),
                      'output': self._directory('output', output_path)}
        if backing_path is not None:
            self.paths['backing'] = self._directory('backing', backing_path)
        self.minimum = {}
        self.snapshot = {}

    @staticmethod
    def _directory(role, value):
        path = Path(value)
        if not path.is_absolute() or not path.is_dir():
            raise ResourceError(f'{role} storage path must be an absolute existing directory: {path}')
        return path.resolve()

    def preflight(self, additional_bytes=0):
        additional_bytes = int(additional_bytes)
        if additional_bytes < 0:
            raise ValueError('additional_bytes must be nonnegative')
        current = {}
        failures = []
        for role, path in self.paths.items():
            try:
                free = shutil.disk_usage(path).free
            except OSError as exc:
                raise ResourceError(f'{role} storage check failed for {path}: {exc}') from exc
            required = self.reserve + (additional_bytes if role in ('output', 'backing') else 0)
            previous = self.minimum.get(role, {}).get('free_bytes', free)
            self.minimum[role] = {'path': str(path), 'free_bytes': min(previous, free)}
            current[role] = {'path': str(path), 'free_bytes': free, 'required_bytes': required}
            if free <= required:
                failures.append(f'{role} storage {path}: {free} bytes free; must exceed {required}')
        self.snapshot = current
        if failures:
            raise ResourceError('; '.join(failures))
        return current

    def environment(self, env=None, additional_bytes=0):
        """Check capacity before creating the private temporary directory."""
        self.preflight(additional_bytes)
        tmp = self.paths['output'] / 'tmp'
        try:
            tmp.mkdir(exist_ok=True)
            if tmp.is_symlink() or not tmp.is_dir() or tmp.resolve().parent != self.paths['output']:
                raise OSError('temporary path must be a direct directory under output')
        except OSError as exc:
            raise ResourceError(f'output temporary directory {tmp}: {exc}') from exc
        result = dict(os.environ if env is None else env)
        result['TMPDIR'] = str(tmp)
        return result
