"""Read-only structural diagnosis of an offline published installation.

The retained publication is historical evidence, not authentication. Locks are
cooperative and the image must be offline; this is not a live health check.
"""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import stat
import subprocess
import tarfile
import time
import io
import uuid
import zlib

import installation_image as image
import profile_bundle as p

HASH = re.compile(r'[0-9a-f]{64}\Z')
HOME = re.compile(r'/fat/home/[A-Za-z0-9][A-Za-z0-9_-]{0,63}\Z')


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def guest_home(home):
    if type(home) is not str or not home.startswith('/fat/home/') or len(home) > 95:
        return False
    leaf = home[len('/fat/home/'):]
    return bool(leaf and leaf not in ('.', '..') and len(leaf) <= 255 and
                all(32 <= ord(c) <= 126 and c not in '/\\' for c in leaf))


def fd_hash_range(fd, offset, size, digest):
    while size:
        chunk = os.pread(fd, min(size, 1024 * 1024), offset)
        require(chunk, 'truncated baseline or image')
        digest.update(chunk)
        offset += len(chunk)
        size -= len(chunk)


def file_stamp(st):
    return (st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns, st.st_ctime_ns)


def read(path, limit):
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        st = os.fstat(fd)
        require(stat.S_ISREG(st.st_mode) and st.st_nlink == 1 and st.st_size <= limit, 'invalid or oversized ' + path.name)
        with os.fdopen(fd, 'rb', closefd=False) as f:
            data = f.read(limit + 1)
        require(len(data) == st.st_size, 'file changed: ' + path.name)
        return data
    finally:
        os.close(fd)


def json_read(path, limit=1024*1024):
    return p.decode_json(read(path, limit))


def record_plan(target):
    st = target.stat()
    require(stat.S_ISDIR(st.st_mode) and st.st_uid == os.geteuid() and not stat.S_IMODE(st.st_mode) & 0o077, 'stage must be private and owned')
    raw = read(target / 'plan.json', 1024*1024)
    require(read(target / 'plan.sha256', 128) == hashlib.sha256(raw).hexdigest().encode() + b'\n', 'plan checksum mismatch')
    plan = p.decode_json(raw)
    require(type(plan) is dict and type(plan.get('version')) is int and plan['version'] in (1, 2), 'invalid plan version')
    keys = {'version','target_uuid','target_directory','target_device','target_inode','sources','layout'}
    if 'selection' in plan: keys |= {'selection','gpt','config'}
    if plan['version'] == 2: keys.add('profile')
    require(set(plan) == keys and plan['target_directory'] == str(target) and (st.st_dev, st.st_ino) == (plan['target_device'], plan['target_inode']), 'target or plan identity mismatch')
    require(type(plan['target_device']) is int and type(plan['target_inode']) is int and str(uuid.UUID(plan['target_uuid'])) == plan['target_uuid'], 'invalid target identity')
    sources = plan['sources']
    require(type(sources) is dict and set(sources) == ({'kernel','bootx64','exfat','init'} | ({'profile_bundle','profile_driver'} if plan['version'] == 2 else set())), 'invalid sources')
    for source in sources.values():
        require(type(source) is dict and set(source) == {'path','device','inode','size','sha256'}, 'invalid source record')
        require(type(source['path']) is str and os.path.isabs(source['path']) and os.path.normpath(source['path']) == source['path'] and all(type(source[k]) is int and source[k] > 0 for k in ('device','inode','size')) and type(source['sha256']) is str and HASH.fullmatch(source['sha256']), 'invalid source metadata')
    require(sources['exfat']['size'] <= 2*1024*1024*1024, 'exFAT volume exceeds inspection limit')
    require(plan['layout'] == image.layout(sources['exfat']['size']), 'invalid layout')
    selection = plan.get('selection')
    if selection is not None:
        require(selection == {'mode':'identity-bound','format':'exfat-v1','role':'system'}, 'invalid selection')
        g = plan['gpt']
        require(type(g) is dict and set(g) == {'disk_guid','esp_partition_guid','exfat_partition_guid','exfat_partition_name'}, 'invalid GPT plan')
        ids = [g[k] for k in ('disk_guid','esp_partition_guid','exfat_partition_guid')]
        require(all(type(v) is str and str(uuid.UUID(v)) == v and uuid.UUID(v).int for v in ids) and len(set(ids)) == 3 and g['exfat_partition_name'] == 'srvros-system-v1' and plan['config'] == image.identity_config(g['exfat_partition_guid']).decode('ascii'), 'invalid GPT identity')
    profile = plan.get('profile')
    require((plan['version'] == 2) == (profile is not None), 'invalid profile version')
    if profile is not None:
        require(selection is not None and type(profile) is dict and set(profile) == {'profile_id','home','alias','uid','gid'} and type(profile['home']) is str and HOME.fullmatch(profile['home']), 'invalid planned profile')
        p.profile_id(profile['profile_id']); p.binding(profile['alias'], profile['uid'], profile['gid'])
    return plan, hashlib.sha256(raw).hexdigest()


