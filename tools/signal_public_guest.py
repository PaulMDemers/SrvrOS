#!/usr/bin/env python3
"""Prepare, or explicitly run once, the experimental public-signal guest.

The generated receipt describes only this private diskless QEMU test. The
public production capability remains a separate decision.
"""
import argparse
import ast
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tarfile
import tempfile
import time

from browser_storage import StorageGuard
from pack_runtime_elf import runtime_elf

ROOT = Path(__file__).resolve().parents[1]
BROWSER = ROOT.parent / 'SrvrOS-browser'
EXIT = re.compile(rb'(?m)^run: pid=(\d+) exited status=(\d+)\r?\n')
MARKERS = (b'signal-public: native main and worker context ok',
           b'signal-public: nested suspend resume handshake ok')
BOOT_NETWORK_LINES = (b'net: no supported network device, stack idle',
                      b'net: worker not started')


def boot_network_state(data):
    counts = {line.decode():len(re.findall(rb'(?m)^'+re.escape(line)+rb'\r?\n',data))
              for line in BOOT_NETWORK_LINES}
    provider = bool(re.search(rb'(?m)^net: (?:provider=|static ipv4=)',data))
    return {'required_line_counts':counts,'unexpected_provider':provider,
            'allowed':all(count==1 for count in counts.values()) and not provider}


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for data in iter(lambda: f.read(1024 * 1024), b''):
            h.update(data)
    return h.hexdigest()


def sdk_headers(sdk):
    h = hashlib.sha256()
    files = sorted((sdk / 'include').rglob('*'))
    for p in files:
        if p.is_symlink():
            raise ValueError(f'symlink in SDK headers: {p}')
        if p.is_file():
            h.update(str(p.relative_to(sdk)).encode() + b'\0' + bytes.fromhex(sha(p)))
    return h.hexdigest()


def parsed(data):
    """Return an exit only after its full LF line and a subsequent prompt."""
    matches = list(EXIT.finditer(data))
    if len(matches) != 1:
        return None
    m = matches[0]
    if b'srv> ' not in data[m.end():]:
        return None
    return int(m.group(1)), int(m.group(2))


def parser_selftest():
    assert parsed(b'srv> run /signal-public\r\nrun: pid=4 exited status=0') is None
    assert parsed(b'run: pid=4 exited status=0\n') is None
    assert parsed(b'srv> \nrun: pid=4 exited status=0\r\nsrv> ') == (4, 0)
    assert parsed(b'run: pid=4 exited status=0\nsrv> \nrun: pid=5 exited status=0\nsrv> ') is None
    boot=b'\r\n'.join(BOOT_NETWORK_LINES)+b'\r\nsrv> '
    assert boot_network_state(boot)['allowed']
    assert not boot_network_state(BOOT_NETWORK_LINES[0]+b'\n')['allowed']
    assert not boot_network_state(boot.replace(b'\r\n',b'',1))['allowed']
    assert not boot_network_state(boot+b'\nnet: provider=e1000\n')['allowed']
    assert not boot_network_state(boot+b'\n'+BOOT_NETWORK_LINES[0]+b'\n')['allowed']
    assert full_line_count(b'prefix marker\nmarker\r\nmarker', 'marker') == 1
    assert full_line_count(b'marker\nmarker\n', 'marker') == 2
    suite_selftest()
    print('signal-public-parser: ok')


def required_file(path):
    path = Path(path).resolve()
    if not path.is_file():
        raise ValueError(f'missing input: {path}')
    return path


NAME = re.compile(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,63}\Z')
ARG = re.compile(r'[ -~]{0,512}\Z')
HEX = re.compile(r'[0-9a-f]{64}\Z')


def monitor_arg(value):
    return '"' + value.replace('\\','\\\\').replace('"','\\"') + '"'


def full_line_count(data, marker):
    return len(re.findall(rb'(?m)^' + re.escape(marker.encode()) + rb'\r?\n', data))


