#!/usr/bin/env python3
"""Run pinned, unmodified Lua tests against SrvrOS libc in a disposable VM.

The upstream portable/soft profile omits host-shell/platform assumptions and
large-memory cases. A separate process check exercises SrvrOS popen/exit status.
This is consumer evidence, not POSIX certification or all of Lua's test suite.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

from thread_smoke import connect_serial, read_for, read_until, has_fatal_exception

ROOT = Path(__file__).resolve().parents[1]
REVISION = '6e22fedb74cf0c9b6656e9fce8b7331db847c605'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default=os.environ.get('QEMU', 'qemu-system-x86_64'))
    parser.add_argument('--iso', type=Path, default=ROOT/'build/srvros-x86_64.iso')
    parser.add_argument('--test', choices=('files', 'strings', 'utf8', 'bitwise'), default='files')
    parser.add_argument('--timeout', type=float, default=180)
    args = parser.parse_args()
    upstream = ROOT/'ports/upstream/lua'
    revision = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REVISION:
        raise RuntimeError('Lua revision changed: review the test/profile pin before updating it')
    subprocess.run(['git', '-C', str(upstream), 'diff', '--quiet', 'HEAD', '--'], check=True)
    test = upstream/'testes'/(args.test+'.lua')
    receipt = {'upstream': 'Lua 5.4.8', 'revision': revision,
               'test': str(test.relative_to(ROOT)), 'sha256': hashlib.sha256(test.read_bytes()).hexdigest(),
               'profile': {'_port': True, '_soft': True}, 'process_check': True, 'environment': {'PATH': '/fat/bin'}}
    print(json.dumps(receipt, sort_keys=True), flush=True)
    with tempfile.TemporaryDirectory(prefix='srvros-lua-upstream-') as temporary:
        directory = Path(temporary)
        driver = directory/'luatest.lua'
        driver.write_text('''_port=true; _soft=true
print("lua-upstream: begin")
dofile("/fat/bin/upstream.lua")
print("lua-upstream: upstream passed")
local p=assert(io.popen("printf child-output", "r"))
local output=p:read("a")
assert(output=="child-output", "popen output: "..string.format("%q",output))
local ok,how,status=p:close()
assert(ok and how=="exit" and status==0)
local temporary=os.tmpname()
local writer=assert(io.popen("cat > "..temporary,"w"))
assert(writer:write("from-parent"))
local closed,kind,code=writer:close()
assert(closed and kind=="exit" and code==0)
local saved=assert(io.open(temporary,"r"))
assert(saved:read("a")=="from-parent");assert(saved:close());assert(os.remove(temporary))
local ok,how,status=os.execute("exit 7")
assert(ok==nil and how=="exit" and status==7)
print("lua-upstream: complete")
''')
        disk=directory/'test.exfat'
        command=['python3', str(ROOT/'tools/mk_exfat_image.py'), str(disk)]
        command += [f'{name}={ROOT}/build/userspace/{name}.elf' for name in ('lua','sh','cat','true','false','env','printf')]
        command += [f'upstream.lua={test}', f'luatest.lua={driver}']
        subprocess.run(command, cwd=ROOT, check=True)
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0));port=reservation.getsockname()[1]
        vm=subprocess.Popen([args.qemu, '-M', 'q35', '-m', '512M', '-cdrom', str(args.iso),
            '-boot', 'd', '-display', 'none', '-serial', f'tcp:127.0.0.1:{port},server,nowait',
            '-drive', f'if=none,id=exfat,file={disk},format=raw', '-device', 'ich9-ahci,id=ahci',
            '-device', 'ide-hd,drive=exfat,bus=ahci.0', '-monitor', 'none', '-no-reboot'],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        output=b''
        try:
            with connect_serial(port,20) as serial:
                serial.settimeout(.25)
                output=read_until(serial,b'srv> ',30)
                if b'srv> ' not in output: raise RuntimeError('monitor did not become ready')
                serial.sendall(b'run /fat/bin/env PATH=/fat/bin /fat/bin/lua /fat/bin/luatest.lua\n')
                deadline=time.monotonic()+args.timeout
                run=b''
                while time.monotonic()<deadline:
                    run+=read_for(serial,.5)
                    if b'\nsrv> ' in run or b'exception:' in run: break
                output+=run
                text=run.decode('utf-8','replace')
                if ('lua-upstream: complete' not in text or 'process: exited status=0' not in text
                        or has_fatal_exception(text)):
                    raise RuntimeError('upstream test or process check failed/incomplete')
        finally:
            print(output.decode('utf-8','replace'),flush=True)
            vm.terminate()
            try: vm.wait(timeout=3)
            except subprocess.TimeoutExpired: vm.kill();vm.wait()
            error=vm.stderr.read().decode('utf-8','replace');vm.stderr.close()
            if error: print(error,flush=True)
    print(f'lua-upstream-smoke: {args.test} PASS (portable/soft; explicit process check)',flush=True)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
