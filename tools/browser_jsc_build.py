#!/usr/bin/env python3
"""Retained, bounded static no-JIT JSCOnly cross-build experiment."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path, PurePosixPath
import shutil
import signal
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
from browser_probe import ARCHIVE, DIGEST, LIMIT, ROOT, SIZE, sha
from browser_evidence import verify_runtime, verify_icu
from browser_jsc_elf import inspect_target_elf
from browser_storage import StorageGuard

base = ROOT / 'build/browser-jsc'
port = ROOT / 'ports/srvros/webkit'
icu = ROOT / 'build/browser-icu/sysroot'
budget = 6 * 1024**3
base.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--keep-going', action='store_true', help='diagnostic build: compile independent targets after up to 10 failures')
parser.add_argument('--resume', type=Path, help='resume a verified prepared run directory')
parser.add_argument('--refresh-runtime-proofs', metavar='REASON', help='controlled same-SDK runtime archive/proof refresh')
parser.add_argument('--refresh-sdk-headers', metavar='REASON', help='allow only the recorded semaphore/regex C++ linkage changes')
parser.add_argument('--filesystem-guest-receipt', type=Path, required=True, help='passing private filesystem guest receipt')
parser.add_argument('--math-guest-receipt', type=Path, required=True, help='passing guest receipt for math and C++ runtime extensions')
parser.add_argument('--add-patch', action='append', help='apply a new named .patch from ports/srvros/webkit after validating the retained tree')
parser.add_argument('--experimental-signal-context', action='store_true', help='opt in to the unqualified SrvrOS WTF signal/context overlay for disposable guest tests')
args = parser.parse_args()
if args.refresh_sdk_headers and (not args.resume or args.refresh_runtime_proofs or not args.refresh_sdk_headers.strip()):
    parser.error('--refresh-sdk-headers requires --resume, a reason, and no runtime-only refresh')
if args.refresh_runtime_proofs and (not args.resume or args.add_patch or not args.refresh_runtime_proofs.strip()):
    parser.error('--refresh-runtime-proofs requires --resume, a reason, and no added patch')
if args.add_patch and (not args.resume or any(Path(name).name != name or not name.endswith('.patch') for name in args.add_patch) or len(set(args.add_patch)) != len(args.add_patch)):
    parser.error('--add-patch requires --resume and unique simple .patch filenames')
run = args.resume.resolve() if args.resume else Path(tempfile.mkdtemp(prefix='run-', dir=base))
if run.parent != base.resolve() or not run.name.startswith('run-') or not run.is_dir():
    parser.error('resume directory must be a run-* directory under build/browser-jsc')
source = run / 'wpewebkit-2.54.2'
build = run / 'cmake'
receipt = {'scope': 'target JSCOnly build; no guest runtime or browser claim',
           'source_observation_only': '', 'commands': [], 'result': 'failed', 'inputs': {}}
storage = StorageGuard(run.resolve())
env = os.environ.copy()
for key in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS',
            'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH',
            'CONFIG_SITE', 'MAKEFLAGS', 'MFLAGS', 'ICU_ROOT', 'CMAKE_PREFIX_PATH',
            'PKG_CONFIG_PATH', 'PKG_CONFIG_LIBDIR', 'PKG_CONFIG_SYSROOT_DIR',
            'DYLD_LIBRARY_PATH', 'LD_LIBRARY_PATH', 'ZIG_LIB_DIR', 'ZIG_LIBC'):
    env.pop(key, None)
env['PYTHONDONTWRITEBYTECODE'] = '1'
env['PKG_CONFIG_LIBDIR'] = str(run / 'empty-pkgconfig')
env['PKG_CONFIG_SYSROOT_DIR'] = str(ROOT / 'build/sysroot/srvros')
env['ZIG_GLOBAL_CACHE_DIR'] = str(run / 'zig-global-cache')
env['ZIG_LOCAL_CACHE_DIR'] = str(run / 'zig-local-cache')

def verify_filesystem(runtime):
    fsdir = ROOT / 'build/browser-filesystem'
    proof = fsdir / 'receipt.json'
    data = json.loads(proof.read_text())
    if data.get('result') != 'success' or data.get('runtime', {}).get('sha256') != runtime['sha256'] or data.get('runtime', {}).get('sdk_headers_sha256') != runtime['sdk_headers_sha256']:
        raise ValueError('filesystem build/runtime proof mismatch')
    hashes = data['sha256']
    required = ('ports/srvros/libcxx/build_filesystem.py', 'ports/srvros/libcxx/filesystem_guest.cc',
                'ports/srvros/libcxx/patches/filesystem-copy.patch', 'ports/srvros/libcxx/config.h',
                'build/browser-filesystem/libcxx-filesystem.a', 'build/browser-filesystem/fsprobe.elf',
                'build/sysroot/srvros/lib/libsrvros.a', 'build/sysroot/srvros/lib/crt0.o',
                'build/sysroot/srvros/lib/app.ld', 'build/tooling/zig/zig',
                'build/browser-runtime/libcxx-threaded.a', 'build/browser-runtime/libcxxabi-rtti.a',
                'build/browser-runtime/rtti-receipt.json', 'ports/srvros/webkit/cxx',
                'ports/srvros/webkit/compiler.py', 'build/sysroot/srvros/include/sys/statvfs.h',
                'build/tooling/zig/lib/libcxx/src/filesystem/operations.cpp',
                'build/tooling/zig/lib/libcxx/src/filesystem/path.cpp')
    if not all(name in hashes for name in required): raise ValueError('filesystem proof missing required inputs')
    for name, expected in hashes.items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or not (ROOT/path).is_file() or sha(ROOT/path) != expected:
            raise ValueError('filesystem proof mismatch: ' + name)
    compilations = [c for c in data['commands'] if c['name'].startswith('compile-')]
    if len(compilations) < 11 or any(c['status'] != 0 or '-D_LIBCPP_HAS_FILESYSTEM=1' not in c['argv'] for c in compilations):
        raise ValueError('filesystem archive build flags or compilation proof missing')
    guest = args.filesystem_guest_receipt.resolve()
    if guest.name != 'receipt.json' or guest.parent.parent != fsdir.resolve() or not guest.parent.name.startswith('guest-'):
        raise ValueError('filesystem guest receipt must be a private guest run under build/browser-filesystem')
    observation = json.loads(guest.read_text())
    if observation.get('result') != 'passed' or observation.get('guest_stopped') is not True:
        raise ValueError('filesystem guest did not pass and stop')
    if observation.get('filesystem_provenance', {}).get('sha256') != sha(proof) or observation.get('runtime_provenance', {}).get('sha256') != runtime['sha256']:
        raise ValueError('filesystem guest used a different build or SDK')
    tests = observation.get('tests', [])
    if len(tests) != 2 or any(t.get('passed') is not True or t.get('status') != '0' for t in tests):
        raise ValueError('filesystem guest requires two passing runs')
    for name in ('kernel', 'init', 'fsprobe', 'harness'):
        artifact = observation['artifacts'][name]
        path = Path(artifact['path']).resolve()
        if not path.is_relative_to(ROOT) or not path.is_file() or sha(path) != artifact['sha256']:
            raise ValueError('filesystem guest artifact mismatch: ' + name)
    serial = guest.parent / 'serial.log'
    output = serial.read_bytes()
    if output.count(b'fsprobe: ok') != 2 or re.findall(rb'run: pid=\d+ exited status=(\d+)', output) != [b'0', b'0']:
        raise ValueError('filesystem guest serial does not contain two successful runs')
    return {'build_receipt_sha256': sha(proof), 'guest_receipt': str(guest.relative_to(ROOT)),
            'guest_receipt_sha256': sha(guest), 'guest_serial_sha256': sha(serial)}

def verify_math_provider(runtime):
    proof = ROOT / 'build/browser-libm/receipt.json'
    data = json.loads(proof.read_text())
    if data.get('result') != 'success' or data.get('runtime', {}).get('sha256') != runtime['sha256'] or data.get('runtime', {}).get('sdk_headers_sha256') != runtime['sdk_headers_sha256']:
        raise ValueError('math provider build/runtime proof mismatch')
    required = ('ports/srvros/libm/build.py', 'ports/srvros/libm/host_test.py', 'ports/srvros/libm/probe.c',
                'build/browser-libm/host-receipt.json', 'build/browser-libm/libsrvros-jsc-math.a',
                'build/browser-libm/libmprobe.elf', 'build/tooling/zig/lib/libc/musl/COPYRIGHT')
    if not all(name in data['sha256'] for name in required): raise ValueError('math proof missing required input')
    for name, expected in data['sha256'].items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or not (ROOT/path).is_file() or sha(ROOT/path) != expected:
            raise ValueError('math provider proof mismatch: ' + name)
    if json.loads((ROOT/'build/browser-libm/host-receipt.json').read_text()).get('result') != 'success':
        raise ValueError('math host numerical proof not successful')
    extension_path = ROOT / 'build/browser-runtime/extension-receipt.json'
    extension = json.loads(extension_path.read_text())
    if extension.get('result') != 'success' or extension.get('sdk_headers_sha256') != runtime['sdk_headers_sha256']:
        raise ValueError('runtime extension proof mismatch')
    for name in ('build/browser-libm/receipt.json', 'build/browser-libm/libmprobe.elf',
                 'build/browser-runtime/cpp-linkage.elf', 'build/browser-runtime/algorithmprobe.elf'):
        if name not in extension['sha256']: raise ValueError('runtime extension proof missing: ' + name)
    for name, expected in extension['sha256'].items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or not (ROOT/path).is_file() or sha(ROOT/path) != expected:
            raise ValueError('runtime extension proof mismatch: ' + name)
    guest = args.math_guest_receipt.resolve()
    if guest.name != 'receipt.json' or guest.parent.parent != (ROOT/'build/browser-runtime').resolve() or not guest.parent.name.startswith('guest-'):
        raise ValueError('math guest receipt must be a runtime guest run')
    obs = json.loads(guest.read_text())
    if obs.get('result') != 'passed' or obs.get('guest_stopped') is not True or obs.get('runtime_provenance', {}).get('sha256') != runtime['sha256'] or obs.get('extension_provenance', {}).get('sha256') != sha(extension_path):
        raise ValueError('math guest used a different runtime/extension proof or did not pass')
    for name in ('libmprobe', 'cpp-linkage', 'algorithmprobe'):
        found = [test for test in obs.get('tests', []) if test.get('mode') == name]
        if len(found) != 1 or found[0].get('passed') is not True or found[0].get('expected_status') != 0:
            raise ValueError('required runtime guest test did not pass: ' + name)
        artifact = obs['artifacts'][name]
        path = Path(artifact['path']).resolve()
        if not path.is_relative_to(ROOT) or not path.is_file() or sha(path) != artifact['sha256']:
            raise ValueError('runtime guest artifact mismatch: ' + name)
    serial = guest.parent / 'serial.log'; output = serial.read_bytes()
    for marker in (b'libmprobe: ok', b'browser-c-linkage: ok', b'browser-algorithm: ok'):
        if output.count(marker) != 1: raise ValueError('missing/duplicate runtime guest marker')
    return {'build_receipt_sha256': sha(proof), 'extension_receipt_sha256': sha(extension_path),
            'guest_receipt': str(guest.relative_to(ROOT)), 'guest_receipt_sha256': sha(guest),
            'guest_serial_sha256': sha(serial), 'math_archive_sha256': sha(ROOT/'build/browser-libm/libsrvros-jsc-math.a')}

def source_digest():
    h = hashlib.sha256()
    for path in sorted(source.rglob('*')):
        if path.is_symlink():
            raise RuntimeError('unexpected symlink in retained source: ' + str(path))
        if path.is_file():
            h.update(str(path.relative_to(source)).encode() + b'\0')
            h.update(str(path.stat().st_mode & 0o777).encode() + b'\0')
            h.update(bytes.fromhex(sha(path)))
    return h.hexdigest()

def archive_info(path):
    count = 0
    with path.open('rb') as stream:
        if stream.read(8) != b'!<arch>\n': raise ValueError('not an ar archive: ' + str(path))
        while header := stream.read(60):
            if len(header) != 60 or header[58:] != b'`\n': raise ValueError('malformed archive: ' + str(path))
            name = header[:16].decode('ascii').strip()
            size = int(header[48:58]); start = stream.tell()
            if name not in ('/', '//', '/SYM64/'):
                elf = stream.read(20)
                if len(elf) < 20 or elf[:6] != b'\x7fELF\x02\x01' or struct.unpack('<H', elf[16:18])[0] != 1 or struct.unpack('<H', elf[18:20])[0] != 62:
                    raise ValueError('non-x86_64 ELF member in ' + str(path) + ': ' + name)
                count += 1
            stream.seek(start + size + (size & 1))
    if not count: raise ValueError('empty target archive: ' + str(path))
    return {'sha256': sha(path), 'elf64_x86_64_members': count}

def check_space():
    storage.preflight()
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
                try: storage.preflight()
                except RuntimeError as error: reason = str(error)
                if not reason and now - start > timeout:
                    reason = 'timeout'
                elif not reason and now >= next_size:
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
    env = storage.environment(env)
    check_space()
    receipt['runtime'] = verify_runtime(); receipt['icu'] = verify_icu()
    receipt['filesystem'] = verify_filesystem(receipt['runtime'])
    receipt['math_provider'] = verify_math_provider(receipt['runtime'])
    if not ARCHIVE.is_file() or ARCHIVE.stat().st_size != SIZE or sha(ARCHIVE) != DIGEST:
        raise RuntimeError('pinned WPE archive absent or invalid')
    receipt['source_observation_only'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    receipt['dirty_observation_only'] = subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)
    for path in [Path(__file__), ROOT/'tools/browser_jsc_elf.py', ROOT/'tools/browser_storage.py', ARCHIVE, *sorted(port.glob('*'))]:
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
                            storage.preflight()
                            out.write(block)
                    dest.chmod(member.mode & 0o777)
        receipt['extracted_bytes'] = total
        patches = ['srvros-platform.patch', 'bmalloc-stdlib.patch', 'platform-registers.patch',
                   'available-memory.patch', 'filesystem-detection.patch', 'filesystem-utf8.patch',
                   'int128-streams.patch', 'simdutf-icelake.patch', 'simdutf-scalar.patch',
                   'elf-local-labels.patch', 'filelock-include.patch', 'memory-footprint.patch',
                   'file-map-flags.patch', 'optional-signals.patch', 'mincore-unavailable.patch',
                   'cache-version.patch', 'repatch-nojit.patch', 'cache-build-id.patch',
                   'target-archive-tools.patch', 'repatch-llint-includes.patch', 'jsc-math-provider.patch']
        if args.experimental_signal_context:
            patches.extend(('srvros-signal-context.patch', 'signal-stack-bounds.patch',
                            'signal-machine-context.patch'))
        for name in patches:
            command('patch-' + name, ['patch','--batch','--fuzz=0','-p1','-d',source,'-i',port/name])
        state = {'patches': {name: sha(port/name) for name in patches},
                 'toolchain': {name: sha(port/name) for name in ('cc','cxx','compiler.py','toolchain.cmake')},
                 'runtime_receipt_sha256': receipt['runtime']['sha256'],
                 'icu_receipt_sha256': receipt['icu']['sha256'], 'filesystem_proof': receipt['filesystem'],
                 'math_proof': receipt['math_provider'],
                 'source_sha256': source_digest()}
        (run / 'source-state.json').write_text(json.dumps(state, indent=2) + '\n')
    else:
        state_path = run / 'source-state.json'
        if not state_path.is_file():
            raise RuntimeError('resume lacks source-state manifest; start a fresh run')
        state = json.loads(state_path.read_text())
        build_state_path = run / 'build-state.json'
        if build.exists():
            if not build_state_path.is_file(): raise RuntimeError('retained build lacks verified configure state')
            recorded = json.loads(build_state_path.read_text())
            if not (build / 'CMakeCache.txt').is_file() or sha(build / 'CMakeCache.txt') != recorded['cmake_cache_sha256']:
                raise RuntimeError('retained configure cache changed; start a fresh run')
        if state['source_sha256'] != source_digest():
            raise RuntimeError('retained source differs from recorded patch state')
        for key in ('patches', 'toolchain'):
            for name, expected in state[key].items():
                if not (port/name).is_file() or sha(port/name) != expected:
                    raise RuntimeError('recorded ' + key + ' input changed: ' + name)
        changed_proofs = (state['runtime_receipt_sha256'] != receipt['runtime']['sha256'] or
                          state['icu_receipt_sha256'] != receipt['icu']['sha256'] or
                          state['filesystem_proof'] != receipt['filesystem'] or
                          state.get('math_proof') != receipt['math_provider'])
        if changed_proofs and not (args.refresh_runtime_proofs or args.refresh_sdk_headers):
            raise RuntimeError('runtime or ICU proof changed; use a verified transition or start fresh')
        if args.refresh_sdk_headers:
            if not changed_proofs:
                raise RuntimeError('SDK/runtime proofs have not changed')
            snapdir = run / 'sdk-header-transition'
            snapshot_path = snapdir / 'headers.json'
            snapshot = json.loads(snapshot_path.read_text())
            if not snapshot['matched'] or snapshot['source_sha256'] != state['source_sha256'] or snapshot['cmake_cache_sha256'] != sha(build / 'CMakeCache.txt'):
                raise RuntimeError('source/cache differs from SDK header snapshot')
            if snapshot['old_runtime_receipt_sha256'] != state['runtime_receipt_sha256'] or snapshot['old_icu_receipt_sha256'] != state['icu_receipt_sha256'] or snapshot['old_filesystem_proof'] != state['filesystem_proof']:
                raise RuntimeError('prior proofs differ from SDK header snapshot')
            oldtree = hashlib.sha256(); newtree = hashlib.sha256(); actual = {}
            sdk = ROOT / 'build/sysroot/srvros'
            for path in sorted((sdk / 'include').rglob('*')):
                if not path.is_file(): continue
                name = str(path.relative_to(sdk)); actual[name] = sha(path)
            if set(actual) != set(snapshot['files']):
                raise RuntimeError('SDK header path set changed')
            for name in sorted(actual):
                old_hash = snapshot['files'][name]['old']; new_hash = snapshot['files'][name]['new']
                if actual[name] != new_hash:
                    raise RuntimeError('SDK header changed after snapshot: ' + name)
                if old_hash != new_hash and name not in ('include/semaphore.h', 'include/regex.h'):
                    raise RuntimeError('unapproved SDK header difference: ' + name)
                oldtree.update(name.encode() + b'\0' + bytes.fromhex(old_hash))
                newtree.update(name.encode() + b'\0' + bytes.fromhex(new_hash))
            if oldtree.hexdigest() != snapshot['expected_old'] or newtree.hexdigest() != receipt['runtime']['sdk_headers_sha256']:
                raise RuntimeError('SDK header tree digest mismatch')
            preproc = json.loads((snapdir / 'c-preprocess.json').read_text())
            if preproc.get('result') != 'matched': raise RuntimeError('C preprocessing proof absent')
            for name, anchor in (('semaphore.h', b'int sem_init('), ('regex.h', b'int regcomp(')):
                old = (snapdir / ('old-' + name)).read_bytes()
                new = (snapdir / ('new-' + name)).read_bytes()
                expected_old = subprocess.check_output(['git', 'show', '16527164:userspace/lib/include/' + name], cwd=ROOT)
                if old != expected_old or hashlib.sha256(old).hexdigest() != snapshot['files']['include/' + name]['old'] or hashlib.sha256(new).hexdigest() != actual['include/' + name]:
                    raise RuntimeError('header snapshot content mismatch: ' + name)
                opening = b'#ifdef __cplusplus\nextern "C" {\n#endif\n\n'
                closing = b'#ifdef __cplusplus\n}\n#endif\n\n'
                if old.count(anchor) != 1 or old.count(b'#endif\n') != 1:
                    raise RuntimeError('unexpected historical header shape: ' + name)
                expected_new = old.replace(b'#endif\n', closing + b'#endif\n', 1).replace(anchor, opening + anchor, 1)
                if new != expected_new:
                    raise RuntimeError('header change exceeds C++ linkage guards: ' + name)
                old_i = snapdir / ('old-' + name + '.i'); new_i = snapdir / ('new-' + name + '.i')
                if sha(old_i) != sha(new_i) or sha(old_i) != preproc['sha256']['old-' + name] or sha(new_i) != preproc['sha256']['new-' + name]:
                    raise RuntimeError('C preprocessed declarations differ: ' + name)
            baseline = json.loads((run / 'runtime-transition-baseline/baseline.json').read_text())
            fixed = ('build/tooling/zig/zig', 'ports/srvros/libcxx/config.h', 'ports/srvros/webkit/compiler.py',
                     'ports/srvros/webkit/cc', 'ports/srvros/webkit/cxx', 'ports/srvros/webkit/toolchain.cmake',
                     'build/sysroot/srvros/lib/crt0.o', 'build/sysroot/srvros/lib/app.ld')
            for name in fixed:
                if sha(ROOT/name) != baseline['files'][name]: raise RuntimeError('compiler/config/startup changed: ' + name)
            transition = {'reason': args.refresh_sdk_headers, 'scope': 'C++ linkage guards only; C declarations identical',
                          'header_snapshot_sha256': sha(snapshot_path), 'c_preprocess_sha256': sha(snapdir / 'c-preprocess.json'),
                          'old_sdk_headers_sha256': oldtree.hexdigest(), 'new_sdk_headers_sha256': newtree.hexdigest(),
                          'old_sdk_archive_sha256': baseline['files']['build/sysroot/srvros/lib/libsrvros.a'],
                          'new_sdk_archive_sha256': sha(sdk / 'lib/libsrvros.a'),
                          'old_proofs': {'runtime': state['runtime_receipt_sha256'], 'icu': state['icu_receipt_sha256'], 'filesystem': state['filesystem_proof']},
                          'new_proofs': {'runtime': receipt['runtime'], 'icu': receipt['icu'], 'filesystem': receipt['filesystem'], 'math': receipt['math_provider']}}
            receipt['sdk_header_transition'] = transition
            (run / ('sdk-header-transition-' + str(time.time_ns()) + '.json')).write_text(json.dumps(transition, indent=2) + '\n')
            state['runtime_receipt_sha256'] = receipt['runtime']['sha256']
            state['icu_receipt_sha256'] = receipt['icu']['sha256']
            state['filesystem_proof'] = receipt['filesystem']
            state['math_proof'] = receipt['math_provider']
            state['sdk_header_transition'] = transition
            state_path.write_text(json.dumps(state, indent=2) + '\n')
        if args.refresh_runtime_proofs:
            if not changed_proofs:
                raise RuntimeError('runtime proofs have not changed')
            baseline_path = run / 'runtime-transition-baseline/baseline.json'
            baseline = json.loads(baseline_path.read_text())
            if baseline['source_sha256'] != state['source_sha256'] or baseline['cmake_cache_sha256'] != sha(build / 'CMakeCache.txt'):
                raise RuntimeError('source/configuration differs from transition baseline')
            if baseline['sdk_headers_sha256'] != receipt['runtime']['sdk_headers_sha256']:
                raise RuntimeError('SDK headers differ from transition baseline')
            if (baseline['proofs']['build/browser-runtime/receipt.json'] != state['runtime_receipt_sha256'] or
                baseline['proofs']['build/browser-icu/receipt.json'] != state['icu_receipt_sha256'] or
                baseline['proofs']['build/browser-filesystem/receipt.json'] != state['filesystem_proof']['build_receipt_sha256'] or
                baseline['proofs'].get(state['filesystem_proof']['guest_receipt']) != state['filesystem_proof']['guest_receipt_sha256']):
                raise RuntimeError('old proofs differ from transition baseline')
            fixed = ('build/tooling/zig/zig', 'ports/srvros/libcxx/config.h',
                     'ports/srvros/webkit/compiler.py', 'ports/srvros/webkit/cc', 'ports/srvros/webkit/cxx',
                     'ports/srvros/webkit/toolchain.cmake', 'build/sysroot/srvros/lib/libsrvros.a',
                     'build/sysroot/srvros/lib/crt0.o', 'build/sysroot/srvros/lib/app.ld')
            for name in fixed:
                if sha(ROOT/name) != baseline['files'][name]:
                    raise RuntimeError('SDK/compiler identity changed: ' + name)
            archives = ('build/browser-runtime/libcxx-threaded.a', 'build/browser-runtime/libcxxabi-rtti.a',
                        'build/browser-filesystem/libcxx-filesystem.a', 'build/browser-icu/sysroot/lib/libicuuc.a',
                        'build/browser-icu/sysroot/lib/libicui18n.a', 'build/browser-icu/sysroot/lib/libicudata.a')
            transition = {'reason': args.refresh_runtime_proofs, 'baseline_sha256': sha(baseline_path),
                          'old_proofs': baseline['proofs'],
                          'new_proofs': {'runtime': receipt['runtime'], 'icu': receipt['icu'],
                                         'filesystem': receipt['filesystem'], 'math': receipt['math_provider']},
                          'old_archives': {name: baseline['files'][name] for name in archives},
                          'new_archives': {name: sha(ROOT/name) for name in archives},
                          'unchanged_sdk_headers_sha256': baseline['sdk_headers_sha256'],
                          'unchanged_compiler_profile': baseline['compiler_profile']}
            receipt['runtime_transition'] = transition
            (run / ('runtime-transition-' + str(time.time_ns()) + '.json')).write_text(json.dumps(transition, indent=2) + '\n')
            state['runtime_receipt_sha256'] = receipt['runtime']['sha256']
            state['icu_receipt_sha256'] = receipt['icu']['sha256']
            state['filesystem_proof'] = receipt['filesystem']
            state['math_proof'] = receipt['math_provider']
            state['last_runtime_transition'] = transition
            state_path.write_text(json.dumps(state, indent=2) + '\n')
        if args.add_patch:
            for name in args.add_patch:
                if name in state['patches'] or not (port/name).is_file():
                    raise RuntimeError('new patch already applied or missing: ' + name)
            for name in args.add_patch:
                command('patch-' + name, ['patch','--batch','--fuzz=0','-p1','-d',source,'-i',port/name])
                state['patches'][name] = sha(port/name)
                state['source_sha256'] = source_digest()
                state_path.write_text(json.dumps(state, indent=2) + '\n')
    receipt['source_state'] = state
    if args.experimental_signal_context and not all(name in state['patches'] for name in
            ('srvros-signal-context.patch', 'signal-stack-bounds.patch', 'signal-machine-context.patch')):
        raise RuntimeError('retained run lacks the experimental overlay; use explicit --add-patch or start a fresh run')
    receipt['experimental_signal_context'] = 'srvros-signal-context.patch' in state['patches']
    if state.get('last_runtime_transition'):
        receipt['runtime_transition'] = state['last_runtime_transition']
    if state.get('sdk_header_transition'):
        receipt['sdk_header_transition'] = state['sdk_header_transition']
    identity_inputs = {'source_sha256': state['source_sha256'], 'toolchain': state['toolchain'],
                       'runtime_receipt_sha256': receipt['runtime']['sha256'],
                       'icu_receipt_sha256': receipt['icu']['sha256'],
                       'filesystem_receipt_sha256': receipt['filesystem']['build_receipt_sha256'],
                       'math_archive_sha256': receipt['math_provider']['math_archive_sha256'],
                       'configuration': {'port': 'JSCOnly', 'static': True, 'jit': False,
                           'webassembly': False, 'remote_inspector': False, 'system_malloc': True,
                           'header_maps': False, 'unified_builds': False,
                           'sampling_profiler': False, 'build_type': 'Release',
                           'filesystem': True, 'localization': False}}
    identity = hashlib.sha256(json.dumps(identity_inputs, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    receipt['build_identifier'] = {'sha256': identity, 'inputs': identity_inputs}
    command('configure', ['cmake','-S',source,'-B',build,'-G','Ninja',
        '-DCMAKE_TOOLCHAIN_FILE='+str(port/'toolchain.cmake'), '-DPORT=JSCOnly',
        '-DSRVROS_ICU_ROOT='+str(icu), '-DICU_ROOT='+str(icu), '-DSRVROS_JSC_FILESYSTEM=ON',
        '-DSRVROS_JSC_BUILD_IDENTIFIER='+identity, '-DENABLE_STATIC_JSC=ON',
        '-DENABLE_JIT=OFF', '-DENABLE_WEBASSEMBLY=OFF', '-DENABLE_REMOTE_INSPECTOR=OFF',
        '-DUSE_SYSTEM_MALLOC=ON', '-DUSE_HEADER_MAPS=OFF', '-DENABLE_UNIFIED_BUILDS=OFF',
        '-DENABLE_SAMPLING_PROFILER=OFF',
        '-DCMAKE_BUILD_TYPE=Release',
        *(['-U', 'STD_FILESYSTEM_IS_AVAILABLE', '-U', 'STD_EXPERIMENTAL_FILESYSTEM_IS_AVAILABLE'] if (args.refresh_runtime_proofs or args.refresh_sdk_headers) else [])])
    cache_path = build / 'CMakeCache.txt'
    cache = {}
    for line in cache_path.read_text().splitlines():
        if ':' in line and '=' in line and not line.startswith(('//', '#')):
            key, value = line.split('=', 1); cache[key.split(':', 1)[0]] = value
    expected = {'ICU_INCLUDE_DIR': str(icu / 'include'),
                'ICU_DATA_LIBRARY_RELEASE': str(icu / 'lib/libicudata.a'),
                'ICU_I18N_LIBRARY_RELEASE': str(icu / 'lib/libicui18n.a'),
                'ICU_UC_LIBRARY_RELEASE': str(icu / 'lib/libicuuc.a'),
                'ENABLE_STATIC_JSC': 'ON', 'ENABLE_JIT': 'OFF', 'ENABLE_WEBASSEMBLY': 'OFF',
                'ENABLE_REMOTE_INSPECTOR': 'OFF',
                'USE_SYSTEM_MALLOC': 'ON', 'USE_HEADER_MAPS': 'OFF',
                'ENABLE_UNIFIED_BUILDS': 'OFF', 'ENABLE_SAMPLING_PROFILER': 'OFF', 'CMAKE_BUILD_TYPE': 'Release',
                'SRVROS_JSC_FILESYSTEM': 'ON', 'STD_FILESYSTEM_IS_AVAILABLE': '1',
                'SRVROS_JSC_BUILD_IDENTIFIER': identity}
    mismatch = {key: cache.get(key) for key, value in expected.items() if cache.get(key) != value}
    if mismatch: raise RuntimeError('unexpected target configuration: ' + str(mismatch))
    fs_archive = str(ROOT / 'build/browser-filesystem/libcxx-filesystem.a')
    standard_libs = cache.get('CMAKE_CXX_STANDARD_LIBRARIES', '')
    cxx_flags = cache.get('CMAKE_CXX_FLAGS', '')
    if fs_archive not in standard_libs or '-D_LIBCPP_HAS_FILESYSTEM=1' not in cxx_flags or '-D_LIBCPP_HAS_LOCALIZATION=0' not in cxx_flags:
        raise RuntimeError('CMake target filesystem flags or archive missing')
    configure_log = (build / 'CMakeFiles/CMakeConfigureLog.yaml').read_text()
    start = configure_log.rfind('Performing Test STD_FILESYSTEM_IS_AVAILABLE')
    if start < 0: raise RuntimeError('filesystem try_compile evidence missing')
    end = configure_log.find('\n  - ', start)
    section = configure_log[start:end if end >= 0 else None]
    try_archives = [fs_archive, str(ROOT / 'build/browser-runtime/libcxx-threaded.a'),
                    str(ROOT / 'build/browser-runtime/libcxxabi-rtti.a'),
                    str(ROOT / 'build/sysroot/srvros/lib/libsrvros.a')]
    if any(path not in section for path in try_archives) or '-D_LIBCPP_HAS_FILESYSTEM=1' not in section or '-D_LIBCPP_HAS_LOCALIZATION=0' not in section or 'exitCode: 0' not in section:
        raise RuntimeError('filesystem try_compile did not link target archives with required flags')
    receipt['configuration'] = {key: cache.get(key) for key in expected}
    receipt['filesystem_try_compile'] = {'archive': fs_archive, 'log_sha256': sha(build / 'CMakeFiles/CMakeConfigureLog.yaml')}
    rules_path = build / 'CMakeFiles/rules.ninja'
    rules = rules_path.read_text()
    archive_commands = {}
    zig = str(ROOT / 'build/tooling/zig/zig')
    for target in ('bmalloc', 'WTF', 'JavaScriptCore'):
        pattern = r'^rule CXX_STATIC_LIBRARY_LINKER__' + target + r'_Release\n  command = (.*)$'
        match = re.search(pattern, rules, re.MULTILINE)
        if not match or zig not in match.group(1) or ' ar qc ' not in match.group(1) or ' ranlib ' not in match.group(1):
            raise RuntimeError('target archive rule does not use pinned Zig ar/ranlib: ' + target)
        archive_commands[target] = match.group(1)
    receipt['archive_rules'] = {'sha256': sha(rules_path), 'commands': archive_commands}
    receipt['cmake_cache_sha256'] = sha(cache_path)
    (run / 'build-state.json').write_text(json.dumps({'cmake_cache_sha256': sha(cache_path)}, indent=2) + '\n')
    if args.refresh_runtime_proofs or args.refresh_sdk_headers:
        # Rebuild archive indexes with the verified pinned tools without recompiling unchanged objects.
        for name in ('libJavaScriptCore.a', 'libWTF.a', 'libbmalloc.a'):
            (build / 'lib' / name).unlink(missing_ok=True)
        (build / 'bin/jsc').unlink(missing_ok=True)
    command('build-jsc', ['cmake','--build',build,'--target','jsc','--parallel','2', *(['--', '-k', '10'] if args.keep_going else [])])
    actual_source = source_digest()
    if actual_source != state['source_sha256']:
        raise RuntimeError('source tree changed during build')
    receipt['verified_final_source_sha256'] = actual_source
    outputs = [p for p in build.rglob('*') if p.is_file() and (p.name == 'jsc' or p.name in ('libJavaScriptCore.a', 'libWTF.a', 'libbmalloc.a'))]
    if not any(p.name == 'jsc' for p in outputs): raise RuntimeError('build succeeded without jsc executable')
    archives = {p.name: archive_info(p) for p in outputs if p.suffix == '.a'}
    if not all(name in archives for name in ('libJavaScriptCore.a', 'libWTF.a', 'libbmalloc.a')):
        raise RuntimeError('expected target archives missing')
    jsc = next(p for p in outputs if p.name == 'jsc')
    receipt['jsc_elf'] = inspect_target_elf(jsc)
    receipt['target_archives'] = archives
    receipt['outputs'] = {str(p.relative_to(ROOT)): sha(p) for p in outputs}
    receipt['result'] = 'built; not executed'
except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError, tarfile.TarError) as error:
    receipt['error'] = str(error)
finally:
    receipt['storage_minimum'] = storage.minimum
    receipt['storage_snapshot'] = storage.snapshot
    # Preserve target archive evidence even when the final executable does not link.
    try:
        recorded = {}
        for name in ('libJavaScriptCore.a', 'libWTF.a', 'libbmalloc.a'):
            path = build / 'lib' / name
            if path.is_file(): recorded[name] = archive_info(path)
        if recorded: receipt['observed_target_archives'] = recorded
    except (OSError, ValueError) as error:
        receipt['archive_inspection_error'] = str(error)
        receipt['result'] = 'failed'
    receipt['seconds'] = round(time.monotonic() - started, 3) if 'started' in locals() else 0
    path = run / ('receipt-' + str(time.time_ns()) + '.json')
    path.write_text(json.dumps(receipt, indent=2) + '\n')
    (run / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print('Receipt:', path, 'result:', receipt['result'])
if receipt['result'] != 'built; not executed': sys.exit(1)