def source_file(root, value):
    if not isinstance(value, str) or not value:
        raise ValueError('missing source path')
    path = Path(value)
    path = required_file(path if path.is_absolute() else root/path)
    if not path.is_relative_to(root):
        raise ValueError('source path escapes source_root: '+value)
    return path


def check_map(root, mapping, label):
    if not isinstance(mapping, dict) or not mapping:
        raise ValueError(label+' is empty')
    for name, expected in mapping.items():
        if not isinstance(expected, str) or not HEX.fullmatch(expected):
            raise ValueError('invalid '+label+' hash')
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('invalid '+label+' path')
        path = required_file(root/relative)
        # The pinned, hashed Zig distribution is deliberately shared by the
        # physical task checkout. Payloads/receipts remain confined to root.
        if not path.is_relative_to(root) and relative.parts[:3] != ('build','tooling','zig'):
            raise ValueError(label+' path escapes source_root: '+name)
        if sha(path) != expected:
            raise ValueError(label+' mismatch: '+name)


def suite_manifest(path, sdk):
    path = required_file(path)
    data = json.loads(path.read_text())
    root_value = data.get('source_root')
    if not isinstance(root_value, str) or not Path(root_value).is_absolute():
        raise ValueError('source_root must be absolute')
    root = Path(root_value).resolve()
    if not root.is_dir(): raise ValueError('source_root missing')
    jsc_path = source_file(root, data.get('jsc_receipt'))
    jsc = json.loads(jsc_path.read_text())
    if jsc.get('result') != 'built; not executed' or jsc.get('experimental_signal_context') is not True:
        raise ValueError('experimental JSC build required')
    if jsc.get('configuration', {}).get('ENABLE_JIT') != 'OFF' or jsc.get('configuration', {}).get('ENABLE_WEBASSEMBLY') != 'OFF':
        raise ValueError('JSC must disable JIT and WebAssembly')
    identity = jsc.get('build_identifier', {})
    if not HEX.fullmatch(str(identity.get('sha256',''))) or identity.get('inputs', {}).get('source_sha256') != jsc.get('verified_final_source_sha256'):
        raise ValueError('JSC source/build identity mismatch')
    canonical = json.dumps(identity['inputs'],sort_keys=True,separators=(',',':')).encode()
    if hashlib.sha256(canonical).hexdigest() != identity['sha256']: raise ValueError('JSC build identifier hash mismatch')
    check_map(root, jsc.get('inputs'), 'JSC inputs')
    check_map(root, jsc.get('outputs'), 'JSC outputs')
    runtime = jsc.get('runtime', {})
    runtime_path = source_file(root, runtime.get('path'))
    if sha(runtime_path) != runtime.get('sha256'): raise ValueError('JSC runtime receipt mismatch')
    runtime_data = json.loads(runtime_path.read_text())
    check_map(root, runtime_data.get('sha256'), 'runtime inputs')
    if runtime_data.get('result') != 'success' or runtime.get('sdk_headers_sha256') != sdk_headers(sdk) or runtime_data.get('sdk_headers_sha256') != runtime.get('sdk_headers_sha256'):
        raise ValueError('SDK headers do not match JSC runtime')
    archive_hash = runtime_data['sha256'].get('build/sysroot/srvros/lib/libsrvros.a')
    if archive_hash != sha(sdk/'lib/libsrvros.a'):
        raise ValueError('SDK archive does not match JSC runtime')
    inputs = {'suite_manifest':path,'suite_jsc_receipt':jsc_path,'suite_runtime_receipt':runtime_path}
    payloads = {}
    entries = data.get('payloads')
    if not isinstance(entries, list) or not entries: raise ValueError('payloads required')
    for entry in entries:
        if not isinstance(entry, dict): raise ValueError('invalid payload')
        name, kind = entry.get('name'), entry.get('kind','utility')
        if not isinstance(name, str) or not NAME.fullmatch(name) or name in payloads or name == 'init': raise ValueError('invalid or duplicate payload name')
        if kind not in ('utility','wtf-signal','gc-root'): raise ValueError('invalid payload kind')
        elf = source_file(root, entry.get('elf')); key = str(elf.relative_to(root)); digest = sha(elf)
        fixture_name = entry.get('fixture_receipt')
        if fixture_name is None:
            if kind != 'utility' or jsc['outputs'].get(key) != digest: raise ValueError('unbound payload ELF')
        else:
            fixture_path = source_file(root, fixture_name); fixture = json.loads(fixture_path.read_text())
            if fixture.get('result') != 'linked; not run' or fixture.get('jsc_source_sha256') != jsc['verified_final_source_sha256'] or fixture.get('jsc_build_identifier') != identity['sha256']:
                raise ValueError('fixture build identity mismatch')
            profile=fixture.get('ninja_profile',{}).get('command','')
            if not isinstance(profile,str) or '-DNDEBUG' not in profile.split() or '-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE' not in profile.split():
                raise ValueError('fixture Release ABI profile missing')
            check_map(root, fixture.get('sha256'), 'fixture inputs')
            if fixture['sha256'].get(key) != digest: raise ValueError('fixture does not bind ELF')
            if kind in ('wtf-signal','gc-root'):
                adapter = fixture.get('signal_probe_adapter' if kind == 'wtf-signal' else 'gc_root_probe_adapter', {})
                adapter_path = source_file(root, adapter.get('path'))
                expected_adapter = root/'ports/srvros/webkit'/('build_wtf_signal_probe.py' if kind=='wtf-signal' else 'build_jsc_gc_roots_probe.py')
                if adapter_path != expected_adapter.resolve() or sha(adapter_path) != adapter.get('sha256') or adapter.get('experimental_jsc_receipt_sha256') != sha(jsc_path):
                    raise ValueError('fixture adapter mismatch')
                tree=ast.parse(adapter_path.read_text())
                substitutions=[ast.literal_eval(node.value) for node in ast.walk(tree) if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='substitutions' for t in node.targets)]
                if len(substitutions)!=1 or not isinstance(substitutions[0],dict):raise ValueError('invalid adapter substitutions')
                builder=source_file(root,'tools/browser_wtf_probe.py'); adapted=builder.read_text()
                for old,new in substitutions[0].items():
                    if not isinstance(old,str) or not isinstance(new,str) or adapted.count(old)!=1:raise ValueError('adapter builder mismatch')
                    adapted=adapted.replace(old,new,1)
                if hashlib.sha256(adapted.encode()).hexdigest()!=adapter.get('adapted_builder_sha256'):raise ValueError('adapted builder hash mismatch')
                inputs['adapter:'+name] = adapter_path
            inputs['fixture:'+name] = fixture_path
        inputs['payload:'+name] = elf
        payloads[name] = {'name':name,'kind':kind,'elf':str(elf)}
    cases = data.get('cases')
    if not isinstance(cases, list) or not cases: raise ValueError('cases required')
    result = []; names = set()
    for case in cases:
        if not isinstance(case, dict): raise ValueError('invalid case')
        name,payload = case.get('name'),case.get('payload'); argv=case.get('args',[]); markers=case.get('markers'); timeout=case.get('timeout')
        if not isinstance(name,str) or not NAME.fullmatch(name) or name in names or payload not in payloads: raise ValueError('invalid case name or payload')
        if not isinstance(argv,list) or any(not isinstance(a,str) or not ARG.fullmatch(a) for a in argv): raise ValueError('invalid case arguments')
        if not isinstance(markers,list) or not markers or any(not isinstance(m,str) or not m or len(m)>512 or any(ord(c)<32 or ord(c)>126 for c in m) for m in markers) or len(set(markers))!=len(markers): raise ValueError('invalid markers')
        if isinstance(timeout,bool) or not isinstance(timeout,(int,float)) or not 0 < timeout <= 150: raise ValueError('invalid case timeout')
        names.add(name); result.append({'name':name,'payload':payload,'args':argv,'markers':markers,'timeout':timeout})
    return {'source_root':str(root),'jsc_receipt_sha256':sha(jsc_path),'jsc_build_identifier':identity['sha256'],'payloads':list(payloads.values()),'cases':result}, inputs


