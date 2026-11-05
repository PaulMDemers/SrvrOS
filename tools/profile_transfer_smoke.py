#!/usr/bin/env python3
"""Graphical profile transfer across disposable installed QEMU disks.

Requires a source-matched build and creates every disk it touches. The export is
created by the Profiles UI on A and the same mounted transport is attached to B.
Evidence is retained under build/review/profile-transfer. Run only one VM/build
at a time. No host device paths or pre-existing disks are accepted.
"""
import argparse
import gzip
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path
import installation_image
import profile_bundle
import profile_chooser_smoke as c
import profile_desktop_smoke as d
import profile_guest_smoke as guest
import profile_image
import source_provenance

ROOT = Path(__file__).resolve().parents[1]
A, B = guest.A, '/fat/home/local_b'
CHECKPOINTS = ('journal', 'directories', 'document', 'identity', 'commit-ready')
# Synthetic only: these files must remain on A and must not enter the bundle.
SENTINELS = (
    ('unselected_document', 'Documents/unselected.txt', 'synthetic-unselected-document'),
    ('credential_fixture', '.config/credential-sentinel.fixture', 'synthetic-not-a-credential'),
    ('application_state', '.local/share/app-state.fixture', 'synthetic-app-state'),
    ('cache', '.cache/runtime-cache.fixture', 'synthetic-runtime-cache'),
)
ARCHIVE_BUDGET = 1024**3


