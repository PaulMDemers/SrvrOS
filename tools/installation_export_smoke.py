#!/usr/bin/env python3
"""Four-boot, disposable installed-profile export and import proof.

Uses generated images only; does not establish physical-device or power-loss acceptance.
"""
import argparse
import base64
import fcntl
import json
import os
from pathlib import Path
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import traceback

from exfat_test_image import ExfatFixture
from installation_io_smoke import Storage, MIB
from pack_runtime_elf import runtime_elf
from profile_desktop_smoke import (ROOT, ID, BINARY, EDITED, boot, data_image, digest,
    machine_preferences, make_bundle, outside_data, partitions, save_json, verified_build)
from profile_installation_smoke import clone, seed_volume
from uefi_usb_smoke import default_ovmf_path
import installation_image as image
import profile_bundle
import source_provenance

DOCS = ['Documents/migration.txt', 'Documents/binary.dat']


def snapshot(root):
    result = {}
    for p in sorted(root.rglob('*')):
        st = p.lstat()
        item = {'mode': stat.S_IFMT(st.st_mode), 'permissions': stat.S_IMODE(st.st_mode),
                'inode': st.st_ino, 'device': st.st_dev, 'links': st.st_nlink, 'size': st.st_size}
        if stat.S_ISREG(st.st_mode): item['sha256'] = image.digest(p)
        if stat.S_ISLNK(st.st_mode): item['target'] = os.readlink(p)
        result[str(p.relative_to(root))] = item
    return result


def payload_offset(fs, path):
    directory = fs.chain(fs.root)
    for part in path.split('/'):
        entry = next(e for name, e in fs.entries(directory) if name == part)
        first = struct.unpack_from('<I', entry, 52)[0]
        size = struct.unpack_from('<Q', entry, 56)[0]
        directory = fs.chain(first, size, bool(entry[33] & 2))
    if not size: raise RuntimeError('empty fixture: ' + path)
    return fs.offset(first), directory


def differences(old, new):
    if len(old) != len(new): raise RuntimeError('fixture length changed')
    changes = []; i = 0
    while i < len(old):
        if old[i:i+4096] == new[i:i+4096]:
            i += min(4096, len(old)-i); continue
        if old[i] == new[i]: i += 1; continue
        start = i
        while i < len(old) and old[i] != new[i]: i += 1
        changes.append({'offset': start, 'before': old[start:i].hex(), 'after': new[start:i].hex()})
    if not changes: raise RuntimeError('fault did not change image')
    return changes