def suite_selftest():
    with tempfile.TemporaryDirectory(prefix='signal-suite-test-') as temp:
        root=Path(temp)/'source';root.mkdir();sdk=root/'build/sysroot/srvros';(sdk/'lib').mkdir(parents=True);(sdk/'include').mkdir()
        (sdk/'include/test.h').write_text('header');(sdk/'lib/libsrvros.a').write_bytes(b'sdk')
        toolchain=Path(temp)/'pinned-zig';toolchain.mkdir();(toolchain/'zig').write_bytes(b'pinned')
        (root/'build/tooling').mkdir();(root/'build/tooling/zig').symlink_to(toolchain,target_is_directory=True)
        check_map(root,{'build/tooling/zig/zig':sha(toolchain/'zig')},'pinned toolchain')
        (root/'escape').symlink_to(toolchain/'zig')
        try:check_map(root,{'escape':sha(toolchain/'zig')},'escaped input')
        except ValueError:pass
        else:raise AssertionError('non-toolchain input escaped root')
        elf=root/'jsc';elf.write_bytes(b'elf'); recipe=root/'recipe';recipe.write_text('recipe')
        runtime=root/'runtime.json';runtime.write_text(json.dumps({'result':'success','sdk_headers_sha256':sdk_headers(sdk),'sha256':{'build/sysroot/srvros/lib/libsrvros.a':sha(sdk/'lib/libsrvros.a')}}))
        identity_inputs={'source_sha256':'a'*64,'configuration':{'jit':False}}
        identity={'inputs':identity_inputs,'sha256':hashlib.sha256(json.dumps(identity_inputs,sort_keys=True,separators=(',',':')).encode()).hexdigest()}
        receipt=root/'jsc.json';jsc={'result':'built; not executed','experimental_signal_context':True,'configuration':{'ENABLE_JIT':'OFF','ENABLE_WEBASSEMBLY':'OFF'},'build_identifier':identity,'verified_final_source_sha256':'a'*64,'inputs':{'recipe':sha(recipe)},'outputs':{'jsc':sha(elf)},'runtime':{'path':str(runtime),'sha256':sha(runtime),'sdk_headers_sha256':sdk_headers(sdk)}};receipt.write_text(json.dumps(jsc))
        manifest=root/'manifest.json';spec={'source_root':str(root),'jsc_receipt':'jsc.json','payloads':[{'name':'jsc','elf':'jsc','kind':'utility'}],'cases':[{'name':'basic','payload':'jsc','args':['-e','print(1+1)'],'markers':['ok'],'timeout':10}]}
        def run(value):
            manifest.write_text(json.dumps(value));return suite_manifest(manifest,sdk)
        assert run(spec)[0]['cases'][0]['args']==['-e','print(1+1)']
        assert monitor_arg('a \"b\\c') == '\"a \\\"b\\\\c\"'
        (root/'tools').mkdir();builder=root/'tools/browser_wtf_probe.py';builder.write_text('old fixture')
        adapter=root/'ports/srvros/webkit/build_wtf_signal_probe.py';adapter.parent.mkdir(parents=True);adapter.write_text("substitutions = {'old fixture': 'new fixture'}\n")
        fixture_elf=root/'wtf';fixture_elf.write_bytes(b'fixture')
        fixture=root/'fixture.json';fixture.write_text(json.dumps({'result':'linked; not run','jsc_source_sha256':'a'*64,'jsc_build_identifier':identity['sha256'],'ninja_profile':{'command':'c++ -DNDEBUG -D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE'},'sha256':{'wtf':sha(fixture_elf),'tools/browser_wtf_probe.py':sha(builder)},'signal_probe_adapter':{'path':str(adapter),'sha256':sha(adapter),'adapted_builder_sha256':hashlib.sha256(b'new fixture').hexdigest(),'experimental_jsc_receipt_sha256':sha(receipt)}}))
        tested=json.loads(json.dumps(spec));tested['payloads'].append({'name':'wtf','elf':'wtf','kind':'wtf-signal','fixture_receipt':'fixture.json'})
        assert len(run(tested)[0]['payloads'])==2
        fixture_data=json.loads(fixture.read_text());fixture_data['signal_probe_adapter']['adapted_builder_sha256']='0'*64;fixture.write_text(json.dumps(fixture_data))
        try:run(tested)
        except ValueError:pass
        else:raise AssertionError('invalid adapter accepted')
        import copy
        bad=[]
        for change in ('newline','missing_marker','duplicate','timeout','unbound','badkind','escape'):
            v=copy.deepcopy(spec)
            if change=='newline':v['cases'][0]['args']=['x\nrun /other']
            if change=='missing_marker':v['cases'][0]['markers']=[]
            if change=='duplicate':v['cases'].append(copy.deepcopy(v['cases'][0]))
            if change=='timeout':v['cases'][0]['timeout']=151
            if change=='unbound':v['payloads'][0]['elf']='recipe'
            if change=='badkind':v['payloads'][0]['kind']='wtf-signal'
            if change=='escape':v['payloads'][0]['elf']='/etc/passwd'
            bad.append(v)
        for v in bad:
            try:run(v)
            except (ValueError,KeyError,TypeError):pass
            else:raise AssertionError('invalid suite accepted')
        jsc['configuration']['ENABLE_JIT']='ON';receipt.write_text(json.dumps(jsc))
        try:run(spec)
        except ValueError:pass
        else:raise AssertionError('JIT-enabled receipt accepted')