def archive_completed(path, out):
    """Retain a completed image losslessly, verify readback, then release raw bytes.

    Call only after the last consumer; failures retain the original image.
    The finite archive budget is part of the preflight peak-space bound.
    """
    path = Path(path)
    manifest_path = out / 'archived-images.json'
    manifest = profile_bundle.decode_json(manifest_path.read_bytes()) if manifest_path.exists() else []
    archive = path.with_name(path.name + '.gz')
    original = hashlib.sha256()
    size = 0
    with path.open('rb') as source, archive.open('xb') as target:
        with gzip.GzipFile(filename='', mode='wb', fileobj=target, mtime=0) as packed:
            for block in iter(lambda: source.read(1024 * 1024), b''):
                size += len(block); original.update(block); packed.write(block)
    restored = hashlib.sha256()
    restored_size = 0
    with gzip.open(archive, 'rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            restored_size += len(block); restored.update(block)
    if restored_size != size or restored.digest() != original.digest():
        raise RuntimeError(f'archive readback failed: {path}')
    if sum(item['archive_bytes'] for item in manifest) + archive.stat().st_size > ARCHIVE_BUDGET:
        raise RuntimeError('completed-image archive budget exceeded; raw image retained')
    manifest.append({'path': str(path), 'bytes': size, 'sha256': original.hexdigest(),
        'archive': str(archive), 'archive_bytes': archive.stat().st_size,
        'archive_sha256': installation_image.digest(archive),
        'restore': 'gzip -dc ARCHIVE > PATH', 'round_trip_verified': True})
    d.save_json(manifest_path, manifest)
    path.unlink()


def check_allocation(disk, out, label):
    fixture = guest.check_allocation(disk, out, label)
    # ExfatFixture owns the validated payload in memory, independently of this file.
    archive_completed(out / f'data-{label}.exfat', out)
    return fixture


def text(vm, value):
    keys = {' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus', '_': 'minus', '=': 'equal'}
    for ch in value:
        vm.qmp.key(keys.get(ch, ch.lower()), ch.isupper() or ch == '_')


def wait_trace(vm, expression, start, timeout=120):
    return c.observe(vm, expression.encode(), start, timeout)


def checkpoint_flag(checkpoint):
    if checkpoint not in CHECKPOINTS + ('export-partial',):
        raise ValueError('invalid diagnostic checkpoint')
    return '--profile-transfer-stop=' + checkpoint


def launch_desktop(vm, home=None, checkpoint=None):
    start = len(vm.output)
    flags = ('--profile=' + home + ' ' if home else '') + '--apps-trace --scale=1'
    if checkpoint:
        flags += ' ' + checkpoint_flag(checkpoint)
    vm.launch(flags)
    c.observe(vm, rb'deskd: commit', start)
    if home:
        c.observe(vm, rb'deskd: profile id=' + d.ID.encode() + rb' home=' + re.escape(home.encode()), start)


def launch_profiles(vm, checkpoint=None):
    start = len(vm.output)
    vm.key('meta_l-spc'); text(vm, 'Profiles new app'); vm.key('ret')
    c.observe(vm, rb'launcher application=13', start)
    if checkpoint:
        c.observe(vm, rb'profile-ui: diagnostic checkpoint=' + re.escape(checkpoint.encode()), start)
    return start


def picker(vm, directory, name):
    # Ctrl-L edits the shared picker's folder; its filename field is reached
    # from the list with Tab. The picker itself validates the selected file.
    vm.key('ctrl-l'); text(vm, directory); vm.key('ret')
    time.sleep(1)
    vm.key('tab'); text(vm, name); vm.key('ret')


def prepare(args, out, receipt):
    receipt['build'] = d.verified_build(ROOT)
    driver = ROOT / 'build/tests/profile-image-driver'
    base = ROOT / 'build/srvros.exfat'
    machines = {'a': d.machine_preferences(4, -300), 'b': d.machine_preferences(2, 60)}
    source = out / 'source.exfat'
    profile_image.provision(base, source, d.make_bundle(), A, 'local_a', 1001, 1001, driver, machines['a'])
    target = out / 'destination.exfat'
    with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=out) as temporary:
        image = Path(temporary) / 'image'; shutil.copyfile(base, image)
        prefs = Path(temporary) / 'prefs'; prefs.write_bytes(machines['b'])
        for op, path, extra in (('ensure-home', '/fat/home', []),
                                ('write-desktop', '/fat/home/desktop.conf', [str(prefs)]),
                                ('check-clean', '/fat/home', [])):
            subprocess.run([str(driver), str(image), op, path, *extra], check=True)
        shutil.copyfile(image, target)
    disks, bounds = {}, {}
    receipt['installations'] = {}
    for name, volume in (('a', source), ('b', target), ('transport', base)):
        stage = out / ('target-' + name)
        installation_image.prepare(stage, ROOT / 'build/kernel/srvros.elf',
            ROOT / 'build/limine-binary/BOOTX64.EFI', volume,
            ROOT / 'build/userspace/init.elf', identity_bound=True)
        ready = installation_image.apply(stage)
        disk = stage / 'image.img'; disks[name] = disk
        _, (offset, size) = d.partitions(disk)
        bounds[name] = d.outside_data(disk, offset, size)
        receipt['installations'][name] = {'disk': str(disk), 'gpt': ready['gpt'],
            'data_offset': offset, 'data_bytes': size, 'protected_regions': bounds[name],
            'initial_disk_sha256': d.digest(disk.read_bytes())}
        check_allocation(disk, out, name + '-initial')
    if len({v['gpt']['disk_guid'] for v in receipt['installations'].values()}) != 3:
        raise RuntimeError('fixture disk GUIDs must differ')
    archive_completed(source, out)
    archive_completed(target, out)
    return disks, bounds, machines


def check_profile(fixture, home, machine, bundle, *, source=False):
    prefix = home.removeprefix('/fat/') + '/'
    if fixture.read(prefix + 'profile.id') != b'srvros-profile-v1\n' + d.ID.encode() + b'\n':
        raise RuntimeError('profile identity mismatch')
    if profile_bundle.decode_json(fixture.read(prefix + 'identity.json')) != bundle['profile']:
        raise RuntimeError('full profile identity mismatch')
    binding = profile_bundle.decode_json(fixture.read(prefix + 'profile-local.json'))
    if binding['alias'] != ('local_a' if source else 'local_b'):
        raise RuntimeError('local alias mismatch')
    if source and binding != {'alias': 'local_a', 'uid': 1001, 'gid': 1001}:
        raise RuntimeError('source binding mismatch')
    if not source and (binding['uid'] == 1001 or binding['gid'] == 1001):
        raise RuntimeError('destination local binding did not differ')
    if (fixture.read(prefix + 'Documents/migration.txt') != d.SOURCE or
        fixture.read(prefix + 'Documents/binary.dat') != d.BINARY or
        profile_bundle.decode_json(fixture.read(prefix + profile_bundle.PREF_PATH)) != bundle['preferences'] or
        fixture.read('home/desktop.conf') != machine):
        raise RuntimeError('document, portable preferences, or machine settings mismatch')
    c.absent(fixture, prefix + '.profile-import-pending')
    for _, path, payload in SENTINELS:
        if source:
            if fixture.read(prefix + path) != (payload + '\n').encode('ascii'):
                raise RuntimeError(f'source sentinel changed: {path}')
        else:
            c.absent(fixture, prefix + path)
    for binary in ('deskd', 'desktools', 'deskterm', 'deskprofiles', 'profile'):
        expected = runtime_elf((ROOT / f'build/userspace/{binary}.elf').read_bytes())
        if fixture.read('bin/' + binary) != expected:
            raise RuntimeError(f'installed {binary} differs from matched build')
    return binding


