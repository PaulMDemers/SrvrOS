#!/usr/bin/env python3
"""Exercise graphical profile creation and transfer on disposable installations.

Only generated images are used. Run after a source-matched build, with no other
build or VM harness active. Evidence and verified archives are retained.
"""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import uuid

from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path
import installation_image
import profile_bundle
import profile_chooser_smoke as c
import profile_desktop_smoke as d
import profile_transfer_smoke as t
import source_provenance

ROOT = Path(__file__).resolve().parents[1]
A = '/fat/home/created_a'
B = '/fat/home/imported_b'
SECOND = '/fat/home/created_second'
NAME = 'Created profile'
CHECKPOINTS = ('journal', 'directories', 'identity', 'commit-ready', 'published')
DEFAULTS = {'text_size': 16, 'clock_flags': 3, 'wallpaper': 0,
            'theme_accent': 0, 'glass_level': 1, 'theme_style': 0}
BINARY = b'\x00\xffsynthetic-binary\x80\x00'
TEXT = b'Created profile document'
LEGACY = b'synthetic legacy document\n'


def canonical_uuid(value):
    try:
        parsed = uuid.UUID(value)
    except (ValueError, AttributeError, TypeError) as error:
        raise RuntimeError('invalid generated profile UUID') from error
    if str(parsed) != value or parsed.version != 4 or parsed.variant != uuid.RFC_4122:
        raise RuntimeError('profile UUID is not canonical RFC-variant version 4')
    return value


def absent(fixture, path):
    c.absent(fixture, path)


def empty_profile(fixture, home, name, machine):
    prefix = home.removeprefix('/fat/') + '/'
    identity = profile_bundle.decode_json(fixture.read(prefix + 'identity.json'))
    if set(identity) != {'id', 'display_name'} or identity['display_name'] != name:
        raise RuntimeError('creation identity differs from requested display name')
    canonical_uuid(identity['id'])
    if fixture.read(prefix + 'profile.id') != b'srvros-profile-v1\n' + identity['id'].encode() + b'\n':
        raise RuntimeError('creation marker and identity differ')
    binding = profile_bundle.decode_json(fixture.read(prefix + 'profile-local.json'))
    if (binding.get('alias') != home.rsplit('/', 1)[1] or set(binding) != {'alias', 'uid', 'gid'} or
            not isinstance(binding['uid'], int) or not isinstance(binding['gid'], int) or
            binding['uid'] < 1000 or binding['gid'] < 1000):
        raise RuntimeError('invalid destination-local binding')
    if profile_bundle.decode_json(fixture.read(prefix + profile_bundle.PREF_PATH)) != DEFAULTS:
        raise RuntimeError('created portable defaults differ')
    # Read the directory as a directory, rather than inferring emptiness from
    # known filenames. ExfatFixture.entries yields only actual directory entries.
    entries = list(fixture.entries(fixture.read(prefix + 'Documents')))
    if any(name not in ('.', '..') for name, _ in entries):
        raise RuntimeError('new Documents is not empty')
    absent(fixture, prefix + '.profile-import-pending')
    absent(fixture, home.removeprefix('/fat/') + '.profile-import.json')
    if fixture.read('home/desktop.conf') != machine:
        raise RuntimeError('machine preferences changed')
    return identity, binding


def panel(vm):
    pw, ph = min(vm.width - 48, 760), min(vm.height - 88, 680)
    return (vm.width - pw) // 2, (vm.height - ph) // 2 + 6, pw, ph


def chooser_start(vm, checkpoint=None):
    start = len(vm.output)
    flags = '--choose-profile --apps-trace --scale=1'
    if checkpoint: flags += ' --profile-transfer-stop=' + checkpoint
    vm.launch(flags)
    c.observe(vm, rb'profile-chooser: ready rows=\d+', start)
    return start


