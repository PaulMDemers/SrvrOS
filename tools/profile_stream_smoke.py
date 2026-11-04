#!/usr/bin/env python3
"""Graphical v2 profile transfer and recovery on disposable QEMU installations.

Only generated images are touched. Requires a source-matched build; run alone
in an approved resource window. Evidence, failed runs and primary images remain.
"""
import argparse
import hashlib
import io
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import time

from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path
import installation_image
import profile_bundle as v1
import profile_chooser_smoke as c
import profile_desktop_smoke as d
import profile_image
import profile_stream as host_stream
import profile_transfer_smoke as t
import source_provenance

ROOT = Path(__file__).resolve().parents[1]
A = '/fat/home/local-a'
B = '/fat/home/local_b'
MAGIC = b'SRVROS-PROFILE2\n'
CHUNK = 65536
MAX_DOC = 16 * 1024 * 1024
MAX_TOTAL = 64 * 1024 * 1024
MAPPED_PEAK_LIMIT = 32 * 1024 * 1024
MAPPED_GROWTH_LIMIT = 4 * 1024 * 1024
CHECKPOINTS = ('journal', 'directories', 'copy-partial', 'document', 'identity', 'commit-ready', 'published')
DOCS = ('Documents/large.bin', 'Documents/photo.bmp', 'Documents/migration.txt')
TEXT = b'Guest created streaming profile text'
SENTINELS = (('Documents/unselected.txt', b'synthetic-unselected\n'),
             ('.config/credential-sentinel.fixture', b'synthetic-not-a-credential\n'),
             ('.local/share/app-state.fixture', b'synthetic-app-state\n'),
             ('.cache/runtime-cache.fixture', b'synthetic-cache\n'))


def bmp_fixture():
    # 24-bit BMP, 2 MiB+ and within Preview's 8 MiB / 4M pixel bounds.
    width, height = 1024, 768
    stride = (width * 3 + 3) & ~3
    pixels = bytes(range(256)) * (stride * height // 256)
    header = (b'BM' + struct.pack('<IHHI', 54 + len(pixels), 0, 0, 54) +
              struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0,
                          len(pixels), 2835, 2835, 0, 0))
    return header + pixels


def binary_fixture():
    return bytes(range(256)) * (MAX_DOC // 256)


def memory_scripts():
    workloads = {
        'small': ['Documents/photo.bmp'],
        'file_16m': ['Documents/large.bin'],
        'total_64m': ['Documents/large.bin', 'Documents/a.bin', 'Documents/b.bin', 'Documents/c.bin'],
    }
    return {f'memory-{name}.sh': ('export SRVROS_PROFILE_TRACE_MEMORY=1\n/fat/bin/profile preview-export-auto ' +
            A + ' ' + ' '.join(paths) + '\n').encode('ascii') for name, paths in workloads.items()}


def invalid_fixtures():
    sample = b'\0' * CHUNK
    def container(docs, payload=b'', *, reserved=0, manifest_override=None):
        metadata = {'format': v1.FORMAT, 'schema_version': 2,
                    'profile': d.make_bundle()['profile'],
                    'preferences': d.make_bundle()['preferences'], 'documents': docs}
        raw = manifest_override if manifest_override is not None else v1.encode_json(metadata)
        return MAGIC + struct.pack('<II', len(raw), reserved) + raw + payload
    good = {'path': 'Documents/x.bin', 'size': len(sample), 'sha256': hashlib.sha256(sample).hexdigest()}
    corrupt = sample[:-1] + b'X'
    return {
        'legacy-invalid.json': b'{\"format\":\"org.srvros.portable-profile\",\"schema_version\":1}',
        'corrupt.srvrprofile': container([good], corrupt),
        'truncated.srvrprofile': container([good], sample[:-1]),
        'trailing.srvrprofile': container([good], sample + b'X'),
        'reserved.srvrprofile': container([good], sample, reserved=1),
        'oversize.srvrprofile': container([dict(good, size=MAX_DOC + 1)]),
        'aggregate.srvrprofile': container([dict(good, path=f'Documents/f{i}.bin', size=MAX_DOC) for i in range(5)]),
        'count.srvrprofile': container([dict(good, path=f'Documents/f{i}.bin', size=0) for i in range(65)]),
        'metadata.srvrprofile': container([], manifest_override=b' ' * 65537),
        'path.srvrprofile': container([dict(good, path='Documents/../outside')], sample),
        'case.srvrprofile': container([good, dict(good, path='Documents/X.bin')], sample + sample),
        'version.srvrprofile': container([], manifest_override=b'{\"format\":\"org.srvros.portable-profile\",\"schema_version\":3,\"profile\":{},\"preferences\":{},\"documents\":[]}'),
        'duplicate.srvrprofile': container([], manifest_override=b'{\"format\":\"org.srvros.portable-profile\",\"format\":\"org.srvros.portable-profile\",\"schema_version\":2,\"profile\":{},\"preferences\":{},\"documents\":[]}'),
    }


def verify_v2(stream, expected):
    """Independent bounded wire check; no product v2 decoder is used."""
    full = hashlib.sha256()
    def read_exact(n):
        data = stream.read(n)
        if len(data) != n: raise RuntimeError('truncated v2 container')
        full.update(data); return data
    header = read_exact(24)
    if header[:16] != MAGIC: raise RuntimeError('not a v2 container')
    size, reserved = struct.unpack('<II', header[16:])
    if not 0 < size <= 65536 or reserved: raise RuntimeError('invalid v2 header')
    manifest = v1.decode_json(read_exact(size))
    v1.exact(manifest, ('format', 'schema_version', 'profile', 'preferences', 'documents'), 'v2 manifest')
    if manifest['format'] != v1.FORMAT or type(manifest['schema_version']) is not int or manifest['schema_version'] != 2:
        raise RuntimeError('invalid v2 version')
    v1.identity(manifest['profile']); v1.preferences(manifest['preferences'])
    docs = manifest['documents']
    if type(docs) is not list or len(docs) > 64: raise RuntimeError('invalid v2 document count')
    seen, total, evidence = set(), 0, {}
    for item in docs:
        v1.exact(item, ('path', 'size', 'sha256'), 'v2 document')
        path = item['path']; v1.document_path(path)
        n, digest = item['size'], item['sha256']
        if type(n) is not int or not 0 <= n <= MAX_DOC or not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest):
            raise RuntimeError('invalid v2 document metadata')
        folded = path.lower()
        if folded in seen or any(folded.startswith(old + '/') or old.startswith(folded + '/') for old in seen):
            raise RuntimeError('duplicate or colliding v2 path')
        seen.add(folded); total += n
        if total > MAX_TOTAL: raise RuntimeError('v2 aggregate exceeds limit')
        document = hashlib.sha256(); expected_bytes = expected.get(path)
        if expected_bytes is None or len(expected_bytes) != n: raise RuntimeError('unexpected selected document')
        offset = 0
        while offset < n:
            piece = read_exact(min(CHUNK, n - offset)); document.update(piece)
            if piece != expected_bytes[offset:offset + len(piece)]: raise RuntimeError('v2 payload bytes differ: ' + path)
            offset += len(piece)
        if document.hexdigest() != digest: raise RuntimeError('v2 payload hash mismatch: ' + path)
        evidence[path] = {'size': n, 'sha256': digest}
    if set(evidence) != set(expected) or stream.read(1): raise RuntimeError('v2 selection or trailing bytes differ')
    return manifest, full.hexdigest(), evidence


