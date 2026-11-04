#!/usr/bin/env python3
"""Exercise cooperative profile switching on a disposable installed QEMU disk.

Requires a source-matched build and an idle VM/build environment. The main
proof keeps one QEMU instance alive across A -> B -> A; all evidence is retained.
"""
import argparse
from pathlib import Path
import re
import shutil
import socket
import subprocess
import tempfile
import time

from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path
import installation_image
import profile_bundle
import profile_chooser_smoke as c
import profile_desktop_smoke as d
import profile_image
import profile_transfer_smoke as t
import source_provenance

ROOT = Path(__file__).resolve().parents[1]
A = '/fat/home/switch_a'
B = '/fat/home/switch_b'
ID_A = '12345678-1234-4234-8234-123456789abc'
ID_B = '87654321-4321-4321-8321-cba987654321'
SAVED = b'Switch draft survives'
CLIP = b'private synthetic clipboard'
PASTE = b'B clean clipboard'


def observe(vm, pattern, start, timeout=120):
    return c.observe(vm, pattern, start, timeout)


def pump(vm, seconds=1):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if vm.vm.poll() is not None: raise RuntimeError('QEMU exited unexpectedly')
        try:
            chunk = vm.serial.recv(16384)
            if not chunk: raise RuntimeError('serial disconnected')
            vm.output.extend(chunk)
            (vm.out / f'serial-{vm.label}.log').write_bytes(vm.output)
        except socket.timeout:
            pass


def launch(vm, query, marker):
    app_id = {'Files': 1, 'Text': 2, 'Terminal': 6, 'Clock': 9, 'Sketch': 10, 'Profiles': 13}[query]
    start = len(vm.output)
    vm.key('meta_l-spc'); t.text(vm, query + ' new app'); vm.key('ret')
    observe(vm, rb'deskd: launcher application=' + str(app_id).encode(), start)
    spawned = observe(vm, rb'desktop-launch: app=' + str(app_id).encode() + rb' pid=(\d+) ', start)
    pid = spawned.group(1)
    observe(vm, marker, start)
    opened = observe(vm, rb'deskd: opened pid=' + pid + rb' window=\d+ id=(\d+) parent=0', start)
    window = opened.group(1)
    geometry = observe(vm, rb'deskd: geometry pid=' + pid + rb' window=' + window +
                       rb' frame=(\d+),(\d+),(\d+),(\d+)', start)
    observe(vm, rb'deskd: commit pid=' + pid + rb' generation=\d+ size=', start)
    return start, pid, window, tuple(map(int, geometry.groups()))


def focus_window(vm, pid, window, frame, *, close_start=None):
    # Text uses owned modal windows; Sketch and Terminal currently draw their
    # dialogs inline. If this close wave opened an owned modal, the compositor
    # redirects root focus to that modal, so verify that exact identity.
    expected = window
    if close_start is not None:
        pump(vm, .5)
        pattern = (rb'deskd: opened pid=' + pid + rb' window=\d+ id=(\d+) parent=' +
                   window + rb'(?:\r?\n|\s)')
        modals = list(re.finditer(pattern, vm.output[close_start:]))
        if modals: expected = modals[-1].group(1)
    x, y, _, _ = frame
    vm.click(x + 50, y + 20)
    pump(vm, .8)
    matches = list(re.finditer(rb'deskd: focused pid=(\d+) window=(\d+) identity=\d+', vm.output))
    if not matches or matches[-1].group(1, 2) != (pid, expected):
        raise RuntimeError('intended app or owned modal window did not receive focus')


def switch_request(vm):
    # In the system menu Return to console is last; the disabled Cancel item
    # is skipped, so Up selects Switch Profile. This is a real menu action.
    start = len(vm.output)
    # Submit ordered press/release edges together so accepted I/O is still
    # in progress when the normal menu action requests cooperative closure.
    events = [{'type': 'key', 'data': {'down': down, 'key': {'type': 'qcode', 'data': key}}}
              for key in ('f10', 'end', 'up', 'ret') for down in (True, False)]
    vm.qmp.call('input-send-event', {'events': events})
    observe(vm, rb'session-ui: phase=closing action=switch', start)
    return start


def canceled(vm, start):
    observe(vm, rb'session-ui: phase=canceled reason=(?:app pid=\d+|user|preferences[^\r\n]*)', start)
    vm.pending_canceled = start