def chooser_create(vm, name, alias, checkpoint=None, keyboard=False):
    start = chooser_start(vm, checkpoint)
    x, y, w, h = panel(vm)
    vm.click(x + 260, y + h - 33)
    if keyboard:
        t.text(vm, 'wrong name'); vm.key('ctrl-a'); t.text(vm, name)
        vm.key('tab'); t.text(vm, 'wrong_alias'); vm.key('ctrl-a'); t.text(vm, alias)
        vm.screenshot('create-form-corrected')
        vm.key('tab'); vm.key('ret')
    else:
        vm.click(x + 100, y + 117); t.text(vm, name)
        vm.click(x + 100, y + 187); t.text(vm, alias)
        vm.click(x + 80, y + h - 33)
    home = '/fat/home/' + alias
    c.observe(vm, rb'profile-chooser: create preview home=' + re.escape(home.encode()), start)
    c.observe(vm, rb'profile-chooser: create preview result=success', start)
    c.observe(vm, rb'profile-chooser: create worker retired operation=preview eof=1 reaped=1 status=0', start)
    vm.screenshot('create-review')
    if keyboard: vm.key('ret')
    else: vm.click(x + 80, y + h - 33)
    c.observe(vm, rb'profile-chooser: create start home=' + re.escape(home.encode()), start)
    if checkpoint:
        c.observe(vm, rb'profile-chooser: create result=failed home=' + re.escape(home.encode()) + rb' status=86', start, 120)
        c.observe(vm, rb'profile-chooser: create worker retired operation=create eof=1 reaped=1 status=86', start)
        vm.screenshot('create-interrupted')
        c.back(vm, start)
        return
    c.observe(vm, rb'profile-chooser: create result=success home=' + re.escape(home.encode()) + rb' status=0', start, 120)
    c.observe(vm, rb'profile-chooser: create worker retired operation=create eof=1 reaped=1 status=0', start)
    vm.screenshot('create-ready')
    index = c.row(vm, home, 'ready', start)
    c.select(vm, index)
    c.observe(vm, rb'profile-chooser: remember saved home=' + re.escape(home.encode()), start)
    c.observe(vm, rb'deskd: profile id=[0-9a-f-]{36} home=' + re.escape(home.encode()) + rb' credentials=unchanged', start)
    c.observe(vm, rb'deskd: commit', start)
    vm.close_desktop()


def app_create(vm, name, alias, expect='success'):
    start = t.launch_profiles(vm)
    c.observe(vm, rb'profile-ui: state=editing', start)
    frame = t.geometry(vm, start); w, h = frame[2:]
    t.click_client(vm, frame, 5 * w // 6, 25)
    t.click_client(vm, frame, 180, 64); vm.key('ctrl-a'); t.text(vm, name)
    t.click_client(vm, frame, 180, 105); vm.key('ctrl-a'); t.text(vm, alias)
    before = len(vm.output); t.click_client(vm, frame, w - 60, h - 54)
    if expect == 'invalid':
        # Invalid input is rejected locally and cannot start a preview worker.
        time.sleep(1); vm.screenshot('invalid-input')
        t.close_profiles(vm, start)
        if b'profile-ui: state=previewing operation=create' in vm.output[before:]:
            raise RuntimeError('invalid alias reached preview')
        return
    c.observe(vm, rb'profile-ui: state=previewing operation=create result=started', before)
    if expect == 'preview-error':
        c.observe(vm, rb'profile-ui: state=editing operation=create result=error', before)
        vm.screenshot('creation-refused'); t.close_profiles(vm, start); return
    c.observe(vm, rb'profile-ui: state=reviewing operation=create result=ready', before)
    vm.screenshot('app-create-review')
    t.click_client(vm, frame, 40, 80); vm.key('end'); vm.screenshot('app-create-review-details')
    mutate = len(vm.output); t.click_client(vm, frame, w - 60, h - 54)
    c.observe(vm, rb'profile-ui: state=transferring operation=create result=started', mutate)
    c.observe(vm, rb'profile-ui: state=complete operation=create result=success', mutate, 120)
    vm.screenshot('app-created'); t.close_profiles(vm, start)


def make_volume(base, out, name, machine, *, capacity=False):
    target = out / (name + '.exfat')
    with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=out) as tmp:
        image = Path(tmp) / 'image'; shutil.copyfile(base, image)
        payload = Path(tmp) / 'payload'
        driver = ROOT / 'build/tests/profile-image-driver'
        def operation(op, path, data=None):
            argv = [str(driver), str(image), op, path]
            if data is not None:
                payload.write_bytes(data); argv.append(str(payload))
            subprocess.run(argv, check=True)
        operation('ensure-home', '/fat/home')
        operation('write-desktop', '/fat/home/desktop.conf', machine)
        operation('write', '/fat/home/legacy-text.fixture', LEGACY)
        operation('write', '/fat/home/legacy-binary.fixture', BINARY)
        if capacity:
            for index in range(16):
                home = f'/fat/home/slot{index:02d}'
                operation('mkdir', home)
                operation('write', home + '/profile.id', b'blocked-synthetic\n')
        operation('check-clean', '/fat/home')
        shutil.copyfile(image, target)
    return target