def fixture(out, receipt, image, binary):
    base = (ROOT / 'build/srvros.exfat').resolve()
    driver = (ROOT / 'build/tests/profile-image-driver').resolve()
    ma, mb = d.machine_preferences(4, -300), d.machine_preferences(2, 60)
    source = out / 'source.exfat'
    profile_image.provision(base, source, d.make_bundle(), A, 'local_a', 1001, 1001, driver, ma)
    target = out / 'destination.exfat'
    with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=out) as tmp:
        stage = Path(tmp) / 'image'; shutil.copyfile(base, stage)
        prefs = Path(tmp) / 'prefs'; prefs.write_bytes(mb)
        for op, path, args in (('ensure-home', '/fat/home', []),
                               ('write-desktop', '/fat/home/desktop.conf', [str(prefs)]),
                               ('check-clean', '/fat/home', [])):
            subprocess.run([str(driver), str(stage), op, path, *args], check=True)
        shutil.copyfile(stage, target)
    # Only raw synthetic source documents are injected, never a bundle.
    transport = out / 'transport-source.exfat'
    seeded = ExfatFixture(base.read_bytes())
    seeded.add_directory_files('stream-input', {'photo.bmp': image, 'large.bin': binary,
        'change-expected.txt': b'changed-after-review\n', **invalid_fixtures(), **memory_scripts()})
    transport.write_bytes(seeded.data)
    disks = {}; receipt['installations'] = {}
    for name, volume in (('a', source), ('b', target), ('transport', transport)):
        stage = out / ('target-' + name)
        installation_image.prepare(stage, (ROOT / 'build/kernel/srvros.elf').resolve(),
            (ROOT / 'build/limine-binary/BOOTX64.EFI').resolve(), volume.resolve(),
            (ROOT / 'build/userspace/init.elf').resolve(), identity_bound=True)
        ready = installation_image.apply(stage); disk = stage / 'image.img'; disks[name] = disk
        _, (offset, size) = d.partitions(disk)
        receipt['installations'][name] = {'disk': str(disk), 'gpt': ready['gpt'],
            'initial_disk_sha256': d.digest(disk.read_bytes()),
            'protected_regions': d.outside_data(disk, offset, size)}
        t.check_allocation(disk, out, name + '-initial')
    if len({x['gpt']['disk_guid'] for x in receipt['installations'].values()}) != 3:
        raise RuntimeError('installation and transport GUIDs differ')
    return disks, ma, mb


def desktop(vm, home=None, checkpoint=None):
    flags = ('--profile=' + home + ' ' if home else '') + '--apps-trace --scale=1'
    if checkpoint:
        if checkpoint not in CHECKPOINTS + ('export-partial',): raise ValueError('unapproved checkpoint')
        flags += ' --profile-transfer-stop=' + checkpoint
    start = len(vm.output); vm.launch(flags)
    c.observe(vm, rb'deskd: commit', start)
    if home:
        c.observe(vm, rb'deskd: profile id=' + d.ID.encode() + rb' home=' + re.escape(home.encode()), start)


