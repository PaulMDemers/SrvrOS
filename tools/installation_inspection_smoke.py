#!/usr/bin/env python3
"""Offline installed-image inspection proof on private generated QEMU images.

All evidence is retained. This does not test physical disks or power loss.
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import traceback
import zlib

from exfat_test_image import ExfatFixture
from installation_io_faults import snapshot
from installation_io_smoke import Storage, MIB
from pack_runtime_elf import runtime_elf
from profile_desktop_smoke import (ROOT, ID, BINARY, EDITED, boot, data_image, digest,
    machine_preferences, make_bundle, outside_data, partitions, save_json, verified_build)
from profile_installation_smoke import clone, seed_volume
from uefi_usb_smoke import default_ovmf_path
import installation_image as image
import profile_bundle
import source_provenance


def payload_offset(fs, path):
    directory = fs.chain(fs.root)
    for part in path.split('/'):
        entry = next(e for name, e in fs.entries(directory) if name == part)
        first = struct.unpack_from('<I', entry, 52)[0]
        size = struct.unpack_from('<Q', entry, 56)[0]
        directory = fs.chain(first, size, bool(entry[33] & 2))
    if not size:
        raise RuntimeError('empty fixture payload: ' + path)
    return fs.offset(first), directory


def differences(old, new):
    """Compact, reversible byte patches; never store an entire second image."""
    if len(old) != len(new):
        raise RuntimeError('fixture length changed')
    changes = []
    i = 0
    while i < len(old):
        if old[i:i+4096] == new[i:i+4096]:
            i += min(4096, len(old)-i)
            continue
        if old[i] == new[i]:
            i += 1
            continue
        start = i
        while i < len(old) and old[i] != new[i]:
            i += 1
        changes.append({'offset': start, 'before': old[start:i].hex(), 'after': new[start:i].hex()})
    if not changes:
        raise RuntimeError('fault did not change image')
    return changes


def run(args, storage):
    out = storage.out
    receipt_path = out / 'receipt.json'
    receipt = {'status': 'INCOMPLETE', 'scope': 'private generated offline QEMU image',
               'limits': ['not physical disk acceptance', 'not power-loss or media-fault simulation'],
               'steps': [], 'boots': {}, 'faults': {}}
    save_json(receipt_path, receipt)
    print(f'Installation inspection evidence: {out}', flush=True)
    try:
        receipt['source'] = source_provenance.capture(ROOT)
        receipt['build'] = verified_build(ROOT)
        with tarfile.open(ROOT / 'build/initramfs.tar') as archive:
            member = next(m for m in archive.getmembers() if m.name.lstrip('./') == 'init')
            if archive.extractfile(member).read() != runtime_elf((ROOT / 'build/userspace/init.elf').read_bytes()):
                raise RuntimeError('bootstrap init differs from matched build')
        receipt['qemu_version'] = subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]
        receipt['firmware_sha256'] = image.digest(args.ovmf)
        receipt['firmware_vars_sha256'] = image.digest(args.ovmf_vars)
        originals = {'kernel': ROOT/'build/kernel/srvros.elf', 'bootx64': ROOT/'build/limine-binary/BOOTX64.EFI',
                     'exfat': ROOT/'build/srvros.exfat', 'init': ROOT/'build/userspace/init.elf',
                     'profile_driver': ROOT/'build/tests/profile-image-driver',
                     'inspect_driver': ROOT/'build/tests/installation-inspect-driver',
                     'fs_driver': ROOT/'build/tests/exfat-driver',
                     **{app:ROOT/f'build/userspace/{app}.elf' for app in ('deskd','desktools','deskterm')}}
        receipt['inputs'] = {k:image.digest(v) for k,v in originals.items()}
        receipt['tools'] = {name:image.digest(ROOT/'tools'/name) for name in
            ('installation_inspection_smoke.py','installation_inspection.py','installation_image.py',
             'profile_desktop_smoke.py','profile_installation_smoke.py','profile_image.py',
             'profile_bundle.py','installation_io_smoke.py','exfat_test_image.py')}
        private = out/'inputs'; private.mkdir()
        paths = {}
        for name in ('kernel','bootx64','init','profile_driver','inspect_driver'):
            paths[name] = private/name
            storage.call(originals[name].stat().st_size, clone, originals[name], paths[name])
        for name in ('profile_driver','inspect_driver'): paths[name].chmod(0o700)
        paths['exfat'] = private/'base.exfat'
        prefs = machine_preferences(4, -300)
        storage.call(2*originals['exfat'].stat().st_size, seed_volume, originals['exfat'],
                     paths['exfat'], prefs, paths['profile_driver'], out)
        paths['bundle'] = private/'bundle.json'
        profile_bundle.publish_bundle(paths['bundle'], make_bundle())
        hashes = {k:image.digest(v) for k,v in paths.items()}
        receipt['private_inputs'] = hashes
        save_json(receipt_path, receipt)
        def verify_sources():
            now = {k:image.digest(v) for k,v in paths.items()}
            build = {k:image.digest(v) for k,v in originals.items()}
            if now != hashes or build != receipt['inputs']:
                raise RuntimeError('source or driver changed')
            return {'private':now, 'build':build}
        stage = out/'stage'; home = '/fat/home/local-a'
        storage.check(4*paths['exfat'].stat().st_size + 512*MIB)
        verify_sources()
        plan = image.prepare(stage, paths['kernel'], paths['bootx64'], paths['exfat'], paths['init'],
            identity_bound=True, profile_bundle=paths['bundle'], profile_home=home,
            profile_alias='local_a', profile_uid=1001, profile_gid=1001, profile_driver=paths['profile_driver'])
        verify_sources()
        if plan['version'] != 2: raise RuntimeError('profile plan version')
        receipt['plan'] = plan; receipt['prepared'] = snapshot(stage); save_json(receipt_path, receipt)
        verify_sources(); image.apply(stage); verify_sources(); storage.check()
        ready_hash = image.digest(stage/'ready.json')
        receipt['ready_sha256'] = ready_hash
        receipt['publication'] = snapshot(stage); save_json(receipt_path, receipt)
        disk = stage/'image.img'; (esp_offset, esp_size), (offset, size) = partitions(disk)
        protected = outside_data(disk, offset, size)
        def command(action, *extra):
            cmd = [sys.executable, str(ROOT/'tools/installation_image.py'), action, str(stage), *map(str,extra)]
            proc = subprocess.run(cmd, capture_output=True, text=True)
            return {'argv':cmd, 'exit':proc.returncode, 'stdout':proc.stdout, 'stderr':proc.stderr}
        expected_refusals = {
            'kernel-payload':'ESP payload mismatch', 'config-payload':'expected Limine config',
            'esp-padding':'protected image bytes changed', 'gpt-root-guid':'invalid GPT entries checksum',
            'gpt-partition-label':'invalid GPT entries checksum', 'dirty-boot':'clean-admission',
            'bad-boot-backup':'clean-admission', 'allocation-leak':'clean-admission',
            'marker-uuid':'invalid profile ID', 'identity-json':'invalid or truncated JSON',
            'local-binding':'invalid or truncated JSON', 'portable-preferences':'invalid or truncated JSON',
            'pending-marker':'pending import or selection scratch', 'journal':'pending import or selection scratch',
            'torn-journal':'pending import or selection scratch', 'copy-scratch':'pending import or selection scratch',
            'selection-scratch':'pending import or selection scratch', 'selection-stale':'stale remembered selection',
            'selection-checksum':'invalid remembered selection checksum', 'torn-ready':'historical ready record mismatch',
            'unknown-stage-entry':'stage is not a complete settled publication',
            'missing-retained-baseline':'historical data baseline unavailable or changed',
            'tampered-retained-baseline':'historical profile image changed',
            'busy-image-lock':'Resource temporarily unavailable',
        }
        def audit(action, expect, label, used=None):
            before_sources = verify_sources(); before = snapshot(stage)
            call = command(action, *(['--driver',paths['inspect_driver']] if action == 'inspect-installed' else []))
            after = snapshot(stage); after_sources = verify_sources()
            step = {'label':label, 'call':call, 'before':before, 'after':after,
                    'sources_before':before_sources, 'sources_after':after_sources}
            receipt['steps'].append(step); save_json(receipt_path, receipt)
            if before != after or before_sources != after_sources:
                raise RuntimeError(label + ': audit changed inputs or stage')
            if action == 'inspect-installed':
                result = json.loads(call['stdout']); step['result'] = result
                if call['exit'] != (0 if expect else 1) or result['consistent'] is not expect:
                    raise RuntimeError(label + ': unexpected installed result ' + str(call))
                if not expect:
                    expected_reason = expected_refusals[label]
                    actual_reason = result.get('reason')
                    step['expected_reason'] = expected_reason; step['actual_reason'] = actual_reason
                    save_json(receipt_path, receipt)
                    if not isinstance(actual_reason,str) or expected_reason not in actual_reason:
                        raise RuntimeError(label + ': wrong refusal reason: ' + repr(actual_reason))
                if expect:
                    fs = result['filesystem']
                    if result['used'] is not used or fs['writes'] != 0 or fs['flushes'] != 0 or any(fs['allocation'][k] for k in ('leaked','fat_bitmap_mismatches','errors')):
                        raise RuntimeError(label + ': invalid used/allocation/counters')
            elif action == 'inspect':
                result = json.loads(call['stdout']); step['result'] = result
                if call['exit'] or result['can_reconcile'] is not expect:
                    raise RuntimeError(label + ': unexpected publication inspection')
            elif (call['exit'] == 0) is not expect:
                raise RuntimeError(label + ': unexpected reconcile result')
            save_json(receipt_path, receipt)
            return step
        audit('inspect-installed', True, 'pristine', False)
        for number in ('a1','a2'):
            verify_sources()
            storage.call(64*MIB, boot, disk, out, number, args, home, 4, -300, edit=(number=='a1'))
            verify_sources()
            serial = (out/f'serial-{number}.log').read_bytes()
            markers = [b'installed-root: selected device=ahci0p2 format=exfat-v1 role=system',
                       b'namespace: /bin /usr /var /home active',
                       f'deskd: profile id={ID} home={home} credentials=unchanged'.encode()]
            indexes = [serial.find(x) for x in markers]
            if not 0 <= indexes[0] < indexes[1] < indexes[2]: raise RuntimeError('boot launch order')
            if outside_data(disk,offset,size) != protected: raise RuntimeError('protected regions changed')
            data = data_image(disk,offset,size); fs = ExfatFixture(data); prefix = home.removeprefix('/fat/')+'/'
            if fs.read(prefix+'profile.id') != b'srvros-profile-v1\n'+ID.encode()+b'\n': raise RuntimeError('profile marker')
            identity = profile_bundle.decode_json(fs.read(prefix+'identity.json'))
            binding = profile_bundle.decode_json(fs.read(prefix+'profile-local.json'))
            portable = profile_bundle.decode_json(fs.read(prefix+profile_bundle.PREF_PATH))
            if identity != make_bundle()['profile'] or binding != {'alias':'local_a','uid':1001,'gid':1001}:
                raise RuntimeError('identity or local binding')
            if fs.read(prefix+'Documents/migration.txt') != EDITED or fs.read(prefix+'Documents/binary.dat') != BINARY or portable != dict(make_bundle()['preferences'],theme_style=1) or fs.read('home/desktop.conf') != prefs:
                raise RuntimeError('documents or preferences changed')
            env = fs.read(prefix+'Documents/environment.txt').decode(); cwd = fs.read(prefix+'Documents/cwd.txt').decode().strip()
            expected = {'HOME':home,'SRVROS_DESKTOP_HOME':home,'SRVROS_PROFILE_ID':ID,
                        'XDG_CONFIG_HOME':home+'/.config','XDG_DATA_HOME':home+'/.local/share','XDG_CACHE_HOME':home+'/.cache'}
            if cwd != home or any(f'{k}={v}' not in env.splitlines() for k,v in expected.items()): raise RuntimeError('environment or cwd')
            apps = {}
            for app in ('deskd','desktools','deskterm'):
                payload = fs.read('bin/'+app)
                if payload != runtime_elf((ROOT/f'build/userspace/{app}.elf').read_bytes()): raise RuntimeError('installed app '+app)
                apps[app] = digest(payload)
            receipt['boots'][number] = {'serial_order':indexes,'data_sha256':digest(data),'identity':identity,'binding':binding,
                'portable':portable,'document_sha256':digest(EDITED),'binary_sha256':digest(BINARY),
                'machine_preferences_sha256':digest(prefs),'environment':env,'cwd':cwd,'applications':apps,
                'screenshots':[str(out/f'{name}-{number}.ppm') for name in ('text','files','terminal','desktop')]}
            save_json(receipt_path, receipt)
            for repeat in range(2): audit('inspect-installed',True,f'{number}-installed-{repeat}',True)
            audit('inspect',False,number+'-old-inspect'); audit('reconcile',False,number+'-old-reconcile')
            if image.digest(stage/'ready.json') != ready_hash: raise RuntimeError('ready changed')
        baseline = data_image(disk,offset,size); baseline_hash = image.digest(disk)
        storage.call(len(baseline), (out/'used-data.exfat').write_bytes, baseline)
        receipt['used_image_sha256'] = baseline_hash
        def write_disk(where, data):
            with disk.open('r+b', buffering=0) as f:
                f.seek(where); f.write(data)
        def fault(name, where, replacement):
            with disk.open('rb') as f: f.seek(where); old = f.read(len(replacement))
            if old == replacement: raise RuntimeError(name+': unchanged fault')
            write_disk(where,replacement)
            receipt['faults'][name] = {'offset':where,'before_hex':old.hex(),'after_hex':replacement.hex(), 'mutant_sha256':image.digest(disk)}
            save_json(receipt_path,receipt)
            try: audit('inspect-installed',False,name)
            finally: write_disk(where,old)
            if image.digest(disk) != baseline_hash: raise RuntimeError(name+': restoration failed')
        def byte_fault(name, pos, xor=1):
            with disk.open('rb') as f: f.seek(pos); old=f.read(1)
            fault(name,pos,bytes([old[0]^xor]))
        # Locate actual ESP payload bytes without trusting fixture-only offsets.
        patterns = {'kernel-payload':paths['kernel'].read_bytes()[:4096],
                    'config-payload':(stage/'limine.conf').read_bytes()}
        if any(not data for data in patterns.values()): raise RuntimeError('empty ESP payload pattern')
        matches = {name:[] for name in patterns}
        overlap = max(map(len,patterns.values()))-1
        with disk.open('rb') as stream:
            stream.seek(esp_offset); tail=b''; consumed=0
            while consumed < esp_size:
                chunk=stream.read(min(MIB,esp_size-consumed))
                if not chunk: raise RuntimeError('truncated ESP')
                combined=tail+chunk; start=esp_offset+consumed-len(tail)
                for name,pattern in patterns.items():
                    pos=combined.find(pattern)
                    while pos >= 0:
                        absolute=start+pos
                        if not matches[name] or matches[name][-1] != absolute: matches[name].append(absolute)
                        pos=combined.find(pattern,pos+1)
                consumed += len(chunk); tail=combined[-overlap:] if overlap else b''
        for name,found in matches.items():
            expected = 5 if name == 'config-payload' else 1
            if len(found)!=expected: raise RuntimeError(name+': unexpected payload match count')
            byte_fault(name,found[0])
            receipt['faults'][name]['match_count'] = len(found)
            receipt['faults'][name]['match_offsets'] = found
            save_json(receipt_path,receipt)
        byte_fault('esp-padding',esp_offset+esp_size-1)
        byte_fault('gpt-root-guid',1024+128+16)
        byte_fault('gpt-partition-label',1024+128+56)
        byte_fault('dirty-boot',offset+106,2)
        byte_fault('bad-boot-backup',offset+12*512+3)
        fs = ExfatFixture(baseline)
        root = fs.chain(fs.root)
        bitmap_entry = next(root[n:n+32] for n in range(0,len(root),32) if root[n]==0x81)
        bitmap = fs.offset(struct.unpack_from('<I',bitmap_entry,20)[0])
        free = next(i for i in range(fs.count) if not fs.data[bitmap+i//8] & (1<<(i%8)))
        byte_fault('allocation-leak',offset+bitmap+free//8,1<<(free%8))
        for name,path in [('marker-uuid','profile.id'),('identity-json','identity.json'),('local-binding','profile-local.json'),('portable-preferences',profile_bundle.PREF_PATH)]:
            pos, contents = payload_offset(fs,'home/local-a/'+path)
            # ASCII corruption keeps the directory entry, allocation, and length unchanged.
            ix = contents.index(b'{') if b'{' in contents else len(contents)-2
            if name == 'marker-uuid': fault(name,offset+pos+ix,b'g')
            else: byte_fault(name,offset+pos+ix,1)
        # The actual native profile driver creates files in a private extracted partition.
        scratch = out/'.profile-image-fixture'; scratch.mkdir(mode=0o700)
        scratch_image = scratch/'image'; fixture_input = scratch/'payload'
        storage.call(size,scratch_image.write_bytes,baseline)
        stale_prefix = b'srvros-desktop-profile-v1\n00000000-0000-4000-8000-000000000000\n/fat/home/missing\n'
        stale = stale_prefix + f'{zlib.crc32(stale_prefix):08x}\n'.encode()
        wrong_checksum = stale_prefix + b'00000000\n'
        for name,path,payload in [('pending-marker',home+'/.profile-import-pending',b'pending\n'),
                ('journal','/fat/home/local-a.profile-import.json',b'{}\n'),
                ('torn-journal','/fat/home/local-b.profile-import.json',b'{'),
                ('copy-scratch',home+'/.profile-copy-temp-unknown',b'x'),
                ('selection-scratch','/fat/home/.desktop-profile.tmp',b'x'),
                ('selection-stale','/fat/home/.desktop-profile',stale),
                ('selection-checksum','/fat/home/.desktop-profile',wrong_checksum)]:
            scratch_image.write_bytes(baseline); fixture_input.write_bytes(payload)
            cmd = [str(paths['profile_driver']),str(scratch_image),'write',path,str(fixture_input)]
            proc = subprocess.run(cmd,capture_output=True,text=True)
            if proc.returncode: raise RuntimeError('fixture creation failed: '+str(cmd)+' '+proc.stderr)
            mutant = scratch_image.read_bytes(); patches = differences(baseline,mutant)
            patch_path = out/(name+'-patch.json'); save_json(patch_path,patches)
            write_disk(offset,mutant)
            receipt['faults'][name] = {'driver':cmd,'exit':proc.returncode,'stdout':proc.stdout,'stderr':proc.stderr,
                'patch_file':str(patch_path),'patch_sha256':image.digest(patch_path),'mutant_sha256':image.digest(disk)}
            save_json(receipt_path,receipt)
            try: audit('inspect-installed',False,name)
            finally: write_disk(offset,baseline)
            if image.digest(disk) != baseline_hash: raise RuntimeError(name+': restoration failed')
        ready = stage/'ready.json'; ready_bytes = ready.read_bytes()
        receipt['faults']['torn-ready'] = {'offset':0,'before_hex':ready_bytes[:1].hex(),'after_hex':b'!'.hex()}
        with ready.open('r+b') as f: f.seek(0); f.write(b'!')
        try: audit('inspect-installed',False,'torn-ready')
        finally:
            with ready.open('r+b') as f: f.seek(0); f.write(ready_bytes)
        extra = stage/'unexpected'; extra.write_bytes(b'private fixture\n')
        receipt['faults']['unknown-stage-entry'] = {'path':str(extra),'payload_hex':b'private fixture\n'.hex()}
        try: audit('inspect-installed',False,'unknown-stage-entry')
        finally: extra.unlink()
        baseline_file = stage/'profile.exfat'; hidden = out/'retained-profile.exfat'
        receipt['faults']['missing-retained-baseline'] = {'moved_to':str(hidden),'sha256':image.digest(baseline_file)}
        baseline_file.rename(hidden)
        try: audit('inspect-installed',False,'missing-retained-baseline')
        finally: hidden.rename(baseline_file)
        with baseline_file.open('r+b') as f:
            f.seek(0); b=f.read(1); f.seek(0); f.write(bytes([b[0]^1]))
        receipt['faults']['tampered-retained-baseline'] = {'offset':0,'before_hex':b.hex(),'after_hex':bytes([b[0]^1]).hex()}
        try: audit('inspect-installed',False,'tampered-retained-baseline')
        finally:
            with baseline_file.open('r+b') as f: f.seek(0); f.write(b)
        with disk.open('rb') as locked:
            fcntl.flock(locked,fcntl.LOCK_EX|fcntl.LOCK_NB)
            audit('inspect-installed',False,'busy-image-lock')
        if image.digest(disk) != baseline_hash or image.digest(ready) != ready_hash: raise RuntimeError('final restoration mismatch')
        audit('inspect-installed',True,'restored-used',True)
        verify_sources(); receipt['status'] = 'PASS'
    except Exception as error:
        receipt.update(status='FAIL',error=f'{type(error).__name__}: {error}',traceback=traceback.format_exc())
        raise
    finally:
        save_json(receipt_path,receipt)
    print(f'installation-inspection: PASS; evidence {out}',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu',default='qemu-system-x86_64')
    parser.add_argument('--ovmf',default=default_ovmf_path())
    parser.add_argument('--ovmf-vars')
    parser.add_argument('--boot-wait',type=int,default=120)
    parser.add_argument('--run-budget-gib',type=float,default=8)
    parser.add_argument('--min-free-gib',type=float,default=4)
    args=parser.parse_args()
    if not 10 <= args.boot_wait <= 240 or args.run_budget_gib <= 0 or args.min_free_gib <= 0:
        parser.error('invalid wait or storage limit')
    for firmware in (args.ovmf,args.ovmf_vars):
        if not firmware or not Path(firmware).is_file() or ',' in firmware: parser.error('invalid firmware path')
    base=ROOT/'build/review/installation-inspection'
    if any(p.is_symlink() for p in (base,*base.parents)): raise RuntimeError('linked evidence directory')
    base.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='proof-',dir=base))
    run(args,Storage(out,int(args.run_budget_gib*1024**3),int(args.min_free_gib*1024**3)))
    return 0


if __name__=='__main__': sys.exit(main())