def install(volume, stage, *, small=False):
    original = installation_image.CONFIG
    config = original.replace(b'resolution: 1280x800', b'resolution: 640x480') if small else original
    if small and config == original: raise RuntimeError('small mode override unavailable')
    try:
        installation_image.CONFIG = config
        plan = installation_image.prepare(stage, ROOT / 'build/kernel/srvros.elf',
            ROOT / 'build/limine-binary/BOOTX64.EFI', volume,
            ROOT / 'build/userspace/init.elf', identity_bound=True)
        ready = installation_image.apply(stage)
    finally:
        installation_image.CONFIG = original
    payload = plan['config'].encode('ascii')
    if (stage / 'limine.conf').read_bytes() != payload or (small and b'resolution: 640x480' not in payload):
        raise RuntimeError('installation configuration mismatch')
    disk = stage / 'image.img'
    _, (offset, size) = d.partitions(disk)
    return disk, {'disk': str(disk), 'gpt': ready['gpt'], 'data_offset': offset,
        'data_bytes': size, 'config_sha256': d.digest(payload), 'config': payload.decode('ascii'),
        'protected_regions': d.outside_data(disk, offset, size),
        'initial_disk_sha256': d.digest(disk.read_bytes())}


def legacy_and_binaries(fixture, machine):
    if (fixture.read('home/legacy-text.fixture') != LEGACY or
        fixture.read('home/legacy-binary.fixture') != BINARY or
        fixture.read('home/desktop.conf') != machine):
        raise RuntimeError('legacy fixture or machine preferences changed')
    for binary in ('deskd', 'desktools', 'deskterm', 'deskprofiles', 'profile'):
        if fixture.read('bin/' + binary) != runtime_elf((ROOT / f'build/userspace/{binary}.elf').read_bytes()):
            raise RuntimeError('installed binary differs: ' + binary)


def start_restored(vm, home, identity):
    start = len(vm.output); vm.launch('--apps-trace --scale=1')
    c.observe(vm, rb'profile-chooser: remembered restored home=' + re.escape(home.encode()), start)
    c.observe(vm, rb'deskd: profile id=' + identity['id'].encode() + rb' home=' + re.escape(home.encode()) +
              rb' credentials=unchanged', start)
    c.observe(vm, rb'deskd: commit', start)
    if b'profile-chooser: ready rows=' in vm.output[start:]:
        raise RuntimeError('remembered session unexpectedly showed chooser')


def edit_data(vm, home):
    start = len(vm.output)
    vm.key('meta_l-spc'); t.text(vm, 'Text new app'); vm.key('ret')
    c.observe(vm, rb'desktools: starting role=text', start)
    c.observe(vm, rb'deskd: commit', start)
    t.text(vm, TEXT.decode()); vm.key('ctrl-l'); vm.key('ctrl-a')
    t.text(vm, home + '/Documents/migration.txt'); vm.key('ctrl-s')
    c.observe(vm, rb'picker ready mode=2', start); vm.key('ret')
    c.observe(vm, rb'file-result role=text op=2 status=0', start)
    vm.screenshot('source-text')
    vm.close_desktop()
    vm.run('/fat/bin/cp /fat/home/legacy-binary.fixture ' + home + '/Documents/binary.dat')
    vm.run('/fat/bin/write ' + home + '/Documents/unselected.txt synthetic-unselected')
    vm.run('/fat/bin/write ' + home + '/.config/credential-sentinel.fixture synthetic-not-a-credential')
    vm.run('/fat/bin/write ' + home + '/.local/share/app-state.fixture synthetic-app-state')
    vm.run('/fat/bin/write ' + home + '/.cache/runtime-cache.fixture synthetic-cache')