def canceled_boundary(vm):
    # Pump until the next explicit input action, including screenshot time,
    # and audit the entire interval since the canceled close wave began.
    if not hasattr(vm, 'pending_canceled'): raise RuntimeError('no cancellation to audit')
    pump(vm, 1)
    interval = vm.output[vm.pending_canceled:]
    if re.search(rb'session-ui: phase=committed action=switch|session: phase=(?:retired|revoked|chooser)', interval):
        raise RuntimeError('canceled close wave committed or retired the session')
    del vm.pending_canceled


def transition(vm, start, token, pid, *, held=False, probe_pid=None):
    token, pid = str(token).encode(), str(pid).encode()
    committed = observe(vm, rb'session-ui: phase=committed action=switch', start)
    retired = observe(vm, rb'session: phase=retired token=' + token +
                      rb' pid=' + pid + rb' status=42 console_mute=0 unmute=0', committed.end())
    revoked = observe(vm, rb'session: phase=revoked token=' + token + rb' pid=' + pid, retired.end())
    begin = revoked.end()
    if held:
        waiting = observe(vm, rb'session: phase=waiting token=' + token + rb' pid=' + pid + rb' members=([1-9]\d*)', begin)
        if probe_pid is None: raise RuntimeError('held transition lacks probe identity')
        release = observe(vm, rb'session-probe: phase=natural-release pid=' + str(probe_pid).encode() + rb' status=17', waiting.end(), 180)
        if b'session: phase=chooser' in vm.output[waiting.end():release.start()]:
            raise RuntimeError('chooser started before the held descendant retired')
        begin = release.end()
    empty = observe(vm, rb'session: phase=empty token=' + token + rb' pid=' + pid + rb' members=0 ', begin, 180)
    released = observe(vm, rb'session: phase=released token=' + token + rb' pid=' + pid, empty.end())
    chooser = observe(vm, rb'session: phase=chooser previous_token=' + token + rb' previous_pid=' + pid, released.end())
    new = observe(vm, rb'session: phase=started token=(\d+) pid=(\d+)', chooser.end())
    if new.group(1) == token: raise RuntimeError('new session reused the old scope token')
    return int(new.group(1)), int(new.group(2)), chooser.end()


def select_profile(vm, home, profile_id, start):
    observe(vm, rb'profile-chooser: ready rows=\d+', start)
    index = c.row(vm, home, 'ready', start)
    c.select(vm, index); c.opened(vm, home, start, profile_id)


def save_text(vm, path, start):
    vm.key('ctrl-l'); vm.key('ctrl-a'); t.text(vm, path)
    vm.key('ctrl-s'); observe(vm, rb'picker ready mode=2', start)
    vm.key('ret'); observe(vm, rb'file-result role=text op=2 status=0', start)