def free_space_preflight(out):
    # At most four live disks (A/B/transport plus small-screen OR one checkpoint)
    # and one compression transient. Eight volumes cover preparation/snapshots,
    # plus a capped 1 GiB archive and 1 GiB reserve for screenshots/other evidence.
    # Do not count sparse allocation or an assumed compression ratio as a guarantee.
    volume_size = (ROOT / 'build/srvros.exfat').stat().st_size
    disk_size = installation_image.layout(volume_size)['size_bytes']
    estimate = 5 * disk_size + 8 * volume_size + ARCHIVE_BUDGET + 1024**3
    available = shutil.disk_usage(out).free
    if available < estimate:
        raise RuntimeError(f'insufficient free space: need {estimate} bytes including 1 GiB reserve; have {available}')
    return {'required_bytes': estimate, 'available_bytes': available,
            'reserve_bytes': 1024**3, 'archive_budget_bytes': ARCHIVE_BUDGET,
            'maximum_live_disks': 4, 'compression_transient_disks': 1,
            'volume_allowance': 8, 'disk_bytes': disk_size, 'volume_bytes': volume_size}


def geometry(vm, start):
    pid = c.observe(vm, rb'profile-ui: started pid=(\d+)', start).group(1)
    match = c.observe(vm, rb'deskd: geometry pid=' + pid + rb' window=\d+ frame=(\d+),(\d+),(\d+),(\d+)', start)
    x, y, w, h = map(int, match.groups())
    return x + 1, y + 44, w - 2, h - 45


def click_client(vm, frame, x, y):
    vm.click(frame[0] + x, frame[1] + y)


def select_file(vm, frame, button, directory, name, mode):
    start = len(vm.output)
    click_client(vm, frame, *button)
    c.observe(vm, rb'profile-ui: state=picker operation=\w+ result=ready', start)
    vm.key('ctrl-l'); text(vm, directory); vm.key('ret')
    c.observe(vm, rb'profile-ui: state=picker operation=\w+ result=ready', len(vm.output), 120)
    if mode == 'open': vm.key('tab')
    else: vm.key('ctrl-a')
    text(vm, name); vm.key('ret')
    c.observe(vm, rb'profile-ui: state=picker operation=\w+ result=selected', start)


def close_profiles(vm, start):
    vm.key('ctrl-w')
    c.observe(vm, rb'profile-ui: exited pid=\d+', start)


