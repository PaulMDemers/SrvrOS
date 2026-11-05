#!/usr/bin/env python3
"""Record bounded compile-only WPEWebKit SDK evidence; this is not a port test."""
import argparse
import datetime
import hashlib
import json
import os
import platform
from pathlib import Path, PurePosixPath
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
BASE = ROOT / 'build/browser-probe'
ARCHIVE = BASE / 'wpewebkit-2.54.2.tar.xz'
URL = 'https://wpewebkit.org/releases/wpewebkit-2.54.2.tar.xz'
SIZE = 49565492
DIGEST = 'f3284e19e47fcb5ac8c63c2e1c113dbd65b2423eb8a545f5b345707829004f56'
LIMIT = 1 << 30


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    started = time.monotonic()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fetch', action='store_true', help='explicitly download the pinned source archive')
    args = parser.parse_args()
    BASE.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix='run-', dir=BASE))
    receipt = {'scope': 'cross compile/link and modeled host libc contract only; no engine port or guest runtime claim', 'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'host_arch': platform.machine(), 'host_platform': platform.platform(), 'archive_url': URL, 'tests': [], 'artifacts': {}}
    def save():
        receipt['wall_duration_seconds'] = round(time.monotonic() - started, 3)
        (run / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    def fail(message):
        receipt['error'] = message
        save()
        print(message, file=sys.stderr)
        print('Receipt:', run / 'receipt.json')
        return 2
    zig = ROOT / 'build/tooling/zig/zig'
    sdk = ROOT / 'build/sysroot/srvros'
    required = [zig, sdk / '.ready', sdk / 'lib/crt0.o', sdk / 'lib/libsrvros.a', sdk / 'lib/app.ld']
    if not all(p.is_file() for p in required):
        return fail('SDK missing; run: make srvros-sysroot')
    if shutil.disk_usage(BASE).free < 2 * (1 << 30):
        return fail('At least 2 GiB free space is required.')
    if args.fetch:
        try:
            # Each public read has a 10 s socket timeout; the deadline is checked
            # between reads, so the practical upper bound is about 130 s.
            deadline = time.monotonic() + 120
            with urllib.request.urlopen(URL, timeout=10) as src, tempfile.NamedTemporaryFile(dir=BASE, delete=False) as dst:
                temp = Path(dst.name)
                total = 0
                while True:
                    if time.monotonic() >= deadline:
                        raise TimeoutError('download exceeded 120 seconds')
                    block = src.read1(1024 * 1024)
                    if not block:
                        break
                    total += len(block)
                    if total > SIZE:
                        raise ValueError('download exceeds pinned size')
                    dst.write(block)
            if total != SIZE or sha(temp) != DIGEST:
                raise ValueError('download size or SHA-256 mismatch')
            os.replace(temp, ARCHIVE)
        except Exception as exc:
            if 'temp' in locals():
                temp.unlink(missing_ok=True)
            return fail('Download failed: ' + str(exc))
    if not ARCHIVE.is_file() or ARCHIVE.stat().st_size != SIZE or sha(ARCHIVE) != DIGEST:
        return fail('Archive absent or invalid; use --fetch to download the pinned archive.')
    receipt['archive'] = {'path': str(ARCHIVE), 'size': SIZE, 'sha256': DIGEST}
    for p in [Path(__file__).resolve(), ROOT / 'tools/test_browser_memory_contract.c', zig, sdk / 'lib/crt0.o', sdk / 'lib/libsrvros.a', sdk / 'lib/app.ld']:
        receipt['artifacts'][str(p.relative_to(ROOT))] = sha(p)
    def command(name, cmd, expected=None):
        start = time.monotonic()
        try:
            proc = subprocess.Popen([str(x) for x in cmd], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
            try:
                out, err = proc.communicate(timeout=60)
                code = proc.returncode
            except subprocess.TimeoutExpired:
                os.killpg(proc.pid, signal.SIGKILL)
                out, err = proc.communicate()
                code = None
                err += b'\nTIMEOUT: process group killed\n'
        except OSError as e:
            code, out, err = None, b'', str(e).encode()
        (run / (name + '.command.json')).write_text(json.dumps([str(x) for x in cmd], indent=2) + '\n')
        (run / (name + '.stdout')).write_bytes(out)
        (run / (name + '.stderr')).write_bytes(err)
        ok = code == 0 if expected is None else code not in (0, None) and expected.encode() in err
        receipt['tests'].append({'name': name, 'returncode': code, 'duration_seconds': round(time.monotonic()-start, 3), 'expected_diagnostic': expected, 'matched': ok})
        print(name + ': ' + ('matched' if ok else 'UNEXPECTED'))
        return ok
    for name, cmd in [('git_head', ['git', 'rev-parse', 'HEAD']), ('git_status', ['git', 'status', '--porcelain']), ('zig_version', [zig, 'version'])]:
        if not command(name, cmd):
            return fail(name + ' failed')
        receipt[name] = (run / (name + '.stdout')).read_text().strip()
    tree = hashlib.sha256()
    for header in sorted((sdk / 'include').rglob('*')):
        if header.is_file():
            tree.update(str(header.relative_to(sdk)).encode() + b'\0' + bytes.fromhex(sha(header)))
    receipt['sdk_include_tree_sha256'] = tree.hexdigest()
    def source(name, content):
        p = run / name
        p.write_text(content)
        return p
    try:
        with tempfile.TemporaryDirectory(prefix='source-', dir=BASE) as temporary:
            # Extract regular files and directories only, never materialize archive links.
            with tarfile.open(ARCHIVE, 'r:xz') as tar:
                total = 0
                for member in tar:
                    path = PurePosixPath(member.name)
                    if path.is_absolute() or '..' in path.parts:
                        raise ValueError('unsafe archive path: ' + member.name)
                    if member.issym() or member.islnk():
                        receipt.setdefault('excluded_links', []).append({'name': member.name, 'target': member.linkname})
                        continue  # Never extract links, including the unrelated Skia link.
                    if not (member.isfile() or member.isdir()):
                        raise ValueError('link or special file in selected tree: ' + member.name)
                    total += member.size
                    if total > LIMIT:
                        raise ValueError('extracted data exceeds 1 GiB')
                    dest = Path(temporary).joinpath(*path.parts)
                    if member.isdir():
                        dest.mkdir(parents=True, exist_ok=True)
                    else:
                        dest.parent.mkdir(parents=True, exist_ok=True)
                        with tar.extractfile(member) as src, dest.open('wb') as dst:
                            shutil.copyfileobj(src, dst, 1024 * 1024)
            receipt['extracted_regular_bytes'] = total
            wtf = Path(temporary) / 'wpewebkit-2.54.2/Source/WTF'
            for header in [wtf / 'wtf/Platform.h', wtf / 'wtf/Threading.h']:
                receipt['artifacts'][str(header.relative_to(Path(temporary)))] = sha(header)
            common = [zig, 'cc', '-target', 'x86_64-freestanding-none', '-ffreestanding', '-fno-sanitize=undefined', '-fno-stack-protector', '-mno-red-zone', '-I', sdk / 'include']
            cpp = [zig, 'c++', '-target', 'x86_64-freestanding-none', '-std=c++23', '-ffreestanding', '-fno-sanitize=undefined', '-fno-stack-protector', '-mno-red-zone', '-fno-exceptions', '-fno-rtti', '-D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE', '-DUSE_SYSTEM_MALLOC=1', '-nostdinc++', '-I', ROOT / 'build/tooling/zig/lib/libcxx/include', '-I', ROOT / 'build/tooling/zig/lib/libcxxabi/include', '-I', ROOT / 'build/tooling/zig/lib/include', '-isystem', sdk / 'include']
            link = [zig, 'ld.lld', '-m', 'elf_x86_64', '-nostdlib', '-static', '-z', 'max-page-size=0x1000', '--gc-sections', '-T', sdk / 'lib/app.ld', sdk / 'lib/crt0.o']
            checks = []
            for name, code in [('c_control', 'int main(void) { return 0; }\n'), ('c_api', '#include <sys/mman.h>\n#include <sys/socket.h>\n#include <pthread.h>\nint main(void) { int f[2]; pthread_mutex_t m; pthread_mutex_init(&m,0); pthread_mutex_lock(&m); pthread_mutex_unlock(&m); pthread_mutex_destroy(&m); void *p=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0); return socketpair(AF_UNIX,SOCK_STREAM,0,f) + munmap(p,4096); }\n')]:
                src = source(name + '.c', code); obj = run / (name + '.o'); binary = run / (name + '.elf')
                checks.append(command(name + '_compile', common + ['-c', src, '-o', obj]))
                checks.append(command(name + '_link', link + [obj, sdk / 'lib/libsrvros.a', '-o', binary]))
                if binary.is_file(): receipt['artifacts'][binary.name] = sha(binary)
            tests = [
                ('cpp_builtin_atomic', 'static_assert(__atomic_always_lock_free(4, nullptr)); int value; extern "C" int main() { return __atomic_fetch_add(&value, 1, __ATOMIC_SEQ_CST); }\n', None),
                ('cpp_atomic', '#include <atomic>\nstd::atomic<int> value{0}; int f() { return value.fetch_add(1); }\n', None),
                ('webkit_platform', '#include <wtf/Platform.h>\nint x;\n', None),
                ('webkit_unix', '#include <wtf/Platform.h>\n#if !OS(UNIX)\n#error SRVROS_PROBE_WEBKIT_DOES_NOT_RECOGNIZE_UNIX\n#endif\n', 'SRVROS_PROBE_WEBKIT_DOES_NOT_RECOGNIZE_UNIX'),
                ('cpp_mutex', '#include <mutex>\nstd::mutex value;\n', "no type named 'mutex'"),
                ('webkit_threading', '#include <wtf/Threading.h>\nint x;\n', 'unicode/utypes.h'),
            ]
            for name, code, expected in tests:
                src = source(name + '.cpp', code)
                checks.append(command(name, cpp + ['-I', wtf, '-I', wtf.parent / 'bmalloc', '-c', src, '-o', run / (name + '.o')], expected))
            checks.append(command('cpp_builtin_atomic_link', link + [run / 'cpp_builtin_atomic.o', sdk / 'lib/libsrvros.a', '-o', run / 'cpp_builtin_atomic.elf']))
            for artifact in run.glob('*.elf'):
                receipt['artifacts'][artifact.name] = sha(artifact)
            toolchain = source('toolchain.cmake', f'''set(CMAKE_SYSTEM_NAME SrvrOS)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "{zig}")
set(CMAKE_C_COMPILER_ARG1 cc)
set(CMAKE_CXX_COMPILER "{zig}")
set(CMAKE_CXX_COMPILER_ARG1 c++)
set(CMAKE_C_COMPILER_TARGET x86_64-freestanding-none)
set(CMAKE_CXX_COMPILER_TARGET x86_64-freestanding-none)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_FLAGS_INIT "-ffreestanding -isystem {sdk / 'include'}")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -fno-sanitize=undefined -fno-stack-protector -mno-red-zone -fno-exceptions -fno-rtti -D_LIBCPP_HARDENING_MODE_DEFAULT=_LIBCPP_HARDENING_MODE_NONE -DUSE_SYSTEM_MALLOC=1 -nostdinc++ -I {ROOT / 'build/tooling/zig/lib/libcxx/include'} -I {ROOT / 'build/tooling/zig/lib/libcxxabi/include'} -I {ROOT / 'build/tooling/zig/lib/include'} -isystem {sdk / 'include'}")
set(CMAKE_FIND_ROOT_PATH "{sdk}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
''')
            cmake = shutil.which('cmake'); ninja = shutil.which('ninja')
            if not cmake or not ninja:
                raise ValueError('cmake and ninja are required')
            checks.append(command('jsc_configure', [cmake, '-G', 'Ninja', '-S', Path(temporary) / 'wpewebkit-2.54.2', '-B', run / 'cmake-build', '-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain), '-DPORT=JSCOnly', '-DENABLE_STATIC_JSC=ON', '-DENABLE_JIT=OFF', '-DENABLE_WEBASSEMBLY=OFF', '-DUSE_SYSTEM_MALLOC=ON', '-DENABLE_REMOTE_INSPECTOR=OFF'], "Unknown OS 'SrvrOS'"))
            receipt['configure_scope'] = 'STATIC_LIBRARY compiler checks only; no engine link or runtime evidence'
            host_test = ROOT / 'tools/test_browser_memory_contract.c'
            if host_test.is_file():
                cc = shutil.which('cc')
                if not cc or (platform.machine() not in ('x86_64', 'AMD64') and not (sys.platform == 'darwin' and platform.machine() == 'arm64')):
                    raise ValueError('host cc or supported architecture unavailable')
                host_cmd = [cc] + (['-arch', 'x86_64'] if sys.platform == 'darwin' else []) + ['-std=c11', '-I', ROOT / 'userspace/lib/include', '-I', ROOT / 'shared/include', host_test, '-o', run / 'memory-contract']
                compiled = command('memory_contract_compile', host_cmd)
                checks.append(compiled)
                if compiled:
                    checks.append(command('memory_contract_run', [run / 'memory-contract']))
                    output = (run / 'memory_contract_run.stdout').read_text(errors='replace')
                    signature = 'browser-memory: shared rejected; discard retains bytes; noreserve stripped'
                    matched = signature in output
                    receipt['tests'][-1]['expected_stdout'] = signature
                    receipt['tests'][-1]['matched'] &= matched
                    checks.append(matched)
            else:
                raise ValueError('tools/test_browser_memory_contract.c missing')
            receipt['result'] = 'expected signatures matched' if all(checks) else 'unexpected failure or changed signature'
            save()
            print('Receipt:', run / 'receipt.json')
            return 0 if all(checks) else 1
    except (OSError, ValueError, tarfile.TarError) as exc:
        return fail('Probe setup failed: ' + str(exc))


if __name__ == '__main__':
    sys.exit(main())
