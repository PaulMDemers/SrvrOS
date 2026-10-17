#!/usr/bin/env python3
"""Unpack Ubuntu/Debian build tools locally when sudo is unavailable.

Uses the configured APT repositories and dependency resolver; no system package
installation, service changes, firmware writes, or device access. Run commands
through tools/host-env.sh afterwards. System-installed tools remain preferred
for long-term use (see docs/development.md).
"""
import os
from pathlib import Path
import shlex
import shutil
import subprocess


def main():
    repo = Path(__file__).resolve().parent.parent
    destination = repo / 'build/host-tools'
    debs = destination / 'debs'
    root = destination / 'root'
    debs.mkdir(parents=True, exist_ok=True)
    packages = ['xorriso', 'qemu-system-x86', 'qemu-system-gui', 'ovmf', 'acpica-tools']
    env = dict(os.environ, LC_ALL='C')
    plan = subprocess.check_output(
        ['apt-get', '-s', 'install', '--no-install-recommends', *packages],
        env=env, text=True)
    needed = sorted(set(packages + [line.split()[1] for line in plan.splitlines()
                                    if line.startswith('Inst ')]))
    print('Downloading locally: ' + ', '.join(needed), flush=True)
    subprocess.run(['apt-get', 'download', *needed], cwd=debs, env=env, check=True)
    for deb in sorted(debs.glob('*.deb')):
        subprocess.run(['dpkg-deb', '-x', str(deb), str(root)], check=True)
    # Debian splits SeaBIOS and iPXE from QEMU's firmware search directory.
    firmware = root / 'usr/share/qemu'
    for directory in ['seabios', 'ipxe/qemu']:
        source = root / 'usr/share' / directory
        if source.is_dir():
            for item in source.iterdir():
                target = firmware / item.name
                if item.is_file() and (not target.exists()):
                    if target.is_symlink():
                        target.unlink()
                    shutil.copy2(item, target)
    wrappers = destination / 'bin'
    wrappers.mkdir(exist_ok=True)
    wrapper = wrappers / 'qemu-system-x86_64'
    wrapper.write_text('#!/bin/sh\nexec ' + shlex.quote(str(root / 'usr/bin/qemu-system-x86_64')) +
                       ' -L ' + shlex.quote(str(firmware)) + ' "$@"\n')
    wrapper.chmod(0o755)
    print('Ready. Use: tools/host-env.sh make -j2')


if __name__ == '__main__':
    main()
