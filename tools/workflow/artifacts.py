"""Lossless packing of a completed run's own disposable VM disk images."""
import gzip
import hashlib
import os
from pathlib import Path
import shutil

from workflow.store import atomic, fsync_dir, sha


def unpacked_digest(path):
    with gzip.open(path, 'rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def archive_image(path):
    path = Path(path)
    packed = path.with_name(path.name + '.gz')
    if path.is_symlink() or not path.is_file():
        raise ValueError('Archive only a regular completed VM image')
    original, size = sha(path), path.stat().st_size
    if packed.exists():
        if packed.is_symlink() or unpacked_digest(packed) != original:
            raise ValueError('Existing archive differs; original image retained')
    else:
        temporary = packed.with_name(packed.name + '.packing')
        if temporary.is_symlink():
            raise ValueError('Unsafe archive temporary path')
        with temporary.open('wb') as output:
            with path.open('rb') as source, gzip.GzipFile(filename='', mode='wb',
                    fileobj=output, compresslevel=1, mtime=0) as destination:
                shutil.copyfileobj(source, destination, 1024*1024)
            output.flush()
            os.fsync(output.fileno())
        if unpacked_digest(temporary) != original or sha(path) != original:
            raise ValueError('Archive verification failed; original image retained')
        temporary.replace(packed)
    if sha(path) != original:
        raise ValueError('VM image changed; retain both artifacts for review')
    receipt = dict(original_name=path.name, original_sha256=original, original_bytes=size,
                   archive=packed.name, archive_sha256=sha(packed), archive_bytes=packed.stat().st_size,
                   method='gzip', roundtrip_verified=True)
    atomic(path.with_name(path.name + '.archive.json'), receipt)
    path.unlink()  # only after verified durable archive AND its receipt
    fsync_dir(path.parent)
    return receipt