def run(args, storage):
    out = storage.out; receipt_path = out/'receipt.json'
    receipt = {'status': 'INCOMPLETE', 'scope': 'disposable offline QEMU images only',
               'limits': ['not physical-device acceptance', 'not power-loss simulation'],
               'steps': [], 'boots': {}, 'faults': {}, 'installations': {}}
    save_json(receipt_path, receipt)
    print('Installation export evidence: ' + str(out), flush=True)
    try:
        receipt['source'] = source_provenance.capture(ROOT)
        if any(record.get('dirty') for record in receipt['source'].values()):
            raise RuntimeError('source and initialized submodules must be clean')
        receipt['build'] = verified_build(ROOT)
        with tarfile.open(ROOT/'build/initramfs.tar') as archive:
            member = next(m for m in archive.getmembers() if m.name.lstrip('./') == 'init')
            if archive.extractfile(member).read() != runtime_elf((ROOT/'build/userspace/init.elf').read_bytes()):
                raise RuntimeError('bootstrap init differs from matched build')
        receipt['qemu_version'] = subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]
        receipt['firmware'] = {str(p): image.digest(p) for p in (args.ovmf,args.ovmf_vars)}
        originals = {'kernel': ROOT/'build/kernel/srvros.elf', 'bootx64': ROOT/'build/limine-binary/BOOTX64.EFI',
            'exfat': ROOT/'build/srvros.exfat', 'init': ROOT/'build/userspace/init.elf',
            'profile_driver': ROOT/'build/tests/profile-image-driver',
            'inspect_driver': ROOT/'build/tests/installation-inspect-driver',
            'export_driver': ROOT/'build/tests/installation-export-driver',
            'fs_driver': ROOT/'build/tests/exfat-driver',
            **{app: ROOT/f'build/userspace/{app}.elf' for app in ('deskd','desktools','deskterm')}}
        receipt['inputs'] = {k: {'path':str(v),'sha256':image.digest(v)} for k,v in originals.items()}
        receipt['tools'] = {name:image.digest(ROOT/'tools'/name) for name in
            ('installation_export_smoke.py','installation_export.py','installation_inspection.py',
             'installation_image.py','installation_export_driver.c','installation_inspect_driver.c',
             'installation_readonly_host.h','profile_desktop_smoke.py','profile_installation_smoke.py',
             'profile_bundle.py','profile_image.py','installation_io_smoke.py','exfat_test_image.py')}
        private = out/'inputs'; private.mkdir(); exports = out/'exports'; exports.mkdir()
        paths = {}
        for name in ('kernel','bootx64','init','profile_driver','inspect_driver','export_driver'):
            paths[name] = private/name
            storage.call(originals[name].stat().st_size, clone, originals[name], paths[name])
        for name in ('profile_driver','inspect_driver','export_driver'): paths[name].chmod(0o700)
        paths['bundle_a'] = private/'bundle-a.json'
        profile_bundle.publish_bundle(paths['bundle_a'], make_bundle())
        for label,speed,tz in (('a',4,-300),('b',2,60)):
            paths['exfat_'+label] = private/('base-'+label+'.exfat')
            storage.call(2*originals['exfat'].stat().st_size, seed_volume, originals['exfat'],
                         paths['exfat_'+label], machine_preferences(speed,tz), paths['profile_driver'], out)
        hashes = {k:image.digest(v) for k,v in paths.items()}
        receipt['private_inputs'] = hashes; save_json(receipt_path,receipt)
        def verify_sources():
            current = {k:image.digest(v) for k,v in paths.items()}
            built = {k:image.digest(v) for k,v in originals.items()}
            if current != hashes or built != {k:v['sha256'] for k,v in receipt['inputs'].items()}:
                raise RuntimeError('private or build source changed')
            if {str(p):image.digest(p) for p in (args.ovmf,args.ovmf_vars)} != receipt['firmware']:
                raise RuntimeError('firmware changed')
            return {'private':current,'build':built}
        def command(action, stage, *extra):
            cmd = [sys.executable,str(ROOT/'tools/installation_image.py'),action,str(stage),*map(str,extra)]
            proc = subprocess.run(cmd,capture_output=True,text=True)
            return {'argv':cmd,'exit':proc.returncode,'stdout':proc.stdout,'stderr':proc.stderr}
        def audit_used(stage, label, ready_hash):
            for action in ('inspect-installed','inspect','reconcile'):
                before = snapshot(stage); sources = verify_sources()
                result = command(action,stage,*(['--driver',paths['inspect_driver']] if action=='inspect-installed' else []))
                after = snapshot(stage); after_sources = verify_sources()
                step = {'label':label+'-'+action,'call':result,'before':before,'after':after,
                        'sources_before':sources,'sources_after':after_sources}
                receipt['steps'].append(step); save_json(receipt_path,receipt)
                if before != after or sources != after_sources or image.digest(stage/'ready.json') != ready_hash:
                    raise RuntimeError('inspection changed used stage')
                if action == 'inspect-installed':
                    parsed=json.loads(result['stdout']); step['result']=parsed
                    fs=parsed.get('filesystem',{})
                    if result['exit'] or not parsed.get('consistent') or not parsed.get('used') or fs.get('writes') != 0 or fs.get('flushes') != 0 or any(fs['allocation'][k] for k in ('leaked','fat_bitmap_mismatches','errors')):
                        raise RuntimeError('used installed inspection failed')
                elif action == 'inspect':
                    if result['exit'] or json.loads(result['stdout'])['can_reconcile']: raise RuntimeError('old inspect accepted used stage')
                elif not result['exit']: raise RuntimeError('old reconcile accepted used stage')
        def install(label, home, alias, uid, gid, speed, tz, bundle):
            stage=out/('stage-'+label); prefs=machine_preferences(speed,tz)
            storage.check(4*paths['exfat_'+label].stat().st_size+512*MIB); verify_sources()
            plan=image.prepare(stage,paths['kernel'],paths['bootx64'],paths['exfat_'+label],paths['init'],
                identity_bound=True,profile_bundle=bundle,profile_home=home,profile_alias=alias,
                profile_uid=uid,profile_gid=gid,profile_driver=paths['profile_driver'])
            if plan['version'] != 2: raise RuntimeError('expected v2 plan')
            prepared=snapshot(stage); verify_sources(); image.apply(stage); storage.check(); verify_sources()
            ready=image.digest(stage/'ready.json'); disk=stage/'image.img'
            (_, _),(offset,size)=partitions(disk); protected=outside_data(disk,offset,size)
            receipt['installations'][label]={'plan':plan,'prepared':prepared,'publication':snapshot(stage),
                'ready_sha256':ready,'protected':protected}
            if label == 'b':
                initial_data=data_image(disk,offset,size); initial=ExfatFixture(initial_data)
                prefix=home.removeprefix('/fat/')+'/'
                contents={doc:initial.read(prefix+doc) for doc in DOCS}
                names={name for name,_ in initial.entries(initial.read(prefix+'Documents'))}
                initial_binding=profile_bundle.decode_json(initial.read(prefix+'profile-local.json'))
                initial_portable=profile_bundle.decode_json(initial.read(prefix+profile_bundle.PREF_PATH))
                if (contents!={DOCS[0]:EDITED,DOCS[1]:BINARY} or
                    names.intersection({'environment.txt','cwd.txt'}) or
                    initial_binding!={'alias':alias,'uid':uid,'gid':gid} or
                    initial_portable!=dict(make_bundle()['preferences'],theme_style=1) or
                    initial.read('home/desktop.conf')!=prefs):
                    raise RuntimeError('b: initial imported data or machine state mismatch')
                receipt['installations'][label]['preboot']={'data_sha256':digest(initial_data),
                    'document_sha256':{k:digest(v) for k,v in contents.items()},
                    'document_names':sorted(names),'binding':initial_binding,'portable':initial_portable,
                    'machine_preferences_sha256':digest(prefs)}
                save_json(receipt_path,receipt)
            for n in (1,2):
                name=label+str(n); verify_sources()
                storage.call(64*MIB,boot,disk,out,name,args,home,speed,tz,edit=(name=='a1'))
                verify_sources(); serial=(out/f'serial-{name}.log').read_bytes()
                markers=[b'installed-root: selected device=ahci0p2 format=exfat-v1 role=system',
                    b'namespace: /bin /usr /var /home active',
                    f'deskd: profile id={ID} home={home} credentials=unchanged'.encode()]
                order=[serial.find(m) for m in markers]
                if not 0<=order[0]<order[1]<order[2]: raise RuntimeError(name+': serial order')
                if outside_data(disk,offset,size)!=protected: raise RuntimeError(name+': protected regions changed')
                data=data_image(disk,offset,size); fs=ExfatFixture(data); prefix=home.removeprefix('/fat/')+'/'
                identity=profile_bundle.decode_json(fs.read(prefix+'identity.json'))
                binding=profile_bundle.decode_json(fs.read(prefix+'profile-local.json'))
                portable=profile_bundle.decode_json(fs.read(prefix+profile_bundle.PREF_PATH))
                if fs.read(prefix+'profile.id') != b'srvros-profile-v1\n'+ID.encode()+b'\n' or identity != make_bundle()['profile'] or binding != {'alias':alias,'uid':uid,'gid':gid}:
                    raise RuntimeError(name+': identity/binding')
                if fs.read(prefix+DOCS[0])!=EDITED or fs.read(prefix+DOCS[1])!=BINARY or portable!=dict(make_bundle()['preferences'],theme_style=1) or fs.read('home/desktop.conf')!=prefs:
                    raise RuntimeError(name+': portable or machine data')
                env=fs.read(prefix+'Documents/environment.txt').decode(); cwd=fs.read(prefix+'Documents/cwd.txt').decode().strip()
                expected={'HOME':home,'SRVROS_DESKTOP_HOME':home,'SRVROS_PROFILE_ID':ID,
                    'XDG_CONFIG_HOME':home+'/.config','XDG_DATA_HOME':home+'/.local/share','XDG_CACHE_HOME':home+'/.cache'}
                if cwd!=home or any(f'{k}={v}' not in env.splitlines() for k,v in expected.items()): raise RuntimeError(name+': environment')
                apps={}
                for app in ('deskd','desktools','deskterm'):
                    content=fs.read('bin/'+app)
                    if content!=runtime_elf(originals[app].read_bytes()): raise RuntimeError(name+': app '+app)
                    apps[app]=digest(content)
                receipt['boots'][name]={'serial_order':order,'data_sha256':digest(data),'identity':identity,
                    'binding':binding,'portable':portable,'document_sha256':digest(EDITED),'binary_sha256':digest(BINARY),
                    'machine_preferences_sha256':digest(prefs),'environment':env,'cwd':cwd,'applications':apps,
                    'command':json.loads((out/f'command-{name}.json').read_text()), 'exit':0,
                    'screenshots':{str(out/f'{x}-{name}.ppm'):image.digest(out/f'{x}-{name}.ppm') for x in ('text','files','terminal','desktop')}}
                save_json(receipt_path,receipt); audit_used(stage,name,ready)
            return stage,disk,offset,size,ready
        a,disk,offset,size,ready=install('a','/fat/home/local-a','local_a',1001,1001,4,-300,paths['bundle_a'])
        home='/fat/home/local-a'
        expected_bundle=make_bundle()
        expected_bundle['preferences']['theme_style']=1
        expected_bundle['documents'][0].update(size=len(EDITED),sha256=digest(EDITED),
            data_base64=base64.b64encode(EDITED).decode())
        expected_bundle_hash=digest(profile_bundle.encode_json(expected_bundle))
        def export(label, output, *, profile=ID, selected_home=home, docs=DOCS, expected=None):
            before=snapshot(a); sources=verify_sources(); output_before=snapshot(exports)
            extra=[str(output),'--profile-id',profile,'--home',selected_home]
            for doc in docs: extra += ['--document',doc]
            extra += ['--inspect-driver',str(paths['inspect_driver']),'--export-driver',str(paths['export_driver'])]
            result=command('export-profile',a,*extra); after=snapshot(a); sources_after=verify_sources(); output_after=snapshot(exports)
            step={'label':label,'call':result,'before':before,'after':after,'sources_before':sources,
                  'sources_after':sources_after,'outputs_before':output_before,'outputs_after':output_after,
                  'expected_reason':expected}
            receipt['steps'].append(step); save_json(receipt_path,receipt)
            if before!=after or sources!=sources_after or image.digest(a/'ready.json')!=ready: raise RuntimeError(label+': changed source or stage')
            parsed=json.loads(result['stdout']); step['result']=parsed
            if expected is None:
                fs=parsed.get('filesystem',{})
                if (result['exit'] or parsed.get('state')!='exported' or parsed.get('source_used') is not True or
                    fs.get('writes')!=0 or fs.get('flushes')!=0 or
                    parsed.get('current_image_sha256')!=image.digest(disk) or
                    parsed.get('bundle_sha256')!=expected_bundle_hash or image.digest(output)!=expected_bundle_hash):
                    raise RuntimeError(label+': export failed positive checks '+repr(result))
            else:
                if result['exit']!=1 or parsed.get('state')!='refused' or expected not in parsed.get('reason','') or output_before!=output_after:
                    raise RuntimeError(label+': wrong refusal '+repr(result))
            save_json(receipt_path,receipt)
            return parsed
        first=exports/'a.json'; second=exports/'a-repeat.json'
        export('export-a',first); export('export-a-repeat',second)
        if first.read_bytes()!=second.read_bytes(): raise RuntimeError('repeat export differs')
        bundle=profile_bundle.decode_json(first.read_bytes()); payloads=profile_bundle.validate_bundle(bundle)
        if bundle['profile']!=make_bundle()['profile'] or bundle['preferences']!=dict(make_bundle()['preferences'],theme_style=1) or list(payloads)!=DOCS or payloads!={DOCS[0]:EDITED,DOCS[1]:BINARY}:
            raise RuntimeError('export bundle content mismatch')
        paths['export_bundle'] = first
        hashes['export_bundle'] = image.digest(first)
        receipt['export']={'sha256':image.digest(first),'repeat_sha256':image.digest(second),'bundle':bundle}
        save_json(receipt_path,receipt)
        baseline=data_image(disk,offset,size); baseline_hash=image.digest(disk)
        def write_disk(pos,data):
            with disk.open('r+b',buffering=0) as f: f.seek(pos); f.write(data)
        def fault(name,pos,replacement,reason):
            with disk.open('rb') as f: f.seek(pos); old=f.read(len(replacement))
            if old==replacement: raise RuntimeError(name+': no change')
            try:
                write_disk(pos,replacement)
                receipt['faults'][name]={'offset':pos,'before_hex':old.hex(),'after_hex':replacement.hex(),'mutant_sha256':image.digest(disk)}
                save_json(receipt_path,receipt)
                export(name,exports/(name+'.json'),expected=reason)
            finally:
                write_disk(pos,old)
                receipt['faults'].setdefault(name,{})['restored_sha256']=image.digest(disk); save_json(receipt_path,receipt)
            if image.digest(disk)!=baseline_hash: raise RuntimeError(name+': failed restoration')
        def byte_fault(name,pos,reason,xor=1):
            with disk.open('rb') as f: f.seek(pos); old=f.read(1)
            fault(name,pos,bytes([old[0]^xor]),reason)
        negative=[('wrong-uuid',{'profile':'00000000-0000-4000-8000-000000000000'},'selected profile not admitted'),
            ('wrong-home',{'selected_home':'/fat/home/missing'},'selected profile not admitted'),
            ('missing-file',{'docs':['Documents/missing.txt']},'export-document'),
            ('traversal',{'docs':['Documents/../escape']},'nonportable document path'),
            ('duplicate',{'docs':[DOCS[0],DOCS[0]]},'duplicate document'),
            ('case-collision',{'docs':['Documents/Folder/a','Documents/folder/b']},'case collision')]
        for name,kw,reason in negative: export(name,exports/(name+'.json'),expected=reason,**kw)
        export('existing-output',first,expected='output already exists or invalid')
        link=exports/'link.json'; link.symlink_to(first.name)
        try: export('symlink-output',link,expected='symlink path refused')
        finally: link.unlink()
        export('inside-stage',a/'not-created.json',expected='output inside stage')
        with disk.open('rb') as locked:
            fcntl.flock(locked,fcntl.LOCK_EX|fcntl.LOCK_NB)
            export('busy-image-lock',exports/'busy.json',expected='Resource temporarily unavailable')
        (esp_offset,esp_size),_=partitions(disk)
        byte_fault('esp-padding',esp_offset+esp_size-1,'protected image bytes changed')
        byte_fault('dirty-boot',offset+106,'clean-admission',2)
        fs=ExfatFixture(baseline); pos,contents=payload_offset(fs,'home/local-a/'+profile_bundle.PREF_PATH)
        byte_fault('invalid-prefs',offset+pos+contents.index(b'{'),'invalid guest portable preferences syntax')
        scratch=out/'.profile-image-fixture'; scratch.mkdir(mode=0o700)
        scratch_image=scratch/'image'; fixture_input=scratch/'payload'
        storage.call(size,scratch_image.write_bytes,baseline)
        for name,path,payload in [('pending-marker',home+'/.profile-import-pending',b'pending\n'),
                                  ('sibling-torn-journal','/fat/home/local-b.profile-import.json',b'{'),
                                  ('directory',home+'/Documents/empty',None)]:
            scratch_image.write_bytes(baseline)
            if payload is None:
                cmd=[str(paths['profile_driver']),str(scratch_image),'mkdir',path]
            else:
                fixture_input.write_bytes(payload)
                cmd=[str(paths['profile_driver']),str(scratch_image),'write',path,str(fixture_input)]
            proc=subprocess.run(cmd,capture_output=True,text=True)
            if proc.returncode: raise RuntimeError('fixture creation failed: '+proc.stderr)
            mutant=scratch_image.read_bytes(); patches=differences(baseline,mutant)
            patch_path=out/(name+'-patch.json'); save_json(patch_path,patches)
            try:
                write_disk(offset,mutant)
                receipt['faults'][name]={'driver':cmd,'exit':proc.returncode,'stdout':proc.stdout,'stderr':proc.stderr,
                    'patch_file':str(patch_path),'patch_sha256':image.digest(patch_path),'mutant_sha256':image.digest(disk)}
                save_json(receipt_path,receipt)
                export(name,exports/(name+'.json'),
                       docs=['Documents/empty'] if name=='directory' else DOCS,
                       expected='export-document' if name=='directory' else 'pending import or selection scratch')
            finally:
                write_disk(offset,baseline)
                receipt['faults'].setdefault(name,{})['restored_sha256']=image.digest(disk); save_json(receipt_path,receipt)
            if image.digest(disk)!=baseline_hash: raise RuntimeError(name+': failed restoration')
        audit_used(a,'restored',ready)
        b,bdisk,_,_,bready=install('b','/fat/home/local-b','local_b',2050,2051,2,60,first)
        with disk.open('rb') as fa,bdisk.open('rb') as fb:
            fa.seek(512+56); fb.seek(512+56); ga=fa.read(16); gb=fb.read(16)
            fa.seek(1024+16); fb.seek(1024+16); pa=fa.read(16); pb=fb.read(16)
            fa.seek(1024+128+16); fb.seek(1024+128+16); da=fa.read(16); db=fb.read(16)
        if len({ga,gb,pa,pb,da,db})!=6 or any(len(x)!=16 or x==bytes(16) for x in (ga,gb,pa,pb,da,db)):
            raise RuntimeError('GPT identifiers are not distinct nonzero UUIDs')
        receipt['gpt_ids']={'a':[ga.hex(),pa.hex(),da.hex()],'b':[gb.hex(),pb.hex(),db.hex()]}
        if image.digest(a/'ready.json')!=ready or image.digest(b/'ready.json')!=bready or image.digest(disk)!=baseline_hash:
            raise RuntimeError('final ready or restored image changed')
        verify_sources(); receipt['status']='PASS'
    except Exception as error:
        receipt.update(status='INCOMPLETE',error=f'{type(error).__name__}: {error}',traceback=traceback.format_exc())
        raise
    finally:
        save_json(receipt_path,receipt)
    print('installation-export: PASS; evidence '+str(out),flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu',default='qemu-system-x86_64')
    parser.add_argument('--ovmf',default=default_ovmf_path()); parser.add_argument('--ovmf-vars',required=True)
    parser.add_argument('--boot-wait',type=int,default=120)
    parser.add_argument('--run-budget-gib',type=float,default=8); parser.add_argument('--min-free-gib',type=float,default=4)
    args=parser.parse_args()
    if not 10<=args.boot_wait<=240 or not 0<args.run_budget_gib<=8 or args.min_free_gib<4: parser.error('invalid wait or storage limits')
    for name in ('ovmf','ovmf_vars'):
        try:
            firmware=Path(getattr(args,name)).resolve(strict=True)
            if not firmware.is_file() or ',' in str(firmware) or len(os.fsencode(firmware)) >= 4096:
                raise ValueError('invalid firmware path')
            setattr(args,name,str(firmware))
        except (OSError,ValueError,TypeError):
            parser.error('invalid firmware path')
    base=ROOT/'build/review/installation-export'
    if any(p.is_symlink() for p in (base,*base.parents)): raise RuntimeError('linked evidence directory')
    base.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='proof-',dir=base)).resolve()
    run(args,Storage(out,int(args.run_budget_gib*1024**3),int(args.min_free_gib*1024**3)))
    return 0


if __name__=='__main__': sys.exit(main())