def prepare(args):
    parent = Path(args.output_parent)
    guard = StorageGuard(parent)
    guard.preflight()
    run = Path(tempfile.mkdtemp(prefix='signal-public-', dir=parent))
    receipt = {'schema': 1, 'result': 'preparing', 'created_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
               'scope': 'experimental diskless Q35 public-signal probe; production capability not qualified',
               'storage': guard.snapshot, 'inputs': {}, 'commands': []}
    try:
        sdk = Path(args.sdk).resolve()
        inputs = {
            'kernel': required_file(args.kernel), 'init': required_file(args.init),
            'runtime_packer': required_file(ROOT/'tools/pack_runtime_elf.py'),
            'storage_guard': required_file(ROOT/'tools/browser_storage.py'),
            'harness': required_file(__file__), 'sdk_archive': required_file(sdk/'lib/libsrvros.a'),
            'sdk_crt': required_file(sdk/'lib/crt0.o'), 'sdk_linker': required_file(sdk/'lib/app.ld'),
            'limine_sys': required_file(BROWSER/'build/limine-binary/limine-bios.sys'),
            'limine_cd': required_file(BROWSER/'build/limine-binary/limine-bios-cd.bin'),
            'limine_tool': required_file(BROWSER/'build/limine-binary/limine'),
        }
        suite = None
        if not args.suite_manifest:
            inputs.update(probe=required_file(args.probe), probe_source=required_file(ROOT/'tools/test_signal_public_guest.c'),
                          probe_asm=required_file(ROOT/'tools/test_signal_delivery_guest.S'),
                          probe_recipe=required_file(ROOT/'tools/signal_public_guest.mk'))
        if args.suite_manifest:
            suite, suite_inputs = suite_manifest(args.suite_manifest, sdk)
            inputs.update(suite_inputs)
            receipt['suite'] = suite
            receipt['scope'] = 'experimental diskless Q35 suite; results apply only to recorded case markers'
        for name in ('kernel/src/process.c','kernel/src/process_signal_public.inc',
                     'kernel/src/arch/x86_64/idt.c','kernel/src/arch/x86_64/syscall.c',
                     'userspace/lib/src/posix_signal.c','userspace/lib/src/signal_runtime.c',
                     'userspace/lib/src/posix_signal_context.c'):
            inputs[name] = required_file(ROOT/name)
        xorriso = required_file(shutil.which('xorriso') or '')
        qemu = required_file(shutil.which('qemu-system-x86_64') or '')
        inputs.update(xorriso=xorriso, qemu=qemu)
        receipt['git_head'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
        receipt['git_dirty'] = subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)
        receipt['sdk'] = str(sdk)
        receipt['sdk_headers_sha256'] = sdk_headers(sdk)
        receipt['inputs'] = {k:{'path':str(p),'sha256':sha(p)} for k,p in inputs.items()}
        iso_root = run/'iso-root'; boot = iso_root/'boot/limine'; boot.mkdir(parents=True)
        shutil.copyfile(inputs['kernel'],iso_root/'boot/srvros.elf')
        shutil.copyfile(inputs['limine_sys'],boot/'limine-bios.sys')
        shutil.copyfile(inputs['limine_cd'],boot/'limine-bios-cd.bin')
        with tarfile.open(iso_root/'boot/initramfs.tar','w',format=tarfile.USTAR_FORMAT) as ar:
            members = [('init',inputs['init'])] + ([(p['name'],inputs['payload:'+p['name']]) for p in suite['payloads']] if suite else [('signal-public',inputs['probe'])])
            for name,path in members:
                payload=runtime_elf(path.read_bytes())
                ti=tarfile.TarInfo(name)
                ti.size=len(payload);ti.mode=0o755;ti.mtime=0
                ar.addfile(ti,io.BytesIO(payload))
                receipt.setdefault('packed',{})[name]={'sha256':hashlib.sha256(payload).hexdigest(),'bytes':len(payload)}
        config=('timeout: 0\nserial: yes\n\n/signal-public\n    protocol: limine\n'
                '    path: boot():/boot/srvros.elf\n    cmdline: srvros.log=quiet\n'
                '    module_path: boot():/boot/initramfs.tar\n    module_string: initramfs\n')
        (boot/'limine.conf').write_text(config)
        iso=run/'guest.iso'
        commands=[[str(xorriso),'-as','mkisofs','-R','-r','-J','-b','boot/limine/limine-bios-cd.bin',
                   '-no-emul-boot','-boot-load-size','4','-boot-info-table',str(iso_root),'-o',str(iso)],
                  [str(inputs['limine_tool']),'bios-install',str(iso)]]
        with (run/'prepare.log').open('wb') as log:
            for cmd in commands:
                guard.preflight()
                started=time.monotonic()
                result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,timeout=60,check=False)
                receipt['commands'].append({'argv':cmd,'status':result.returncode,'seconds':round(time.monotonic()-started,3)})
                if result.returncode: raise RuntimeError(f'prepare command failed: {cmd[0]}')
        receipt['qemu_argv_template']=[str(qemu),'-name','SrvrOS experimental public signal',
            '-machine','q35','-accel','tcg,thread=single','-cpu',
            'qemu64,+rdrand' if args.rdrand else 'qemu64','-smp','1','-m','1536M',
            '-cdrom',str(iso),'-boot','d','-display','none','-nic','none',
            '-serial','unix:{private_uart},server=on,wait=off','-monitor','none','-no-reboot']
        receipt['boot_network_requirements']=[line.decode() for line in BOOT_NETWORK_LINES]
        receipt['config_sha256']=sha(boot/'limine.conf')
        receipt['initramfs_sha256']=sha(iso_root/'boot/initramfs.tar')
        receipt['iso_sha256']=sha(iso)
        receipt['result']='prepared'
        raw=(json.dumps(receipt,indent=2,sort_keys=True)+'\n').encode()
        (run/'prepare-receipt.json').write_bytes(raw)
        (run/'prepare-receipt.sha256').write_text(hashlib.sha256(raw).hexdigest()+'\n')
        print(run)
    except Exception as e:
        receipt['result']='failed';receipt['error']=f'{type(e).__name__}: {e}'
        (run/'prepare-failure.json').write_text(json.dumps(receipt,indent=2)+'\n')
        raise


