#!/usr/bin/env python3
"""Qualify host export recovery from an existing synthetic A, then boot fresh B.

A and its original baseline are read-only inputs. Every output/attempt and guest
disk is newly generated under a private evidence directory. Run one build/VM
harness at a time after a source-matched build. No physical device is accepted.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import traceback

from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from profile_installation_smoke import clone
from uefi_usb_smoke import default_ovmf_path
import installation_export_publication as publication
import installation_image as image
import installation_inspection as inspection
import profile_bundle as p
import profile_chooser_smoke as c
import profile_create_smoke as create
import profile_desktop_smoke as d
import profile_switch_smoke as switch
import profile_transfer_smoke as transfer
import source_provenance

ROOT = Path(__file__).resolve().parents[1]
DOCS = ['Documents/migration.txt', 'Documents/binary.dat']
HOME_B = '/fat/home/recovered_b'
GIB = 1024**3

# Fresh-process diagnostics intercept only host publication syscalls, armed only
# after native source admission and the durable publication record exist.
FAULT_WORKER = r'''
import json,os,stat,sys
from pathlib import Path
from unittest import mock
sys.path.insert(0,sys.argv[1])
import installation_export as export
import installation_export_publication as pub
mode,stage,out,attempt,who,home,inspect_driver,export_driver=sys.argv[2:]
real_write,real_sync,real_link,real_unlink,real_close=os.write,os.fsync,os.link,os.unlink,os.close
armed=False;after_unlink=False;triggered=[];bid=None;size=None
def checkpoint(name):
 global armed,after_unlink,bid,size
 if name=='recorded':
  record=json.loads((Path(attempt)/'intent.json').read_bytes());bid=record['bundle_identity'];size=record['size'];armed=True
 if name=='unlinked':after_unlink=True
def candidate(fd):return armed and pub.identity(os.fstat(fd))==bid
def write(fd,data):
 if mode in ('write','short-write') and candidate(fd):
  triggered.append(mode)
  if mode=='short-write':return real_write(fd,data[:1])
  raise OSError('injected payload write')
 if mode=='ack-write' and after_unlink and stat.S_ISREG(os.fstat(fd).st_mode) and not candidate(fd):
  triggered.append(mode);return real_write(fd,data[:1])
 return real_write(fd,data)
def sync(fd):
 if mode=='file-sync' and candidate(fd) and os.fstat(fd).st_size==size:
  triggered.append(mode);raise OSError('injected payload sync')
 if mode=='directory-sync' and after_unlink and pub.identity(os.fstat(fd))==pub.identity(Path(out).parent.stat()):
  triggered.append(mode);raise OSError('injected directory sync')
 return real_sync(fd)
def link(*a,**kw):
 if mode=='link' and armed:triggered.append(mode);raise OSError('injected link')
 return real_link(*a,**kw)
def unlink(path,*a,**kw):
 if mode=='unlink' and path=='bundle.partial':triggered.append(mode);raise OSError('injected unlink')
 return real_unlink(path,*a,**kw)
def close(fd):
 if mode=='close' and after_unlink and not triggered and pub.identity(os.fstat(fd))==pub.identity(Path(out).parent.stat()):
  triggered.append(mode);real_close(fd);raise OSError('injected directory close')
 return real_close(fd)
with mock.patch.object(os,'write',side_effect=write),mock.patch.object(os,'fsync',side_effect=sync),mock.patch.object(os,'link',side_effect=link),mock.patch.object(os,'unlink',side_effect=unlink),mock.patch.object(os,'close',side_effect=close):
 result=export.export_installed(stage,out,who,home,['Documents/migration.txt','Documents/binary.dat'],inspect_driver,export_driver,attempt,checkpoint)
print(json.dumps({'injected':triggered,'result':result}))
'''


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def stamp(path):
    st = path.lstat()
    result = {'device': st.st_dev, 'inode': st.st_ino, 'mode': st.st_mode, 'uid': st.st_uid,
              'links': st.st_nlink, 'size': st.st_size, 'mtime': st.st_mtime_ns, 'ctime': st.st_ctime_ns}
    if stat.S_ISREG(st.st_mode): result['sha256'] = image.digest(path)
    if stat.S_ISLNK(st.st_mode): result['symlink'] = os.readlink(path)
    return result


def tree(path):
    return {'.': stamp(path), **{str(p.relative_to(path)): stamp(p) for p in sorted(path.rglob('*'))}}


def review_vms():
    result = {}
    for pid in (61610, 6233, 85869):
        command = subprocess.check_output(['ps', '-p', str(pid), '-o', 'comm='], text=True).strip()
        require('qemu-system-x86_64' in command, 'review VM missing or replaced')
        result[str(pid)] = command
    return result


def readback(vm):
    start = len(vm.output)
    vm.key('meta_l-spc'); transfer.text(vm, 'Text new app'); vm.key('ret')
    c.observe(vm, rb'desktools: starting role=text', start); c.observe(vm, rb'deskd: commit', start)
    vm.key('ctrl-l'); vm.key('ctrl-a'); transfer.text(vm, HOME_B + '/Documents/migration.txt')
    vm.key('ctrl-o'); c.observe(vm, rb'picker ready mode=1', start)
    transfer.text(vm, 'migration'); vm.key('ret')
    c.observe(vm, rb'file-result role=text op=1 status=0', start)
    vm.key('ctrl-a'); vm.key('ctrl-c')
    c.observe(vm, rb'clipboard role=2 action=8 bytes=24', start)
    vm.screenshot('recovered-document')
    terminal, *_ = switch.launch(vm, 'Terminal', rb'deskterm: ready shell=\d+')
    switch.type_shell(vm, '/fat/bin/env > ' + HOME_B + '/.cache/proof-env; /fat/bin/pwd > ' +
                      HOME_B + '/.cache/proof-cwd; exit')
    c.observe(vm, rb'deskterm: shell exited status=0', terminal)
    vm.key('ctrl-shift-w'); c.observe(vm, rb'deskterm: exited failure=0', terminal)
    vm.screenshot('recovered-session'); vm.close_desktop(); vm.finish()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-stage', required=True, type=Path)
    parser.add_argument('--profile-id', required=True)
    parser.add_argument('--home', required=True)
    parser.add_argument('--evidence-parent', type=Path, default=ROOT / 'build/review/export-recovery')
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    require(10 <= args.boot_wait <= 240, 'invalid boot wait')
    for firmware in (args.ovmf, args.ovmf_vars):
        require(Path(firmware).is_file() and ',' not in str(firmware), 'invalid firmware path')
    stage = image.checked_path(args.source_stage)
    parent = image.checked_path(args.evidence_parent); parent.mkdir(parents=True, exist_ok=True)
    require(parent != stage and stage not in parent.parents, 'evidence overlaps source stage')
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=parent))
    receipt = {'status': 'INCOMPLETE', 'scope': 'synthetic offline A; host process/I/O faults; two fresh-B boots',
               'limits': ['no physical media or power-loss acceptance', 'source observation is historical, not authentication'],
               'steps': [], 'boots': []}
    def save(): d.save_json(out / 'receipt.json', receipt)
    print('Export recovery evidence: ' + str(out), flush=True); save()
    try:
        receipt['source'] = source_provenance.capture(ROOT)
        require(not any(v.get('dirty') for v in receipt['source'].values()), 'source must be clean')
        receipt['build'] = d.verified_build(ROOT)
        receipt['review_vms_before'] = review_vms()
        receipt['firmware'] = {str(path): image.digest(path) for path in map(Path, (args.ovmf, args.ovmf_vars))}
        receipt['qemu_version'] = subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]
        receipt['tools'] = {name: image.digest(ROOT / 'tools' / name) for name in
            ('installation_export_recovery_smoke.py', 'installation_export_publication.py',
             'installation_export.py', 'installation_image.py', 'installation_inspection.py',
             'profile_create_smoke.py', 'profile_chooser_smoke.py', 'profile_switch_smoke.py',
             'profile_transfer_smoke.py', 'profile_bundle.py', 'exfat_test_image.py')}
        require(shutil.disk_usage(out).free >= 8 * GIB, 'need 8 GiB free for bounded two-disk proof')
        receipt['space'] = {'free_before': shutil.disk_usage(out).free, 'reserve': 4 * GIB}
        inputs = {name: ROOT / path for name, path in {
            'kernel': 'build/kernel/srvros.elf', 'boot': 'build/limine-binary/BOOTX64.EFI',
            'init': 'build/userspace/init.elf', 'base': 'build/srvros.exfat',
            'profile_driver': 'build/tests/profile-image-driver', 'inspect_driver': 'build/tests/installation-inspect-driver',
            'export_driver': 'build/tests/installation-export-driver', 'fs_driver': 'build/tests/exfat-driver'}.items()}
        originals = {name: stamp(path) for name, path in inputs.items()}
        receipt['inputs'] = originals
        plan, _ = inspection.record_plan(stage)
        baseline = stage / 'profile.exfat' if plan['version'] == 2 else Path(plan['sources']['exfat']['path'])
        source_before = {'stage': tree(stage), 'baseline_path': str(baseline), 'baseline': stamp(baseline)}
        receipt['preserved_source'] = source_before
        def guard():
            require(tree(stage) == source_before['stage'] and stamp(baseline) == source_before['baseline'], 'A/baseline changed')
            require({name: stamp(path) for name, path in inputs.items()} == originals, 'matched build input changed')
            require(shutil.disk_usage(out).free >= 4 * GIB, 'evidence reserve exhausted')
        command = [sys.executable, str(ROOT / 'tools/installation_image.py')]
        def call(argv, code=0):
            guard(); proc = subprocess.run(argv, capture_output=True, text=True)
            row = {'argv': argv, 'exit': proc.returncode, 'stdout': proc.stdout, 'stderr': proc.stderr}
            receipt['steps'].append(row); save(); guard()
            require(proc.returncode == code, 'unexpected command exit: ' + str(row))
            return json.loads(proc.stdout) if proc.stdout.strip() else None
        def export_args(folder, checkpoint=None, source=stage, home=args.home):
            argv = command + ['export-profile', str(source), str(folder / 'output.json'),
                '--profile-id', args.profile_id, '--home', home,
                '--inspect-driver', str(inputs['inspect_driver']), '--export-driver', str(inputs['export_driver']),
                '--publication-dir', str(folder / 'attempt')]
            for doc in DOCS: argv += ['--document', doc]
            if checkpoint: argv += ['--interrupt-after', checkpoint]
            return argv
        def case(name):
            folder = out / name; folder.mkdir(); return folder
        normal = case('normal')
        result = call(export_args(normal)); require(result['state'] == 'exported', 'normal tracked export failed')
        expected_bytes = (normal / 'output.json').read_bytes(); bundle = p.decode_json(expected_bytes)
        payloads = p.validate_bundle(bundle); identity = bundle['profile']
        require(identity['id'] == args.profile_id and payloads == dict(zip(DOCS, (create.TEXT, create.BINARY))) and
                bundle['preferences'] == create.DEFAULTS, 'source is not the qualified synthetic default-profile fixture')
        receipt['export'] = {'sha256': hashlib.sha256(expected_bytes).hexdigest(), 'profile': identity,
                             'preferences': bundle['preferences'], 'documents': bundle['documents']}
        def inspect_case(folder):
            before = tree(folder); result = call(command + ['inspect-export', str(folder / 'attempt')])
            require(tree(folder) == before, 'inspect-export mutated publication'); return result
        def recover(folder):
            result = call(command + ['reconcile-export', str(folder / 'attempt')])
            require(result['state'] == 'complete' and (folder / 'output.json').read_bytes() == expected_bytes, 'reconcile content mismatch')
            before = tree(folder); call(command + ['reconcile-export', str(folder / 'attempt')])
            require(tree(folder) == before, 'repeat reconciliation changed bytes/identity/metadata')
        for checkpoint in publication.CHECKPOINTS:
            folder = case('stop-' + checkpoint); call(export_args(folder, checkpoint), 75)
            observed = inspect_case(folder)
            if checkpoint in ('recorded', 'partial-write'):
                require(observed['state'] == 'incomplete', 'partial state not recognized')
                before = tree(folder); call(command + ['reconcile-export', str(folder / 'attempt')], 1)
                require(tree(folder) == before, 'partial refusal mutated files')
            else: recover(folder)
        for checkpoint in publication.FINISH_CHECKPOINTS:
            folder = case('reconcile-stop-' + checkpoint); call(export_args(folder, 'written'), 75)
            call(command + ['reconcile-export', str(folder / 'attempt'), '--interrupt-after', checkpoint], 75)
            require(inspect_case(folder)['can_reconcile'], 'reconcile interruption became ambiguous'); recover(folder)
        for fault in ('write', 'short-write', 'file-sync', 'link', 'unlink', 'directory-sync', 'ack-write', 'close'):
            folder = case('io-' + fault)
            result = call([sys.executable, '-c', FAULT_WORKER, str(ROOT / 'tools'), fault, str(stage),
                str(folder / 'output.json'), str(folder / 'attempt'), args.profile_id, args.home,
                str(inputs['inspect_driver']), str(inputs['export_driver'])])
            require(result['injected'] and result['result']['state'] == 'refused', 'fault not reached/refused')
            if fault in ('write', 'short-write', 'ack-write'):
                before = tree(folder)
                call(command + ['inspect-export', str(folder / 'attempt')], 1 if fault == 'ack-write' else 0)
                call(command + ['reconcile-export', str(folder / 'attempt')], 1)
                require(tree(folder) == before, 'I/O refusal mutated retained evidence')
            else: inspect_case(folder); recover(folder)
        # Fixture publication for corruption cases uses the already admitted
        # exact snapshot; only these new output/record files are mutated.
        record = json.loads((normal / 'attempt/intent.json').read_bytes())
        for kind in ('torn-record', 'torn-ack', 'wrong-hash', 'wrong-inode', 'extra-link', 'symlink', 'unknown', 'parent', 'mode', 'future-v2'):
            folder = case('refuse-' + kind); dest = folder / 'dest'; dest.mkdir()
            attempt, output = folder / 'attempt', dest / 'output.json'
            require(publication.publish(attempt, output, bundle, record['source_snapshot'])['state'] == 'complete', 'refusal fixture failed')
            if kind == 'torn-record': (attempt / 'intent.json').write_bytes(b'{')
            elif kind == 'torn-ack': (attempt / 'complete.json').write_bytes(b'{')
            elif kind == 'wrong-hash': output.write_bytes(expected_bytes.upper())
            elif kind == 'wrong-inode':
                output.rename(dest / 'preserved-original'); output.write_bytes(expected_bytes)
            elif kind == 'extra-link': os.link(output, folder / 'extra')
            elif kind == 'symlink':
                output.rename(dest / 'preserved-original'); output.symlink_to(dest / 'preserved-original')
            elif kind == 'unknown': (attempt / 'unknown').write_bytes(b'preserve')
            elif kind == 'parent': dest.rename(folder / 'moved'); dest.mkdir()
            elif kind == 'mode': attempt.chmod(0o755)
            else:
                altered = p.encode_json(dict(bundle, schema_version=2)); output.write_bytes(altered)
                value = json.loads((attempt / 'intent.json').read_bytes()); value['sha256'] = hashlib.sha256(altered).hexdigest(); value['size'] = len(altered)
                encoded = p.encode_json(value); (attempt / 'intent.json').write_bytes(encoded)
                (attempt / 'intent.sha256').write_text(hashlib.sha256(encoded).hexdigest() + '\n')
            before = tree(folder)
            call(command + ['inspect-export', str(attempt)], 1); call(command + ['reconcile-export', str(attempt)], 1)
            require(tree(folder) == before, 'refusal changed namespace/content/metadata: ' + kind)
        for lock_path, argv in ((normal / 'attempt/.lock', command + ['reconcile-export', str(normal / 'attempt')]),
                                (stage / 'plan.json', export_args(case('refuse-source-busy')))):
            before = tree(lock_path.parent)
            with lock_path.open('rb') as fd:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                call(argv, 1)
            require(tree(lock_path.parent) == before, 'busy refusal mutated protected namespace')
        restored_bundle = out / 'io-directory-sync/output.json'
        require(restored_bundle.read_bytes() == expected_bytes, 'restore must consume exact reconciled bytes')
        volume = create.make_volume(inputs['base'], out, 'b-base', d.machine_preferences(2, 60))
        bstage = out / 'stage-b'
        image.prepare(bstage, inputs['kernel'], inputs['boot'], volume, inputs['init'], identity_bound=True,
            profile_bundle=restored_bundle, profile_home=HOME_B, profile_alias='recovered_b',
            profile_uid=3050, profile_gid=3051, profile_driver=inputs['profile_driver'])
        ready = image.apply(bstage); disk = bstage / 'image.img'
        transport, transport_record = create.install(inputs['base'], out / 'transport')
        offset, size = d.partitions(disk)[1]; protected = d.outside_data(disk, offset, size)
        source_guids = set(plan['gpt'][key] for key in ('disk_guid', 'esp_partition_guid', 'exfat_partition_guid'))
        require(not source_guids & set(ready['gpt'].values()), 'fresh B reused source GPT identity')
        receipt['fresh_b'] = {'ready': ready, 'protected_regions': protected, 'bundle_sha256': image.digest(restored_bundle),
                              'baseline': stamp(volume), 'transport': transport_record}
        preboot = transfer.check_allocation(disk, out, 'b-preboot')
        prefix = HOME_B.removeprefix('/fat/') + '/'
        for directory in ('.cache', '.local/share'):
            require(not {name for name, _ in preboot.entries(preboot.read(prefix + directory)) if name not in ('.', '..')},
                    'fresh B inherited runtime state')
        for path in ('Documents/unselected.txt', '.config/credential-sentinel.fixture',
                     '.local/share/app-state.fixture', '.cache/runtime-cache.fixture'):
            create.absent(preboot, prefix + path)
        for iteration in (1, 2):
            label = 'b' + str(iteration)
            with c.VM(disk, transport, out, label, args) as vm:
                if iteration == 1:
                    start, index = c.chooser(vm, HOME_B); c.select(vm, index); c.opened(vm, HOME_B, start, identity['id'])
                else: create.start_restored(vm, HOME_B, identity)
                readback(vm)
            fs = transfer.check_allocation(disk, out, label)
            prefix = HOME_B.removeprefix('/fat/') + '/'
            require(json.loads(fs.read(prefix + 'identity.json')) == identity and
                    json.loads(fs.read(prefix + 'profile-local.json')) == {'alias': 'recovered_b', 'uid': 3050, 'gid': 3051} and
                    json.loads(fs.read(prefix + p.PREF_PATH)) == bundle['preferences'] and
                    all(fs.read(prefix + name) == data for name, data in payloads.items()), 'B restore mismatch')
            require({name for name, _ in fs.entries(fs.read(prefix + 'Documents')) if name not in ('.', '..')} == {'migration.txt', 'binary.dat'}, 'B inherited unselected documents')
            create.legacy_and_binaries(fs, d.machine_preferences(2, 60)); create.check_choice(fs, HOME_B, identity)
            for name in ('env', 'pwd'):
                require(fs.read('bin/' + name) == runtime_elf((ROOT / f'build/userspace/{name}.elf').read_bytes()), 'context binary mismatch')
            create.absent(fs, prefix + '.profile-import-pending')
            create.absent(fs, HOME_B.removeprefix('/fat/') + '.profile-import.json')
            env = fs.read(prefix + '.cache/proof-env').decode().splitlines()
            expected_env = {'HOME': HOME_B, 'SRVROS_DESKTOP_HOME': HOME_B, 'SRVROS_PROFILE_ID': identity['id'],
                            'XDG_CONFIG_HOME': HOME_B + '/.config', 'XDG_DATA_HOME': HOME_B + '/.local/share', 'XDG_CACHE_HOME': HOME_B + '/.cache'}
            require(all([line for line in env if line.startswith(key + '=')] == [key + '=' + value] for key, value in expected_env.items()) and
                    fs.read(prefix + '.cache/proof-cwd').decode().strip() == HOME_B, 'B environment/cwd mismatch')
            require(d.outside_data(disk, offset, size) == protected, 'B protected regions changed')
            receipt['boots'].append(label); guard(); save()
        b_before = tree(bstage)
        inspected = call(command + ['inspect-installed', str(bstage), '--driver', str(inputs['inspect_driver'])])
        require(inspected['consistent'] and inspected['used'] and inspected['filesystem']['writes'] ==
                inspected['filesystem']['flushes'] == 0, 'used B native inspection failed')
        reexport = case('b-reexport'); result = call(export_args(reexport, source=bstage, home=HOME_B))
        require(result['state'] == 'exported' and (reexport / 'output.json').read_bytes() == expected_bytes and
                tree(bstage) == b_before, 'B re-export changed source or portable bytes')
        receipt['b_roundtrip_export'] = result
        guard(); require(source_provenance.capture(ROOT) == receipt['source'], 'source checkout changed')
        require({str(path): image.digest(path) for path in map(Path, (args.ovmf, args.ovmf_vars))} == receipt['firmware'], 'firmware changed')
        require({name: image.digest(ROOT / 'tools' / name) for name in receipt['tools']} == receipt['tools'], 'proof tool changed')
        receipt['review_vms_after'] = review_vms()
        processes = subprocess.check_output(['ps', '-axo', 'comm=,command='], text=True).splitlines()
        require(not [line for line in processes if line.split()[0].endswith('qemu-system-x86_64') and str(out) in line], 'owned QEMU still active')
        receipt['owned_qemu_remaining'] = 0
        receipt['final_b'] = stamp(disk); receipt['final_source'] = source_before
        artifacts = out / 'matched-artifacts'; artifacts.mkdir()
        for name, src in {**inputs, 'build.json': ROOT / 'build/macos-build.json',
                          'initramfs': ROOT / 'build/initramfs.tar', **{name: ROOT / f'build/userspace/{name}.elf' for name in
                           ('deskd', 'desktools', 'deskterm', 'profile', 'deskprofiles')}}.items():
            target = artifacts / name; clone(src, target); require(image.digest(target) == image.digest(src), 'artifact preservation mismatch')
        receipt['status'] = 'PASS'; save(); print('PASS: interruption/I/O/reconciliation/refusal and fresh-B restore/reboot', flush=True)
    except BaseException as error:
        receipt.update(status='FAIL', error=str(error)); save(); traceback.print_exc(); raise


if __name__ == '__main__':
    main()
