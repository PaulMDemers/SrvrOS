#!/usr/bin/env python3
"""Retained, bounded static no-JIT JSCOnly cross-build experiment."""
import argparse
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
import time
from browser_probe import ARCHIVE, DIGEST, LIMIT, ROOT, SIZE, sha
from browser_evidence import verify_runtime, verify_icu

base = ROOT / 'build/browser-jsc'
port = ROOT / 'ports/srvros/webkit'
icu = ROOT / 'build/browser-icu/sysroot'
reserve = 2 * 1024**3
budget = 6 * 1024**3
base.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--resume', type=Path, help='resume a previously prepared run directory')
args = parser.parse_args()
run = args.resume.resolve() if args.resume else Path(tempfile.mkdtemp(prefix='run-', dir=base))
if run.parent != base.resolve() or not run.name.startswith('run-') or not run.is_dir():
    parser.error('resume directory must be a run-* directory under build/browser-jsc')
source = run / 'wpewebkit-2.54.2'
build = run / 'cmake'
receipt = {'scope': 'target JSCOnly build; no guest runtime or browser claim',
           'source_observation_only': '', 'commands': [], 'result': 'failed', 'inputs': {}}
env = os.environ.copy()
for key in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS',
            'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH',
            'CONFIG_SITE', 'MAKEFLAGS', 'MFLAGS', 'ICU_ROOT', 'CMAKE_PREFIX_PATH',
            'PKG_CONFIG_PATH', 'PKG_CONFIG_LIBDIR', 'PKG_CONFIG_SYSROOT_DIR',
            'DYLD_LIBRARY_PATH', 'LD_LIBRARY_PATH', 'ZIG_LIB_DIR', 'ZIG_LIBC'):
    env.pop(key, None)
env['PKG_CONFIG_LIBDIR'] = str(run / 'empty-pkgconfig')
env['PKG_CONFIG_SYSROOT_DIR'] = str(ROOT / 'build/sysroot/srvros')
env['ZIG_GLOBAL_CACHE_DIR'] = str(run / 'zig-global-cache')
env['ZIG_LOCAL_CACHE_DIR'] = str(run / 'zig-local-cache')

def check_space():
    if shutil.disk_usage(base).free < reserve:
        raise RuntimeError('2 GiB free-disk reserve reached')
    size = subprocess.check_output(['du', '-sk', str(run)], text=True).split()[0]
    if int(size) * 1024 > budget:
        raise RuntimeError('6 GiB build directory budget reached')

def command(name, argv, timeout=3600):
    check_space()
    logpath = run / (name + '-' + str(time.time_ns()) + '.log')
    argv = [str(x) for x in argv]
    with logpath.open('wb') as log:
        log.write((json.dumps(argv) + '\n').encode()); log.flush()
        proc = subprocess.Popen(argv, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        start = time.monotonic(); reason = None; next_size = 0
        try:
            while proc.poll() is None:
                now = time.monotonic()
                if shutil.disk_usage(base).free < reserve:
                    reason = 'disk reserve'
                elif now - start > timeout:
                    reason = 'timeout'
                elif now >= next_size:
                    try: check_space()
                    except RuntimeError as error: reason = str(error)
                    next_size = now + 10
                if reason:
                    os.killpg(proc.pid, signal.SIGTERM)
                    try: proc.wait(timeout=5)
                    except subprocess.TimeoutExpired: os.killpg(proc.pid, signal.SIGKILL); proc.wait()
                    break
                time.sleep(1)
        except BaseException:
            os.killpg(proc.pid, signal.SIGTERM)
            try: proc.wait(timeout=5)
            except subprocess.TimeoutExpired: os.killpg(proc.pid, signal.SIGKILL); proc.wait()
            raise
        receipt['commands'].append({'name': name, 'argv': argv, 'status': proc.returncode,
                                    'seconds': round(time.monotonic()-start, 3),
                                    'log': str(logpath.relative_to(ROOT)), 'stop_reason': reason})
        if reason or proc.returncode:
            raise RuntimeError(name + ' failed; inspect ' + str(logpath))

try:
    started = time.monotonic()
    check_space()
    receipt['runtime'] = verify_runtime(); receipt['icu'] = verify_icu()
    if not ARCHIVE.is_file() or ARCHIVE.stat().st_size != SIZE or sha(ARCHIVE) != DIGEST:
        raise RuntimeError('pinned WPE archive absent or invalid')
    receipt['source_observation_only'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    receipt['dirty_observation_only'] = subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)
    for path in [Path(__file__), ARCHIVE, *sorted(port.glob('*'))]:
        if path.is_file(): receipt['inputs'][str(path.relative_to(ROOT))] = sha(path)
    if not args.resume:
        total = 0
        with tarfile.open(ARCHIVE, 'r:xz') as archive:
            for member in archive:
                name = PurePosixPath(member.name)
                if name.is_absolute() or '..' in name.parts: raise RuntimeError('unsafe archive path')
                if member.issym() or member.islnk():
                    receipt.setdefault('excluded_links', []).append(member.name); continue
                if not (member.isdir() or member.isfile()): raise RuntimeError('special archive member')
                total += member.size
                if total > LIMIT: raise RuntimeError('source exceeds extraction limit')
                dest = run.joinpath(*name.parts)
                if member.isdir(): dest.mkdir(parents=True, exist_ok=True)
                else:
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    with archive.extractfile(member) as inp, dest.open('wb') as out:
                        while True:
                            block = inp.read(1024*1024)
                            if not block: break
                            if shutil.disk_usage(base).free < reserve: raise RuntimeError('disk reserve during extraction')
                            out.write(block)
                    dest.chmod(member.mode & 0o777)
        receipt['extracted_bytes'] = total
        command('patch', ['patch','--batch','--fuzz=0','-p1','-d',source,'-i',port/'srvros-platform.patch'])
    elif not (source / 'CMakeLists.txt').is_file():
        raise RuntimeError('resume source tree missing')
    command('configure', ['cmake','-S',source,'-B',build,'-G','Ninja',
        '-DCMAKE_TOOLCHAIN_FILE='+str(port/'toolchain.cmake'), '-DPORT=JSCOnly',
        '-DSRVROS_ICU_ROOT='+str(icu), '-DICU_ROOT='+str(icu), '-DENABLE_STATIC_JSC=ON',
        '-DENABLE_JIT=OFF', '-DENABLE_WEBASSEMBLY=OFF', '-DENABLE_REMOTE_INSPECTOR=OFF',
        '-DUSE_SYSTEM_MALLOC=ON', '-DUSE_HEADER_MAPS=OFF', '-DENABLE_UNIFIED_BUILDS=OFF'])
    command('build-jsc', ['cmake','--build',build,'--target','jsc','--parallel','2'])
    receipt['result'] = 'built; not executed'
except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError, tarfile.TarError) as error:
    receipt['error'] = str(error)
finally:
    receipt['seconds'] = round(time.monotonic() - started, 3) if 'started' in locals() else 0
    path = run / ('receipt-' + str(time.time_ns()) + '.json')
    path.write_text(json.dumps(receipt, indent=2) + '\n')
    (run / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Receipt:', path, 'result:', receipt['result'])
if receipt['result'] != 'built; not executed': sys.exit(1)
