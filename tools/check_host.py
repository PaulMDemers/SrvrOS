#!/usr/bin/env python3
"""Check prerequisites without downloading or changing anything."""
import os
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    failed = False
    for command in ('git', 'make', 'cc', 'tar', 'xorriso', 'qemu-system-x86_64'):
        path = shutil.which(command)
        print(f'{"ok" if path else "MISSING"}: {command}: {path or "see docs/development.md"}')
        failed |= path is None
    print(f'ok: Python: {sys.executable}')
    firmware = [os.environ.get('OVMF_CODE', ''), '/usr/share/OVMF/OVMF_CODE_4M.fd',
                '/usr/share/OVMF/OVMF_CODE.fd', '/usr/share/edk2/x64/OVMF_CODE.fd',
                '/ucrt64/share/qemu/edk2-x86_64-code.fd',
                r'C:\msys64\ucrt64\share\qemu\edk2-x86_64-code.fd']
    qemu = shutil.which('qemu-system-x86_64')
    if qemu:
        firmware.append(str(Path(qemu).resolve().parent.parent / 'share/qemu/edk2-x86_64-code.fd'))
    ovmf = next((p for p in firmware if p and Path(p).is_file()), None)
    print(f'{"ok" if ovmf else "MISSING"}: UEFI firmware: {ovmf or "install OVMF or set OVMF_CODE"}')
    failed |= ovmf is None
    repo = Path(__file__).resolve().parent.parent
    status = subprocess.check_output(['git', 'submodule', 'status'], cwd=repo, text=True)
    for line in status.splitlines():
        print(f'{"ok" if line.startswith(" ") else "CHECK"}: submodule {line.strip()}')
        failed |= line.startswith(('-', 'U'))
    for name, marker in [('zlib', 'zlib.h'), ('lua', 'lua.h'), ('cjson', 'cJSON.h'),
                         ('inih', 'ini.h'), ('linenoise', 'linenoise.h'),
                         ('libuv', 'include/uv.h'), ('node', 'configure.py')]:
        if not (repo / 'ports/upstream' / name / marker).is_file():
            print(f'MISSING: {name} checkout; run git submodule update --init --recursive')
            failed = True
    zig = repo / 'build/tooling/zig' / ('zig.exe' if os.name == 'nt' else 'zig')
    print(f'ok: Zig: {zig if zig.is_file() else "downloaded and checksum-verified on first make"}')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
