#!/usr/bin/env python3
"""Reproduce host float checks and the target math guest without rebuilding the SDK."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

from browser_evidence import verify_runtime

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/browser-runtime'
SDK = ROOT / 'build/sysroot/srvros'
OUT.mkdir(parents=True, exist_ok=True)
run = Path(tempfile.mkdtemp(prefix='math-', dir=OUT))
receipt = {'result': 'failed', 'scope': 'host numerical comparison and target compile/link; guest not run',
           'commands': [], 'sha256': {}, 'guest_marker': 'mathprobe: ok'}

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def command(name, args, env):
    args = list(map(str, args))
    start = time.monotonic()
    log_path = run / (name + '.log')
    with log_path.open('w') as log:
        log.write(json.dumps(args) + '\n')
        log.flush()
        try:
            proc = subprocess.Popen(args, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                status = proc.wait(timeout=60)
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                proc.wait()
                status = None
                log.write('TIMEOUT: process group killed\n')
        except OSError as exc:
            status = None
            log.write(str(exc) + '\n')
    receipt['commands'].append({'name': name, 'argv': args, 'status': status, 'seconds': round(time.monotonic()-start, 3), 'log': str(log_path.relative_to(ROOT))})
    if status != 0:
        raise RuntimeError('command failed: ' + str(log_path))

try:
    runtime = verify_runtime()
    rtti_path = OUT / 'rtti-receipt.json'
    rtti = json.loads(rtti_path.read_text())
    if rtti.get('result') != 'success' or rtti.get('sdk_headers_sha256') != runtime['sdk_headers_sha256']:
        raise ValueError('RTTI receipt not successful or SDK header mismatch')
    required = ('build/browser-runtime/libcxxabi-rtti.a', 'build/browser-runtime/rttiprobe.elf',
                'build/browser-runtime/libcxx-threaded.a', 'build/sysroot/srvros/lib/libsrvros.a',
                'ports/srvros/libcxx/config.h', 'ports/srvros/libcxx/build_rtti.py', 'ports/srvros/libcxx/rtti_guest.cc')
    hashes = rtti['sha256']
    if not all(name in hashes for name in required):
        raise ValueError('RTTI receipt missing required inputs')
    for name, digest in hashes.items():
        p = Path(name)
        if p.is_absolute() or '..' in p.parts or not (ROOT / p).is_file() or sha(ROOT / p) != digest:
            raise ValueError('RTTI receipt mismatch: ' + name)
    cc = shutil.which('cc')
    if not cc:
        raise RuntimeError('host cc unavailable')
    env = os.environ.copy()
    for key in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS', 'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'SDKROOT', 'MACOSX_DEPLOYMENT_TARGET'):
        env.pop(key, None)
    env['ZIG_LOCAL_CACHE_DIR'] = str(run / 'zig-cache')
    env['ZIG_GLOBAL_CACHE_DIR'] = str(run / 'zig-global-cache')
    obj = run / 'math-host.o'
    host = run / 'math-host'
    source = ROOT / 'userspace/lib/src/posix_math_hyperbolic.c'
    command('host-compile', [cc, '-std=c11', '-O2', '-fno-builtin', '-Dtanhf=srv_test_tanhf', '-Dexpm1f=srv_test_expm1f', '-c', source, '-o', obj], env)
    command('host-link', [cc, '-std=c11', '-O2', '-fno-builtin', ROOT/'tools/test_math_hyperbolic.c', obj, '-lm', '-o', host], env)
    command('host-test', [host], env)
    if 'math-hyperbolic-host: ok' not in (run/'host-test.log').read_text():
        raise ValueError('host success marker missing')
    guest = OUT/'mathprobe.elf'
    command('guest-link', [ROOT/'ports/srvros/webkit/cc', '-std=c11', '-fno-builtin', ROOT/'tools/mathprobe.c', '-o', guest], env)
    sources = [Path(__file__).resolve(), source, ROOT/'userspace/lib/include/math.h', ROOT/'tools/test_math_hyperbolic.c', ROOT/'tools/mathprobe.c', ROOT/'build/tooling/zig/zig', ROOT/'build/tooling/zig/lib/libc/musl/src/math/tanhf.c', ROOT/'build/tooling/zig/lib/libc/musl/src/math/expm1f.c', ROOT/'ports/srvros/webkit/cc', ROOT/'ports/srvros/webkit/compiler.py', SDK/'lib/libsrvros.a', SDK/'lib/crt0.o', SDK/'lib/app.ld', OUT/'receipt.json', rtti_path, OUT/'libcxx-threaded.a', OUT/'libcxxabi-rtti.a', guest, host, run/'host-test.log']
    receipt['sha256'] = {str(p.relative_to(ROOT)): sha(p) for p in sources}
    receipt['sdk_headers_sha256'] = runtime['sdk_headers_sha256']
    receipt['runtime_provenance'] = runtime
    receipt['host_test'] = '3 ULP maximum; edge and threshold values plus 200000 deterministic bit patterns'
    receipt['result'] = 'success'
except (OSError, KeyError, ValueError, RuntimeError, json.JSONDecodeError) as exc:
    receipt['error'] = str(exc)
finally:
    data = json.dumps(receipt, indent=2) + '\n'
    (run/'receipt.json').write_text(data)
    (OUT/'math-receipt.json').write_text(data)
    print('Math receipt:', OUT/'math-receipt.json', 'result:', receipt['result'])
sys.exit(0 if receipt['result'] == 'success' else 1)
