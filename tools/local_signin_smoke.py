#!/usr/bin/env python3
"""Exercise opt-in console sign-in on one owned disposable installed disk.

Requires a matching built tree. The fixture changes its own ESP only while
QEMU is stopped and never discovers or opens physical media.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import tempfile
import time

from desktop_foundation_smoke import QMP
from displayd_frame_smoke import choose_port, connect_serial, stop_process
from exfat_test_image import ExfatFixture
import installation_image as installation
import installed_disk_smoke as proof
from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path

ROOT = Path(__file__).resolve().parents[1]
ACCOUNT_PATH = '/fat/etc/local-auth/accounts'
# Synthetic test credentials only; they are never written to host evidence.
PASSWORDS = {'alpha': b'fixture-alpha-pass-91', 'beta': b'fixture-beta-pass-72',
             'disabled': b'fixture-disabled-pass-63'}
WRONG = b'fixture-incorrect-pass-54'
RESET_ALPHA = b'fixture-reset-alpha-pass-23'
RESET_DISABLED = b'fixture-reset-disabled-pass-36'
RESET_BEFORE = b'fixture-reset-before-pass-48'
RESET_AFTER = b'fixture-reset-after-pass-59'
ENABLE_PASSWORD = b'fixture-suspended-reset-pass-81'
SECRETS = tuple(PASSWORDS.values()) + (WRONG, RESET_ALPHA, RESET_DISABLED, RESET_BEFORE, RESET_AFTER, ENABLE_PASSWORD)


def save_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def prepare_stage(stage, mode, root_guid=None, volume=None):
    # prepare and apply both validate identity_config; keep the temporary
    # override in scope through both calls and always restore the module.
    original = installation.identity_config
    original_esp = installation.ESP_MIB
    def fixture_config(guid):
        config = original(root_guid or guid)
        before = b' srvros.root-role=system\n'
        after = b' srvros.root-role=system srvros.auth=' + mode.encode('ascii') + b'\n'
        if config.count(before) != 1:
            raise RuntimeError('unexpected installation configuration')
        return config.replace(before, after)
    try:
        installation.identity_config = fixture_config
        # Leave space for both owned images and the saved ESP on the build host.
        # 288 MiB still exceeds FAT32's minimum cluster count at 4 KiB.
        installation.ESP_MIB = 288
        plan = installation.prepare(stage, ROOT / 'build/kernel/srvros.elf',
            ROOT / 'build/limine-binary/BOOTX64.EFI', volume or ROOT / 'build/srvros.exfat',
            ROOT / 'build/userspace/init.elf', identity_bound=True)
        ready = installation.apply(stage)
    finally:
        installation.identity_config = original
        installation.ESP_MIB = original_esp
    if ('srvros.auth=' + mode) not in plan['config']:
        raise RuntimeError('fixture configuration missing requested mode')
    if root_guid and ('srvros.root=' + root_guid) not in plan['config']:
        raise RuntimeError('fixture configuration bound to wrong data partition')
    return plan, ready


def region_hash(path, offset, size):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        stream.seek(offset)
        while size:
            data = stream.read(min(size, 1024 * 1024))
            if not data: raise RuntimeError('short image region')
            h.update(data); size -= len(data)
    return h.hexdigest()


def copy_esp(source, target):
    source_esp, _ = proof.partitions(source)
    target_esp, _ = proof.partitions(target)
    if source_esp != target_esp:
        raise RuntimeError('fixture ESP layouts differ')
    offset, size = source_esp
    with source.open('rb') as src, target.open('r+b') as dst:
        src.seek(offset); dst.seek(offset); remaining = size
        while remaining:
            data = src.read(min(remaining, 1024 * 1024))
            if not data: raise RuntimeError('short fixture ESP')
            dst.write(data); remaining -= len(data)
        dst.flush()
        import os
        os.fsync(dst.fileno())
    if region_hash(source, offset, size) != region_hash(target, offset, size):
        raise RuntimeError('fixture ESP copy mismatch')


def account_bytes(disk):
    _, (offset, size) = proof.partitions(disk)
    return ExfatFixture(proof.data_image(disk, offset, size)).read('etc/local-auth/accounts')


def account_rows(raw):
    lines = raw.splitlines()
    if not lines or lines[0] != b'SRVROS-LOCAL-AUTH-1':
        raise RuntimeError('unexpected committed accounts format')
    rows = {}
    for line in lines[1:]:
        fields = line.split(b' ')
        if len(fields) != 6 or fields[0] in rows:
            raise RuntimeError('invalid committed account row')
        rows[fields[0]] = fields
    return rows


def reset_record_matches(before, after):
    return (before[:4] == after[:4] and
            before[4] != after[4] and before[5] != after[5])


def require_enabled_transition(before, after, name, old, new):
    previous, current = account_rows(before), account_rows(after)
    key = name.encode('ascii')
    if previous.keys() != current.keys() or key not in previous:
        raise RuntimeError('enable transition changed account set')
    expected = list(previous[key])
    if expected[3] != old: raise RuntimeError('unexpected pre-transition enabled state')
    expected[3] = new
    if current[key] != expected or any(previous[k] != current[k] for k in previous if k != key):
        raise RuntimeError('enable transition changed fields other than enabled flag')


def verify_persisted_files(disk, partition):
    image = ExfatFixture(proof.data_image(disk, *partition))
    for name in ('alpha', 'beta'):
        expected = (name + '-private-persisted').encode()
        if image.read('home/local-' + name + '/evidence.txt') != expected:
            raise RuntimeError(f'{name}: persisted file bytes differ')


def marker_bytes(disk, name):
    _, (offset, size) = proof.partitions(disk)
    fs = ExfatFixture(proof.data_image(disk, offset, size))
    try:
        return fs.read('etc/local-auth/' + name)
    except StopIteration:
        return None


def sidecar_variant(data, variant):
    """Alter one exFAT sidecar in an owned, stopped data image.

    Keep directory checksums and allocation consistent so refusal tests the
    metadata coverage gate, not an unrelated filesystem structure error.
    """
    fs = ExfatFixture(data)
    def entry_in_directory(first, size, contiguous, wanted):
        raw = fs.chain(first, size, contiguous)
        at = 0
        while at + 32 <= len(raw) and raw[at]:
            if raw[at] != 0x85:
                at += 32
                continue
            n = (raw[at + 1] + 1) * 32
            entry = raw[at:at+n]
            names = list(fs.entries(entry))
            if len(names) != 1: raise RuntimeError('invalid fixture entry')
            if names[0][0] == wanted:
                cluster = first
                for _ in range(at // fs.cluster):
                    cluster = cluster + 1 if contiguous else fs.u32(fs.fat + cluster * 4)
                if at % fs.cluster + n > fs.cluster: raise RuntimeError('split fixture entry')
                return fs.offset(cluster) + at % fs.cluster, bytearray(entry)
            at += n
        raise RuntimeError('missing fixture entry: ' + wanted)
    _, directory = entry_in_directory(fs.root, None, False, '.srvros')
    first = struct.unpack_from('<I', directory, 52)[0]
    size = struct.unpack_from('<Q', directory, 56)[0]
    entry_offset, entry = entry_in_directory(first, size, bool(directory[33] & 2), 'meta')
    old = fs.read('.srvros/meta')
    if not old.startswith(b'SRVROSMETA1\n') or not old.endswith(b'\n'):
        raise RuntimeError('fixture sidecar is not valid metadata')
    if variant == 'absent':
        if any(name == 'metx' for name, _ in fs.entries(fs.read('.srvros'))):
            raise RuntimeError('fixture sidecar alternate name already exists')
        if entry[35] != 4 or entry[64] != 0xc1:
            raise RuntimeError('unexpected sidecar name entry')
        entry[66:74] = 'metx'.encode('utf-16le')
        name_hash = 0
        for byte in 'METX'.encode('utf-16le'):
            name_hash = ((((name_hash & 1) << 15) | (name_hash >> 1)) + byte) & 0xffff
        struct.pack_into('<H', entry, 36, name_hash)
    elif variant == 'incomplete':
        records = [line for line in old.splitlines(keepends=True)
                   if line.startswith(b'/fat/bin/local-signin\t')]
        if len(records) != 1:
            raise RuntimeError('fixture does not contain one sign-in executable record')
        # Blank lines are legal and keep the file's length and allocation fixed.
        removed = records[0]
        new = old.replace(removed, b'\n' * len(removed), 1)
        first_data = struct.unpack_from('<I', entry, 52)[0]
        contiguous = bool(entry[33] & 2)
        cluster = first_data
        for at in range(0, len(new), fs.cluster):
            piece = new[at:at + fs.cluster]
            offset = fs.offset(cluster)
            fs.data[offset:offset + len(piece)] = piece
            cluster = cluster + 1 if contiguous else fs.u32(fs.fat + cluster * 4)
    else:
        raise ValueError('unknown sidecar variant')
    checksum = 0
    for i, byte in enumerate(entry):
        if i not in (2, 3): checksum = ((((checksum & 1) << 15) | (checksum >> 1)) + byte) & 0xffff
    struct.pack_into('<H', entry, 2, checksum)
    fs.data[entry_offset:entry_offset + len(entry)] = entry
    if variant == 'incomplete' and fs.read('.srvros/meta') != new:
        raise RuntimeError('sidecar mutation differs from expected bytes')
    if variant == 'absent':
        try: fs.read('.srvros/meta')
        except StopIteration: pass
        else: raise RuntimeError('sidecar still present')
        if fs.read('.srvros/metx') != old: raise RuntimeError('renamed sidecar contents differ')
    return bytes(fs.data)


def write_data(disk, offset, data):
    import os
    with disk.open('r+b') as target:
        target.seek(offset)
        if target.write(data) != len(data): raise RuntimeError('short fixture data write')
        target.flush(); os.fsync(target.fileno())


class VM:
    def __init__(self, disk, out, label, args, mode):
        self.disk, self.out, self.label, self.args, self.mode = disk, out, label, args, mode
        self.output = bytearray(); self.cursor = 0
        self.extra_redactions = []
        self.serial = self.qmp = self.proc = self.log = self.tmp = None

    def __enter__(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='srv-local-signin-', dir='/tmp')
        qmp_path = Path(self.tmp.name) / 'qmp'
        port = choose_port(30001, 38000)
        variables = self.out / ('firmware-' + self.label + '.fd')
        shutil.copyfile(self.args.ovmf_vars, variables)
        cmd = [self.args.qemu, '-machine', 'q35,i8042=off', '-accel', 'tcg', '-m', '1024M',
               '-smp', '1', '-nic', 'none', '-drive', f'if=pflash,format=raw,readonly=on,file={self.args.ovmf}',
               '-drive', f'if=pflash,format=raw,file={variables}', '-device', 'ich9-ahci,id=ahci',
               '-drive', f'if=none,id=installed,format=raw,file={self.disk}',
               '-device', 'ide-hd,drive=installed,bus=ahci.0,bootindex=1',
               '-object', 'rng-random,id=rng0,filename=/dev/urandom',
               '-device', 'virtio-rng-pci,rng=rng0,disable-legacy=on',
               '-vga', 'none', '-device', 'VGA,vgamem_mb=64,xres=1280,yres=800',
               '-device', 'virtio-keyboard-pci,disable-legacy=on',
               '-serial', f'tcp:127.0.0.1:{port},server=on,wait=off',
               '-qmp', f'unix:{qmp_path},server=on,wait=off', '-display', 'none', '-monitor', 'none', '-no-reboot']
        save_json(self.out / ('command-' + self.label + '.json'), cmd)
        self.log = (self.out / ('qemu-' + self.label + '.log')).open('wb')
        try:
            self.proc = subprocess.Popen(cmd, stdout=self.log, stderr=self.log)
            self.serial = connect_serial(port, 20); self.serial.settimeout(.2)
            self.qmp = QMP(qmp_path)
            prompt = {'recovery': b'srv> ', 'local': b'Login: ',
                      'blocked': b'local-auth: sign-in unavailable.',
                      'root-recovery': b'No fallback volume or repair was selected.'}
            self.wait(prompt[self.mode], self.args.boot_wait)
            if self.mode != 'root-recovery' and b'installed-root: selected device=ahci0p2 format=exfat-v1 role=system' not in self.output:
                raise RuntimeError('installed identity-selected root missing')
            if self.mode == 'root-recovery' and b'installed-root: recovery reason=filesystem-recovery' not in self.output:
                raise RuntimeError('strict sidecar refusal reason missing')
            if b'trying initramfs fallback' in self.output:
                raise RuntimeError('unexpected initramfs fallback')
            if self.mode == 'recovery' and (b'local-auth: recovery console' not in self.output or
                                            b'init: readiness=ready' not in self.output):
                raise RuntimeError('explicit recovery did not reach ready administrator console')
            if self.mode == 'local':
                if b'local-auth: console sign-in' not in self.output or b'srv> ' in self.output:
                    raise RuntimeError('local mode did not start restricted sign-in')
            if self.mode == 'root-recovery':
                if any(marker in self.output for marker in (b'Login: ', b'srv> ', b'bootlog: persisted',
                       b'installed-root: selected', b'init: readiness=', b'local-auth: console sign-in')):
                    raise RuntimeError('strict sidecar refusal reached normal boot')
            if self.mode == 'blocked':
                if any(marker in self.output for marker in
                       (b'local-auth: console sign-in', b'Login: ', b'srv> ')):
                    raise RuntimeError('unsafe launch path was exposed')
            return self
        except BaseException:
            self.__exit__(*__import__('sys').exc_info()); raise

    def wait(self, marker, timeout=90):
        deadline = time.monotonic() + timeout
        while marker not in self.output[self.cursor:]:
            if time.monotonic() > deadline or self.proc.poll() is not None:
                raise RuntimeError(f'{self.label}: timed out waiting for {marker!r}')
            try:
                data = self.serial.recv(16384)
                if not data: raise RuntimeError('serial disconnected')
                self.output.extend(data)
            except socket.timeout:
                pass
        pos = self.output.index(marker, self.cursor) + len(marker)
        part = bytes(self.output[self.cursor:pos]); self.cursor = pos
        return part

    def wait_events(self, markers, start, timeout=90):
        deadline = time.monotonic() + timeout
        while not all(marker in self.output[start:] for marker in markers):
            if time.monotonic() > deadline or self.proc.poll() is not None:
                raise RuntimeError(f'{self.label}: missing background process event')
            try:
                chunk = self.serial.recv(16384)
                if not chunk: raise RuntimeError('serial disconnected')
                self.output.extend(chunk)
            except socket.timeout: pass
        self.cursor = len(self.output)
        return bytes(self.output[start:])

    def send(self, line):
        self.serial.sendall(line + b'\n')

    def command(self, command):
        start = len(self.output); self.send(('run ' + command).encode('ascii'))
        self.wait(b' exited status='); self.wait(b'\nsrv> ')
        result = re.search(rb'run: pid=\d+ exited status=(\d+)', self.output[start:])
        if not result or int(result.group(1)):
            raise RuntimeError(f'{self.label}: command failed: {command}')
        return bytes(self.output[start:])

    def command_refused(self, command):
        start = len(self.output); self.send(('run ' + command).encode('ascii'))
        self.wait(b' exited status='); self.wait(b'\nsrv> ')
        segment = bytes(self.output[start:])
        result = re.search(rb'run: pid=\d+ exited status=(\d+)', segment)
        if not result or int(result.group(1)) == 0 or b'New password: ' in segment:
            raise RuntimeError(f'{self.label}: expected command refusal: {command}')
        return segment

    def paused_data_state(self):
        self.qmp.call('stop')
        try:
            if self.qmp.call('query-status').get('status') != 'paused':
                raise RuntimeError('QEMU did not pause before inspection')
            _, (offset, size) = proof.partitions(self.disk)
            stats = next(item['stats'] for item in self.qmp.call('query-blockstats')
                         if item.get('device') == 'installed')
            return region_hash(self.disk, offset, size), stats['wr_operations'], stats['wr_bytes']
        finally:
            self.qmp.call('cont')

    def settled_data_state(self):
        # Boot and executable access can dirty exFAT's metadata sidecar.
        # Measure the recovery command after that work has stopped writing,
        # with no additional sync command inside the measured interval.
        previous = self.paused_data_state()
        unchanged = 0
        for _ in range(6):
            self.command('/fat/bin/sync')
            current = self.paused_data_state()
            unchanged = unchanged + 1 if current == previous else 0
            if unchanged == 2:
                return current
            previous = current
        raise RuntimeError('guest data partition did not settle before inspection')

    def paused_account_snapshot(self):
        self.qmp.call('stop')
        try:
            if self.qmp.call('query-status').get('status') != 'paused':
                raise RuntimeError('QEMU did not pause for account readback')
            return (account_bytes(self.disk), marker_bytes(self.disk, 'accounts.tmp'),
                    marker_bytes(self.disk, 'enroll.lock'))
        finally:
            self.qmp.call('cont')

    def enroll(self, name, uid):
        start = len(self.output)
        self.send(f'run /fat/bin/local-enroll add {name} {uid} {uid}'.encode())
        self.wait(b'New password: '); self.send(PASSWORDS[name])
        self.wait(b'Confirm password: '); self.send(PASSWORDS[name])
        self.wait(b' exited status='); self.wait(b'\nsrv> ')
        result = re.search(rb'run: pid=\d+ exited status=(\d+)', self.output[start:])
        if not result or int(result.group(1)):
            raise RuntimeError(f'{self.label}: enrollment failed: {name}')

    def reset_password(self, name, password, checkpoint=None):
        binary = '/fat/signin-test/reset' if checkpoint else '/fat/bin/local-enroll'
        line = f'run {binary} reset-password {name}'
        if checkpoint: line += ' --interrupt-' + checkpoint + '-rename'
        start = len(self.output)
        self.send(line.encode('ascii'))
        for prompt in (b'New password: ', b'Confirm password: '):
            self.wait(prompt)
            self.send(password)
            if self.wait(b'\r\n') != b'\r\n':
                raise RuntimeError('password reset input echoed')
        self.wait(b' exited status='); self.wait(b'\nsrv> ')
        segment = bytes(self.output[start:])
        result = re.search(rb'run: pid=\d+ exited status=(\d+)', segment)
        expected = 86 if checkpoint else 0
        if not result or int(result.group(1)) != expected:
            raise RuntimeError(f'{self.label}: reset exit did not match expected status {expected}')
        if (b'local-enroll: password reset' in segment) != (checkpoint is None):
            raise RuntimeError('reset publication message did not match requested outcome')

    def enable_account(self, name, checkpoint=None):
        binary = '/fat/signin-test/reset' if checkpoint else '/fat/bin/local-enroll'
        line = f'run {binary} enable {name}'
        if checkpoint: line += ' --interrupt-' + checkpoint + '-rename'
        start = len(self.output); self.send(line.encode('ascii'))
        self.wait(b' exited status='); self.wait(b'\nsrv> ')
        segment = bytes(self.output[start:])
        result = re.search(rb'run: pid=\d+ exited status=(\d+)', segment)
        if not result or int(result.group(1)) != (86 if checkpoint else 0):
            raise RuntimeError(f'{self.label}: unexpected enable exit status')
        if b'New password: ' in segment or b'Confirm password: ' in segment:
            raise RuntimeError('enable unexpectedly requested a password')
        if (b'local-enroll: account enabled' in segment) != (checkpoint is None):
            raise RuntimeError('enable publication message did not match requested outcome')

    def attempt(self, name, password, uid=None, session=None, ending='logout', diagnostic=True,
                interrupt_retry=False):
        start = len(self.output)
        self.send(name.encode('ascii')); self.wait(b'Password: ')
        sent = time.monotonic()
        self.send(password)
        if uid is None:
            if interrupt_retry:
                # The secret reader restores cooked mode before emitting this newline.
                self.wait(b'\r\n')
                self.serial.sendall(b'\x03')
            self.wait(b'Sign-in failed.')
            if time.monotonic() - sent < 0.8:
                raise RuntimeError('failed sign-in returned before its retry delay')
            self.wait(b'Login: ')
            segment = bytes(self.output[start:])
            if b'Session uid=' in segment or b'session-check: PASS' in segment:
                raise RuntimeError('failed sign-in unexpectedly started a session')
        else:
            expected = f'Session uid={uid} gid={uid} home=/fat/home/local-{name}'.encode()
            self.wait(expected)
            if diagnostic:
                self.wait(b'session-check: PASS')
            self.wait(b'session> ')
            if session is not None: session(self)
            if ending == 'interrupt':
                self.serial.sendall(b'\x03')
            else:
                self.send(ending.encode('ascii'))
            self.wait({'interrupt': b'Session interrupted.', 'crash': b'Session crashed.',
                       'logout': b'Signed out.'}[ending])
            self.wait(b'Session closed.'); self.wait(b'Login: ')
            segment = bytes(self.output[start:])
            if b'Sign-in failed.' in segment or segment.count(b'Session uid=') != 1:
                raise RuntimeError('successful sign-in produced inconsistent outcome')
            if (b'Session crashed.' in segment) != (ending == 'crash'):
                raise RuntimeError('session exit status did not match requested path')
        if b'srv> ' in segment:
            raise RuntimeError('privileged prompt exposed in local mode')

    def session_command(self, command, expected, paced=False):
        encoded = command.encode('ascii')
        if paced:
            # Type within COM1's RX capacity and verify that each chunk arrived.
            for start in range(0, len(encoded), 32):
                chunk = encoded[start:start + 32]
                self.serial.sendall(chunk)
                self.wait(chunk)
            self.serial.sendall(b'\n')
        else:
            self.send(encoded)
        response = self.wait(b'session> ')
        if expected not in response:
            raise RuntimeError(f'{self.label}: missing session result for {command!r}')
        return response

    def session_files(self, name, uid, setup=False, peer=None, hold=False):
        text = name + '-private-persisted'
        if setup:
            self.session_command('write evidence.txt ' + text, b'Written.')
        self.session_command('read evidence.txt', ('Data: ' + text).encode())
        self.session_command('stat evidence.txt',
                             f'Owner: {uid}:{uid} mode=0600 size={len(text)}'.encode())
        if peer:
            path = '/fat/home/local-' + peer + '/evidence.txt'
            self.session_command('read ' + path, b'Error: read: permission denied.')
            self.session_command('write ' + path + ' intrusion', b'Error: write: permission denied.')
            self.session_command('stat ' + path, b'Error: stat: permission denied.')
        if hold:
            self.session_command('hold', b'session-worker: ready')

    def finish(self):
        # Recovery uses the monitor's flush/reboot sequence. Local mode has no
        # privileged monitor; reset terminates QEMU under -no-reboot after the
        # completed session and causes QEMU to close its owned image normally.
        if self.mode == 'root-recovery':
            until = time.monotonic() + .7
            while time.monotonic() < until:
                if self.proc.poll() is not None: raise RuntimeError('recovery did not halt')
                try:
                    chunk = self.serial.recv(16384)
                    if not chunk: raise RuntimeError('recovery serial disconnected')
                    self.output.extend(chunk)
                except socket.timeout: pass
            if any(marker in self.output for marker in (b'Login: ', b'srv> ', b'bootlog: persisted',
                   b'installed-root: selected', b'init: readiness=', b'local-auth: console sign-in')):
                raise RuntimeError('strict sidecar refusal reached normal boot')
            self.qmp.call('system_reset')
        elif self.mode == 'recovery':
            self.command('/fat/bin/sync')
            self.send(b'reboot')
        else:
            self.qmp.call('system_reset')
        self.proc.wait(timeout=30)
        if self.proc.returncode: raise RuntimeError(f'{self.label}: VM shutdown failed')
        if any(secret in self.output for secret in SECRETS):
            raise RuntimeError(f'{self.label}: password echoed to serial')
        if self.mode != 'recovery' and b'srv> ' in self.output:
            raise RuntimeError('privileged prompt exposed in local mode')

    def __exit__(self, typ, value, tb):
        if self.serial: self.serial.close()
        if self.qmp: self.qmp.stream.close(); self.qmp.sock.close()
        if self.proc: stop_process(self.proc)
        if self.log: self.log.close()
        # Buffer in memory until here so even a split echoed password never
        # reaches evidence; retain a redacted diagnostic if the test fails.
        safe = bytes(self.output)
        for secret in SECRETS + tuple(self.extra_redactions):
            # Also scrub a partially echoed secret if the guest disconnected
            # midway through input; do not retain recognizable prefixes.
            for length in range(len(secret), 2, -1):
                safe = safe.replace(secret[:length], b'[REDACTED]')
        (self.out / ('serial-' + self.label + '.log')).write_bytes(safe)
        if self.tmp: self.tmp.cleanup()


def assert_report_private(segment, rows=None):
    # The test secrets and the committed cryptographic fields must not appear
    # in any inspection or cleanup report; never print the fields on failure.
    sensitive = list(SECRETS)
    if rows is not None:
        for fields in rows.values(): sensitive.extend(fields[4:6])
    if any(value and value in segment for value in sensitive):
        raise RuntimeError('account recovery report exposed credential material')


def inspect_and_clear(vm, committed, staged, lock_present, staged_is_database=True):
    rows = account_rows(committed)
    for values in rows.values(): vm.extra_redactions.extend(values[4:6])
    if staged is not None and staged_is_database:
        for values in account_rows(staged).values(): vm.extra_redactions.extend(values[4:6])
    before = vm.settled_data_state()
    report = vm.command('/fat/bin/local-enroll recover-store')
    for name, fields in rows.items():
        state = b'enabled' if fields[3] == b'1' else b'suspended'
        if not re.search(rb'(?:^|[\r\n])account ' + re.escape(name) + b' ' + state + rb'(?:[\r\n]|$)', report):
            raise RuntimeError('recovery inspection omitted expected account state')
    for name, present in ((b'accounts.tmp', staged is not None), (b'enroll.lock', lock_present)):
        expected = b'local-enroll: ' + name + (b' present' if present else b' absent')
        if expected not in report: raise RuntimeError('recovery inspection reported wrong marker state')
    assert_report_private(report, rows)
    if staged is not None and staged_is_database: assert_report_private(report, account_rows(staged))
    after = vm.paused_data_state()
    if before != after: raise RuntimeError('read-only recovery inspection changed guest data partition')
    result = vm.command('/fat/bin/local-enroll recover-store --clear-pending')
    if b'local-enroll: pending markers cleared' not in result:
        raise RuntimeError('recovery did not report clearing pending markers')
    assert_report_private(result, rows)
    if staged is not None and staged_is_database: assert_report_private(result, account_rows(staged))
    result = vm.command('/fat/bin/local-enroll recover-store --clear-pending')
    if b'local-enroll: no pending markers' not in result:
        raise RuntimeError('repeated recovery did not report clean no-op')
    assert_report_private(result, rows)
    if staged is not None and staged_is_database: assert_report_private(result, account_rows(staged))
    return before[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    parser.add_argument('--interactive-only', action='store_true',
                        help='run enrollment, interactive boots, reset and enable cases only')
    args = parser.parse_args()
    for name in (args.ovmf, args.ovmf_vars):
        if not Path(name).is_file() or ',' in name:
            parser.error('firmware templates must be regular files without commas')
    if not 10 <= args.boot_wait <= 240: parser.error('--boot-wait must be 10..240')
    build = proof.verified_build(ROOT)
    base = ROOT / 'build/review/local-signin'; base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'local-signin: evidence {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'build': build, 'cases': [],
               'scope': 'private disposable QEMU image; no physical media',
               'interactive_only': args.interactive_only,
               'firmware_sha256': installation.digest(Path(args.ovmf)),
               'firmware_vars_sha256': installation.digest(Path(args.ovmf_vars)),
               'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]}
    def save(): save_json(out / 'receipt.json', receipt)
    save()
    try:
        fs = ExfatFixture((ROOT / 'build/srvros.exfat').read_bytes())
        for name in ('local-enroll', 'local-signin', 'local-session'):
            actual = fs.read('bin/' + name)
            expected = runtime_elf((ROOT / ('build/userspace/' + name + '.elf')).read_bytes())
            if actual != expected: raise RuntimeError('installed executable differs from build: ' + name)
            receipt[name + '_sha256'] = sha(actual)
        diagnostic = runtime_elf((ROOT / 'build/userspace/test_local_signin_session.elf').read_bytes())
        receipt['diagnostic_session_sha256'] = sha(diagnostic)
        reset_tool = runtime_elf((ROOT / 'build/userspace/test_local_auth_reset.elf').read_bytes())
        receipt['diagnostic_reset_sha256'] = sha(reset_tool)
        fs.add_directory_files('signin-test', {'session': diagnostic, 'reset': reset_tool, 'hold': reset_tool})
        volume = out / 'fixture.exfat'; volume.write_bytes(fs.data)
        plan, ready = prepare_stage(out / 'local', 'local', volume=volume)
        target = out / 'local/image.img'
        guid = ready['gpt']['exfat_partition_guid']
        _, recovery = prepare_stage(out / 'recovery', 'recovery', guid)
        recovery_image = out / 'recovery/image.img'
        local_esp = out / 'local-esp.img'
        esp, data = proof.partitions(target)
        with target.open('rb') as stream:
            stream.seek(esp[0]); local_esp.write_bytes(stream.read(esp[1]))
        local_esp_hash = region_hash(target, *esp)
        recovery_esp_hash = region_hash(recovery_image, *esp)
        receipt.update(target=ready, recovery_template=recovery, root_guid=guid,
                       local_esp_sha256=local_esp_hash, recovery_esp_sha256=recovery_esp_hash)
        def local_mode():
            # local-esp is a raw region, not a disk; restore exactly this saved
            # generated region while QEMU is stopped.
            with target.open('r+b') as dst, local_esp.open('rb') as src:
                dst.seek(esp[0]); shutil.copyfileobj(src, dst, 1024 * 1024); dst.flush()
                import os
                os.fsync(dst.fileno())
            if region_hash(target, *esp) != local_esp_hash: raise RuntimeError('local ESP restore mismatch')
        initial_data_hash = region_hash(target, *data)
        copy_esp(recovery_image, target)
        if region_hash(target, *data) != initial_data_hash: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'enroll', args, 'recovery') as vm:
            vm.command('/fat/bin/chmod 755 /fat/signin-test/reset')
            vm.command('/fat/bin/chmod 755 /fat/signin-test/hold')
            for name, uid in (('alpha',1001), ('beta',1002), ('disabled',1003)):
                vm.enroll(name, uid)
            vm.command('/fat/bin/local-enroll disable disabled')
            vm.finish()
        original_accounts = account_bytes(target)
        if not original_accounts: raise RuntimeError('enrollment left empty account store')
        receipt['accounts_before_sha256'] = sha(original_accounts)
        before_swap = region_hash(target, *data)
        local_mode()
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        def stock_alpha(vm):
            vm.session_command('help', b'Commands: help, pwd, write PATH TEXT')
            vm.session_command('pwd', b'/fat/home/local-alpha')
            vm.session_command('crash', b'Error: unknown command.')
            vm.session_command('write overlong.txt ' + 'x' * 513,
                               b'Error: invalid or too-long command.', paced=True)
            vm.session_command('stat overlong.txt', b'Error: stat: not found.')
            vm.session_files('alpha', 1001, setup=True)

        with VM(target, out, 'production', args, 'local') as vm:
            vm.attempt('alpha', PASSWORDS['alpha'], 1001, session=stock_alpha, diagnostic=False)
            vm.attempt('alpha', PASSWORDS['alpha'], 1001, ending='interrupt', diagnostic=False)
            vm.attempt('beta', PASSWORDS['beta'], 1002, diagnostic=False,
                       session=lambda v: v.session_files('beta', 1002, setup=True, peer='alpha'))
            vm.finish()
        receipt['cases'].append({'production_both_users': 'PASS', 'bounded_input': 'PASS',
                                 'interrupt_returns_to_login': 'PASS',
                                 'fixture_command_unavailable': 'PASS'})
        save()

        before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        before_reset = account_rows(account_bytes(target))
        with VM(target, out, 'reset-stock', args, 'recovery') as vm:
            vm.command_refused('/fat/bin/local-enroll reset-password alpha --interrupt-before-rename')
            vm.command_refused('/fat/bin/local-enroll reset-password alpha --interrupt-after-rename')
            vm.reset_password('alpha', RESET_ALPHA)
            vm.reset_password('disabled', RESET_DISABLED)
            vm.finish()
        after_reset = account_rows(account_bytes(target))
        for name in (b'alpha', b'disabled'):
            if not reset_record_matches(before_reset[name], after_reset[name]):
                raise RuntimeError('stock reset changed identity/enabled state or left verifier unchanged')
        if before_reset.keys() != after_reset.keys() or before_reset[b'beta'] != after_reset[b'beta']:
            raise RuntimeError('stock reset changed another account')
        before_swap = region_hash(target, *data); local_mode()
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'reset-stock-verify', args, 'local') as vm:
            vm.attempt('alpha', PASSWORDS['alpha'])
            vm.attempt('disabled', PASSWORDS['disabled'])
            vm.attempt('disabled', RESET_DISABLED)
            vm.attempt('alpha', RESET_ALPHA, 1001, diagnostic=False,
                       session=lambda v: v.session_files('alpha', 1001, peer='beta'))
            vm.attempt('beta', PASSWORDS['beta'], 1002, diagnostic=False,
                       session=lambda v: v.session_files('beta', 1002, peer='alpha'))
            vm.finish()
        verify_persisted_files(target, data)
        alpha_password = RESET_ALPHA
        original_accounts = account_bytes(target)
        receipt['cases'].append({'stock_password_reset': 'PASS', 'test_flags_refused': 'PASS', 'old_credential_refused': 'PASS',
            'disabled_remains_disabled': 'PASS', 'other_account_and_files_unchanged': 'PASS'})
        save()

        before_swap = region_hash(target, *data)
        copy_esp(recovery_image, target)
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'install-diagnostic', args, 'recovery') as vm:
            vm.command('/fat/bin/cp /fat/signin-test/session /fat/bin/local-session')
            vm.command('/fat/bin/chmod 755 /fat/bin/local-session')
            vm.finish()
        before_swap = region_hash(target, *data)
        local_mode()
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        for number in (1, 2):
            with VM(target, out, f'local-{number}', args, 'local') as vm:
                vm.attempt('alpha', WRONG, interrupt_retry=True)
                vm.attempt('unknown', WRONG)
                vm.attempt('disabled', PASSWORDS['disabled'])
                vm.attempt('alpha', alpha_password, 1001,
                    session=lambda v: v.session_files('alpha', 1001, hold=True),
                    ending='crash' if number == 2 else 'logout')
                vm.attempt('beta', PASSWORDS['beta'], 1002,
                    session=lambda v: v.session_files('beta', 1002, peer='alpha', hold=True),
                    ending='crash' if number == 1 else 'logout')
                vm.attempt('alpha', alpha_password, 1001,
                    session=lambda v: v.session_files('alpha', 1001, peer='beta'))
                vm.finish()
            verify_persisted_files(target, data)
            receipt['cases'].append({'boot': number, 'wrong_unknown_disabled': 'PASS',
                'interrupt_during_retry_returns_to_login': 'PASS',
                'alpha': 'PASS', 'beta': 'PASS', 'file_readback': 'PASS',
                'cross_user_denial': 'PASS', 'worker_logout_and_crash_retired': 'PASS'})
            save()
        # Stop the reset process on each side of account-store replacement.
        # All decisions below come from the committed store and actual marker
        # files on the same disk, with a fresh boot at each admission boundary.
        for checkpoint, proposed in (('before', RESET_BEFORE), ('after', RESET_AFTER)):
            old_password = alpha_password
            committed = account_bytes(target)
            rows_before = account_rows(committed)
            before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'reset-' + checkpoint + '-interrupt', args, 'recovery') as vm:
                vm.reset_password('alpha', proposed, checkpoint=checkpoint)
                vm.finish()
            interrupted = account_bytes(target)
            staged = marker_bytes(target, 'accounts.tmp')
            if marker_bytes(target, 'enroll.lock') is None:
                raise RuntimeError('interrupted reset has no transaction lock')
            if checkpoint == 'before':
                if interrupted != committed or staged is None:
                    raise RuntimeError('before-rename reset changed committed store or lacks staged store')
                candidate = staged
            else:
                if interrupted == committed or staged is not None:
                    raise RuntimeError('after-rename reset did not replace store or retained staged name')
                candidate = interrupted
            rows_after = account_rows(candidate)
            if (rows_before.keys() != rows_after.keys() or
                not reset_record_matches(rows_before[b'alpha'], rows_after[b'alpha']) or
                rows_before[b'beta'] != rows_after[b'beta'] or
                rows_before[b'disabled'] != rows_after[b'disabled']):
                raise RuntimeError('interrupted reset changed unrelated account or failed to change verifier')
            before_swap = region_hash(target, *data); local_mode()
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'reset-' + checkpoint + '-pending', args, 'local') as vm:
                vm.attempt('alpha', old_password); vm.attempt('alpha', proposed); vm.finish()
            if account_bytes(target) != interrupted:
                raise RuntimeError('pending reset sign-in changed committed store')
            before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'reset-' + checkpoint + '-recovery', args, 'recovery') as vm:
                inspection_hash = inspect_and_clear(vm, interrupted, staged, True)
                vm.finish()
            if (account_bytes(target) != interrupted or marker_bytes(target, 'accounts.tmp') is not None or
                marker_bytes(target, 'enroll.lock') is not None):
                raise RuntimeError('reset recovery changed committed store or retained transaction markers')
            accepted, rejected = (old_password, proposed) if checkpoint == 'before' else (proposed, old_password)
            before_swap = region_hash(target, *data); local_mode()
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'reset-' + checkpoint + '-verified', args, 'local') as vm:
                vm.attempt('alpha', rejected)
                vm.attempt('alpha', accepted, 1001,
                           session=lambda v: v.session_files('alpha', 1001, peer='beta'))
                vm.attempt('beta', PASSWORDS['beta'], 1002,
                           session=lambda v: v.session_files('beta', 1002, peer='alpha'))
                vm.attempt('disabled', RESET_DISABLED)
                vm.finish()
            verify_persisted_files(target, data)
            alpha_password = accepted
            original_accounts = account_bytes(target)
            receipt['cases'].append({'reset_interruption': checkpoint, 'real_marker_refusal': 'PASS',
                'committed_store_preserved_during_recovery': 'PASS', 'read_only_inspection': 'PASS',
                'inspection_data_sha256': inspection_hash, 'recovery_noop': 'PASS', 'expected_credentials': 'PASS',
                'both_users_data_and_permissions': 'PASS', 'committed_sha256': sha(interrupted)})
            save()
        # A disabled account can be reset without becoming enabled.  Stock
        # enable must only toggle its flag and must not accept test controls.
        old_alpha_password = alpha_password
        enabled_before = account_bytes(target)
        before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'enable-suspend', args, 'recovery') as vm:
            vm.command('/fat/bin/local-enroll disable alpha'); vm.finish()
        suspended = account_bytes(target)
        require_enabled_transition(enabled_before, suspended, 'alpha', b'1', b'0')
        with VM(target, out, 'enable-suspended-reset', args, 'recovery') as vm:
            vm.command_refused('/fat/bin/local-enroll enable alpha --interrupt-before-rename')
            vm.command_refused('/fat/bin/local-enroll enable alpha --interrupt-after-rename')
            vm.reset_password('alpha', ENABLE_PASSWORD); vm.finish()
        suspended_reset = account_bytes(target)
        previous, current = account_rows(suspended), account_rows(suspended_reset)
        if (previous.keys() != current.keys() or not reset_record_matches(previous[b'alpha'], current[b'alpha']) or
            previous[b'alpha'][3] != b'0' or previous[b'beta'] != current[b'beta'] or
            previous[b'disabled'] != current[b'disabled']):
            raise RuntimeError('reset while suspended changed enabled state or another account')
        if marker_bytes(target, 'accounts.tmp') is not None or marker_bytes(target, 'enroll.lock') is not None:
            raise RuntimeError('successful suspended reset retained transaction markers')
        before_swap = region_hash(target, *data); local_mode()
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'enable-suspended-refusal', args, 'local') as vm:
            vm.attempt('alpha', old_alpha_password); vm.attempt('alpha', ENABLE_PASSWORD)
            vm.attempt('beta', PASSWORDS['beta'], 1002,
                       session=lambda v: v.session_files('beta', 1002, peer='alpha'))
            vm.finish()
        before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'enable-stock', args, 'recovery') as vm:
            vm.enable_account('alpha'); vm.finish()
        stock_enabled = account_bytes(target)
        require_enabled_transition(suspended_reset, stock_enabled, 'alpha', b'0', b'1')
        if marker_bytes(target, 'accounts.tmp') is not None or marker_bytes(target, 'enroll.lock') is not None:
            raise RuntimeError('successful enable retained transaction markers')
        before_swap = region_hash(target, *data); local_mode()
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'enable-stock-verified', args, 'local') as vm:
            vm.attempt('alpha', old_alpha_password)
            vm.attempt('alpha', ENABLE_PASSWORD, 1001,
                       session=lambda v: v.session_files('alpha', 1001, peer='beta'))
            vm.attempt('beta', PASSWORDS['beta'], 1002,
                       session=lambda v: v.session_files('beta', 1002, peer='alpha'))
            vm.finish()
        verify_persisted_files(target, data)
        alpha_password = ENABLE_PASSWORD
        receipt['cases'].append({'stock_enable': 'PASS', 'suspended_reset_remains_suspended': 'PASS',
            'test_flags_refused': 'PASS', 'unchanged_account_fields': 'PASS',
            'both_users_data_and_permissions': 'PASS'})
        save()
        for checkpoint in ('before', 'after'):
            enabled = account_bytes(target)
            before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'enable-' + checkpoint + '-suspend', args, 'recovery') as vm:
                vm.command('/fat/bin/local-enroll disable alpha'); vm.finish()
            suspended = account_bytes(target)
            require_enabled_transition(enabled, suspended, 'alpha', b'1', b'0')
            with VM(target, out, 'enable-' + checkpoint + '-interrupt', args, 'recovery') as vm:
                vm.enable_account('alpha', checkpoint=checkpoint); vm.finish()
            interrupted = account_bytes(target)
            staged = marker_bytes(target, 'accounts.tmp')
            if marker_bytes(target, 'enroll.lock') is None:
                raise RuntimeError('interrupted enable has no transaction lock')
            if checkpoint == 'before':
                if interrupted != suspended or staged is None:
                    raise RuntimeError('before-rename enable changed store or lacks staged store')
                candidate = staged
            else:
                if interrupted == suspended or staged is not None:
                    raise RuntimeError('after-rename enable did not replace store or retained staged name')
                candidate = interrupted
            require_enabled_transition(suspended, candidate, 'alpha', b'0', b'1')
            before_swap = region_hash(target, *data); local_mode()
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'enable-' + checkpoint + '-pending', args, 'local') as vm:
                vm.attempt('alpha', alpha_password); vm.attempt('beta', PASSWORDS['beta']); vm.finish()
            if account_bytes(target) != interrupted:
                raise RuntimeError('pending enable sign-in changed committed store')
            before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'enable-' + checkpoint + '-recovery', args, 'recovery') as vm:
                inspection_hash = inspect_and_clear(vm, interrupted, staged, True)
                vm.finish()
            if (account_bytes(target) != interrupted or marker_bytes(target, 'accounts.tmp') is not None or
                marker_bytes(target, 'enroll.lock') is not None):
                raise RuntimeError('enable recovery changed committed store or retained markers')
            before_swap = region_hash(target, *data); local_mode()
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            if checkpoint == 'before':
                with VM(target, out, 'enable-before-still-disabled', args, 'local') as vm:
                    vm.attempt('alpha', alpha_password)
                    vm.attempt('beta', PASSWORDS['beta'], 1002,
                               session=lambda v: v.session_files('beta', 1002, peer='alpha'))
                    vm.finish()
                before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
                if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
                with VM(target, out, 'enable-before-retry', args, 'recovery') as vm:
                    vm.enable_account('alpha'); vm.finish()
                retried = account_bytes(target)
                require_enabled_transition(interrupted, retried, 'alpha', b'0', b'1')
                if marker_bytes(target, 'accounts.tmp') is not None or marker_bytes(target, 'enroll.lock') is not None:
                    raise RuntimeError('retried enable retained transaction markers')
                before_swap = region_hash(target, *data); local_mode()
                if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'enable-' + checkpoint + '-verified', args, 'local') as vm:
                vm.attempt('alpha', alpha_password, 1001,
                           session=lambda v: v.session_files('alpha', 1001, peer='beta'))
                vm.attempt('beta', PASSWORDS['beta'], 1002,
                           session=lambda v: v.session_files('beta', 1002, peer='alpha'))
                vm.attempt('disabled', RESET_DISABLED)
                vm.finish()
            verify_persisted_files(target, data)
            original_accounts = account_bytes(target)
            receipt['cases'].append({'enable_interruption': checkpoint, 'real_marker_refusal': 'PASS',
                'committed_store_preserved_during_recovery': 'PASS', 'read_only_inspection': 'PASS',
                'inspection_data_sha256': inspection_hash, 'recovery_noop': 'PASS', 'expected_enabled_state': 'PASS',
                'both_users_data_and_permissions': 'PASS', 'committed_sha256': sha(interrupted)})
            save()
        # A live, stopped-before-publication writer must keep cleanup from
        # stealing its lock. The helper is a test-only executable alias with
        # no arguments, matching the monitor's background-launch interface.
        enabled = account_bytes(target)
        before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'live-writer-suspend', args, 'recovery') as vm:
            vm.command('/fat/bin/local-enroll disable alpha'); vm.finish()
        live_suspended = account_bytes(target)
        require_enabled_transition(enabled, live_suspended, 'alpha', b'1', b'0')
        with VM(target, out, 'live-writer', args, 'recovery') as vm:
            start = len(vm.output)
            vm.send(b'bg /fat/signin-test/hold')
            segment = vm.wait_events((b'bg: started pid=', b'local-enroll: transaction parked'), start)
            match = re.search(rb'bg: started pid=(\d+) /fat/signin-test/hold', segment)
            if not match or int(match.group(1)) <= 0:
                raise RuntimeError('could not identify owned parked fixture process')
            pid = match.group(1)
            exit_marker = b'bg: pid=' + pid + b' exited status='
            if exit_marker in vm.output[start:]: raise RuntimeError('fixture writer exited before contention test')
            live_before = vm.settled_data_state()
            live_committed, live_staged, live_lock = vm.paused_account_snapshot()
            if live_committed != live_suspended or live_staged is None or live_lock is None:
                raise RuntimeError('parked writer did not retain expected store and markers')
            require_enabled_transition(live_committed, live_staged, 'alpha', b'0', b'1')
            live_rows = account_rows(live_committed)
            for rows in (live_rows, account_rows(live_staged)):
                for fields in rows.values(): vm.extra_redactions.extend(fields[4:6])
            for command in ('/fat/bin/local-enroll recover-store',
                            '/fat/bin/local-enroll recover-store --clear-pending'):
                report = vm.command_refused(command)
                if b'local-enroll: account update is active' not in report:
                    raise RuntimeError('live account update was not reported as active')
                assert_report_private(report, live_rows)
                assert_report_private(report, account_rows(live_staged))
            if vm.paused_data_state() != live_before or exit_marker in vm.output[start:]:
                raise RuntimeError('live-writer refusal changed data or stopped fixture writer')
            kill_start = len(vm.output)
            vm.send(b'kill ' + pid)
            vm.wait_events((b'kill: requested process stop', exit_marker, b'srv> '), kill_start)
            # Successful stock inspection/clear proves the exited writer no
            # longer owns its OS lock, in addition to the observed PID exit.
            # The paused snapshot is the only safe host read while QEMU runs.
            committed, staged, lock = vm.paused_account_snapshot()
            if (committed, staged, lock) != (live_committed, live_staged, live_lock):
                raise RuntimeError('parked transaction markers or committed store differ')
            require_enabled_transition(committed, staged, 'alpha', b'0', b'1')
            inspection_hash = inspect_and_clear(vm, committed, staged, True)
            vm.enable_account('alpha'); vm.finish()
        live_enabled = account_bytes(target)
        require_enabled_transition(live_suspended, live_enabled, 'alpha', b'0', b'1')
        if marker_bytes(target, 'accounts.tmp') is not None or marker_bytes(target, 'enroll.lock') is not None:
            raise RuntimeError('live-writer recovery retained transaction markers')
        before_swap = region_hash(target, *data); local_mode()
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'live-writer-verified', args, 'local') as vm:
            vm.attempt('alpha', alpha_password, 1001,
                       session=lambda v: v.session_files('alpha', 1001, peer='beta'))
            vm.attempt('beta', PASSWORDS['beta'], 1002,
                       session=lambda v: v.session_files('beta', 1002, peer='alpha'))
            vm.finish()
        verify_persisted_files(target, data)
        original_accounts = account_bytes(target)
        receipt['cases'].append({'live_writer_cleanup_refused': 'PASS',
            'owned_writer_stopped_and_recovered': 'PASS', 'both_users_data_and_permissions': 'PASS',
            'inspection_data_sha256': inspection_hash})
        save()
        if args.interactive_only:
            receipt.update(status='PASS', final_disk_sha256=installation.digest(target),
                           limitations=['interactive, reset and enable subset only; full security suite not run',
                                        'disposable QEMU only', 'no physical or power-loss qualification'])
            print(f'local-signin: interactive, reset and enable subset PASS; evidence {out}', flush=True)
            return
        # Pending transaction markers must block even a correct committed
        # credential. Clear only the marker this invocation created, and prove
        # that the same committed credential works again afterwards.
        for name in ('accounts.tmp', 'enroll.lock'):
            path = '/fat/etc/local-auth/' + name
            before_swap = region_hash(target, *data)
            copy_esp(recovery_image, target)
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'pending-' + name + '-setup', args, 'recovery') as vm:
                vm.send(('write ' + path + ' pending-fixture').encode('ascii'))
                vm.wait(b'write: ' + path.encode('ascii') + b' 15 bytes'); vm.wait(b'srv> ')
                vm.command('/fat/bin/chmod 600 ' + path)
                vm.finish()
            if account_bytes(target) != original_accounts: raise RuntimeError('pending fixture changed committed accounts')
            before_swap = region_hash(target, *data); local_mode()
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'pending-' + name, args, 'local') as vm:
                vm.attempt('alpha', alpha_password); vm.attempt('beta', PASSWORDS['beta'])
                vm.finish()
            before_swap = region_hash(target, *data); copy_esp(recovery_image, target)
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'pending-' + name + '-clear', args, 'recovery') as vm:
                inspection_hash = inspect_and_clear(vm, original_accounts,
                    b'pending-fixture' if name == 'accounts.tmp' else None,
                    name == 'enroll.lock', staged_is_database=False)
                vm.finish()
            if account_bytes(target) != original_accounts: raise RuntimeError('cleanup changed committed accounts')
            before_swap = region_hash(target, *data); local_mode()
            if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            with VM(target, out, 'pending-' + name + '-cleared', args, 'local') as vm:
                vm.attempt('alpha', alpha_password, 1001); vm.finish()
            receipt['cases'].append({'pending': name, 'refused': 'PASS', 'cleared_retry': 'PASS',
                'read_only_inspection': 'PASS', 'recovery_noop': 'PASS', 'inspection_data_sha256': inspection_hash})
            save()
        # Preserve one exact data snapshot. Mutations are made only offline,
        # on this invocation's target image, then undone after each no-write
        # refusal before the rest of the existing suite continues.
        snapshot = proof.data_image(target, *data)
        for variant in ('absent', 'incomplete'):
            changed = sidecar_variant(snapshot, variant)
            write_data(target, data[0], changed)
            before = installation.digest(target)
            with VM(target, out, 'sidecar-' + variant, args, 'root-recovery') as vm:
                vm.finish()
            after = installation.digest(target)
            if before != after: raise RuntimeError('sidecar refusal wrote to installed disk')
            receipt['cases'].append({'sidecar': variant, 'recovery': 'PASS', 'disk_sha256': before})
            if variant == 'absent':
                # The strict policy is specific to opt-in sign-in. Explicit
                # administrator recovery must retain its existing admission.
                before_swap = region_hash(target, *data)
                copy_esp(recovery_image, target)
                if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
                with VM(target, out, 'sidecar-absent-admin-recovery', args, 'recovery') as vm:
                    vm.finish()
                receipt['cases'].append({'sidecar_absent_admin_recovery': 'PASS'})
                before_swap = region_hash(target, *data); local_mode()
                if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
            write_data(target, data[0], snapshot)
            if region_hash(target, *data) != sha(snapshot): raise RuntimeError('sidecar fixture restore mismatch')
            save()
        if account_bytes(target) != original_accounts: raise RuntimeError('sidecar fixture changed committed accounts')
        before_swap = region_hash(target, *data)
        copy_esp(recovery_image, target)
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'damage', args, 'recovery') as vm:
            vm.send(('write ' + ACCOUNT_PATH + ' malformed-fixture').encode('ascii'))
            vm.wait(b'write: ' + ACCOUNT_PATH.encode('ascii') + b' 17 bytes')
            vm.wait(b'srv> ')
            for name in ('accounts.tmp', 'enroll.lock'):
                path = '/fat/etc/local-auth/' + name
                vm.send(('write ' + path + ' pending-fixture').encode('ascii'))
                vm.wait(b'write: ' + path.encode('ascii') + b' 15 bytes'); vm.wait(b'srv> ')
                vm.command('/fat/bin/chmod 600 ' + path)
            invalid_before = vm.settled_data_state()
            for command in ('/fat/bin/local-enroll recover-store',
                            '/fat/bin/local-enroll recover-store --clear-pending'):
                report = vm.command_refused(command)
                if b'local-enroll: committed account store invalid' not in report:
                    raise RuntimeError('invalid account store recovery did not refuse')
                assert_report_private(report)
            if vm.paused_data_state() != invalid_before:
                raise RuntimeError('invalid-store inspection or cleanup changed guest data partition')
            vm.finish()
        damaged = account_bytes(target)
        if damaged != b'malformed-fixture' or damaged == original_accounts:
            raise RuntimeError('controlled account store damage not observed')
        if any(marker_bytes(target, name) != b'pending-fixture' for name in ('accounts.tmp', 'enroll.lock')):
            raise RuntimeError('invalid-store recovery modified pending marker bytes')
        receipt['cases'].append({'invalid_store_recovery_refused': 'PASS',
                                 'inspection_data_sha256': invalid_before[0]})
        receipt['accounts_damaged_sha256'] = sha(damaged)
        # Remove only this test's deliberately created markers in explicit
        # recovery, leaving the malformed committed file for the existing
        # independent sign-in refusal case.
        with VM(target, out, 'damage-fixture-clear', args, 'recovery') as vm:
            vm.command('/fat/bin/rm /fat/etc/local-auth/accounts.tmp')
            vm.command('/fat/bin/rm /fat/etc/local-auth/enroll.lock')
            vm.finish()
        if account_bytes(target) != damaged or any(marker_bytes(target, name) is not None
                for name in ('accounts.tmp', 'enroll.lock')):
            raise RuntimeError('invalid fixture marker cleanup changed committed bytes')
        before_swap = region_hash(target, *data)
        local_mode()
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'damaged', args, 'local') as vm:
            vm.attempt('alpha', alpha_password)
            vm.attempt('beta', PASSWORDS['beta'])
            vm.finish()
        receipt['cases'].append({'damaged_store': 'PASS'})
        previous = None
        for label, path in (('writable-program', '/fat/bin/local-signin'), ('writable-parent', '/fat/bin')):
            before_swap = region_hash(target, *data)
            copy_esp(recovery_image, target)
            if region_hash(target, *data) != before_swap:
                raise RuntimeError('ESP swap changed target data')
            with VM(target, out, label + '-setup', args, 'recovery') as vm:
                if previous:
                    vm.command('/fat/bin/chmod 755 ' + previous)
                vm.command('/fat/bin/chmod 777 ' + path)
                vm.finish()
            before_swap = region_hash(target, *data)
            local_mode()
            if region_hash(target, *data) != before_swap:
                raise RuntimeError('ESP swap changed target data')
            with VM(target, out, label, args, 'blocked') as vm:
                vm.finish()
            receipt['cases'].append({label: 'PASS'})
            previous = path
            save()
        receipt.update(status='PASS', final_disk_sha256=installation.digest(target),
                       limitations=['disposable QEMU only', 'no physical or power-loss qualification'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        save()
    print(f'local-signin: PASS; evidence {out}', flush=True)


if __name__ == '__main__': main()