def run_guest(args):
    run = Path(args.run).resolve()
    result_path=run/'run-result.json'
    with result_path.open('x') as f: json.dump({'result':'started'},f)
    result={'result':'failed','guest_stopped':False,'payload_send_attempted':False}
    serial=bytearray();proc=None
    try:
        receipt_file=run/'prepare-receipt.json'; raw=receipt_file.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=(run/'prepare-receipt.sha256').read_text().strip():
            raise ValueError('prepared receipt hash mismatch')
        receipt=json.loads(raw)
        if receipt.get('schema')!=1 or receipt.get('result')!='prepared': raise ValueError('not a prepared run')
        if receipt.get('boot_network_requirements')!=[line.decode() for line in BOOT_NETWORK_LINES]:
            raise ValueError('prepared boot requirements mismatch')
        result['prepare_sha256']=hashlib.sha256(raw).hexdigest()
        for name,item in receipt['inputs'].items():
            if sha(required_file(item['path']))!=item['sha256']: raise ValueError(f'changed input: {name}')
        if sdk_headers(Path(receipt['sdk']))!=receipt['sdk_headers_sha256']: raise ValueError('SDK headers changed')
        if 'suite' in receipt:
            validated, suite_inputs = suite_manifest(receipt['inputs']['suite_manifest']['path'],Path(receipt['sdk']))
            if validated != receipt['suite']: raise ValueError('prepared suite changed')
            for name,path in suite_inputs.items():
                saved=receipt['inputs'].get(name,{})
                if saved.get('path') != str(path) or saved.get('sha256') != sha(path):raise ValueError('prepared suite input changed: '+name)
        iso=required_file(run/'guest.iso')
        if sha(iso)!=receipt['iso_sha256']: raise ValueError('ISO hash mismatch')
        if sha(run/'iso-root/boot/limine/limine.conf')!=receipt['config_sha256']: raise ValueError('config hash mismatch')
        guard=StorageGuard(run.parent);guard.preflight()
        with tempfile.TemporaryDirectory(prefix='srv-public-',dir='/tmp') as temp:
            uart=str(Path(temp)/'uart')
            cmd=[arg.replace('{private_uart}',uart) for arg in receipt['qemu_argv_template']]
            result['argv']=cmd
            with (run/'qemu.log').open('xb') as log:
                proc=subprocess.Popen(cmd,stdout=log,stderr=log)
                result['owned_qemu_pid']=proc.pid
                with socket.socket(socket.AF_UNIX,socket.SOCK_STREAM) as sock:
                    end=time.monotonic()+30
                    while True:
                        try: sock.connect(uart);break
                        except (FileNotFoundError,ConnectionRefusedError):
                            if proc.poll() is not None or time.monotonic()>=end: raise RuntimeError('serial connection failed')
                            time.sleep(0.05)
                    sock.settimeout(0.2)
                    def receive_until(predicate,seconds):
                        data=bytearray();end=time.monotonic()+seconds
                        while time.monotonic()<end:
                            try:
                                part=sock.recv(65536)
                                if not part: raise RuntimeError('serial closed')
                                data.extend(part);serial.extend(part)
                                if len(serial)>16*1024*1024: raise RuntimeError('serial output exceeded 16 MiB')
                                if predicate(data): return bytes(data)
                            except socket.timeout: pass
                        raise TimeoutError('guest serial deadline expired')
                    boot=receive_until(lambda b:b'srv> ' in b,90)
                    (run/'boot-serial.log').write_bytes(boot)
                    result['boot_serial_sha256']=sha(run/'boot-serial.log')
                    result['boot_network_state']=boot_network_state(boot)
                    if not result['boot_network_state']['allowed']:
                        raise RuntimeError('boot network precondition refused; payload not sent')
                    suite=receipt.get('suite')
                    if suite is None:
                        result['payload_send_attempted']=True
                        sock.sendall(b'run /signal-public\n')
                        data=receive_until(lambda b:parsed(b) is not None,150)
                        pid,status=parsed(data)
                        result.update(pid=pid,exit_status=status,markers={m.decode():data.count(m) for m in MARKERS})
                        if status!=0 or any(data.count(m)!=1 for m in MARKERS): raise RuntimeError('probe exit or marker mismatch')
                    else:
                        result['cases']=[]
                        for case in suite['cases']:
                            guard.preflight()
                            item={'name':case['name'],'payload':case['payload'],'result':'started'}
                            result['cases'].append(item)
                            result_path.write_text(json.dumps(result,indent=2)+'\n')
                            start=len(serial); started=time.monotonic()
                            try:
                                result['payload_send_attempted']=True
                                command='run /'+case['payload']+(' '+' '.join(monitor_arg(a) for a in case['args']) if case['args'] else '')+'\n'
                                item['command']=command.rstrip('\n')
                                sock.sendall(command.encode('ascii'))
                                data=receive_until(lambda b:parsed(b) is not None,case['timeout'])
                                pid,status=parsed(data)
                                item.update(pid=pid,exit_status=status,markers={m:full_line_count(data,m) for m in case['markers']})
                                if status!=0 or any(v!=1 for v in item['markers'].values()): raise RuntimeError('case exit or marker mismatch')
                                item['result']='passed'
                            except Exception as error:
                                item['result']='failed';item['error']=f'{type(error).__name__}: {error}'
                                raise
                            finally:
                                item['seconds']=round(time.monotonic()-started,3)
                                case_log=run/('case-'+case['name']+'.log')
                                case_log.write_bytes(serial[start:])
                                item['serial_sha256']=sha(case_log)
                                result_path.write_text(json.dumps(result,indent=2)+'\n')
                    result['result']='passed'
    except Exception as e:
        result['error']=f'{type(e).__name__}: {e}'
    finally:
        if proc is not None:
            if proc.poll() is None:
                proc.terminate()
                try:proc.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    try:proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:result['stop_error']='owned QEMU did not terminate after kill'
            result['guest_stopped']=proc.poll() is not None
        (run/'serial.log').write_bytes(serial)
        result['serial_sha256']=sha(run/'serial.log')
        result_path.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    if result['result']!='passed' or not result['guest_stopped']: raise SystemExit(1)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    action=p.add_mutually_exclusive_group(required=True)
    action.add_argument('--prepare',action='store_true')
    action.add_argument('--run',type=Path,metavar='PREPARED_DIR')
    action.add_argument('--self-test',action='store_true')
    p.add_argument('--suite-manifest',type=Path,help='JSON suite manifest for --prepare')
    p.add_argument('--rdrand',action='store_true',help='prepare only: expose RDRAND for the reviewed entropy-provider guest')
    p.add_argument('--output-parent',type=Path,help='existing absolute task-owned APFS directory')
    p.add_argument('--kernel',type=Path,default=ROOT/'build/signal-context/public-experimental/kernel.elf')
    p.add_argument('--probe',type=Path,default=ROOT/'build/signal-context/public-experimental/probe.elf')
    p.add_argument('--init',type=Path,default=ROOT/'build/userspace/init.elf')
    p.add_argument('--sdk',type=Path,default=ROOT/'build/sysroot/srvros')
    a=p.parse_args()
    if a.suite_manifest and not a.prepare:p.error('--suite-manifest requires --prepare')
    if a.rdrand and not a.prepare:p.error('--rdrand requires --prepare')
    if a.self_test: parser_selftest()
    elif a.prepare:
        if not a.output_parent or not a.output_parent.is_absolute() or not a.output_parent.is_dir():p.error('--output-parent must be an existing absolute task-owned directory')
        prepare(a)
    else:run_guest(a)


if __name__=='__main__':main()