def active_io_waves(vm):
    # Keep Terminal open as an explicit cancellation point after each owned
    # operation retires. This allows both races to run in the same A session.
    terminal, term_pid, term_window, term_frame = launch(vm, 'Terminal', rb'deskterm: ready shell=\d+')
    files, file_pid, file_window, frame = launch(vm, 'Files', rb'desktools: starting role=files')
    vm.key('ctrl-l'); vm.key('ctrl-a'); t.text(vm, A + '/Documents'); vm.key('ret')
    observe(vm, rb'files-location path=' + re.escape((A + '/Documents').encode()), files)
    observe(vm, rb'file-result role=files op=3 status=0', files)
    t.text(vm, 'bulk'); x, y, w, h = frame
    vm.click(x + 1 + 545, y + 44 + 30)
    observe(vm, rb'files-prompt operation=7 name=bulk.bin', files)
    accepted = len(vm.output); vm.key('ret')
    observe(vm, rb'desktools: file-submitted role=files op=7 id=\d+', accepted)
    ending = switch_request(vm)
    waited = observe(vm, rb'window-lifecycle: close-wait role=1 window=' + file_window +
                     rb' busy=1 operation=7', ending)
    copied = observe(vm, rb'file-result role=files op=7 status=0', waited.end(), 180)
    refreshed = observe(vm, rb'file-result role=files op=3 status=0', copied.end(), 180)
    retired = observe(vm, rb'deskd: retired pid=' + file_pid + rb'(?:\r?\n|\s)', refreshed.end(), 180)
    if b'session-ui: phase=committed action=switch' in vm.output[ending:retired.end()]:
        raise RuntimeError('switch committed before accepted Files copy retired')
    focus_window(vm, term_pid, term_window, term_frame, close_start=ending); vm.key('esc'); canceled(vm, ending)
    vm.screenshot('files-close-wait'); canceled_boundary(vm)

    profiles, profile_pid, profile_window, profile_frame = launch(vm, 'Profiles', rb'profile-ui: started pid=\d+')
    x, y, w, h = profile_frame
    client = (x + 1, y + 44, w - 2, h - 45)
    t.click_client(vm, client, client[2] // 2, 25)
    t.select_file(vm, client, (45, 105), A + '/Documents', 'profile-io.bin', 'open')
    t.select_file(vm, client, (60, 64), '/work', 'io-export.json', 'save')
    preview = len(vm.output); t.click_client(vm, client, client[2] - 60, client[3] - 54)
    observe(vm, rb'profile-ui: state=reviewing operation=export result=ready', preview)
    mutation = len(vm.output); t.click_client(vm, client, client[2] - 60, client[3] - 54)
    observe(vm, rb'profile-ui: state=transferring operation=export result=started', mutation)
    ending = switch_request(vm)
    waiting = observe(vm, rb'profile-ui: state=closing operation=export result=waiting', ending)
    worker = observe(vm, rb'profile-ui: state=worker operation=export result=reaped status=0 stdout_eof=1 stderr_eof=1', waiting.end(), 180)
    complete = observe(vm, rb'profile-ui: state=complete operation=export result=success', worker.end(), 180)
    exited = observe(vm, rb'profile-ui: exited pid=' + profile_pid + rb'(?:\r?\n|\s)', complete.end(), 180)
    if b'session-ui: phase=committed action=switch' in vm.output[ending:exited.end()]:
        raise RuntimeError('switch committed before accepted Profiles export retired')
    focus_window(vm, term_pid, term_window, term_frame, close_start=ending); vm.key('esc'); canceled(vm, ending)
    vm.screenshot('profiles-close-wait'); canceled_boundary(vm)
    type_shell(vm, 'exit'); observe(vm, rb'deskterm: shell exited status=0', terminal)
    vm.key('ctrl-shift-w'); observe(vm, rb'deskterm: exited failure=0', terminal)



def text_cancel_and_save(vm):
    start = len(vm.output)
    observe(vm, rb'desktools: starting role=text', 0)
    t.text(vm, 'Switch draft')
    first = switch_request(vm)
    observe(vm, rb'deskd: opened pid=\d+ window=\d+ id=\d+ parent=\d+', first)
    vm.screenshot('text-switch-prompt'); vm.key('esc'); canceled(vm, first)
    canceled_boundary(vm); t.text(vm, ' survives')
    second = switch_request(vm)
    observe(vm, rb'deskd: opened pid=\d+ window=\d+ id=\d+ parent=\d+', second)
    vm.key('shift-tab'); vm.key('shift-tab'); vm.key('ret')
    observe(vm, rb'picker ready mode=2', second); vm.key('esc'); canceled(vm, second)
    # Reopen the close flow and confirm that a refused write remains in A.
    canceled_boundary(vm); vm.key('ctrl-l'); vm.key('ctrl-a'); t.text(vm, '/fat/bin')
    failed = switch_request(vm)
    observe(vm, rb'deskd: opened pid=\d+ window=\d+ id=\d+ parent=\d+', failed)
    vm.key('shift-tab'); vm.key('shift-tab'); vm.key('ret')
    observe(vm, rb'picker ready mode=2', failed); vm.key('ret')
    observe(vm, rb'file-result role=text op=2 status=[1-9]\d*', failed)
    canceled(vm, failed); vm.screenshot('text-failed-save')
    canceled_boundary(vm); save_text(vm, A + '/Documents/switch-saved.txt', len(vm.output))
    # Keep a known secret-free clipboard marker; B will paste into a blank
    # document and save the result, proving old clipboard bytes are absent.
    vm.key('ctrl-n'); t.text(vm, CLIP.decode()); vm.key('ctrl-a'); vm.key('ctrl-c')
    observe(vm, rb'clipboard role=2 action=8 bytes=' + str(len(CLIP)).encode(), start)
    # Clear the scratch Text without affecting the saved file.
    vm.key('ctrl-a'); vm.key('backspace')
    vm.key('ctrl-w')
    vm.key('shift-tab'); vm.key('ret')
    observe(vm, rb'desktools: exited role=text failure=0', start)


def sketch_cancel(vm):
    start, pid, window, frame = launch(vm, 'Sketch', rb'desksketch: starting')
    x, y, w, h = frame
    vm.click(x + w // 2, y + 44 + min(180, h - 100))
    observe(vm, rb'desksketch: stroke revision=\d+ tool=0', start)
    first = switch_request(vm); observe(vm, rb'desksketch: confirm operation=\d+', first)
    vm.screenshot('sketch-switch-prompt'); vm.key('esc'); canceled(vm, first)
    return pid, window, frame


def type_shell(vm, value):
    keys = {' ': 'spc', '/': 'slash', '.': 'dot', '-': 'minus', '_': 'minus', '>': 'dot'}
    for ch in value:
        vm.qmp.key(keys.get(ch, ch.lower()), ch.isupper() or ch in '_>')
    vm.key('ret')


def context_commands(vm, label):
    type_shell(vm, 'pwd > /work/' + label + '-pwd')
    type_shell(vm, 'env > /work/' + label + '-env')


def clock_notice(vm):
    start, _, _, _ = launch(vm, 'Clock', rb'deskclock: starting')
    vm.key('ctrl-1'); vm.key('tab'); vm.key('ctrl-a'); t.text(vm, '1'); vm.key('ctrl-ret')
    observe(vm, rb'deskclock: timer finished', start, 30)
    observe(vm, rb'deskclock: notification result=1', start)
    vm.key('meta_l-shift-n'); observe(vm, rb'notifications opened count=1', start)
    vm.screenshot('a-notification'); vm.key('esc'); vm.key('ctrl-w')
    observe(vm, rb'deskclock: exited', start)


def clipboard_is_empty(vm, start):
    observe(vm, rb'desktools: starting role=text', start)
    vm.key('ctrl-v')
    observe(vm, rb'clipboard role=2 action=32 bytes=0', start)
    t.text(vm, PASTE.decode())
    save_text(vm, B + '/Documents/clipboard-check.txt', start)
    vm.screenshot('b-clipboard-empty')


def menu_cancel_console(vm, token, pid):
    token, pid = str(token).encode(), str(pid).encode()
    start = len(vm.output); vm.key('ctrl-alt-esc')
    closing = observe(vm, rb'session-ui: phase=closing action=console', start)
    committed = observe(vm, rb'session-ui: phase=committed action=console', closing.end())
    retired = observe(vm, rb'session: phase=retired token=' + token + rb' pid=' + pid + rb' status=43', committed.end())
    revoked = observe(vm, rb'session: phase=revoked token=' + token + rb' pid=' + pid, retired.end())
    empty = observe(vm, rb'session: phase=empty token=' + token + rb' pid=' + pid + rb' members=0 ', revoked.end(), 120)
    observe(vm, rb'session: phase=released token=' + token + rb' pid=' + pid, empty.end())
    vm.wait(b'\nsrv> ')


def fixture(out, receipt):
    base = (ROOT / 'build/srvros.exfat').resolve()
    driver = (ROOT / 'build/tests/profile-image-driver').resolve()
    machine = d.machine_preferences(4, -300)
    bundle_a = d.make_bundle()
    bundle_b = d.make_bundle()
    bundle_b['profile'] = {'id': ID_B, 'display_name': 'Switch profile B'}
    bundle_b['preferences'] = {'text_size': 20, 'clock_flags': 1, 'wallpaper': 1,
                               'theme_accent': 2, 'glass_level': 0, 'theme_style': 1}
    first = out / 'profile-a.exfat'; second = out / 'profiles.exfat'
    profile_image.provision(base, first, bundle_a, A, 'switch_a', 1001, 1001, driver, machine)
    profile_image.provision(first, second, bundle_b, B, 'switch_b', 2001, 2001, driver)
    disks = {}
    receipt['installations'] = {}
    for name, volume in (('system', second), ('transport', base)):
        stage = out / ('target-' + name)
        installation_image.prepare(stage, (ROOT / 'build/kernel/srvros.elf').resolve(),
            (ROOT / 'build/limine-binary/BOOTX64.EFI').resolve(), volume,
            (ROOT / 'build/userspace/init.elf').resolve(), identity_bound=True)
        ready = installation_image.apply(stage)
        disk = stage / 'image.img'; disks[name] = disk
        _, (offset, size) = d.partitions(disk)
        receipt['installations'][name] = {'disk': str(disk), 'gpt': ready['gpt'],
            'initial_disk_sha256': d.digest(disk.read_bytes()),
            'protected_regions': d.outside_data(disk, offset, size)}
        t.check_allocation(disk, out, name + '-initial')
    if receipt['installations']['system']['gpt']['disk_guid'] == receipt['installations']['transport']['gpt']['disk_guid']:
        raise RuntimeError('disk GUIDs are not distinct')
    return disks, machine, bundle_a, bundle_b


def check_context(data, home, profile_id):
    try:
        lines = data.decode('ascii').splitlines()
        values = {}
        for line in lines:
            if '=' not in line: raise ValueError('malformed environment line')
            key, value = line.split('=', 1)
            if key in values: raise ValueError('duplicate environment key')
            values[key] = value
    except (UnicodeDecodeError, ValueError) as error:
        raise RuntimeError('invalid terminal environment capture') from error
    expected = {'HOME': home, 'SRVROS_DESKTOP_HOME': home, 'SRVROS_PROFILE_ID': profile_id,
                'XDG_CONFIG_HOME': home + '/.config', 'XDG_DATA_HOME': home + '/.local/share',
                'XDG_CACHE_HOME': home + '/.cache'}
    if any(values.get(k) != v for k, v in expected.items()):
        raise RuntimeError('terminal environment differs from active profile')
    return {key: values[key] for key in expected}


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
    base = (ROOT / 'build/review/profile-switch').resolve(); base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'Profile switch evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'source': source_provenance.capture(ROOT),
        'firmware_sha256': d.digest(Path(args.ovmf).read_bytes()),
        'firmware_vars_sha256': d.digest(Path(args.ovmf_vars).read_bytes()),
        'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0],
        'tool_sha256': {name: d.digest((ROOT / 'tools' / name).read_bytes()) for name in
            ('profile_switch_smoke.py', 'profile_chooser_smoke.py', 'profile_guest_smoke.py',
             'profile_desktop_smoke.py', 'profile_transfer_smoke.py', 'profile_image.py',
             'profile_bundle.py', 'installation_image.py')}}
    d.save_json(out / 'receipt.json', receipt)
    try:
        receipt['build'] = d.verified_build(ROOT)
        volume = ((ROOT / 'build/srvros.exfat').resolve()).stat().st_size
        disk_size = installation_image.layout(volume)['size_bytes']
        required = 4 * disk_size + 8 * volume + t.ARCHIVE_BUDGET + 1024**3
        free = shutil.disk_usage(out).free
        receipt['space_preflight'] = {'required_bytes': required, 'available_bytes': free,
            'disk_allowance': 4, 'volume_allowance': 8, 'archive_budget_bytes': t.ARCHIVE_BUDGET,
            'reserve_bytes': 1024**3}
        if free < required: raise RuntimeError('insufficient free space for proof and 1 GiB reserve')
        disks, machine, bundle_a, bundle_b = fixture(out, receipt)
        d.save_json(out / 'receipt.json', receipt)
        with c.VM(disks['system'], disks['transport'], out, 'single-boot', args) as vm:
            vm_pid = vm.vm.pid
            vm.run('/fat/bin/dd if=/dev/zero of=' + A + '/Documents/bulk.bin bs=65536 count=256')
            vm.run('/fat/bin/dd if=/dev/zero of=' + A + '/Documents/profile-io.bin bs=65536 count=16')
            start = len(vm.output); vm.launch('--profile=' + A + ' --apps-trace --scale=1')
            initial = observe(vm, rb'session: phase=started token=(\d+) pid=(\d+)', start)
            token_a, pid_a = map(int, initial.groups())
            observe(vm, rb'deskd: profile id=' + ID_A.encode() + rb' home=' + re.escape(A.encode()), start)
            observe(vm, rb'deskd: commit', start)
            active_io_waves(vm)
            launch(vm, 'Text', rb'desktools: starting role=text')
            text_cancel_and_save(vm)
            clock_notice(vm)
            sketch_pid, sketch_window, sketch_frame = sketch_cancel(vm)
            canceled_boundary(vm)
            terminal, term_pid, term_window, term_frame = launch(vm, 'Terminal', rb'deskterm: ready shell=\d+')
            context_commands(vm, 'a')
            # A cooperative close wave can partially close Sketch before the
            # Terminal declines; already closed apps are not reconstructed.
            partial = switch_request(vm)
            observe(vm, rb'desksketch: confirm operation=\d+', partial)
            focus_window(vm, sketch_pid, sketch_window, sketch_frame, close_start=partial)
            vm.key('tab'); vm.key('ret')
            observe(vm, rb'desksketch: exited failure=0', partial)
            focus_window(vm, term_pid, term_window, term_frame, close_start=partial); vm.key('esc'); canceled(vm, partial)
            vm.screenshot('terminal-cancel-partial'); canceled_boundary(vm)
            # Recreate a dirty Sketch, then spawn a naturally releasing orphan
            # last. Scope waiting must be observed before the timer expires.
            sketch, sketch_pid, sketch_window, sketch_frame = launch(vm, 'Sketch', rb'desksketch: starting')
            sx, sy, sw, sh = sketch_frame
            vm.click(sx + sw // 2, sy + 44 + min(180, sh - 100))
            observe(vm, rb'desksketch: stroke revision=\d+ tool=0', sketch)
            focus_window(vm, term_pid, term_window, term_frame)
            type_shell(vm, 'lifetimeprobe --switch-orphan')
            probe_parent = observe(vm, rb'session-probe: phase=parent-exit child=(\d+)', sketch)
            probe = observe(vm, rb'session-probe: phase=ready pid=(\d+) hold_us=60000000', sketch)
            probe_pid = int(probe.group(1))
            if probe_parent.group(1) != probe.group(1):
                raise RuntimeError('released probe is not the parent-reported orphan')
            ending = switch_request(vm)
            observe(vm, rb'desksketch: confirm operation=\d+', ending)
            focus_window(vm, sketch_pid, sketch_window, sketch_frame, close_start=ending)
            vm.key('tab'); vm.key('tab'); vm.key('ret')
            observe(vm, rb'desksketch: picker ready mode=2', ending)
            vm.key('ctrl-a'); t.text(vm, 'switch-sketch.bmp'); vm.key('ret')
            observe(vm, rb'desksketch: saved success=1 status=0', ending, 120)
            observe(vm, rb'desksketch: exited failure=0', ending)
            focus_window(vm, term_pid, term_window, term_frame, close_start=ending)
            vm.key('shift-tab'); vm.key('ret')
            observe(vm, rb'deskterm: close accepted shell=\d+', ending)
            observe(vm, rb'deskterm: shell retired shell=\d+ status=\d+', ending)
            token_b, pid_b, chooser_start = transition(vm, ending, token_a, pid_a, held=True, probe_pid=probe_pid)
            select_profile(vm, B, ID_B, chooser_start)
            b_start = chooser_start
            observe(vm, rb'deskd: preferences revision=\d+ status=\d+ text=20 offset=-300 flags=1 wallpaper=1 .*theme_accent=2 glass_level=0 theme_style=1', b_start)
            vm.key('meta_l-shift-n'); observe(vm, rb'notifications opened count=0', b_start)
            vm.screenshot('b-notifications-empty'); vm.key('esc')
            clipboard_is_empty(vm, b_start)
            # Close the saved Text and inspect B's real shell environment/cwd.
            vm.key('ctrl-w'); observe(vm, rb'desktools: exited role=text failure=0', b_start)
            terminal_b, _, _, _ = launch(vm, 'Terminal', rb'deskterm: ready shell=\d+')
            context_commands(vm, 'b'); type_shell(vm, 'exit')
            observe(vm, rb'deskterm: shell exited status=0', terminal_b)
            vm.key('ctrl-shift-w'); observe(vm, rb'deskterm: exited failure=0', terminal_b)
            ending_b = switch_request(vm)
            token_a2, pid_a2, chooser_a = transition(vm, ending_b, token_b, pid_b)
            select_profile(vm, A, ID_A, chooser_a)
            observe(vm, rb'deskd: preferences revision=\d+ status=\d+ text=16 offset=-300 flags=3 wallpaper=0 .*theme_accent=0 glass_level=1 theme_style=0', chooser_a)
            observe(vm, rb'desktools: starting role=text', chooser_a)
            vm.key('ctrl-l'); vm.key('ctrl-a'); t.text(vm, A + '/Documents/switch-saved.txt')
            vm.key('ctrl-o'); observe(vm, rb'picker ready mode=1', chooser_a)
            t.text(vm, 'switch-saved'); vm.key('ret')
            observe(vm, rb'file-result role=text op=1 status=0', chooser_a)
            vm.key('ctrl-a'); vm.key('ctrl-c')
            observe(vm, rb'clipboard role=2 action=8 bytes=' + str(len(SAVED)).encode(), chooser_a)
            vm.screenshot('a-return-document')
            vm.key('ctrl-w'); observe(vm, rb'desktools: exited role=text failure=0', chooser_a)
            terminal_a2, _, _, _ = launch(vm, 'Terminal', rb'deskterm: ready shell=\d+')
            context_commands(vm, 'a-return'); type_shell(vm, 'exit')
            observe(vm, rb'deskterm: shell exited status=0', terminal_a2)
            vm.key('ctrl-shift-w'); observe(vm, rb'deskterm: exited failure=0', terminal_a2)
            if vm.vm.pid != vm_pid: raise RuntimeError('QEMU changed during same-boot switches')
            menu_cancel_console(vm, token_a2, pid_a2)
            vm.finish()
        receipt['sessions'] = {'qemu_pid': vm_pid, 'tokens': [token_a, token_b, token_a2],
                               'root_pids': [pid_a, pid_b, pid_a2], 'same_boot_a_b_a': True}
        # Exercise keyboard-only cancellation on a separate 640x480 identity-
        # bound copy. Its framebuffer size is independently checked in screenshots.
        _, (offset, size) = d.partitions(disks['system'])
        small_volume = out / 'small-source.exfat'
        small_volume.write_bytes(d.data_image(disks['system'], offset, size))
        stage = out / 'target-small'; original = installation_image.CONFIG
        small_config = original.replace(b'resolution: 1280x800', b'resolution: 640x480')
        if small_config == original: raise RuntimeError('small display override unavailable')
        try:
            installation_image.CONFIG = small_config
            plan = installation_image.prepare(stage, (ROOT / 'build/kernel/srvros.elf').resolve(),
                (ROOT / 'build/limine-binary/BOOTX64.EFI').resolve(), small_volume,
                (ROOT / 'build/userspace/init.elf').resolve(), identity_bound=True)
            ready = installation_image.apply(stage)
        finally:
            installation_image.CONFIG = original
        small = stage / 'image.img'; config = plan['config'].encode('ascii')
        if b'resolution: 640x480' not in config or (stage / 'limine.conf').read_bytes() != config:
            raise RuntimeError('small installation resolution mismatch')
        guid = ready['gpt']['disk_guid']
        if guid in {v['gpt']['disk_guid'] for v in receipt['installations'].values()}:
            raise RuntimeError('small fixture GUID is not distinct')
        bounds = d.outside_data(small, *d.partitions(small)[1])
        with c.VM(small, disks['transport'], out, 'small', args, 640, 480) as vm:
            start = len(vm.output); vm.launch('--profile=' + A + ' --apps-trace --scale=1')
            small_session = observe(vm, rb'session: phase=started token=(\d+) pid=(\d+)', start)
            small_token, small_pid = map(int, small_session.groups())
            observe(vm, rb'desktools: starting role=text', start)
            observe(vm, rb'deskd: display physical=640x480 logical=640x480 scale=1', start)
            observe(vm, rb'deskd: commit', start)
            t.text(vm, 'compact cancel')
            attempt = switch_request(vm)
            observe(vm, rb'deskd: opened pid=\d+ window=\d+ id=\d+ parent=\d+', attempt)
            vm.screenshot('small-keyboard-cancel'); vm.key('esc'); canceled(vm, attempt)
            canceled_boundary(vm); vm.key('ctrl-w'); vm.key('shift-tab'); vm.key('ret')
            observe(vm, rb'desktools: exited role=text failure=0', attempt)
            small_switch = switch_request(vm)
            next_token, next_pid, small_chooser = transition(vm, small_switch, small_token, small_pid)
            vm.screenshot('small-chooser')
            select_profile(vm, B, ID_B, small_chooser)
            vm.screenshot('small-profile-b')
            menu_cancel_console(vm, next_token, next_pid); vm.finish()
        if d.outside_data(small, *d.partitions(small)[1]) != bounds:
            raise RuntimeError('small fixture protected bytes changed')
        t.check_allocation(small, out, 'small-final')
        receipt['small_fixture'] = {'disk': str(small), 'gpt': ready['gpt'],
            'config': config.decode('ascii'), 'config_sha256': d.digest(config),
            'protected_regions': bounds, 'final_disk_sha256': d.digest(small.read_bytes()),
            'keyboard_cancel': True, 'keyboard_switch_to_b': True}
        t.archive_completed(small, out); t.archive_completed(small_volume, out)
        fixture_final = t.check_allocation(disks['system'], out, 'system-final')
        for home, bundle, binding in ((A, bundle_a, {'alias': 'switch_a', 'uid': 1001, 'gid': 1001}),
                                      (B, bundle_b, {'alias': 'switch_b', 'uid': 2001, 'gid': 2001})):
            prefix = home.removeprefix('/fat/') + '/'
            if (profile_bundle.decode_json(fixture_final.read(prefix + 'identity.json')) != bundle['profile'] or
                profile_bundle.decode_json(fixture_final.read(prefix + 'profile-local.json')) != binding or
                profile_bundle.decode_json(fixture_final.read(prefix + profile_bundle.PREF_PATH)) != bundle['preferences'] or
                fixture_final.read(prefix + 'Documents/migration.txt') != d.SOURCE or
                fixture_final.read(prefix + 'Documents/binary.dat') != d.BINARY):
                raise RuntimeError('profile identity, settings, or original data changed: ' + home)
        if (fixture_final.read('home/switch_a/Documents/bulk.bin') != b'\0' * (16 * 1024 * 1024) or
            fixture_final.read('home/switch_a/Documents/bulk copy.bin') != b'\0' * (16 * 1024 * 1024) or
            fixture_final.read('home/switch_a/Documents/profile-io.bin') != b'\0' * (1024 * 1024) or
            fixture_final.read('home/switch_a/Documents/switch-saved.txt') != SAVED or
            fixture_final.read('home/switch_b/Documents/clipboard-check.txt') != PASTE or
            not fixture_final.read('home/switch_a/switch-sketch.bmp').startswith(b'BM') or
            fixture_final.read('home/desktop.conf') != machine or
            fixture_final.read('home/.desktop-profile') != c.expected_choice(A, ID_A)):
            raise RuntimeError('save/discard, clipboard, machine settings, or remembered choice differs')
        transport = t.check_allocation(disks['transport'], out, 'transport-final')
        io_bundle = profile_bundle.decode_json(transport.read('io-export.json'))
        if (io_bundle['profile'] != bundle_a['profile'] or io_bundle['preferences'] != bundle_a['preferences'] or
            profile_bundle.validate_bundle(io_bundle) != {'Documents/profile-io.bin': b'\0' * (1024 * 1024)}):
            raise RuntimeError('accepted Profiles export bundle differs')
        receipt['accepted_io'] = {'files_bytes': 16 * 1024 * 1024,
            'profile_bytes': 1024 * 1024, 'profile_bundle_sha256': d.digest(transport.read('io-export.json')),
            'close_wait_and_retirement_before_commit': True}
        receipt['contexts'] = {}
        for label, home in (('a', A), ('b', B), ('a-return', A)):
            if transport.read(label + '-pwd') != (home + '\n').encode(): raise RuntimeError('terminal cwd mismatch')
            receipt['contexts'][label] = check_context(transport.read(label + '-env'), home, ID_B if label == 'b' else ID_A)
        for binary in ('deskd', 'desktools', 'deskterm', 'desksketch', 'deskclock', 'lifetimeprobe', 'profile'):
            if fixture_final.read('bin/' + binary) != runtime_elf((ROOT / f'build/userspace/{binary}.elf').read_bytes()):
                raise RuntimeError('installed binary differs from matched build: ' + binary)
        for name, disk in disks.items():
            if d.outside_data(disk, *d.partitions(disk)[1]) != receipt['installations'][name]['protected_regions']:
                raise RuntimeError(name + ': protected disk bytes changed')
            receipt['installations'][name]['final_disk_sha256'] = d.digest(disk.read_bytes())
        receipt.update(status='PASS', limits=['disposable QEMU disks only',
            'same-boot switch proof uses one installation; cross-install migration is covered separately',
            'no authentication, physical media, or power-loss acceptance', 'external fsck.exfat not run'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        d.save_json(out / 'receipt.json', receipt)
    print(f'profile-switch: PASS; evidence {out}', flush=True)


if __name__ == '__main__': main()