def refusal(vm, name, alias, kind, receipt):
    before = vm.quiesced_data_snapshot(kind + '-before')
    t.launch_desktop(vm); app_create(vm, name, alias, kind)
    vm.close_desktop(); after = vm.quiesced_data_snapshot(kind + '-after')
    receipt[kind + '_comparison'] = c.compare_blocked(before, after)
    t.archive_completed(before, vm.out); t.archive_completed(after, vm.out)


def check_choice(fixture, home, identity):
    expected = c.expected_choice(home, identity['id'])
    if fixture.read('home/.desktop-profile') != expected:
        raise RuntimeError('remembered profile record differs')
    return d.digest(expected)


def recovery(vm, home, identity):
    start, index = c.chooser(vm, home, 'recoverable')
    vm.screenshot('recoverable')
    c.select(vm, index)
    c.observe(vm, rb'profile-chooser: recovery start home=' + re.escape(home.encode()), start)
    recovered = c.observe(vm, rb'profile-chooser: recovery result=success home=' + re.escape(home.encode()), start, 120)
    if b'deskd: profile id=' in vm.output[start:recovered.start()]:
        raise RuntimeError('profile opened before recovery')
    index = c.row(vm, home, 'ready', start)
    c.select(vm, index); c.opened(vm, home, start, identity['id'])
    vm.close_desktop()