def bootstrap(target, plan):
    size = plan['sources']['init']['size']
    require(size <= 128*1024*1024, 'bootstrap exceeds limit')
    expected_size = ((512 + size + 511)//512*512 + 1024 + 10239)//10240*10240
    raw = read(target / 'bootstrap.tar', expected_size)
    require(len(raw) == expected_size, 'invalid bootstrap length')
    with tarfile.open(fileobj=io.BytesIO(raw), mode='r:') as tar:
        members = tar.getmembers()
        require(len(members) == 1 and members[0].name == 'init' and members[0].isfile() and members[0].mode == 0o755 and members[0].size == size, 'invalid bootstrap')
        data = tar.extractfile(members[0]).read(size + 1)
    require(len(data) == size and hashlib.sha256(data).hexdigest() == plan['sources']['init']['sha256'] and image.bootstrap_bytes(data) == raw, 'bootstrap changed')
    require(read(target / 'limine.conf', 4096) == plan.get('config', image.CONFIG.decode('ascii')).encode('ascii'), 'configuration changed')
    return raw


def driver_output(driver, fd, spec, mode="--fd"):
    proc = subprocess.Popen([str(driver), mode, str(fd), str(spec['offset_bytes']), str(spec['size_bytes'])], pass_fds=(fd,), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    sel = selectors.DefaultSelector()
    for stream in (proc.stdout, proc.stderr): sel.register(stream, selectors.EVENT_READ)
    output = {proc.stdout: bytearray(), proc.stderr: bytearray()}
    try:
        deadline = time.monotonic() + 120
        while sel.get_map():
            require(time.monotonic() < deadline, 'inspection driver timed out')
            for key, _ in sel.select(timeout=min(2, max(0, deadline - time.monotonic()))):
                chunk = os.read(key.fd, 65536)
                if not chunk: sel.unregister(key.fileobj); continue
                output[key.fileobj].extend(chunk)
                if len(output[key.fileobj]) > (1024*1024 if key.fileobj is proc.stdout else 16384):
                    raise ValueError('inspection driver output exceeds limit')
            if proc.poll() is None and not sel.get_map(): break
        require(proc.wait(timeout=30) == 0, 'inspection driver refused filesystem: ' + bytes(output[proc.stderr]).decode('utf-8', 'replace')[:512])
        return p.decode_json(bytes(output[proc.stdout]))
    finally:
        if proc.poll() is None: proc.kill(); proc.wait()
        sel.close(); proc.stdout.close(); proc.stderr.close()


def guest_preferences(raw):
    # Match userspace/lib/include/srvros/profile_preferences.h: the guest
    # accepts a flat object with literal keys and canonical unsigned numbers.
    ws = rb'[ \t\r\n]*'
    member = rb'"[a-z_]+"' + ws + rb':' + ws + rb'(?:0|[1-9][0-9]*)'
    pattern = ws + rb'\{' + ws + rb'(?:' + member + ws + rb'(?:,' + ws + member + ws + rb')*)?\}' + ws
    require(re.fullmatch(pattern, raw) is not None, 'invalid guest portable preferences syntax')
    return p.preferences(p.decode_json(raw))


def catalog(result, planned):
    require(type(result) is dict and set(result) == {'version','filesystem','entries'} and type(result['version']) is int and result['version'] == 1, 'invalid driver output')
    fs = result['filesystem']
    require(type(fs) is dict and set(fs) == {'clean_admission','namespace_targets_valid','allocation','writes','flushes'} and fs['clean_admission'] is True and fs['namespace_targets_valid'] is True and type(fs['writes']) is int and fs['writes'] == 0 and type(fs['flushes']) is int and fs['flushes'] == 0, 'filesystem admission failed')
    alloc = fs['allocation']
    require(type(alloc) is dict and set(alloc) == {'files','clusters','leaked','fat_bitmap_mismatches','errors'} and all(type(v) is int and v >= 0 for v in alloc.values()) and not any(alloc[k] for k in ('leaked','fat_bitmap_mismatches','errors')), 'filesystem allocation invalid')
    entries = result['entries']; require(type(entries) is list and len(entries) <= 4096, 'invalid catalog entries')
    paths = {}; folded = set()
    for e in entries:
        require(type(e) is dict and set(e) in ({'path','type','size'}, {'path','type','size','data_hex'}) and type(e['path']) is str and e['path'].isascii() and len(e['path']) <= 159 and (e['path'] == '/fat/home' or e['path'].startswith('/fat/home/')) and all(x not in ('', '.', '..') for x in e['path'].split('/')[1:]) and e['type'] in ('file','directory') and type(e['size']) is int and e['size'] >= 0 and e['path'].lower() not in folded, 'invalid or duplicate catalog entry')
        folded.add(e['path'].lower()); paths[e['path']] = e
        if 'data_hex' in e:
            require(e['type'] == 'file' and type(e['data_hex']) is str and re.fullmatch(r'(?:[0-9a-f]{2})*', e['data_hex']) and len(e['data_hex']) == e['size']*2, 'invalid catalog data')
    def directory(path): require(path in paths and paths[path]['type'] == 'directory', 'missing directory: ' + path)
    def data(path, limit):
        require(path in paths and paths[path]['type'] == 'file' and 'data_hex' in paths[path] and paths[path]['size'] <= limit, 'missing or oversized metadata: ' + path)
        return bytes.fromhex(paths[path]['data_hex'])
    if '/fat/home' not in paths:
        require(not paths and planned is None, 'profile root missing')
        return fs, [], None
    directory('/fat/home')
    for path in paths:
        leaf = path.rsplit('/',1)[-1].lower()
        require(not ('.profile-import' in leaf or leaf.startswith('.profile-copy-temp') or leaf.startswith('.desktop-profile.tmp')), 'pending import or selection scratch: ' + path)
    homes = [x for x,e in paths.items() if x.count('/') == 3 and e['type'] == 'directory' and any(x+'/'+name in paths for name in ('profile.id','identity.json','profile-local.json','.profile-import-pending'))]
    require(len(homes) <= 16, 'too many profiles')
    profiles = []
    for home in homes:
        require(guest_home(home), 'invalid profile home')
        marker = data(home+'/profile.id',80)
        require(marker.startswith(b'srvros-profile-v1\n') and marker.endswith(b'\n'), 'invalid profile marker')
        ident = marker[18:-1].decode('ascii'); p.profile_id(ident)
        identity_path = home+'/identity.json'
        if identity_path in paths:
            identity = p.identity(p.decode_json(data(identity_path,1024)))
            require(identity['id'] == ident, 'profile identity mismatch')
            display_name = identity['display_name']
        else:
            display_name = home.rsplit('/', 1)[-1]
        binding = p.decode_json(data(home+'/profile-local.json',1024)); p.exact(binding, ('alias','uid','gid'), 'local binding'); p.binding(**binding)
        for suffix in ('Documents','.config','.config/srvros','.local','.local/share','.cache'): directory(home+'/'+suffix)
        guest_preferences(data(home+'/.config/srvros/portable.json',512))
        profiles.append({'home':home, 'profile_id':ident, 'display_name':display_name, 'identity_present':identity_path in paths, **binding})
    for key in ('profile_id','alias','uid'):
        values = [x[key] for x in profiles]; require(len(values) == len(set(values)), 'duplicate profile ' + key)
    if planned:
        require(any(all(x[k] == planned[k] for k in ('home','profile_id','alias','uid','gid')) for x in profiles), 'planned profile missing or changed')
    choice = None
    if '/fat/home/.desktop-profile' in paths:
        raw = data('/fat/home/.desktop-profile',240)
        lines = raw.splitlines(keepends=True)
        require(len(lines) == 4 and lines[0] == b'srvros-desktop-profile-v1\n' and lines[1].endswith(b'\n') and lines[2].endswith(b'\n'), 'invalid remembered selection')
        prefix = b''.join(lines[:3]); require(lines[3] == f'{zlib.crc32(prefix):08x}\n'.encode(), 'invalid remembered selection checksum')
        ident = lines[1][:-1].decode('ascii'); home = lines[2][:-1].decode('ascii')
        matches = [x for x in profiles if x['profile_id'] == ident and x['home'] == home]; require(len(matches) == 1, 'stale remembered selection')
        choice = {'profile_id':ident, 'home':home}
    return fs, profiles, choice


def inspect_installed(stage, driver):
    try:
        target = image.checked_path(stage)
        with image.plan_lock(target):
            return _inspect(target, driver)
    except (OSError, ValueError, KeyError, TypeError, IndexError, RuntimeError, UnicodeError, tarfile.TarError, subprocess.SubprocessError, OverflowError) as error:
        return {'state':'refused', 'consistent':False, 'read_only':True, 'reason':str(error)}


def _inspect(target, driver, reader=None):
    plan, plan_hash = record_plan(target)
    names = {'plan.json','plan.sha256','in-progress.json','bootstrap.tar','limine.conf','publication.json','image.img','ready.json'}
    actual_names = set(os.listdir(target))
    if plan.get('profile') is not None and 'profile.exfat' in actual_names: names.add('profile.exfat')
    require(actual_names == names, 'stage is not a complete settled publication')
    driver_record = image.source(driver)
    require(os.access(driver_record['path'], os.X_OK), 'inspection driver is not executable')
    snapshots = {}
    for name in names:
        st = image.regular(target/name); snapshots[name] = (st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns,st.st_ctime_ns)
    require(json_read(target/'in-progress.json') == {'target_uuid':plan['target_uuid']}, 'invalid in-progress record')
    journal = json_read(target/'publication.json')
    require(type(journal) is dict and set(journal) == {'version','target_uuid','target_device','target_inode','plan_sha256','sources','candidate','data_sha256','ready'} and type(journal['version']) is int and journal['version'] == 1 and journal['target_uuid'] == plan['target_uuid'] and journal['target_device'] == plan['target_device'] and journal['target_inode'] == plan['target_inode'] and journal['plan_sha256'] == plan_hash and journal['sources'] == plan['sources'], 'publication mismatch')
    candidate = journal['candidate']
    require(type(candidate) is dict and set(candidate) == {'device','inode','size','sha256'} and all(type(candidate[k]) is int and candidate[k] > 0 for k in ('device','inode','size')) and type(candidate['sha256']) is str and HASH.fullmatch(candidate['sha256']) and type(journal['data_sha256']) is str and HASH.fullmatch(journal['data_sha256']), 'invalid publication hashes')
    fd = os.open(target/'image.img', os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    baseline_fd = None
    try:
        st = os.fstat(fd); require(stat.S_ISREG(st.st_mode) and st.st_nlink == 1 and (st.st_dev,st.st_ino,st.st_size) == (candidate['device'],candidate['inode'],candidate['size']) and st.st_size == plan['layout']['size_bytes'], 'image identity mismatch')
        fcntl.flock(fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
        boot = bootstrap(target, plan)
        gpt = image.verify_image(target/'image.img', plan, boot)
        expected = image.ready_record(target, plan, gpt, candidate['sha256'], journal['data_sha256'])
        require(journal['ready'] == expected and read(target/'ready.json',1024*1024) == (json.dumps(expected,indent=2,sort_keys=True)+'\n').encode(), 'historical ready record mismatch')
        if 'profile.exfat' in names:
            require(image.digest(target/'profile.exfat') == journal['data_sha256'], 'historical profile image changed')
        current = image.digest(target/'image.img')
        if plan['version'] == 1:
            require(journal['data_sha256'] == plan['sources']['exfat']['sha256'], 'historical data record mismatch')
        baseline_info = {'kind':'original-image', 'sha256':journal['data_sha256']}
        if current == candidate['sha256']:
            h = hashlib.sha256()
            spec = plan['layout']['exfat']
            fd_hash_range(fd, spec['offset_bytes'], spec['size_bytes'], h)
            require(h.hexdigest() == journal['data_sha256'], 'historical data record mismatch')
        else:
            baseline_path = target/'profile.exfat' if plan['version'] == 2 else Path(plan['sources']['exfat']['path'])
            try:
                baseline_path = image.checked_path(baseline_path)
                baseline_fd = os.open(baseline_path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
                baseline_stat = os.fstat(baseline_fd)
                source = plan['sources']['exfat']
                require(stat.S_ISREG(baseline_stat.st_mode) and baseline_stat.st_nlink == 1 and baseline_stat.st_size == plan['layout']['exfat']['size_bytes'], 'baseline identity')
                if plan['version'] == 1:
                    require((baseline_stat.st_dev,baseline_stat.st_ino,baseline_stat.st_size) == (source['device'],source['inode'],source['size']), 'baseline identity')
                fcntl.flock(baseline_fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
                h = hashlib.sha256()
                fd_hash_range(baseline_fd, 0, baseline_stat.st_size, h)
                require(h.hexdigest() == journal['data_sha256'], 'baseline hash')
            except (OSError, ValueError) as error:
                raise ValueError('historical data baseline unavailable or changed') from error
            spec = plan['layout']['exfat']
            recombined = hashlib.sha256()
            fd_hash_range(fd, 0, spec['offset_bytes'], recombined)
            fd_hash_range(baseline_fd, 0, spec['size_bytes'], recombined)
            end = spec['offset_bytes'] + spec['size_bytes']
            fd_hash_range(fd, end, st.st_size - end, recombined)
            require(recombined.hexdigest() == candidate['sha256'], 'protected image bytes changed')
            baseline_info = {'kind':'retained-profile-exfat' if plan['version'] == 2 else 'original-exfat-source', 'sha256':journal['data_sha256']}
        fs, profiles, choice = catalog(driver_output(driver_record['path'], fd, plan['layout']['exfat']), plan.get('profile'))
        reader_value = reader(fd, plan, profiles) if reader is not None else None
        require(image.digest(target/'image.img') == current, 'image changed during inspection')
        if baseline_fd is not None:
            require(file_stamp(os.fstat(baseline_fd)) == file_stamp(baseline_stat) and file_stamp(os.stat(baseline_path, follow_symlinks=False)) == file_stamp(baseline_stat), 'historical data baseline changed during inspection')
            h = hashlib.sha256()
            fd_hash_range(baseline_fd, 0, baseline_stat.st_size, h)
            require(h.hexdigest() == journal['data_sha256'], 'historical data baseline changed during inspection')
        after = os.fstat(fd); require((after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns) == (st.st_dev,st.st_ino,st.st_size,st.st_mtime_ns,st.st_ctime_ns), 'image changed during inspection')
        require(set(os.listdir(target)) == names, 'stage changed during inspection')
        for name in names:
            s = image.regular(target/name); require((s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns) == snapshots[name], 'stage record changed: '+name)
        require(image.source(driver) == driver_record, 'inspection driver changed')
        require(record_plan(target)[1] == plan_hash and json_read(target/'publication.json') == journal and read(target/'ready.json',1024*1024) == (json.dumps(expected,indent=2,sort_keys=True)+'\n').encode(), 'records changed during inspection')
        result = {'state':'installed-consistent','consistent':True,'read_only':True,'used':current != candidate['sha256'],'current_image_sha256':current,'publication':{'historical':True,'original_image_sha256':candidate['sha256'],'original_data_sha256':journal['data_sha256'],'provisioning_sources_revalidated':False,'data_baseline':baseline_info},'driver':driver_record,'gpt':gpt,'filesystem':fs,'profiles':profiles,'planned_profile':plan.get('profile'),'selection':choice,'reason':'offline structural checks passed; publication is historical evidence, not authentication or live health'}
        return (result, reader_value) if reader is not None else result
    finally:
        if baseline_fd is not None:
            os.close(baseline_fd)
        os.close(fd)
