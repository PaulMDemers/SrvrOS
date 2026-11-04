#!/usr/bin/env python3
"""Build a WTF fixture with a retained JSC build's actual Release profile."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from browser_jsc_elf import inspect_target_elf

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'build/browser-jsc'
FIXTURE = ROOT / 'ports/srvros/webkit/wtf_guest.cpp'
RESERVE = 2 * 1024**3
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--jsc-receipt', type=Path, required=True)
parser.add_argument('--build-helper-snapshot', type=Path)
parser.add_argument('--linker-script', type=Path, default=ROOT/'build/sysroot/srvros/lib/app.ld')
args = parser.parse_args()

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''): h.update(block)
    return h.hexdigest()

def tree_digest(source):
    h = hashlib.sha256()
    for path in sorted(source.rglob('*')):
        if path.is_symlink(): raise ValueError('unexpected retained source symlink')
        if path.is_file():
            h.update(str(path.relative_to(source)).encode() + b'\0')
            h.update(str(path.stat().st_mode & 0o777).encode() + b'\0')
            h.update(bytes.fromhex(sha(path)))
    return h.hexdigest()

def relative(path):
    return str(path.resolve().relative_to(ROOT))

def check_inputs(mapping):
    for name, value in mapping.items():
        path = Path(name)
        if path.is_absolute() or '..' in path.parts: raise ValueError('invalid proof path: ' + name)
        path = ROOT / path
        if name == 'tools/browser_jsc_build.py' and args.build_helper_snapshot:
            path = args.build_helper_snapshot.resolve()
            if not path.is_relative_to((ROOT/'build').resolve()): raise ValueError('snapshot must be under build')
        if not path.is_file() or sha(path) != value: raise ValueError('changed proof input: ' + name)

def space():
    if shutil.disk_usage(BASE).free < RESERVE: raise RuntimeError('2 GiB disk reserve reached')

def invoke(name, command, cwd, timeout, result, log):
    space(); started = time.monotonic()
    with log.open('wb') as output:
        process = subprocess.Popen(command, cwd=cwd, env=environment, stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
        reason = None
        try:
            while process.poll() is None:
                if time.monotonic() - started > timeout: reason = 'timeout'; break
                if shutil.disk_usage(BASE).free < RESERVE: reason = 'disk reserve'; break
                time.sleep(0.2)
            if reason:
                os.killpg(process.pid, signal.SIGTERM)
                try: process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL); process.wait()
            else: process.wait()
        except BaseException:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try: process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL); process.wait()
            raise
        finally:
            result['commands'].append({'name':name, 'argv':command, 'cwd':str(cwd), 'status':process.returncode,
                                       'seconds':round(time.monotonic()-started, 3), 'stop_reason':reason, 'log':relative(log)})
    if reason or process.returncode: raise RuntimeError(name + ' failed: ' + (reason or str(process.returncode)))
    space()

environment = os.environ.copy()
for key in ('CFLAGS','CXXFLAGS','LDFLAGS','CPPFLAGS','ICU_ROOT','CMAKE_PREFIX_PATH','PKG_CONFIG_PATH',
            'PKG_CONFIG_LIBDIR','PKG_CONFIG_SYSROOT_DIR','CONFIG_SITE','MAKEFLAGS','MFLAGS','SYSROOT_DIR',
            'ZIG_LIB_DIR','ZIG_LIBC','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','SDKROOT'):
    environment.pop(key, None)

# A unique basename keeps every attempted object, ELF, log and receipt together
# without changing the retained run or replacing a previous fixture.
run = args.jsc_receipt.resolve().parent
result = {'result':'failed', 'scope':'WTF Release profile; target link only', 'commands':[]}
receipt_path = None
try:
    if run.parent != BASE.resolve() or not run.name.startswith('run-') or not run.is_dir():
        raise ValueError('JSC receipt must be inside a retained build/browser-jsc/run-* directory')
    space()
    fd, stem_name = tempfile.mkstemp(prefix='wtfprobe-', suffix='.reservation', dir=run)
    os.close(fd)
    reservation = Path(stem_name); stem = reservation.with_suffix('')
    receipt_path = stem.with_suffix('.json'); obj = stem.with_suffix('.o'); elf = stem.with_suffix('.elf')
    environment['ZIG_GLOBAL_CACHE_DIR'] = str(run/'zig-global-cache')
    environment['ZIG_LOCAL_CACHE_DIR'] = str(run/'zig-local-cache')
    environment['PKG_CONFIG_LIBDIR'] = str(stem) + '.empty-pkgconfig'
    jsc_path = args.jsc_receipt.resolve(); jsc = json.loads(jsc_path.read_text())
    if jsc['result'] != 'built; not executed': raise ValueError('JSC build not successful')
    check_inputs(jsc['inputs']); check_inputs(jsc['outputs'])
    source = run/'wpewebkit-2.54.2'; build = run/'cmake'
    if tree_digest(source) != jsc['verified_final_source_sha256']: raise ValueError('patched source changed')
    if sha(build/'CMakeCache.txt') != jsc['cmake_cache_sha256']: raise ValueError('CMake cache changed')
    result.update({'jsc_source_sha256':jsc['verified_final_source_sha256'],
                   'jsc_build_identifier':jsc['build_identifier']['sha256'],
                   'jsc_source_observation_only':jsc.get('source_observation_only'),
                   'source_observation_only':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True,timeout=15).strip(),
                   'dirty_observation_only':subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True,timeout=15)})
    compdb_log = stem.with_suffix('.compdb.json')
    invoke('compdb', ['ninja','-C',str(build),'-t','compdb','CXX_COMPILER__WTF_unscanned_Release'], ROOT, 60, result, compdb_log)
    entries = [x for x in json.loads(compdb_log.read_text()) if x['file'].endswith('/StringPrintStream.cpp')]
    if len(entries) != 1: raise ValueError('expected one WTF compiler profile')
    entry = entries[0]; command = shlex.split(entry['command'])
    if '-DNDEBUG' not in command or '-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE' not in command:
        raise ValueError('unexpected WTF Release ABI profile')
    for flag, value in [('-MT',str(obj)),('-MF',str(obj)+'.d'),('-o',str(obj))]:
        if command.count(flag) != 1: raise ValueError('unexpected compiler output argument: '+flag)
        command[command.index(flag)+1] = value
    if command[-1] != entry['file']: raise ValueError('unexpected Ninja source position')
    command[-1] = str(FIXTURE); result['ninja_profile'] = entry
    archives = [build/'lib/libWTF.a',build/'lib/libbmalloc.a',ROOT/'build/browser-icu/sysroot/lib/libicui18n.a',ROOT/'build/browser-icu/sysroot/lib/libicuuc.a',ROOT/'build/browser-icu/sysroot/lib/libicudata.a',ROOT/'build/browser-filesystem/libcxx-filesystem.a',ROOT/'build/browser-runtime/libcxx-threaded.a',ROOT/'build/browser-runtime/libcxxabi-rtti.a',ROOT/'build/sysroot/srvros/lib/libsrvros.a',ROOT/'build/browser-libm/libsrvros-jsc-math.a']
    zig=ROOT/'build/tooling/zig/zig'; crt=ROOT/'build/sysroot/srvros/lib/crt0.o'; linker=args.linker_script.resolve()
    relative(linker)
    link=[str(zig),'c++','-target','x86_64-freestanding-none','-nostdlib','-static','-fuse-ld=lld','-Xlinker','-z','-Xlinker','max-page-size=0x1000','-Xlinker','--gc-sections','-Xlinker','-T','-Xlinker',str(linker),str(crt),str(obj),'-Wl,--start-group',*map(str,archives),'-Wl,--end-group','-o',str(elf)]
    invoke('compile',command,entry['directory'],120,result,stem.with_suffix('.compile.log'))
    invoke('link',link,entry['directory'],120,result,stem.with_suffix('.link.log'))
    result['elf'] = inspect_target_elf(elf)
    paths=[Path(__file__),ROOT/'tools/browser_jsc_elf.py',jsc_path,FIXTURE,build/'CMakeCache.txt',build/'build.ninja',build/'cmakeconfig.h',zig,crt,linker,obj,elf,*archives]
    if args.build_helper_snapshot: paths.append(args.build_helper_snapshot.resolve())
    paths += [ROOT/x['log'] for x in result['commands']]
    result['sha256']={relative(path):sha(path) for path in paths}
    result['result']='linked; not run'
except Exception as error:
    result['error']=str(error)
finally:
    if receipt_path:
        receipt_path.write_text(json.dumps(result,indent=2)+'\n')
        reservation.unlink(missing_ok=True)
        print('Receipt:',receipt_path,'result:',result['result'])
    else:
        print('Probe failed before receipt location could be created:',result.get('error'),file=sys.stderr)
if result['result'] != 'linked; not run': sys.exit(1)