def gui_transfer(vm, operation, *, checkpoint=None, filename='transfer.srvrprofile',
                 docs=DOCS, alias='local_b', outcome='success', directory='/work', change_file=None):
    start = t.launch_profiles(vm, checkpoint)
    c.observe(vm, rb'profile-ui: state=editing', start)
    profile_pid = c.observe(vm, rb'profile-ui: started pid=(\d+)', start).group(1)
    frame = t.geometry(vm, start); w, h = frame[2:]
    if operation == 'export':
        t.click_client(vm, frame, w // 2, 25)
        for path in docs:
            t.select_file(vm, frame, (45, 105), A + '/Documents', path.split('/')[-1], 'open')
        t.select_file(vm, frame, (60, 64), '/work', filename, 'save')
    else:
        t.click_client(vm, frame, w // 6, 25)
        t.select_file(vm, frame, (60, 64), directory, filename, 'open')
        t.click_client(vm, frame, 180, 105); vm.key('ctrl-a'); t.text(vm, alias)
    preview = len(vm.output); t.click_client(vm, frame, w - 60, h - 54)
    c.observe(vm, rb'profile-ui: state=previewing operation=' + operation.encode() + rb' result=started', preview)
    if outcome == 'preview-error':
        c.observe(vm, rb'profile-ui: state=editing operation=' + operation.encode() + rb' result=error', preview, 180)
        c.observe(vm, rb'profile-ui: state=worker operation=' + operation.encode() +
                  rb' result=reaped status=[1-9]\d* stdout_eof=1 stderr_eof=1', preview)
        vm.screenshot(operation + '-refusal'); t.close_profiles(vm, start); return
    if outcome == 'space-error':
        result = c.observe(vm, rb'profile-ui: state=(reviewing|editing) operation=' + operation.encode() + rb' result=(ready|error)', preview, 180)
        if result.group(1) == b'editing':
            if result.group(2) != b'error': raise RuntimeError('invalid low-space preview state')
            c.observe(vm, rb'profile-ui: state=worker operation=' + operation.encode() +
                      rb' result=reaped status=[1-9]\d* stdout_eof=1 stderr_eof=1', preview)
            vm.screenshot('low-space-refusal'); t.close_profiles(vm, start); return
        if result.group(2) != b'ready': raise RuntimeError('invalid low-space review state')
    else:
        c.observe(vm, rb'profile-ui: state=reviewing operation=' + operation.encode() + rb' result=ready', preview, 180)
    c.observe(vm, rb'profile-ui: state=worker operation=' + operation.encode() +
              rb' result=reaped status=0 stdout_eof=1 stderr_eof=1', preview)
    vm.screenshot(operation + '-review')
    t.click_client(vm, frame, 40, 80); vm.key('end'); vm.screenshot(operation + '-review-details')
    if change_file:
        # Owned Terminal fixture changes one synthetic input after review.
        shell = len(vm.output); vm.key('meta_l-spc'); t.text(vm, 'Terminal new app'); vm.key('ret')
        c.observe(vm, rb'deskterm: ready shell=\d+', shell)
        t.text(vm, '/fat/bin/write ' + change_file + ' changed-after-review'); vm.key('ret')
        # cmp is an independent guest readback; its status is propagated to
        # the shell's observable exit before the reviewed operation resumes.
        t.text(vm, '/fat/bin/cmp -s ' + change_file + ' /work/stream-input/change-expected.txt'); vm.key('ret')
        t.text(vm, 'exit '); vm.qmp.key('4', True); vm.qmp.key('slash', True); vm.key('ret')
        c.observe(vm, rb'deskterm: shell exited status=0', shell)
        vm.key('ctrl-shift-w'); c.observe(vm, rb'deskterm: exited failure=0', shell)
        vm.key('meta_l-spc'); t.text(vm, 'Profiles window'); vm.key('ret')
        c.observe(vm, rb'deskd: launcher window=' + profile_pid + rb' id=\d+', shell)
    mutation = len(vm.output); t.click_client(vm, frame, w - 60, h - 54)
    c.observe(vm, rb'profile-ui: state=transferring operation=' + operation.encode() + rb' result=started', mutation)
    if checkpoint:
        c.observe(vm, rb'profile-ui: checkpoint=' + checkpoint.encode() + rb' result=error', mutation, 180)
        c.observe(vm, rb'profile-ui: state=worker operation=' + operation.encode() +
                  rb' result=reaped status=86 stdout_eof=1 stderr_eof=1', mutation)
    elif outcome in ('transfer-error', 'space-error'):
        c.observe(vm, rb'profile-ui: state=editing operation=' + operation.encode() + rb' result=error', mutation, 180)
        c.observe(vm, rb'profile-ui: state=worker operation=' + operation.encode() +
                  rb' result=reaped status=[1-9]\d* stdout_eof=1 stderr_eof=1', mutation)
    else:
        c.observe(vm, rb'profile-ui: state=complete operation=' + operation.encode() + rb' result=success', mutation, 180)
        c.observe(vm, rb'profile-ui: state=worker operation=' + operation.encode() +
                  rb' result=reaped status=0 stdout_eof=1 stderr_eof=1', mutation)
    vm.screenshot(operation + '-result'); t.close_profiles(vm, start)


def recover(vm, home=B):
    start, index = c.chooser(vm, home, 'recoverable')
    vm.screenshot('recoverable'); c.select(vm, index)
    c.observe(vm, rb'profile-chooser: recovery start home=' + re.escape(home.encode()), start)
    done = c.observe(vm, rb'profile-chooser: recovery result=success home=' + re.escape(home.encode()), start, 180)
    if b'deskd: profile id=' in vm.output[start:done.start()]: raise RuntimeError('profile opened before recovery')
    index = c.row(vm, home, 'ready', start); c.select(vm, index); c.opened(vm, home, start)
    t.close_session(vm)


def restored_readback(vm):
    start = len(vm.output); vm.launch('--apps-trace --scale=1')
    c.observe(vm, rb'profile-chooser: remembered restored home=' + re.escape(B.encode()), start)
    c.observe(vm, rb'deskd: profile id=' + d.ID.encode() + rb' home=' + re.escape(B.encode()) +
              rb' credentials=unchanged', start)
    c.observe(vm, rb'deskd: commit', start)
    if b'profile-chooser: ready rows=' in vm.output[start:]:
        raise RuntimeError('remembered B session unexpectedly showed chooser')
    # Use Text's actual Open picker and verify the loaded content length; the
    # offline final checks also compare every byte of the original file.
    opened = len(vm.output)
    vm.key('meta_l-spc'); t.text(vm, 'Text new app'); vm.key('ret')
    c.observe(vm, rb'desktools: starting role=text', opened)
    c.observe(vm, rb'deskd: commit', opened)
    vm.key('ctrl-l'); vm.key('ctrl-a'); t.text(vm, B + '/Documents/migration.txt')
    vm.key('ctrl-o'); c.observe(vm, rb'picker ready mode=1', opened)
    t.text(vm, 'migration'); vm.key('ret')
    c.observe(vm, rb'file-result role=text op=1 status=0', opened)
    vm.key('ctrl-a'); vm.key('ctrl-c')
    c.observe(vm, rb'clipboard role=2 action=8 bytes=' + str(len(TEXT)).encode(), opened)
    vm.screenshot('imported-text')
    vm.key('ctrl-w'); c.observe(vm, rb'desktools: exited role=text failure=0', opened)
    # Preview decodes the actual migrated BMP, then a screenshot records its
    # rendered view. No save or transform is requested.
    preview = len(vm.output)
    vm.key('meta_l-spc'); t.text(vm, 'Preview new app'); vm.key('ret')
    c.observe(vm, rb'deskd: launcher application=7', preview)
    c.observe(vm, rb'deskview: starting', preview); c.observe(vm, rb'deskd: commit', preview)
    vm.key('ctrl-o'); c.observe(vm, rb'deskview: picker ready', preview)
    browse = len(vm.output); vm.key('ctrl-l'); t.text(vm, B + '/Documents'); vm.key('ret')
    c.observe(vm, rb'deskview: picker ready', browse)
    vm.key('tab'); vm.key('ctrl-a'); t.text(vm, 'photo.bmp'); vm.key('ret')
    c.observe(vm, rb'deskview: loaded path=' + re.escape((B + '/Documents/photo.bmp').encode()) +
              rb' width=1024 height=768', preview, 180)
    time.sleep(.5); vm.screenshot('imported-bmp')
    vm.key('ctrl-w'); c.observe(vm, rb'deskview: workers retired=1', preview)
    c.observe(vm, rb'deskview: exited failure=0', preview)
    t.close_session(vm)


def check_profile(fixture, home, machine, bundle, payloads, source):
    prefix = home.removeprefix('/fat/') + '/'
    if (fixture.read(prefix + 'profile.id') != b'srvros-profile-v1\n' + d.ID.encode() + b'\n' or
        v1.decode_json(fixture.read(prefix + 'identity.json')) != bundle['profile'] or
        v1.decode_json(fixture.read(prefix + v1.PREF_PATH)) != bundle['preferences'] or
        fixture.read('home/desktop.conf') != machine):
        raise RuntimeError('profile identity, settings, or machine preferences differ')
    binding = v1.decode_json(fixture.read(prefix + 'profile-local.json'))
    if source and binding != {'alias': 'local_a', 'uid': 1001, 'gid': 1001}:
        raise RuntimeError('source binding changed')
    if not source and (binding.get('alias') != 'local_b' or binding.get('uid') == 1001 or binding.get('gid') == 1001):
        raise RuntimeError('import binding is not destination-local')
    for path, data in payloads.items():
        if fixture.read(prefix + path) != data: raise RuntimeError('document changed: ' + path)
    if source:
        if fixture.read(prefix + 'Documents/binary.dat') != d.BINARY: raise RuntimeError('unselected original binary changed')
    else:
        c.absent(fixture, prefix + 'Documents/binary.dat')
    for path, data in SENTINELS:
        if source:
            if fixture.read(prefix + path) != data: raise RuntimeError('source exclusion changed: ' + path)
        else: c.absent(fixture, prefix + path)
    c.absent(fixture, prefix + '.profile-import-pending')
    c.absent(fixture, home.removeprefix('/fat/') + '.profile-import.json')
    for binary in ('deskd', 'deskterm', 'desktools', 'deskprofiles', 'profile'):
        expected = runtime_elf((ROOT / f'build/userspace/{binary}.elf').read_bytes())
        if fixture.read('bin/' + binary) != expected: raise RuntimeError('installed binary differs: ' + binary)
    return binding


def free_bytes(fixture):
    root = fixture.chain(fixture.root)
    bitmap = next((struct.unpack_from('<I', root, i + 20)[0] for i in range(0, len(root), 32)
                   if root[i] == 0x81), None)
    if bitmap is None: raise RuntimeError('exFAT allocation bitmap missing')
    offset = fixture.offset(bitmap)
    return sum(not (fixture.data[offset + i // 8] & (1 << (i % 8)))
               for i in range(fixture.count)) * fixture.cluster


def run_guest(vm, line, *, timeout=300):
    command = 'run ' + line
    if len(command.encode('ascii')) >= 128: raise RuntimeError('monitor command exceeds limit')
    start = len(vm.output); vm.serial.sendall((command + '\n').encode('ascii'))
    vm.wait(b' exited status=', timeout); vm.wait(b'\nsrv> ', timeout)
    output = bytes(vm.output[start:])
    status = re.search(rb'run: pid=\d+ exited status=(\d+)', output)
    if not status or int(status.group(1)): raise RuntimeError('guest command failed: ' + line)
    return output


def memory_preview(vm, label, expected_bytes):
    output = run_guest(vm, '/fat/bin/sh /work/stream-input/memory-' + label + '.sh', timeout=600)
    stats = re.findall(rb'profile-memory: guest_peak_mapped_bytes=(\d+) phase=exit', output)
    reviews = re.findall(rb'\{[^\r\n]*\}', output)
    if len(stats) != 1 or not reviews: raise RuntimeError('missing guest mapped-high-water evidence')
    review = v1.decode_json(reviews[-1])
    if (review.get('format') != 'srvros-profile-review-v1' or review.get('operation') != 'export' or
        review.get('bundle_version') != 2 or review.get('bytes') != expected_bytes):
        raise RuntimeError('memory preview review differs from workload')
    return int(stats[0]), review


def memory_command(vm, path):
    output = run_guest(vm, '/fat/bin/sh ' + path, timeout=600)
    stats = re.findall(rb'profile-memory: guest_peak_mapped_bytes=(\d+) phase=exit', output)
    if len(stats) != 1: raise RuntimeError('missing actual transfer memory evidence')
    return int(stats[0])


def guest_script(vm, path, lines):
    for index, line in enumerate(lines):
        command = '/fat/bin/write ' + ('-a ' if index else '') + path + ' ' + line
        run_guest(vm, command)


def review_guest(vm, line, operation, size):
    output = run_guest(vm, line, timeout=600)
    reviews = re.findall(rb'\{[^\r\n]*\}', output)
    if not reviews: raise RuntimeError('missing guest review')
    review = v1.decode_json(reviews[-1])
    if (review.get('format') != 'srvros-profile-review-v1' or review.get('operation') != operation or
        review.get('bundle_version') != 2 or review.get('bytes') != size or
        not re.fullmatch(r'[0-9a-f]{64}', review.get('sha256', ''))):
        raise RuntimeError('guest transfer review mismatch')
    return review


def fill_guest(vm, count):
    command = 'run /fat/bin/dd if=/dev/zero of=/fat/home/space-fill.bin bs=65536 count=' + str(count)
    if len(command) >= 128: raise RuntimeError('fill command exceeds monitor limit')
    start = len(vm.output); vm.serial.sendall((command + '\n').encode('ascii'))
    vm.wait(b' exited status=', 240); vm.wait(b'\nsrv> ', 240)
    match = re.search(rb'run: pid=\d+ exited status=(\d+)', vm.output[start:])
    if not match or int(match.group(1)): raise RuntimeError('controlled low-space fill failed')


def source_text(vm):
    start = len(vm.output)
    c.observe(vm, rb'desktools: starting role=text', 0)
    vm.key('ctrl-l'); vm.key('ctrl-a'); t.text(vm, A + '/Documents/migration.txt')
    vm.key('ctrl-o'); c.observe(vm, rb'picker ready mode=1', start)
    t.text(vm, 'migration'); vm.key('ret')
    c.observe(vm, rb'file-result role=text op=1 status=0', start)
    vm.key('ctrl-a'); t.text(vm, TEXT.decode()); vm.key('ctrl-s')
    c.observe(vm, rb'file-result role=text op=2 status=0', start)


def create_sentinels(vm):
    for path, data in SENTINELS:
        vm.run('/fat/bin/write ' + A + '/' + path + ' ' + data.decode('ascii').strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    if not 10 <= args.boot_wait <= 240: parser.error('invalid boot wait')
    for firmware in (args.ovmf, args.ovmf_vars):
        if not Path(firmware).is_file() or ',' in firmware: parser.error('invalid firmware path')
    base = ROOT / 'build/review/profile-stream'; base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base)).resolve()
    print(f'Profile streaming evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'source': source_provenance.capture(ROOT),
        'firmware_sha256': d.digest(Path(args.ovmf).read_bytes()),
        'firmware_vars_sha256': d.digest(Path(args.ovmf_vars).read_bytes()),
        'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0],
        'tool_sha256': {name: d.digest((ROOT / 'tools' / name).read_bytes()) for name in
            ('profile_stream_smoke.py', 'profile_stream.py', 'profile_transfer_smoke.py', 'profile_chooser_smoke.py',
             'profile_guest_smoke.py', 'profile_desktop_smoke.py', 'profile_image.py',
             'profile_bundle.py', 'installation_image.py')}}
    d.save_json(out / 'receipt.json', receipt)
    try:
        receipt['build'] = d.verified_build(ROOT)
        receipt['guest_memory_bounds'] = {'absolute_mapped_bytes': MAPPED_PEAK_LIMIT,
            'growth_from_small_bytes': MAPPED_GROWTH_LIMIT, 'frozen_before_guest_run': True}
        d.save_json(out / 'receipt.json', receipt)
        volume_size = (ROOT / 'build/srvros.exfat').stat().st_size
        disk_size = installation_image.layout(volume_size)['size_bytes']
        # Three permanent images plus one probe, one archive transient, and
        # independent volumes/snapshots; never rely on sparse or compressed size.
        required = 7 * disk_size + 12 * volume_size + t.ARCHIVE_BUDGET + 1024**3
        available = shutil.disk_usage(out).free
        receipt['space_preflight'] = {'required_bytes': required, 'available_bytes': available,
            'disk_allowance': 7, 'volume_allowance': 12, 'archive_budget_bytes': t.ARCHIVE_BUDGET,
            'reserve_bytes': 1024**3}
        if available < required: raise RuntimeError('insufficient space for retained evidence and reserve')
        image, binary = bmp_fixture(), binary_fixture()
        if len(image) <= 1024 * 1024 or len(binary) != MAX_DOC: raise RuntimeError('large fixture size mismatch')
        disks, ma, mb = fixture(out, receipt, image, binary)
        with c.VM(disks['a'], disks['transport'], out, 'a-export', args) as vm:
            run_guest(vm, '/fat/bin/cp /work/stream-input/photo.bmp ' + A + '/Documents/photo.bmp')
            run_guest(vm, '/fat/bin/cp /work/stream-input/large.bin ' + A + '/Documents/large.bin')
            create_sentinels(vm)
            start, index = c.chooser(vm, A); c.select(vm, index); c.opened(vm, A, start)
            source_text(vm)
            gui_transfer(vm, 'export', filename='legacy.json', docs=('Documents/migration.txt', 'Documents/binary.dat'))
            gui_transfer(vm, 'export')
            t.close_session(vm)
        expected = {'Documents/large.bin': binary, 'Documents/photo.bmp': image, 'Documents/migration.txt': TEXT}
        transport = t.check_allocation(disks['transport'], out, 'transport-export')
        legacy = transport.read('legacy.json'); legacy_bundle = v1.decode_json(legacy)
        if (legacy.startswith(MAGIC) or legacy_bundle['profile'] != d.make_bundle()['profile'] or
            v1.validate_bundle(legacy_bundle) != {'Documents/migration.txt': TEXT, 'Documents/binary.dat': d.BINARY}):
            raise RuntimeError('small graphical export is not compatible v1')
        exported = transport.read('transfer.srvrprofile')
        path = out / 'guest-export.srvrprofile'; path.write_bytes(exported)
        with path.open('rb') as stream: bundle, digest, documents = verify_v2(stream, expected)
        if bundle['profile'] != d.make_bundle()['profile'] or bundle['preferences'] != d.make_bundle()['preferences']:
            raise RuntimeError('v2 exported identity or preferences differ')
        # Exercise the independently maintained host codec on the actual
        # guest-produced container, while retaining the separate wire verifier.
        with host_stream.open_bundle(path) as host:
            if host.legacy or host.metadata != bundle or host.sha256 != digest:
                raise RuntimeError('host codec disagrees with guest v2 bundle')
            for entry in host.metadata['documents']:
                sink = io.BytesIO(); host.copy_document(entry, sink)
                if sink.getvalue() != expected[entry['path']]:
                    raise RuntimeError('host codec document bytes differ: ' + entry['path'])
        receipt['host_codec'] = {'guest_v2_read': True, 'sha256': digest}
        receipt['guest_export'] = {'path': str(path), 'sha256': digest, 'bytes': len(exported),
                                   'version': 2, 'documents': documents}
        receipt['legacy_export'] = {'sha256': d.digest(legacy), 'bytes': len(legacy), 'version': 1}
        with c.VM(disks['a'], disks['transport'], out, 'a-v2-partial', args) as vm:
            desktop(vm, A, checkpoint='export-partial')
            gui_transfer(vm, 'export', checkpoint='export-partial', filename='partial.srvrprofile')
            t.close_session(vm)
        partial = t.check_allocation(disks['transport'], out, 'transport-v2-partial').read('partial.srvrprofile')
        if not partial or len(partial) >= len(exported) or not exported.startswith(partial):
            raise RuntimeError('v2 interrupted export is not a durable partial prefix')
        with c.VM(disks['a'], disks['transport'], out, 'a-v2-partial-retry', args) as vm:
            desktop(vm, A); gui_transfer(vm, 'export', filename='partial.srvrprofile', outcome='transfer-error')
            t.close_session(vm)
        if t.check_allocation(disks['transport'], out, 'transport-v2-partial-retry').read('partial.srvrprofile') != partial:
            raise RuntimeError('exclusive v2 retry modified partial output')
        receipt['partial_export'] = {'bytes': len(partial), 'sha256': d.digest(partial),
                                     'same_filename_refused_and_preserved': True}
        with c.VM(disks['a'], disks['transport'], out, 'a-existing-output', args) as vm:
            desktop(vm, A); gui_transfer(vm, 'export', outcome='transfer-error'); t.close_session(vm)
        if t.check_allocation(disks['transport'], out, 'transport-existing-output').read('transfer.srvrprofile') != exported:
            raise RuntimeError('exclusive export retry changed existing v2 bundle')
        receipt['guest_export']['existing_output_refused_and_preserved'] = True
        # Reverse interoperability uses a product host-encoder fixture on a
        # different generated transport and a B clone. The main transport stays
        # the untouched guest-produced export throughout the core proof.
        host_store = out / 'host-store'; v1.init_store(host_store)
        host_stream.import_bundle(host_store, path, 'host_fixture', 3100, 3100)
        host_export = out / 'host-produced.srvrprofile'
        host_stream.export_bundle(host_store, d.ID, ['Documents/photo.bmp'], host_export)
        host_bytes = host_export.read_bytes()
        host_manifest, host_digest, _ = verify_v2(io.BytesIO(host_bytes), {'Documents/photo.bmp': image})
        host_volume = out / 'host-transport.exfat'
        host_fixture = ExfatFixture((out / 'transport-source.exfat').read_bytes())
        host_fixture.add_directory_files('host-input', {'host.srvrprofile': host_bytes})
        host_volume.write_bytes(host_fixture.data)
        stage = out / 'target-host-transport'
        installation_image.prepare(stage, (ROOT / 'build/kernel/srvros.elf').resolve(),
            (ROOT / 'build/limine-binary/BOOTX64.EFI').resolve(), host_volume.resolve(),
            (ROOT / 'build/userspace/init.elf').resolve(), identity_bound=True)
        host_ready = installation_image.apply(stage); host_disk = stage / 'image.img'
        if host_ready['gpt']['disk_guid'] in {v['gpt']['disk_guid'] for v in receipt['installations'].values()}:
            raise RuntimeError('host fixture transport GUID is not distinct')
        host_bounds = d.outside_data(host_disk, *d.partitions(host_disk)[1])
        host_receiver = out / 'probe-host-import.img'; shutil.copyfile(disks['b'], host_receiver)
        with c.VM(host_receiver, host_disk, out, 'b-host-import', args) as vm:
            desktop(vm); gui_transfer(vm, 'import', filename='host.srvrprofile', directory='/work/host-input')
            t.close_session(vm)
        host_fs = t.check_allocation(host_receiver, out, 'host-import-final')
        host_prefix = B.removeprefix('/fat/') + '/'
        if (host_fs.read(host_prefix + 'Documents/photo.bmp') != image or
            v1.decode_json(host_fs.read(host_prefix + 'identity.json')) != host_manifest['profile'] or
            v1.decode_json(host_fs.read(host_prefix + v1.PREF_PATH)) != host_manifest['preferences'] or
            host_fs.read('home/desktop.conf') != mb):
            raise RuntimeError('guest import of host-produced v2 differs')
        c.absent(host_fs, host_prefix + '.profile-import-pending')
        c.absent(host_fs, B.removeprefix('/fat/') + '.profile-import.json')
        if d.outside_data(host_receiver, *d.partitions(host_receiver)[1]) != receipt['installations']['b']['protected_regions']:
            raise RuntimeError('host-import receiver protected bytes changed')
        if d.outside_data(host_disk, *d.partitions(host_disk)[1]) != host_bounds:
            raise RuntimeError('host fixture transport protected bytes changed')
        if t.check_allocation(host_disk, out, 'host-transport-final').read('host-input/host.srvrprofile') != host_bytes:
            raise RuntimeError('host-produced transport bundle changed')
        receipt['host_codec'].update(host_v2_guest_import=True, host_export_sha256=host_digest,
                                     host_transport_gpt=host_ready['gpt'])
        t.archive_completed(host_receiver, out); t.archive_completed(host_disk, out); t.archive_completed(host_volume, out)
        # The legacy receiver is a pristine B copy. It never changes the main B.
        legacy_disk = out / 'probe-legacy.img'; shutil.copyfile(disks['b'], legacy_disk)
        with c.VM(legacy_disk, disks['transport'], out, 'b-legacy-import', args) as vm:
            desktop(vm); gui_transfer(vm, 'import', filename='legacy.json'); t.close_session(vm)
        fixture_legacy = t.check_allocation(legacy_disk, out, 'b-legacy-final')
        prefix = B.removeprefix('/fat/') + '/'
        if (fixture_legacy.read(prefix + 'Documents/migration.txt') != TEXT or
            fixture_legacy.read(prefix + 'Documents/binary.dat') != d.BINARY or
            v1.decode_json(fixture_legacy.read(prefix + 'identity.json')) != legacy_bundle['profile'] or
            v1.decode_json(fixture_legacy.read(prefix + v1.PREF_PATH)) != legacy_bundle['preferences'] or
            fixture_legacy.read('home/desktop.conf') != mb or
            fixture_legacy.read(prefix + 'profile.id') != b'srvros-profile-v1\n' + d.ID.encode() + b'\n'):
            raise RuntimeError('v1 graphical import differs')
        legacy_binding = v1.decode_json(fixture_legacy.read(prefix + 'profile-local.json'))
        if legacy_binding.get('alias') != 'local_b' or legacy_binding.get('uid') == 1001 or legacy_binding.get('gid') == 1001:
            raise RuntimeError('v1 imported binding is not destination-local')
        for path, _ in SENTINELS: c.absent(fixture_legacy, prefix + path)
        if d.outside_data(legacy_disk, *d.partitions(legacy_disk)[1]) != receipt['installations']['b']['protected_regions']:
            raise RuntimeError('legacy receiver protected bytes changed')
        t.archive_completed(legacy_disk, out)
        # Revalidation rejects a changed source after the actual review. A and
        # the transport are cloned/isolated so the original inputs survive.
        changed_a = out / 'probe-changed-a.img'; shutil.copyfile(disks['a'], changed_a)
        with c.VM(changed_a, disks['transport'], out, 'a-changed-review', args) as vm:
            desktop(vm, A)
            gui_transfer(vm, 'export', filename='changed.srvrprofile', outcome='transfer-error',
                         change_file=A + '/Documents/migration.txt')
            t.close_session(vm)
        c.absent(t.check_allocation(disks['transport'], out, 'transport-changed-export'), 'changed.srvrprofile')
        if d.outside_data(changed_a, *d.partitions(changed_a)[1]) != receipt['installations']['a']['protected_regions']:
            raise RuntimeError('changed-source probe protected bytes changed')
        t.check_allocation(changed_a, out, 'changed-source-final'); t.archive_completed(changed_a, out)
        changed_transport = out / 'transport-changed.img'; shutil.copyfile(disks['transport'], changed_transport)
        with c.VM(disks['b'], changed_transport, out, 'b-changed-review', args) as vm:
            before = vm.quiesced_data_snapshot('changed-bundle-before')
            desktop(vm)
            gui_transfer(vm, 'import', outcome='transfer-error', change_file='/work/transfer.srvrprofile')
            vm.close_desktop(); after = vm.quiesced_data_snapshot('changed-bundle-after')
            receipt['changed_bundle_comparison'] = c.compare_blocked(before, after)
            vm.finish()
        t.archive_completed(before, out); t.archive_completed(after, out)
        if d.outside_data(changed_transport, *d.partitions(changed_transport)[1]) != receipt['installations']['transport']['protected_regions']:
            raise RuntimeError('changed transport protected bytes changed')
        t.check_allocation(changed_transport, out, 'changed-transport-final'); t.archive_completed(changed_transport, out)
        receipt['changed_review'] = {'source_revalidation_refused': True, 'bundle_revalidation_refused': True}
        # Each invalid container is synthetic and lives beside the raw input
        # fixtures; no host modification is made to the actual guest export.
        receipt['refusals'] = {}
        with c.VM(disks['b'], disks['transport'], out, 'b-invalid', args) as vm:
            for filename in invalid_fixtures():
                label = filename.split('.')[0]
                before = vm.quiesced_data_snapshot(label + '-before')
                desktop(vm)
                gui_transfer(vm, 'import', filename=filename, directory='/work/stream-input', outcome='preview-error')
                vm.close_desktop()
                after = vm.quiesced_data_snapshot(label + '-after')
                receipt['refusals'][label] = c.compare_blocked(before, after)
                t.archive_completed(before, out); t.archive_completed(after, out)
            vm.finish()
        receipt['interruptions'] = []
        for checkpoint in CHECKPOINTS:
            probe = out / ('probe-' + checkpoint + '.img'); shutil.copyfile(disks['b'], probe)
            with c.VM(probe, disks['transport'], out, 'stop-' + checkpoint, args) as vm:
                desktop(vm, checkpoint=checkpoint); gui_transfer(vm, 'import', checkpoint=checkpoint); t.close_session(vm)
            stopped = t.check_allocation(probe, out, 'stopped-' + checkpoint)
            journal_bytes = stopped.read('home/local_b.profile-import.json')
            journal = v1.decode_json(journal_bytes)
            if (set(journal) != {'format', 'home', 'bundle', 'sha256', 'binding', 'profile_id'} or
                    journal['format'] != 'srvros-profile-import-v2' or journal['home'] != B or
                    journal['sha256'] != digest or journal['profile_id'] != d.ID):
                raise RuntimeError('invalid v2 recovery journal')
            if checkpoint == 'copy-partial':
                scratch = stopped.read('home/local_b/.profile-copy-temp')
                if scratch != binary[:CHUNK]: raise RuntimeError('copy-partial did not preserve exactly one durable chunk')
            if checkpoint == 'published':
                c.absent(stopped, 'home/local_b/.profile-import-pending')
                if stopped.read('home/local_b/profile.id') != b'srvros-profile-v1\n' + d.ID.encode() + b'\n':
                    raise RuntimeError('published journal lacks published marker')
            with c.VM(probe, disks['transport'], out, 'recover-' + checkpoint, args) as vm: recover(vm)
            if d.outside_data(probe, *d.partitions(probe)[1]) != receipt['installations']['b']['protected_regions']:
                raise RuntimeError('protected bytes changed: ' + checkpoint)
            recovered = t.check_allocation(probe, out, 'recovered-' + checkpoint)
            binding = check_profile(recovered, B, mb, bundle, expected, False)
            receipt['interruptions'].append({'checkpoint': checkpoint, 'binding': binding,
                'disk_sha256': d.digest(probe.read_bytes()), 'journal_sha256': d.digest(journal_bytes)})
            t.archive_completed(probe, out); d.save_json(out / 'receipt.json', receipt)
        # A controlled low-space receiver has room for journal metadata but not
        # the first 16 MiB document. It is a disposable clone and may retain a
        # transaction-owned journal/scratch after refusal.
        low = out / 'probe-low-space.img'; shutil.copyfile(disks['b'], low)
        available_guest = free_bytes(t.check_allocation(low, out, 'low-space-initial'))
        fill_blocks = (available_guest - 6 * 1024 * 1024) // CHUNK
        if fill_blocks <= 0: raise RuntimeError('cannot build controlled low-space fixture')
        with c.VM(low, disks['transport'], out, 'low-space', args) as vm:
            fill_guest(vm, fill_blocks)
            before = vm.quiesced_data_snapshot('low-space-before')
            before_free = free_bytes(ExfatFixture(before.read_bytes()))
            if not 1024 * 1024 <= before_free < MAX_DOC:
                raise RuntimeError('low-space fixture did not reach intended free range')
            desktop(vm); gui_transfer(vm, 'import', outcome='space-error'); vm.close_desktop()
            after = vm.quiesced_data_snapshot('low-space-after'); vm.finish()
        low_fs = ExfatFixture(after.read_bytes())
        if low_fs.read('home/desktop.conf') != mb or low_fs.read('home/space-fill.bin') != b'\0' * (fill_blocks * CHUNK):
            raise RuntimeError('low-space refusal changed existing data')
        c.absent(low_fs, 'home/local_b/profile.id')
        try: journal = v1.decode_json(low_fs.read('home/local_b.profile-import.json'))
        except StopIteration: journal = None
        if journal is not None and (journal.get('format') != 'srvros-profile-import-v2' or
                                    journal.get('home') != B or journal.get('sha256') != digest):
            raise RuntimeError('low-space refusal retained unowned journal')
        if d.outside_data(low, *d.partitions(low)[1]) != receipt['installations']['b']['protected_regions']:
            raise RuntimeError('low-space probe protected bytes changed')
        t.check_allocation(low, out, 'low-space-refused')
        receipt['low_space'] = {'initial_free_bytes': available_guest, 'free_before_import': before_free,
                                'filler_bytes': fill_blocks * CHUNK, 'journal_retained': journal is not None,
                                'failure_observed': True, 'recovered': False}
        d.save_json(out / 'receipt.json', receipt)
        # Preserve both refusal snapshots before releasing space. The only
        # operator cleanup is the synthetic filler created by this invocation.
        t.archive_completed(before, out); t.archive_completed(after, out)
        with c.VM(low, disks['transport'], out, 'low-space-recover', args) as vm:
            run_guest(vm, '/fat/bin/rm /fat/home/space-fill.bin')
            restored_snapshot = vm.quiesced_data_snapshot('low-space-restored')
            restored_fs = ExfatFixture(restored_snapshot.read_bytes())
            c.absent(restored_fs, 'home/space-fill.bin')
            restored_free = free_bytes(restored_fs)
            if restored_free <= before_free or restored_free < MAX_DOC:
                raise RuntimeError('removing the synthetic filler did not restore enough space')
            if journal is not None:
                retained = v1.decode_json(restored_fs.read('home/local_b.profile-import.json'))
                if retained != journal: raise RuntimeError('space restoration modified retained journal')
                recover(vm)
            else:
                c.absent(restored_fs, 'home/local_b.profile-import.json')
                desktop(vm); gui_transfer(vm, 'import'); t.close_session(vm)
        recovered = t.check_allocation(low, out, 'low-space-recovered')
        binding = check_profile(recovered, B, mb, bundle, expected, False)
        c.absent(recovered, 'home/local_b/.profile-copy-temp')
        c.absent(recovered, 'home/space-fill.bin')
        if d.outside_data(low, *d.partitions(low)[1]) != receipt['installations']['b']['protected_regions']:
            raise RuntimeError('low-space recovery changed protected bytes')
        receipt['low_space'].update(space_restored=True, free_after_cleanup=restored_free,
            recovery_path='chooser' if journal is not None else 'gui-import',
            recovered=True, binding=binding, recovered_disk_sha256=d.digest(low.read_bytes()))
        d.save_json(out / 'receipt.json', receipt)
        t.archive_completed(restored_snapshot, out); t.archive_completed(low, out)
        with c.VM(disks['b'], disks['transport'], out, 'b-import', args) as vm:
            desktop(vm); gui_transfer(vm, 'import'); t.close_session(vm)
        with c.VM(disks['b'], disks['transport'], out, 'b-choose', args) as vm:
            start, index = c.chooser(vm, B); vm.screenshot('imported-chooser')
            c.select(vm, index); c.opened(vm, B, start); t.close_session(vm)
        with c.VM(disks['b'], disks['transport'], out, 'b-restored', args) as vm:
            restored_readback(vm)
        # Measure previews at three sizes, then actual reviewed 64 MiB export
        # and import on fresh owned images; report kernel-mapped high-water, not
        # exact heap or kernel allocation. Native engine allocation is separate.
        memory_volume = out / 'memory-transport-source.exfat'
        memory_fixture = ExfatFixture((ROOT / 'build/srvros.exfat').resolve().read_bytes())
        memory_fixture.add_directory_files('stream-input', memory_scripts())
        if free_bytes(memory_fixture) < MAX_TOTAL + 4 * 1024 * 1024:
            raise RuntimeError('fresh memory transport lacks 64 MiB output reserve')
        memory_volume.write_bytes(memory_fixture.data)
        memory_stage = out / 'target-memory-transport'
        installation_image.prepare(memory_stage, (ROOT / 'build/kernel/srvros.elf').resolve(),
            (ROOT / 'build/limine-binary/BOOTX64.EFI').resolve(), memory_volume.resolve(),
            (ROOT / 'build/userspace/init.elf').resolve(), identity_bound=True)
        memory_ready = installation_image.apply(memory_stage)
        memory_transport = memory_stage / 'image.img'
        memory_transport_bounds = d.outside_data(memory_transport, *d.partitions(memory_transport)[1])
        known_guids = {v['gpt']['disk_guid'] for v in receipt['installations'].values()}
        if memory_ready['gpt']['disk_guid'] in known_guids: raise RuntimeError('memory transport GUID collision')
        memory_disk = out / 'probe-memory-a.img'; shutil.copyfile(disks['a'], memory_disk)
        with c.VM(memory_disk, memory_transport, out, 'memory-a', args) as vm:
            for name in ('a.bin', 'b.bin', 'c.bin'):
                run_guest(vm, '/fat/bin/cp ' + A + '/Documents/large.bin ' + A + '/Documents/' + name)
            before = vm.quiesced_data_snapshot('memory-before')
            small_peak, _ = memory_preview(vm, 'small', len(image))
            file_peak, _ = memory_preview(vm, 'file_16m', MAX_DOC)
            total_peak, export_review = memory_preview(vm, 'total_64m', MAX_TOTAL)
            after = vm.quiesced_data_snapshot('memory-after')
            receipt['memory_readonly_comparison'] = c.compare_blocked(before, after)
            guest_script(vm, '/work/e', [
                'export SRVROS_PROFILE_TRACE_MEMORY=1', 'H=' + export_review['sha256'],
                'D=Documents/large.bin', 'A=Documents/a.bin', 'B=Documents/b.bin', 'C=Documents/c.bin',
                '/fat/bin/profile export-auto-reviewed /fat/home/local-a /work/m64 $H $D $A $B $C'])
            export_peak = memory_command(vm, '/work/e')
            vm.finish()
        if d.outside_data(memory_disk, *d.partitions(memory_disk)[1]) != receipt['installations']['a']['protected_regions']:
            raise RuntimeError('memory source protected bytes changed')
        t.check_allocation(memory_disk, out, 'memory-a-final')
        memory_transport_fs = t.check_allocation(memory_transport, out, 'memory-transport-export')
        memory_bytes = memory_transport_fs.read('m64')
        memory_expected = {path: binary for path in ('Documents/large.bin', 'Documents/a.bin', 'Documents/b.bin', 'Documents/c.bin')}
        memory_manifest, memory_digest, _ = verify_v2(io.BytesIO(memory_bytes), memory_expected)
        if (memory_digest != export_review['sha256'] or memory_manifest['profile'] != bundle['profile'] or
            memory_manifest['preferences'] != bundle['preferences']):
            raise RuntimeError('64 MiB actual export differs from reviewed digest')
        memory_b_stage = out / 'target-memory-b'
        installation_image.prepare(memory_b_stage, (ROOT / 'build/kernel/srvros.elf').resolve(),
            (ROOT / 'build/limine-binary/BOOTX64.EFI').resolve(), (out / 'destination.exfat').resolve(),
            (ROOT / 'build/userspace/init.elf').resolve(), identity_bound=True)
        memory_b_ready = installation_image.apply(memory_b_stage)
        memory_b = memory_b_stage / 'image.img'
        memory_b_bounds = d.outside_data(memory_b, *d.partitions(memory_b)[1])
        if memory_b_ready['gpt']['disk_guid'] in known_guids | {memory_ready['gpt']['disk_guid']}:
            raise RuntimeError('memory receiver GUID collision')
        if free_bytes(t.check_allocation(memory_b, out, 'memory-b-initial')) < MAX_TOTAL + 4 * 1024 * 1024:
            raise RuntimeError('memory receiver lacks 64 MiB import reserve')
        with c.VM(memory_b, memory_transport, out, 'memory-b', args) as vm:
            review = review_guest(vm, '/fat/bin/profile preview-import /work/m64 ' + B + ' local_b', 'import', MAX_TOTAL)
            if review['sha256'] != memory_digest or review['profile'] != bundle['profile']:
                raise RuntimeError('64 MiB import review differs')
            destination = review.get('destination', {})
            if (destination.get('home') != B or destination.get('alias') != 'local_b' or
                type(destination.get('uid')) is not int or type(destination.get('gid')) is not int):
                raise RuntimeError('64 MiB destination review is invalid')
            guest_script(vm, '/work/i', ['export SRVROS_PROFILE_TRACE_MEMORY=1',
                'H=' + review['sha256'], 'U=' + str(destination['uid']), 'G=' + str(destination['gid']),
                '/fat/bin/profile import-reviewed /work/m64 /fat/home/local_b local_b $U $G $H'])
            import_peak = memory_command(vm, '/work/i')
            vm.finish()
        memory_b_fs = t.check_allocation(memory_b, out, 'memory-b-final')
        memory_prefix = B.removeprefix('/fat/') + '/'
        if (memory_b_fs.read(memory_prefix + 'profile.id') != b'srvros-profile-v1\n' + d.ID.encode() + b'\n' or
            v1.decode_json(memory_b_fs.read(memory_prefix + 'identity.json')) != bundle['profile'] or
            v1.decode_json(memory_b_fs.read(memory_prefix + v1.PREF_PATH)) != bundle['preferences'] or
            memory_b_fs.read('home/desktop.conf') != mb):
            raise RuntimeError('64 MiB imported identity/preferences differ')
        actual_binding = v1.decode_json(memory_b_fs.read(memory_prefix + 'profile-local.json'))
        if actual_binding != {'alias': 'local_b', 'uid': destination['uid'], 'gid': destination['gid']}:
            raise RuntimeError('64 MiB imported binding differs from review')
        for document, data in memory_expected.items():
            if memory_b_fs.read(memory_prefix + document) != data:
                raise RuntimeError('64 MiB imported document differs: ' + document)
        for excluded, _ in SENTINELS: c.absent(memory_b_fs, memory_prefix + excluded)
        c.absent(memory_b_fs, memory_prefix + '.profile-import-pending')
        c.absent(memory_b_fs, B.removeprefix('/fat/') + '.profile-import.json')
        if d.outside_data(memory_b, *d.partitions(memory_b)[1]) != memory_b_bounds:
            raise RuntimeError('memory receiver protected bytes changed')
        if d.outside_data(memory_transport, *d.partitions(memory_transport)[1]) != memory_transport_bounds:
            raise RuntimeError('memory transport protected bytes changed')
        if t.check_allocation(memory_transport, out, 'memory-transport-final').read('m64') != memory_bytes:
            raise RuntimeError('64 MiB memory transport output changed')
        peaks = {'small': small_peak, 'file_16m': file_peak, 'total_64m': total_peak,
                 'export_64m': export_peak, 'import_64m': import_peak}
        if max(peaks.values()) > MAPPED_PEAK_LIMIT or max(peaks.values()) - small_peak > MAPPED_GROWTH_LIMIT:
            raise RuntimeError('guest mapped high-water exceeded preregistered absolute or growth bound')
        receipt['guest_mapped_high_water'] = {'bytes': peaks, 'kind': 'srv_usage SELF peak_mapped_bytes',
            'absolute_limit_bytes': MAPPED_PEAK_LIMIT, 'growth_limit_bytes': MAPPED_GROWTH_LIMIT,
            'memory_bundle_sha256': memory_digest, 'actual_64m_export_import': True,
            'memory_transport_gpt': memory_ready['gpt'], 'memory_receiver_gpt': memory_b_ready['gpt'],
            'not_exact_heap_or_kernel_memory': True}
        for artifact in (before, after, memory_disk, memory_b, memory_transport, memory_volume):
            t.archive_completed(artifact, out)
        # 640x480 uses a distinct identity-bound installation from A's offline
        # data; review and confirm remain genuine GUI actions and screenshots
        # independently assert the framebuffer size.
        _, (offset, size) = d.partitions(disks['a'])
        small_volume = out / 'small-source.exfat'
        small_volume.write_bytes(d.data_image(disks['a'], offset, size))
        stage = out / 'target-small'; original = installation_image.CONFIG
        small_config = original.replace(b'resolution: 1280x800', b'resolution: 640x480')
        if small_config == original: raise RuntimeError('small display override unavailable')
        try:
            installation_image.CONFIG = small_config
            plan = installation_image.prepare(stage, (ROOT / 'build/kernel/srvros.elf').resolve(),
                (ROOT / 'build/limine-binary/BOOTX64.EFI').resolve(), small_volume.resolve(),
                (ROOT / 'build/userspace/init.elf').resolve(), identity_bound=True)
            ready = installation_image.apply(stage)
        finally:
            installation_image.CONFIG = original
        small = stage / 'image.img'; config = plan['config'].encode('ascii')
        if b'resolution: 640x480' not in config or (stage / 'limine.conf').read_bytes() != config:
            raise RuntimeError('small fixture EFI configuration differs')
        if ready['gpt']['disk_guid'] in {v['gpt']['disk_guid'] for v in receipt['installations'].values()}:
            raise RuntimeError('small fixture GUID is not distinct')
        bounds = d.outside_data(small, *d.partitions(small)[1])
        with c.VM(small, disks['transport'], out, 'small-export', args, 640, 480) as vm:
            start = len(vm.output); desktop(vm, A)
            c.observe(vm, rb'deskd: display physical=640x480 logical=640x480 scale=1', start)
            gui_transfer(vm, 'export', filename='small.srvrprofile'); t.close_session(vm)
        if d.outside_data(small, *d.partitions(small)[1]) != bounds:
            raise RuntimeError('small fixture protected bytes changed')
        small_fs = t.check_allocation(small, out, 'small-final')
        check_profile(small_fs, A, ma, bundle, expected, True)
        small_bytes = t.check_allocation(disks['transport'], out, 'small-export').read('small.srvrprofile')
        if small_bytes != exported: raise RuntimeError('small GUI export differs from main v2 export')
        receipt['small_fixture'] = {'gpt': ready['gpt'], 'config': config.decode('ascii'),
            'config_sha256': d.digest(config), 'protected_regions': bounds,
            'final_disk_sha256': d.digest(small.read_bytes()), 'export_sha256': d.digest(small_bytes)}
        t.archive_completed(small, out); t.archive_completed(small_volume, out)
        for name, home, machine, source in (('a', A, ma, True), ('b', B, mb, False)):
            fs = t.check_allocation(disks[name], out, name + '-final')
            binding = check_profile(fs, home, machine, bundle, expected, source)
            if fs.read('home/.desktop-profile') != c.expected_choice(home, d.ID):
                raise RuntimeError('remembered profile differs: ' + name)
            receipt['installations'][name]['profile'] = {'id': d.ID, 'binding': binding,
                'machine_sha256': d.digest(machine)}
        final_transport = t.check_allocation(disks['transport'], out, 'transport-final')
        if final_transport.read('transfer.srvrprofile') != exported or final_transport.read('legacy.json') != legacy or final_transport.read('small.srvrprofile') != small_bytes or final_transport.read('partial.srvrprofile') != partial:
            raise RuntimeError('transport bundle changed')
        for name, disk in disks.items():
            if d.outside_data(disk, *d.partitions(disk)[1]) != receipt['installations'][name]['protected_regions']:
                raise RuntimeError(name + ': protected bytes changed')
            receipt['installations'][name]['final_disk_sha256'] = d.digest(disk.read_bytes())
        receipt.update(status='PASS', limits=['disposable QEMU images only',
            'mapped high-water is not exact heap or kernel memory; native allocation qualification is independent',
            'no authentication or access-control claim', 'no physical media or power-loss acceptance',
            'external fsck.exfat not run'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        d.save_json(out / 'receipt.json', receipt)
    print(f'profile-stream: PASS; evidence {out}', flush=True)


if __name__ == '__main__': main()
