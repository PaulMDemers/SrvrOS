#!/usr/bin/env python3
"""Install the checksum-pinned freestanding compiler for this host."""
import argparse
import hashlib
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
    ('0.15.2', 'macos-aarch64'): ('tar.xz', '3cc2bab367e185cdfb27501c4b30b1b0653c28d9f73df8dc91488e66ece5fa6b'),
}


def release_for(version, system, machine):
    """Keep unsupported hosts explicit; compiler host and OS target differ."""
    host = 'windows' if system.startswith(('Windows', 'MSYS', 'MINGW')) else system.lower()
    arch = machine.lower()
    if host == 'darwin' and arch in ('arm64', 'aarch64'):
        key, host, arch = 'macos-aarch64', 'macos', 'aarch64'
    elif arch in ('x86_64', 'amd64'):
        key, arch = host, 'x86_64'
    else:
        raise ValueError('supported hosts: x86_64 Linux/Windows/MSYS2 and ARM64 macOS; override ZIG for other hosts')
    try:
        extension, expected = RELEASES[version, key]
    except KeyError:
        raise ValueError(f'no pinned Zig {version} release for {machine} {system}') from None
    return f'zig-{arch}-{host}-{version}', extension, expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='0.15.2')
    parser.add_argument('--output', default='build/tooling/zig')
    args = parser.parse_args()
    try:
        name, extension, expected = release_for(args.version, platform.system(), platform.machine())
    except ValueError as error:
        parser.error(str(error))
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
                package.extractall(staging, filter='data')
        executable = Path(staging) / name / ('zig.exe' if extension == 'zip' else 'zig')
        actual = subprocess.check_output([str(executable), 'version'], text=True).strip()
        if actual != args.version:
            raise RuntimeError(f'Unexpected Zig version: {actual}')
        shutil.move(str(Path(staging) / name), output)
    print(f'zig: ready: {output}')


if __name__ == '__main__':
    main()
