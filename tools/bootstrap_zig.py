#!/usr/bin/env python3
"""Install the checksum-pinned freestanding compiler for this host."""
import argparse
import hashlib
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
import zipfile

RELEASES = {
    ('0.15.2', 'linux'): ('tar.xz', '02aa270f183da276e5b5920b1dac44a63f1a49e55050ebde3aecc9eb82f93239'),
    ('0.15.2', 'windows'): ('zip', '3a0ed1e8799a2f8ce2a6e6290a9ff22e6906f8227865911fb7ddedc3cc14cb0c'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='0.15.2')
    parser.add_argument('--output', default='build/tooling/zig')
    args = parser.parse_args()
    host = 'windows' if os.name == 'nt' or platform.system().startswith(('MSYS', 'MINGW')) else platform.system().lower()
    if platform.machine().lower() not in ('x86_64', 'amd64') or (args.version, host) not in RELEASES:
        parser.error('supported hosts: x86_64 Linux and Windows/MSYS2; override ZIG for other hosts')
    extension, expected = RELEASES[args.version, host]
    name = f'zig-x86_64-{host}-{args.version}'
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        parser.error(f'{output} already exists; move the incomplete toolchain aside before retrying')
    with tempfile.TemporaryDirectory(prefix='zig-download-', dir=output.parent) as staging:
        archive = Path(staging) / f'{name}.{extension}'
        url = f'https://ziglang.org/download/{args.version}/{archive.name}'
        print(f'zig: downloading {url}', flush=True)
        urllib.request.urlretrieve(url, archive)
        if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
            raise RuntimeError('Zig archive checksum mismatch')
        if extension == 'zip':
            with zipfile.ZipFile(archive) as package:
                package.extractall(staging)
        else:
            with tarfile.open(archive) as package:
                package.extractall(staging)
        executable = Path(staging) / name / ('zig.exe' if host == 'windows' else 'zig')
        actual = subprocess.check_output([str(executable), 'version'], text=True).strip()
        if actual != args.version:
            raise RuntimeError(f'Unexpected Zig version: {actual}')
        shutil.move(str(Path(staging) / name), output)
    print(f'zig: ready: {output}')


if __name__ == '__main__':
    main()
