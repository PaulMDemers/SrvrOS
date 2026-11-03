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


def verify_icu():
    """Reject partial or stale ICU installs before configure or guest boot."""
    receipt_path = ROOT / 'build/browser-icu/receipt.json'
    receipt = json.loads(receipt_path.read_text())
    if receipt.get('result') != 'success':
        raise ValueError('ICU receipt does not record a successful build')
    hashes = receipt['sha256']
    required = ('build/browser-icu/icuprobe.elf',
                'build/browser-runtime/libcxxabi-rtti.a',
                'build/browser-runtime/rttiprobe.elf',
                'build/browser-runtime/rtti-receipt.json',
                'ports/srvros/libcxx/build_rtti.py',
                'ports/srvros/libcxx/rtti_guest.cc',
                'build/browser-runtime/math-receipt.json',
                'build/browser-runtime/mathprobe.elf',
                'userspace/lib/src/posix_math_hyperbolic.c',
                'userspace/lib/include/math.h',
                'tools/browser_math_probe.py', 'tools/mathprobe.c',
                'tools/test_math_hyperbolic.c',
                'build/browser-icu/sysroot/lib/libicuuc.a',
                'build/browser-icu/sysroot/lib/libicui18n.a',
                'build/browser-icu/sysroot/lib/libicudata.a',
                'build/browser-icu/sysroot/include/unicode/uvernum.h',
                'ports/srvros/icu/build.py', 'ports/srvros/icu/guest.cc',
                'build/sysroot/srvros/lib/libsrvros.a',
                'build/browser-runtime/libcxx-threaded.a')
    if not all(key in hashes for key in required):
        raise ValueError('ICU receipt missing required inputs or artifacts')
    for name, expected in hashes.items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts:
            raise ValueError('unsafe ICU receipt path: ' + name)
        if not (ROOT / path).is_file() or sha(ROOT / path) != expected:
            raise ValueError('ICU receipt mismatch: ' + name)
    sdk_headers = verify_runtime()['sdk_headers_sha256']
    for name in ('rtti', 'math'):
        dependency = json.loads((ROOT / f'build/browser-runtime/{name}-receipt.json').read_text())
        if (dependency.get('result') != 'success' or
                dependency.get('sdk_headers_sha256') != sdk_headers):
            raise ValueError(name + ' and threaded runtime SDK header provenance differs')
    # Include every installed file: adding an unrecorded target header/library
    # must not silently change configure discovery or linked guest behavior.
    for path in (ROOT / 'build/browser-icu/sysroot').rglob('*'):
        if path.is_file() and str(path.relative_to(ROOT)) not in hashes:
            raise ValueError('unrecorded ICU install file: ' + str(path))
    return {'path': str(receipt_path), 'sha256': sha(receipt_path)}
