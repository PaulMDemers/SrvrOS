"""Validate that the runtime build receipt still describes the inputs in use."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def verify_runtime():
    receipt_path = ROOT / 'build/browser-runtime/receipt.json'
    receipt = json.loads(receipt_path.read_text())
    hashes = receipt['sha256']
    required = ('build/tooling/zig/zig', 'ports/srvros/libcxx/config.h',
                'ports/srvros/libcxx/build.py', 'build/sysroot/srvros/lib/libsrvros.a',
                'build/sysroot/srvros/lib/crt0.o', 'build/sysroot/srvros/lib/app.ld',
                'build/browser-runtime/libcxx-threaded.a',
                'build/browser-runtime/cxxthreads.elf',
                'build/browser-runtime/cxxthreads-tlsfirst.elf')
    if not all(key in hashes for key in required):
        raise ValueError('runtime receipt missing required inputs or artifacts')
    for name, expected in hashes.items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts:
            raise ValueError('unsafe runtime receipt path: ' + name)
        if not (ROOT / path).is_file() or sha(ROOT / path) != expected:
            raise ValueError('runtime receipt mismatch: ' + name)
    tree = hashlib.sha256()
    sdk = ROOT / 'build/sysroot/srvros'
    for header in sorted((sdk / 'include').rglob('*')):
        if header.is_file():
            tree.update(str(header.relative_to(sdk)).encode() + b'\0' + bytes.fromhex(sha(header)))
    if receipt.get('sdk_headers_sha256') != tree.hexdigest():
        raise ValueError('runtime receipt SDK header tree mismatch or absent')
    return {'path': str(receipt_path), 'sha256': sha(receipt_path),
            'sdk_headers_sha256': tree.hexdigest()}