def transfer(vm, operation, checkpoint=None, filename='transfer.json', alias='local_b', expect='success', source_home=A):
    start = launch_profiles(vm, checkpoint)
    c.observe(vm, rb'profile-ui: state=editing', start)
    frame = geometry(vm, start)
    x, y, w, h = frame
    if operation == 'export':
        click_client(vm, frame, w // 2, 25)
        for name in ('migration.txt', 'binary.dat'):
            select_file(vm, frame, (45, 105), source_home + '/Documents', name, 'open')
        select_file(vm, frame, (60, 64), '/work', filename, 'save')
    else:
        click_client(vm, frame, w // 6, 25)
        select_file(vm, frame, (60, 64), '/work', filename, 'open')
        click_client(vm, frame, 180, 105); vm.key('ctrl-a'); text(vm, alias)
    preview = len(vm.output)
    click_client(vm, frame, w - 60, h - 54)
    c.observe(vm, rb'profile-ui: state=previewing operation=' + operation.encode() + rb' result=started', preview)
    if expect == 'preview-error':
        c.observe(vm, rb'profile-ui: state=editing operation=' + operation.encode() + rb' result=error', preview)
        vm.screenshot(operation + '-refused')
        close_profiles(vm, start)
        return
    c.observe(vm, rb'profile-ui: state=reviewing operation=' + operation.encode() + rb' result=ready', preview)
    vm.screenshot(operation + '-review')
    click_client(vm, frame, 40, 80); vm.key('end')
    vm.screenshot(operation + '-review-details')
    mutation = len(vm.output)
    click_client(vm, frame, w - 60, h - 54)
    c.observe(vm, rb'profile-ui: state=transferring operation=' + operation.encode() + rb' result=started', mutation)
    if checkpoint:
        c.observe(vm, rb'profile-ui: checkpoint=' + checkpoint.encode() + rb' result=error', mutation)
        c.observe(vm, rb'profile-ui: state=worker operation=\w+ result=reaped status=86 stdout_eof=1 stderr_eof=1', mutation)
    elif expect == 'transfer-error':
        c.observe(vm, rb'profile-ui: state=editing operation=' + operation.encode() + rb' result=error', mutation)
        c.observe(vm, rb'profile-ui: state=worker operation=' + operation.encode() +
                  rb' result=reaped status=[1-9]\d* stdout_eof=1 stderr_eof=1', mutation)
    else:
        c.observe(vm, rb'profile-ui: state=complete operation=' + operation.encode() + rb' result=success', mutation)
    vm.screenshot(operation + ('-interrupted' if checkpoint else '-refused' if expect == 'transfer-error' else '-complete'))
    close_profiles(vm, start)


def close_session(vm):
    vm.close_desktop()
    vm.finish()


def recover(vm):
    start, index = c.chooser(vm, B, 'recoverable')
    vm.screenshot('chooser-recoverable')
    c.select(vm, index)
    c.observe(vm, rb'profile-chooser: recovery start home=' + re.escape(B.encode()), start)
    recovered = c.observe(vm, rb'profile-chooser: recovery result=success home=' + re.escape(B.encode()), start, 120)
    if b'deskd: profile id=' in vm.output[start:recovered.start()]:
        raise RuntimeError('pending profile opened before recovery')
    index = c.row(vm, B, 'ready', start)
    c.select(vm, index); c.opened(vm, B, start)
    close_session(vm)


def restored_and_open_document(vm):
    start = len(vm.output)
    vm.launch('--apps-trace --scale=1')
    c.observe(vm, rb'profile-chooser: remembered restored home=' + re.escape(B.encode()), start)
    c.observe(vm, rb'deskd: profile id=' + d.ID.encode() + rb' home=' + re.escape(B.encode()) +
              rb' credentials=unchanged', start)
    c.observe(vm, rb'deskd: commit', start)
    if b'profile-chooser: ready rows=' in vm.output[start:]:
        raise RuntimeError('remembered session unexpectedly showed chooser')
    # Open the imported document through Text's own picker, then copy its
    # contents to exercise a readback of the expected length and capture it.
    opened = len(vm.output)
    vm.key('meta_l-spc'); text(vm, 'Text new app'); vm.key('ret')
    c.observe(vm, rb'desktools: starting role=text', opened)
    c.observe(vm, rb'deskd: commit', opened)
    vm.key('ctrl-l'); vm.key('ctrl-a'); text(vm, B + '/Documents/migration.txt')
    vm.key('ctrl-o'); c.observe(vm, rb'picker ready mode=1', opened)
    text(vm, 'migration'); vm.key('ret')
    c.observe(vm, rb'file-result role=text op=1 status=0', opened)
    vm.key('ctrl-a'); vm.key('ctrl-c')
    c.observe(vm, rb'clipboard role=2 action=8 bytes=' + str(len(d.SOURCE)).encode(), opened)
    vm.screenshot('imported-document')
    close_session(vm)


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
    base = ROOT / 'build/review/profile-transfer'; base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'Profile transfer evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'source': source_provenance.capture(ROOT),
        'firmware_sha256': d.digest(Path(args.ovmf).read_bytes()),
        'firmware_vars_sha256': d.digest(Path(args.ovmf_vars).read_bytes()),
        'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0],
        'tool_sha256': {name: d.digest((ROOT / 'tools' / name).read_bytes()) for name in
            ('profile_transfer_smoke.py', 'profile_chooser_smoke.py', 'profile_guest_smoke.py',
             'profile_desktop_smoke.py', 'profile_image.py', 'profile_bundle.py', 'installation_image.py')}}
    d.save_json(out / 'receipt.json', receipt)
    try:
        receipt['space_preflight'] = free_space_preflight(out)
        disks, bounds, machines = prepare(args, out, receipt)
        c.absent(check_allocation(disks['b'], out, 'b-empty'), 'home/local_b/profile.id')
        c.absent(check_allocation(disks['transport'], out, 'transport-empty'), 'transfer.json')
        d.save_json(out / 'receipt.json', receipt)
        with c.VM(disks['a'], disks['transport'], out, 'a-interrupted-export', args) as vm:
            for _, path, payload in SENTINELS:
                vm.run('/fat/bin/write ' + A + '/' + path + ' ' + payload)
            start, index = c.chooser(vm, A, flags='--choose-profile --apps-trace --scale=1 ' +
                                     checkpoint_flag('export-partial'))
            vm.screenshot('chooser-source')
            c.select(vm, index); c.opened(vm, A, start)
            transfer(vm, 'export', 'export-partial', 'partial.json'); close_session(vm)
        partial = check_allocation(disks['transport'], out, 'transport-partial').read('partial.json')
        if not partial: raise RuntimeError('interrupted export did not retain partial output')
        receipt['partial_export'] = {'sha256': d.digest(partial), 'bytes': len(partial)}
        a_choice = check_allocation(disks['a'], out, 'a-initial-choice').read('home/.desktop-profile')
        if a_choice != c.expected_choice(A): raise RuntimeError('source chooser record mismatch')
        with c.VM(disks['a'], disks['transport'], out, 'a-partial-retry', args) as vm:
            launch_desktop(vm, A); transfer(vm, 'export', filename='partial.json', expect='transfer-error'); close_session(vm)
        if check_allocation(disks['transport'], out, 'transport-partial-retry').read('partial.json') != partial:
            raise RuntimeError('refused retry modified partial export')
        receipt['partial_export']['same_filename_refused_and_preserved'] = True
        with c.VM(disks['a'], disks['transport'], out, 'a-export', args) as vm:
            launch_desktop(vm, A); transfer(vm, 'export'); close_session(vm)
        exported = check_allocation(disks['transport'], out, 'transport-export').read('transfer.json')
        (out / 'guest-export.json').write_bytes(exported)
        bundle = profile_bundle.decode_json(exported)
        payloads = profile_bundle.validate_bundle(bundle)
        if (bundle['profile'] != d.make_bundle()['profile'] or payloads != {'Documents/migration.txt': d.SOURCE,
                'Documents/binary.dat': d.BINARY} or bundle['preferences'] != d.make_bundle()['preferences']):
            raise RuntimeError('GUI export differs from selected source documents or preferences')
        receipt['guest_export'] = {'sha256': d.digest(exported), 'bytes': len(exported),
                                   'path': str(out / 'guest-export.json')}
        receipt['exclusions'] = [{'class': kind, 'path': path,
            'synthetic_sha256': d.digest((payload + '\n').encode('ascii')),
            'bundle_absent': path not in payloads, 'source_preserved': False,
            'destination_absent': False, 'recovered_checkpoints_absent': []}
            for kind, path, payload in SENTINELS]
        # Derive a separate identity-bound installation from A's offline data;
        # apply the 640x480 EFI config to both planning and image creation.
        _, (a_offset, a_size) = d.partitions(disks['a'])
        small_volume = out / 'small-source.exfat'
        small_volume.write_bytes(d.data_image(disks['a'], a_offset, a_size))
        small_stage = out / 'target-small'
        original_config = installation_image.CONFIG
        small_config = original_config.replace(b'resolution: 1280x800', b'resolution: 640x480')
        if small_config == original_config:
            raise RuntimeError('small fixture EFI resolution override failed')
        try:
            installation_image.CONFIG = small_config
            small_plan = installation_image.prepare(small_stage, ROOT / 'build/kernel/srvros.elf',
                ROOT / 'build/limine-binary/BOOTX64.EFI', small_volume,
                ROOT / 'build/userspace/init.elf', identity_bound=True)
            small_ready = installation_image.apply(small_stage)
        finally:
            installation_image.CONFIG = original_config
        small = small_stage / 'image.img'
        config = small_plan['config'].encode('ascii')
        if b'resolution: 640x480' not in config or (small_stage / 'limine.conf').read_bytes() != config:
            raise RuntimeError('small fixture EFI config mismatch')
        if small_ready['gpt']['disk_guid'] in {v['gpt']['disk_guid'] for v in receipt['installations'].values()}:
            raise RuntimeError('small fixture GUID is not distinct')
        _, (small_offset, small_size) = d.partitions(small)
        small_bounds = d.outside_data(small, small_offset, small_size)
        receipt['small_fixture'] = {'disk': str(small), 'gpt': small_ready['gpt'],
            'source_data_sha256': d.digest(small_volume.read_bytes()), 'config': config.decode('ascii'),
            'config_sha256': d.digest(config), 'protected_regions': small_bounds,
            'initial_disk_sha256': d.digest(small.read_bytes())}
        check_allocation(small, out, 'small-initial')
        d.save_json(out / 'receipt.json', receipt)
        with c.VM(small, disks['transport'], out, 'small-export', args, 640, 480) as vm:
            start = len(vm.output)
            launch_desktop(vm, A)
            c.observe(vm, rb'deskd: display physical=640x480 logical=640x480 scale=1', start)
            transfer(vm, 'export', filename='small.json')
            close_session(vm)
        small_export = check_allocation(disks['transport'], out, 'transport-small').read('small.json')
        if small_export != exported:
            raise RuntimeError('640x480 graphical export differs from source export')
        if d.outside_data(small, small_offset, small_size) != small_bounds:
            raise RuntimeError('small fixture protected bytes changed')
        small_fixture = check_allocation(small, out, 'small-final')
        check_profile(small_fixture, A, machines['a'], bundle, source=True)
        if small_fixture.read('home/.desktop-profile') != a_choice:
            raise RuntimeError('small fixture source choice changed')
        receipt['small_fixture'].update(final_disk_sha256=d.digest(small.read_bytes()),
                                        export_sha256=d.digest(small_export))
        d.save_json(out / 'receipt.json', receipt)
        archive_completed(small_volume, out)
        archive_completed(small, out)
        # Invalid data is created in the guest solely as an invalid-input fixture.
        with c.VM(disks['b'], disks['transport'], out, 'b-invalid', args) as vm:
            vm.run('/fat/bin/write /work/invalid.json malformed')
            before = vm.quiesced_data_snapshot('invalid-before')
            launch_desktop(vm); transfer(vm, 'import', filename='invalid.json', expect='preview-error')
            vm.close_desktop()
            after = vm.quiesced_data_snapshot('invalid-after')
            receipt['invalid_comparison'] = c.compare_blocked(before, after)
            vm.finish()
        archive_completed(before, out)
        archive_completed(after, out)
        invalid_b = check_allocation(disks['b'], out, 'b-invalid')
        c.absent(invalid_b, 'home/local_b/profile.id')
        c.absent(invalid_b, 'home/local_b.profile-import.json')
        receipt['interruptions'] = []
        for checkpoint in CHECKPOINTS:
            probe = out / ('probe-' + checkpoint + '.img'); shutil.copyfile(disks['b'], probe)
            with c.VM(probe, disks['transport'], out, 'stop-' + checkpoint, args) as vm:
                launch_desktop(vm, checkpoint=checkpoint); transfer(vm, 'import', checkpoint); close_session(vm)
            with c.VM(probe, disks['transport'], out, 'recover-' + checkpoint, args) as vm:
                recover(vm)
            if d.outside_data(probe, *d.partitions(probe)[1]) != bounds['b']:
                raise RuntimeError(checkpoint + ': protected bytes changed')
            fixture = check_allocation(probe, out, 'recovered-' + checkpoint)
            binding = check_profile(fixture, B, machines['b'], bundle)
            for exclusion in receipt['exclusions']:
                exclusion['recovered_checkpoints_absent'].append(checkpoint)
            receipt['interruptions'].append({'checkpoint': checkpoint, 'disk': str(probe),
                'binding': binding, 'final_disk_sha256': d.digest(probe.read_bytes())})
            archive_completed(probe, out)
            d.save_json(out / 'receipt.json', receipt)
        with c.VM(disks['b'], disks['transport'], out, 'b-import', args) as vm:
            launch_desktop(vm); transfer(vm, 'import'); close_session(vm)
        with c.VM(disks['b'], disks['transport'], out, 'b-choose', args) as vm:
            start, index = c.chooser(vm, B); vm.screenshot('chooser-imported')
            c.select(vm, index); c.opened(vm, B, start); close_session(vm)
        with c.VM(disks['b'], disks['transport'], out, 'b-restored', args) as vm:
            restored_and_open_document(vm)
        with c.VM(disks['b'], disks['transport'], out, 'b-duplicate', args) as vm:
            before = vm.quiesced_data_snapshot('duplicate-before')
            launch_desktop(vm, B); transfer(vm, 'import', alias='duplicate', expect='preview-error')
            vm.close_desktop()
            after = vm.quiesced_data_snapshot('duplicate-after')
            receipt['duplicate_comparison'] = c.compare_blocked(before, after)
            vm.finish()
        archive_completed(before, out)
        archive_completed(after, out)
        duplicate_b = check_allocation(disks['b'], out, 'b-duplicate')
        c.absent(duplicate_b, 'home/duplicate/profile.id')
        c.absent(duplicate_b, 'home/duplicate.profile-import.json')
        for name, home in (('a', A), ('b', B)):
            fixture = check_allocation(disks[name], out, name + '-final')
            binding = check_profile(fixture, home, machines[name], bundle, source=name == 'a')
            receipt['installations'][name]['profile'] = {'id': d.ID, 'home': home, 'binding': binding,
                'machine_sha256': d.digest(machines[name])}
        for exclusion in receipt['exclusions']:
            exclusion['source_preserved'] = True
            exclusion['destination_absent'] = True
        a_final_choice = check_allocation(disks['a'], out, 'a-final-choice').read('home/.desktop-profile')
        if a_final_choice != a_choice or a_final_choice != c.expected_choice(A):
            raise RuntimeError('source remembered choice changed during transfer')
        receipt['installations']['a']['profile']['choice_sha256'] = d.digest(a_final_choice)
        b_fixture = check_allocation(disks['b'], out, 'b-choice')
        if b_fixture.read('home/.desktop-profile') != c.expected_choice(B):
            raise RuntimeError('remembered imported selection mismatch')
        receipt['installations']['b']['profile']['choice_sha256'] = d.digest(b_fixture.read('home/.desktop-profile'))
        transport = check_allocation(disks['transport'], out, 'transport-final')
        if (transport.read('transfer.json') != exported or transport.read('partial.json') != partial or
                transport.read('invalid.json') != b'malformed\n' or transport.read('small.json') != small_export):
            raise RuntimeError('transport output changed after export')
        for name, disk in disks.items():
            if d.outside_data(disk, *d.partitions(disk)[1]) != bounds[name]:
                raise RuntimeError(name + ': protected bytes changed')
            receipt['installations'][name]['final_disk_sha256'] = d.digest(disk.read_bytes())
            archive_completed(disk, out)
        receipt['process_lifecycle'] = {'gui_workers': 'reaped and streams drained; see serial logs',
            'gui_app': 'exit observed after retirement; see serial logs',
            'qemu': 'each owned VM stopped at context exit'}
        receipt.update(status='PASS', limits=['disposable QEMU images only',
            'no authentication or access-control claim', 'no physical media or power-loss acceptance',
            'external fsck.exfat not run'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        d.save_json(out / 'receipt.json', receipt)
    print(f'profile-transfer: PASS; evidence {out}', flush=True)


if __name__ == '__main__': main()