def capacity_refusal(vm):
    before = vm.quiesced_data_snapshot('capacity-before')
    start = chooser_start(vm)
    x, y, w, h = panel(vm)
    vm.click(x + 260, y + h - 33)
    vm.click(x + 100, y + 117); t.text(vm, 'Capacity profile')
    vm.click(x + 100, y + 187); t.text(vm, 'capacity_new')
    vm.click(x + 80, y + h - 33)
    c.observe(vm, rb'profile-chooser: create preview home=/fat/home/capacity_new', start)
    c.observe(vm, rb'profile-chooser: create preview result=failed', start)
    vm.screenshot('capacity-refused')
    vm.key('esc'); c.back(vm, start)
    after = vm.quiesced_data_snapshot('capacity-after')
    comparison = c.compare_blocked(before, after)
    t.archive_completed(before, vm.out); t.archive_completed(after, vm.out)
    return comparison


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
    base = ROOT / 'build/review/profile-create'; base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'Profile creation evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'source': source_provenance.capture(ROOT),
        'firmware_sha256': d.digest(Path(args.ovmf).read_bytes()),
        'firmware_vars_sha256': d.digest(Path(args.ovmf_vars).read_bytes()),
        'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0],
        'tool_sha256': {name: d.digest((ROOT / 'tools' / name).read_bytes()) for name in
            ('profile_create_smoke.py', 'profile_transfer_smoke.py', 'profile_chooser_smoke.py',
             'profile_guest_smoke.py', 'profile_desktop_smoke.py', 'profile_image.py',
             'profile_bundle.py', 'installation_image.py')}}
    d.save_json(out / 'receipt.json', receipt)
    try:
        receipt['build'] = d.verified_build(ROOT)
        base_volume = ROOT / 'build/srvros.exfat'
        volume_bytes = base_volume.stat().st_size
        disk_bytes = installation_image.layout(volume_bytes)['size_bytes']
        # Three permanent disks plus one active probe; one more disk bounds
        # compression while snapshots, staging and allocation copies coexist.
        required = 5 * disk_bytes + 8 * volume_bytes + t.ARCHIVE_BUDGET + 1024**3
        available = shutil.disk_usage(out).free
        receipt['space_preflight'] = {'required_bytes': required, 'available_bytes': available,
            'maximum_live_disks': 4, 'compression_transient_disks': 1,
            'volume_allowance': 8, 'archive_budget_bytes': t.ARCHIVE_BUDGET,
            'reserve_bytes': 1024**3}
        if available < required: raise RuntimeError('insufficient space for bounded proof and 1 GiB reserve')
        machines = {'a': d.machine_preferences(4, -300), 'b': d.machine_preferences(2, 60)}
        volumes = {name: make_volume(base_volume, out, name, machines[name]) for name in ('a', 'b')}
        volumes['capacity'] = make_volume(base_volume, out, 'capacity', machines['a'], capacity=True)
        disks = {}; receipt['installations'] = {}
        for name, volume in (('a', volumes['a']), ('b', volumes['b']), ('transport', base_volume)):
            disks[name], receipt['installations'][name] = install(volume, out / ('target-' + name))
            t.check_allocation(disks[name], out, name + '-initial')
        guids = {entry['gpt']['disk_guid'] for entry in receipt['installations'].values()}
        if len(guids) != 3: raise RuntimeError('installation GUIDs are not distinct')
        d.save_json(out / 'receipt.json', receipt)
        with c.VM(disks['a'], disks['transport'], out, 'a-create', args) as vm:
            chooser_create(vm, NAME, 'created_a'); vm.finish()
        fixture = t.check_allocation(disks['a'], out, 'a-created-empty')
        identity, binding_a = empty_profile(fixture, A, NAME, machines['a'])
        legacy_and_binaries(fixture, machines['a'])
        choice_a = check_choice(fixture, A, identity)
        receipt['created'] = {'identity': identity, 'binding': binding_a, 'choice_sha256': choice_a,
                              'documents_empty_before_edits': True, 'defaults': DEFAULTS}
        # Separate legacy B starts the ordinary desktop with no forced chooser;
        # its Profiles app can create a second independent identity.
        with c.VM(disks['b'], disks['transport'], out, 'b-legacy-create', args) as vm:
            start = len(vm.output); t.launch_desktop(vm)
            if b'profile-chooser: ready rows=' in vm.output[start:]:
                raise RuntimeError('empty legacy startup unexpectedly forced chooser')
            app_create(vm, 'Second profile', 'created_second'); t.close_session(vm)
        fixture = t.check_allocation(disks['b'], out, 'b-second-empty')
        identity2, binding2 = empty_profile(fixture, SECOND, 'Second profile', machines['b'])
        if identity2['id'] == identity['id']: raise RuntimeError('independent creations reused UUID')
        legacy_and_binaries(fixture, machines['b'])
        receipt['app_created'] = {'identity': identity2, 'binding': binding2}
        with c.VM(disks['a'], disks['transport'], out, 'a-remembered-edit', args) as vm:
            start_restored(vm, A, identity); edit_data(vm, A); vm.finish()
        # Locally invalid input and occupied alias are GUI refusals, each with
        # complete before/after data snapshots and strict metadata comparison.
        with c.VM(disks['a'], disks['transport'], out, 'a-refusals', args) as vm:
            refusal(vm, 'Invalid profile', 'bad alias', 'invalid', receipt)
            refusal(vm, 'Duplicate profile', 'created_a', 'preview-error', receipt)
            vm.finish()
        with c.VM(disks['a'], disks['transport'], out, 'a-export', args) as vm:
            start_restored(vm, A, identity)
            t.transfer(vm, 'export', source_home=A); t.close_session(vm)
        transport = t.check_allocation(disks['transport'], out, 'transport-export')
        exported = transport.read('transfer.json')
        (out / 'guest-export.json').write_bytes(exported)
        bundle = profile_bundle.decode_json(exported); payloads = profile_bundle.validate_bundle(bundle)
        expected_prefs = dict(DEFAULTS)
        if (bundle['profile'] != identity or bundle['preferences'] != expected_prefs or
                payloads != {'Documents/migration.txt': TEXT, 'Documents/binary.dat': BINARY}):
            raise RuntimeError('GUI bundle differs from created/edited profile')
        receipt['guest_export'] = {'sha256': d.digest(exported), 'bytes': len(exported)}
        # Explicit profile selection avoids the chooser presented by B's already
        # created second profile; current session is unchanged by import.
        with c.VM(disks['b'], disks['transport'], out, 'b-import', args) as vm:
            start, index = c.chooser(vm, SECOND)
            c.select(vm, index); c.opened(vm, SECOND, start, identity2['id'])
            t.transfer(vm, 'import', alias='imported_b'); t.close_session(vm)
        with c.VM(disks['b'], disks['transport'], out, 'b-choose', args) as vm:
            start, index = c.chooser(vm, B)
            c.select(vm, index); c.opened(vm, B, start, identity['id']); t.close_session(vm)
        with c.VM(disks['b'], disks['transport'], out, 'b-restored', args) as vm:
            start_restored(vm, B, identity)
            start = len(vm.output); vm.key('meta_l-spc'); t.text(vm, 'Text new app'); vm.key('ret')
            c.observe(vm, rb'desktools: starting role=text', start); c.observe(vm, rb'deskd: commit', start)
            vm.key('ctrl-l'); vm.key('ctrl-a'); t.text(vm, B + '/Documents/migration.txt')
            vm.key('ctrl-o'); c.observe(vm, rb'picker ready mode=1', start); t.text(vm, 'migration'); vm.key('ret')
            c.observe(vm, rb'file-result role=text op=1 status=0', start)
            vm.key('ctrl-a'); vm.key('ctrl-c')
            c.observe(vm, rb'clipboard role=2 action=8 bytes=' + str(len(TEXT)).encode(), start)
            vm.screenshot('imported-document'); t.close_session(vm)
        baseline_b = t.check_allocation(disks['b'], out, 'b-before-interruptions')
        imported_binding = profile_bundle.decode_json(baseline_b.read('home/imported_b/profile-local.json'))
        receipt['interruptions'] = []
        seen_ids = {identity['id'], identity2['id']}
        for checkpoint in CHECKPOINTS:
            # B is an owned, offline installation with unrelated ready profiles;
            # each probe starts from the exact same bytes and never modifies B.
            probe = out / ('probe-' + checkpoint + '.img'); shutil.copyfile(disks['b'], probe)
            probe_home = '/fat/home/recovery_' + checkpoint.replace('-', '_')
            alias = probe_home.rsplit('/', 1)[1]
            with c.VM(probe, disks['transport'], out, 'stop-' + checkpoint, args) as vm:
                chooser_create(vm, 'Recovery profile', alias, checkpoint); vm.finish()
            stopped = t.check_allocation(probe, out, 'stopped-' + checkpoint)
            journal = profile_bundle.decode_json(stopped.read(probe_home.removeprefix('/fat/') + '.profile-import.json'))
            if (journal.get('format') != 'srvros-profile-create-v1' or journal.get('home') != probe_home or
                    not isinstance(journal.get('bundle'), dict) or
                    journal['bundle'].get('documents') != [] or journal['bundle'].get('preferences') != DEFAULTS):
                raise RuntimeError('checkpoint does not retain a self-contained creation journal')
            fault_identity = journal['bundle']['profile']
            if fault_identity.get('display_name') != 'Recovery profile': raise RuntimeError('journal identity mismatch')
            canonical_uuid(fault_identity.get('id'))
            if fault_identity['id'] in seen_ids: raise RuntimeError('creation UUID reused')
            seen_ids.add(fault_identity['id'])
            if journal.get('profile_id') != fault_identity['id']: raise RuntimeError('journal marker identity mismatch')
            if checkpoint == 'published':
                absent(stopped, probe_home.removeprefix('/fat/') + '/.profile-import-pending')
                if stopped.read(probe_home.removeprefix('/fat/') + '/profile.id') != b'srvros-profile-v1\n' + fault_identity['id'].encode() + b'\n':
                    raise RuntimeError('published checkpoint lacks published identity')
            with c.VM(probe, disks['transport'], out, 'recover-' + checkpoint, args) as vm:
                recovery(vm, probe_home, fault_identity); vm.finish()
            if d.outside_data(probe, *d.partitions(probe)[1]) != receipt['installations']['b']['protected_regions']:
                raise RuntimeError('checkpoint modified protected disk bytes')
            fixture = t.check_allocation(probe, out, 'recovered-' + checkpoint)
            actual, binding = empty_profile(fixture, probe_home, 'Recovery profile', machines['b'])
            if actual != fault_identity: raise RuntimeError('recovery changed generated identity')
            legacy_and_binaries(fixture, machines['b'])
            old_identity, old_binding = empty_profile(fixture, SECOND, 'Second profile', machines['b'])
            if old_identity != identity2 or old_binding != binding2:
                raise RuntimeError('recovery changed unrelated created profile')
            imported = 'home/imported_b/'
            if (profile_bundle.decode_json(fixture.read(imported + 'identity.json')) != identity or
                profile_bundle.decode_json(fixture.read(imported + 'profile-local.json')) != imported_binding or
                profile_bundle.decode_json(fixture.read(imported + profile_bundle.PREF_PATH)) != DEFAULTS or
                fixture.read(imported + 'Documents/migration.txt') != TEXT or
                fixture.read(imported + 'Documents/binary.dat') != BINARY):
                raise RuntimeError('recovery changed unrelated imported profile')
            receipt['interruptions'].append({'checkpoint': checkpoint, 'identity': actual, 'binding': binding,
                'disk': str(probe), 'sha256': d.digest(probe.read_bytes()),
                'journal_inline': True, 'recovered_after_reboot': True})
            t.archive_completed(probe, out); d.save_json(out / 'receipt.json', receipt)
        # Capacity uses 16 synthetic blocked marker slots on a separate owned
        # installation. No application profile or user data is overwritten.
        capacity, cap_receipt = install(volumes['capacity'], out / 'target-capacity')
        if cap_receipt['gpt']['disk_guid'] in guids: raise RuntimeError('capacity GUID is not distinct')
        with c.VM(capacity, disks['transport'], out, 'capacity', args) as vm:
            receipt['capacity_comparison'] = capacity_refusal(vm); vm.finish()
        if d.outside_data(capacity, *d.partitions(capacity)[1]) != cap_receipt['protected_regions']:
            raise RuntimeError('capacity fixture protected bytes changed')
        cap_fixture = t.check_allocation(capacity, out, 'capacity-final')
        absent(cap_fixture, 'home/capacity_new/profile.id')
        legacy_and_binaries(cap_fixture, machines['a'])
        cap_receipt['final_disk_sha256'] = d.digest(capacity.read_bytes())
        receipt['capacity_fixture'] = cap_receipt
        t.archive_completed(capacity, out); t.archive_completed(volumes['capacity'], out)
        # Fresh 640x480 installation comes from B's offline data and is checked
        # independently, without perturbing either primary installation.
        _, (offset, size) = d.partitions(disks['b'])
        small_volume = out / 'small-source.exfat'
        small_volume.write_bytes(d.data_image(disks['b'], offset, size))
        small, small_receipt = install(small_volume, out / 'target-small', small=True)
        if small_receipt['gpt']['disk_guid'] in guids or small_receipt['gpt']['disk_guid'] == cap_receipt['gpt']['disk_guid']:
            raise RuntimeError('small fixture GUID is not distinct')
        with c.VM(small, disks['transport'], out, 'small-create', args, 640, 480) as vm:
            chooser_create(vm, 'Small profile', 'small_profile', keyboard=True); vm.finish()
        if d.outside_data(small, *d.partitions(small)[1]) != small_receipt['protected_regions']:
            raise RuntimeError('small fixture protected bytes changed')
        fixture = t.check_allocation(small, out, 'small-final')
        small_identity, small_binding = empty_profile(fixture, '/fat/home/small_profile', 'Small profile', machines['b'])
        if small_identity['id'] in seen_ids: raise RuntimeError('small creation UUID reused')
        legacy_and_binaries(fixture, machines['b'])
        small_receipt.update(identity=small_identity, binding=small_binding,
                             final_disk_sha256=d.digest(small.read_bytes()))
        receipt['small_fixture'] = small_receipt
        t.archive_completed(small, out); t.archive_completed(small_volume, out)
        for name, home in (('a', A), ('b', B)):
            fixture = t.check_allocation(disks[name], out, name + '-final')
            prefix = home.removeprefix('/fat/') + '/'
            if (profile_bundle.decode_json(fixture.read(prefix + 'identity.json')) != identity or
                fixture.read(prefix + 'profile.id') != b'srvros-profile-v1\n' + identity['id'].encode() + b'\n' or
                profile_bundle.decode_json(fixture.read(prefix + profile_bundle.PREF_PATH)) != DEFAULTS or
                fixture.read(prefix + 'Documents/migration.txt') != TEXT or
                fixture.read(prefix + 'Documents/binary.dat') != BINARY):
                raise RuntimeError(name + ': final imported identity, defaults, or documents differ')
            binding = profile_bundle.decode_json(fixture.read(prefix + 'profile-local.json'))
            if name == 'a' and binding != binding_a: raise RuntimeError('source local binding changed')
            if name == 'b' and (binding['alias'] != 'imported_b' or binding['uid'] == binding_a['uid'] or binding['gid'] == binding_a['gid']):
                raise RuntimeError('import binding is not destination-local')
            for rel, value in (('Documents/unselected.txt', b'synthetic-unselected\n'),
                    ('.config/credential-sentinel.fixture', b'synthetic-not-a-credential\n'),
                    ('.local/share/app-state.fixture', b'synthetic-app-state\n'),
                    ('.cache/runtime-cache.fixture', b'synthetic-cache\n')):
                if name == 'a':
                    if fixture.read(prefix + rel) != value: raise RuntimeError('source exclusion changed: ' + rel)
                else: absent(fixture, prefix + rel)
            absent(fixture, prefix + '.profile-import-pending')
            absent(fixture, home.removeprefix('/fat/') + '.profile-import.json')
            legacy_and_binaries(fixture, machines[name])
            if name == 'b':
                second_identity, second_binding = empty_profile(fixture, SECOND, 'Second profile', machines['b'])
                if second_identity != identity2 or second_binding != binding2:
                    raise RuntimeError('unrelated created profile changed')
            receipt['installations'][name]['profile'] = {'identity': identity, 'binding': binding,
                'choice_sha256': check_choice(fixture, home, identity)}
        receipt['exclusions'] = [{'path': path, 'sha256': d.digest(data),
            'source_preserved': True, 'bundle_absent': path not in payloads, 'destination_absent': True}
            for path, data in (('Documents/unselected.txt', b'synthetic-unselected\n'),
                ('.config/credential-sentinel.fixture', b'synthetic-not-a-credential\n'),
                ('.local/share/app-state.fixture', b'synthetic-app-state\n'),
                ('.cache/runtime-cache.fixture', b'synthetic-cache\n'))]
        if t.check_allocation(disks['transport'], out, 'transport-final').read('transfer.json') != exported:
            raise RuntimeError('transport bundle changed')
        for name, disk in disks.items():
            if d.outside_data(disk, *d.partitions(disk)[1]) != receipt['installations'][name]['protected_regions']:
                raise RuntimeError(name + ': protected disk bytes changed')
            receipt['installations'][name]['final_disk_sha256'] = d.digest(disk.read_bytes())
        receipt['process_lifecycle'] = {'workers': 'reaped and drained in trace', 'qemu': 'owned processes stopped'}
        receipt.update(status='PASS', limits=['disposable QEMU installations only',
            'no authentication or access-control claim', 'no physical media or power-loss acceptance',
            'UUID randomness strength is not asserted', 'external fsck.exfat not run'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        d.save_json(out / 'receipt.json', receipt)
    print(f'profile-create: PASS; evidence {out}', flush=True)


if __name__ == '__main__': main()
