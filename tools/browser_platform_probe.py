#!/usr/bin/env python3
"""Apply the pinned SrvrOS overlay and record the next real no-JIT JSC dependency."""
import argparse
import json
import os
from pathlib import Path, PurePosixPath
import signal
import subprocess
import tarfile
import tempfile
import time
import shutil

from browser_probe import ARCHIVE, DIGEST, LIMIT, ROOT, SIZE, sha
from browser_evidence import verify_runtime, verify_icu


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--icu', action='store_true', help='require and use the verified pinned ICU port')
    args = parser.parse_args()
    base = ROOT / 'build/browser-platform'
    base.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='run-', dir=base))
    port = ROOT / 'ports/srvros/webkit'
    receipt = {'scope': 'patched no-JIT JSC configure, actual executable feature links; no engine runtime',
               'commands': [], 'inputs': {}}
    start = time.monotonic()
    env = os.environ.copy()
    for key in ('CFLAGS', 'CXXFLAGS', 'LDFLAGS', 'CPPFLAGS', 'ICU_ROOT',
                'CMAKE_PREFIX_PATH', 'PKG_CONFIG_PATH', 'PKG_CONFIG_SYSROOT_DIR'):
        env.pop(key, None)
    env['PKG_CONFIG_LIBDIR'] = str(run / 'empty-pkgconfig')
    env['PKG_CONFIG_SYSROOT_DIR'] = str(ROOT / 'build/sysroot/srvros')

    def command(name, args):
        args = [str(x) for x in args]
        proc = subprocess.Popen(args, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                start_new_session=True, env=env)
        try:
            out, err = proc.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            out, err = proc.communicate()
            err += b'\nTIMEOUT: subprocess group killed\n'
        (run / f'{name}.stdout').write_bytes(out)
        (run / f'{name}.stderr').write_bytes(err)
        receipt['commands'].append({'name': name, 'argv': args, 'status': proc.returncode})
        return proc.returncode, (out + err).decode(errors='replace')

    try:
        if shutil.disk_usage(base).free < 2 << 30:
            raise RuntimeError('need 2 GiB free for the bounded extraction')
        if not ARCHIVE.is_file() or ARCHIVE.stat().st_size != SIZE or sha(ARCHIVE) != DIGEST:
            raise RuntimeError('verified archive required; first run tools/browser_probe.py --fetch')
        receipt['runtime_provenance'] = verify_runtime()
        icu_options = []
        if args.icu:
            receipt['icu_provenance'] = verify_icu()
            icu_root = ROOT / 'build/browser-icu/sysroot'
            icu_options = ['-DSRVROS_ICU_ROOT=' + str(icu_root), '-DICU_ROOT=' + str(icu_root)]
        paths = [ARCHIVE, ROOT / 'build/sysroot/srvros/lib/libsrvros.a',
                 ROOT / 'build/sysroot/srvros/lib/crt0.o',
                 ROOT / 'build/browser-runtime/libcxx-threaded.a', Path(__file__).resolve(),
                 ROOT / 'tools/browser_evidence.py',
                 *[port / name for name in ('PORT.md', 'cc', 'cxx', 'compiler.py',
                                            'srvros-platform.patch', 'toolchain.cmake')],
                 ROOT / 'ports/srvros/libcxx/config.h']
        for path in paths:
            if path.is_file():
                receipt['inputs'][str(path.relative_to(ROOT))] = sha(path)
            else:
                raise RuntimeError(f'missing input: {path}')
        receipt['source'] = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        receipt['dirty'] = subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT, text=True)
        with tempfile.TemporaryDirectory(prefix='source-', dir=base) as temporary:
            source = Path(temporary) / 'wpewebkit-2.54.2'
            with tarfile.open(ARCHIVE, 'r:xz') as archive:
                total = 0
                for member in archive:
                    path = PurePosixPath(member.name)
                    if path.is_absolute() or '..' in path.parts:
                        raise RuntimeError('unsafe archive path')
                    if member.issym() or member.islnk():
                        receipt.setdefault('excluded_links', []).append(member.name)
                        continue
                    if not (member.isdir() or member.isfile()):
                        raise RuntimeError('special archive member')
                    total += member.size
                    if total > LIMIT:
                        raise RuntimeError('source exceeds 1 GiB bound')
                    destination = Path(temporary).joinpath(*path.parts)
                    if member.isdir():
                        destination.mkdir(parents=True, exist_ok=True)
                    else:
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        with archive.extractfile(member) as incoming, destination.open('wb') as outgoing:
                            shutil.copyfileobj(incoming, outgoing)
            receipt['extracted_bytes'] = total
            code, _ = command('patch', ['patch', '--batch', '--fuzz=0', '-p1', '-d', source,
                                         '-i', port / 'srvros-platform.patch'])
            if code:
                raise RuntimeError('pinned patch did not apply exactly')
            c_control = run / 'pthread-control.c'
            c_control.write_text('#include <pthread.h>\n'
                                 'static void *work(void *p) { return p; }\n'
                                 'int main(void) { pthread_t t; return pthread_create(&t, 0, work, 0) || pthread_join(t, 0); }\n')
            code, _ = command('pthread_control', [port / 'cc', c_control, '-o', run / 'pthread.elf'])
            if code:
                raise RuntimeError('pthread create/join compile-link control failed')
            receipt['pthread_elf_sha256'] = sha(run / 'pthread.elf')
            control = run / 'platform.cpp'
            control.write_text('#include <wtf/Platform.h>\n#include <mutex>\n#include <thread>\n'
                               '#if !OS(SRVROS) || !OS(UNIX) || OS(LINUX)\n'
                               '#error incorrect SrvrOS platform identity\n#endif\n'
                               'static std::mutex mutex;\n'
                               'extern "C" int main() { std::thread t([] { std::lock_guard<std::mutex> lock(mutex); }); t.join(); return 0; }\n')
            code, _ = command('platform_control', [port / 'cxx', '-I', source / 'Source/WTF',
                                                   control, '-o', run / 'platform.elf'])
            if code:
                raise RuntimeError('platform identity / threaded C++ compile-link control failed')
            receipt['platform_elf_sha256'] = sha(run / 'platform.elf')
            code, log = command('configure', ['cmake', '-S', source, '-B', run / 'cmake', '-G', 'Ninja',
                                             '-DCMAKE_TOOLCHAIN_FILE=' + str(port / 'toolchain.cmake'),
                                             '-DPORT=JSCOnly', '-DENABLE_STATIC_JSC=ON',
                                             '-DENABLE_JIT=OFF', '-DENABLE_WEBASSEMBLY=OFF',
                                             '-DENABLE_REMOTE_INSPECTOR=OFF', '-DUSE_SYSTEM_MALLOC=ON',
                                             '-DUSE_HEADER_MAPS=OFF',
                                             *icu_options])
            if args.icu:
                cache = {}
                for line in (run / 'cmake/CMakeCache.txt').read_text().splitlines():
                    if ':' in line and '=' in line and not line.startswith(('//', '#')):
                        key, value = line.split('=', 1)
                        cache[key.split(':', 1)[0]] = value
                expected_icu = {
                    'ICU_INCLUDE_DIR': icu_root / 'include',
                    'ICU_DATA_LIBRARY_RELEASE': icu_root / 'lib/libicudata.a',
                    'ICU_I18N_LIBRARY_RELEASE': icu_root / 'lib/libicui18n.a',
                    'ICU_UC_LIBRARY_RELEASE': icu_root / 'lib/libicuuc.a',
                }
                receipt['icu_discovery'] = {key: cache.get(key) for key in expected_icu}
                if any(cache.get(key) != str(value) for key, value in expected_icu.items()):
                    raise RuntimeError('configure did not discover exactly the verified target ICU install')
            if code == 0:
                receipt['result'] = 'configured; next gate is compilation, not executed here'
            elif 'Could NOT find Threads' in log:
                receipt['result'] = 'configure stopped at Threads check; inspect logs for cause'
            elif ('Could NOT find ICU' in log or 'Failed to find all ICU components' in log):
                receipt['result'] = 'configure stopped at ICU check; inspect logs for cause'
            else:
                raise RuntimeError('configuration stopped before a recognized dependency; inspect logs')
    except (OSError, RuntimeError, ValueError, KeyError, subprocess.SubprocessError, tarfile.TarError) as error:
        receipt['result'], receipt['error'] = 'failed', str(error)
    finally:
        receipt['seconds'] = round(time.monotonic() - start, 3)
        (run / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Receipt:', run / 'receipt.json')
    print('Result:', receipt['result'])
    return receipt['result'] == 'failed'


if __name__ == '__main__':
    raise SystemExit(main())
