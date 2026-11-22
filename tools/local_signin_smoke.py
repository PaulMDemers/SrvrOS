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
SECRETS = tuple(PASSWORDS.values()) + (WRONG,)


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


class VM:
    def __init__(self, disk, out, label, args, mode):
        self.disk, self.out, self.label, self.args, self.mode = disk, out, label, args, mode
        self.output = bytearray(); self.cursor = 0
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
            self.wait(b'srv> ' if self.mode == 'recovery' else b'Login: ', self.args.boot_wait)
            if b'installed-root: selected device=ahci0p2 format=exfat-v1 role=system' not in self.output:
                raise RuntimeError('installed identity-selected root missing')
            if b'trying initramfs fallback' in self.output:
                raise RuntimeError('unexpected initramfs fallback')
            if self.mode == 'recovery' and (b'local-auth: recovery console' not in self.output or
                                            b'init: readiness=ready' not in self.output):
                raise RuntimeError('explicit recovery did not reach ready administrator console')
            if self.mode == 'local':
                if b'local-auth: console sign-in' not in self.output or b'srv> ' in self.output:
                    raise RuntimeError('local mode did not start restricted sign-in')
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

    def send(self, line):
        self.serial.sendall(line + b'\n')

    def command(self, command):
        start = len(self.output); self.send(('run ' + command).encode('ascii'))
        self.wait(b' exited status='); self.wait(b'\nsrv> ')
        result = re.search(rb'run: pid=\d+ exited status=(\d+)', self.output[start:])
        if not result or int(result.group(1)):
            raise RuntimeError(f'{self.label}: command failed: {command}')

    def enroll(self, name, uid):
        start = len(self.output)
        self.send(f'run /fat/bin/local-enroll add {name} {uid} {uid}'.encode())
        self.wait(b'New password: '); self.send(PASSWORDS[name])
        self.wait(b'Confirm password: '); self.send(PASSWORDS[name])
        self.wait(b' exited status='); self.wait(b'\nsrv> ')
        result = re.search(rb'run: pid=\d+ exited status=(\d+)', self.output[start:])
        if not result or int(result.group(1)):
            raise RuntimeError(f'{self.label}: enrollment failed: {name}')

    def attempt(self, name, password, uid=None):
        start = len(self.output)
        self.send(name.encode('ascii')); self.wait(b'Password: ')
        sent = time.monotonic()
        self.send(password)
        if uid is None:
            self.wait(b'Sign-in failed.')
            if time.monotonic() - sent < 0.8:
                raise RuntimeError('failed sign-in returned before its retry delay')
            self.wait(b'Login: ')
            segment = bytes(self.output[start:])
            if b'Session uid=' in segment or b'session-check: PASS' in segment:
                raise RuntimeError('failed sign-in unexpectedly started a session')
        else:
            expected = f'Session uid={uid} gid={uid} home=/fat/home/local-{name}'.encode()
            self.wait(expected); self.wait(b'session-check: PASS'); self.wait(b'Login: ')
            segment = bytes(self.output[start:])
            if b'Sign-in failed.' in segment or segment.count(b'Session uid=') != 1:
                raise RuntimeError('successful sign-in produced inconsistent outcome')
        if b'srv> ' in segment:
            raise RuntimeError('privileged prompt exposed in local mode')

    def finish(self):
        # Recovery uses the monitor's flush/reboot sequence. Local mode has no
        # privileged monitor; reset terminates QEMU under -no-reboot after the
        # completed session and causes QEMU to close its owned image normally.
        if self.mode == 'recovery':
            self.command('/fat/bin/sync')
            self.send(b'reboot')
        else:
            self.qmp.call('system_reset')
        self.proc.wait(timeout=30)
        if self.proc.returncode: raise RuntimeError(f'{self.label}: VM shutdown failed')
        if any(secret in self.output for secret in SECRETS):
            raise RuntimeError(f'{self.label}: password echoed to serial')
        if self.mode == 'local' and b'srv> ' in self.output:
            raise RuntimeError('privileged prompt exposed in local mode')

    def __exit__(self, typ, value, tb):
        if self.serial: self.serial.close()
        if self.qmp: self.qmp.stream.close(); self.qmp.sock.close()
        if self.proc: stop_process(self.proc)
        if self.log: self.log.close()
        # Buffer in memory until here so even a split echoed password never
        # reaches evidence; retain a redacted diagnostic if the test fails.
        safe = bytes(self.output)
        for secret in SECRETS:
            # Also scrub a partially echoed secret if the guest disconnected
            # midway through input; do not retain recognizable prefixes.
            for length in range(len(secret), 2, -1):
                safe = safe.replace(secret[:length], b'[REDACTED]')
        (self.out / ('serial-' + self.label + '.log')).write_bytes(safe)
        if self.tmp: self.tmp.cleanup()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
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
        fs.add_directory_files('signin-test', {'session': diagnostic})
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
            vm.command('/fat/bin/cp /fat/signin-test/session /fat/bin/local-session')
            vm.command('/fat/bin/chmod 755 /fat/bin/local-session')
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
        for number in (1, 2):
            with VM(target, out, f'local-{number}', args, 'local') as vm:
                vm.attempt('alpha', WRONG)
                vm.attempt('unknown', WRONG)
                vm.attempt('disabled', PASSWORDS['disabled'])
                vm.attempt('alpha', PASSWORDS['alpha'], 1001)
                vm.attempt('beta', PASSWORDS['beta'], 1002)
                vm.finish()
            receipt['cases'].append({'boot': number, 'wrong_unknown_disabled': 'PASS', 'alpha': 'PASS', 'beta': 'PASS'})
            save()
        before_swap = region_hash(target, *data)
        copy_esp(recovery_image, target)
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'damage', args, 'recovery') as vm:
            vm.send(('write ' + ACCOUNT_PATH + ' malformed-fixture').encode('ascii'))
            vm.wait(b'write: ' + ACCOUNT_PATH.encode('ascii') + b' 17 bytes')
            vm.wait(b'srv> ')
            vm.finish()
        damaged = account_bytes(target)
        if damaged != b'malformed-fixture' or damaged == original_accounts:
            raise RuntimeError('controlled account store damage not observed')
        receipt['accounts_damaged_sha256'] = sha(damaged)
        before_swap = region_hash(target, *data)
        local_mode()
        if region_hash(target, *data) != before_swap: raise RuntimeError('ESP swap changed target data')
        with VM(target, out, 'damaged', args, 'local') as vm:
            vm.attempt('alpha', PASSWORDS['alpha'])
            vm.attempt('beta', PASSWORDS['beta'])
            vm.finish()
        receipt['cases'].append({'damaged_store': 'PASS'})
        receipt.update(status='PASS', final_disk_sha256=installation.digest(target),
                       limitations=['disposable QEMU only', 'no physical or power-loss qualification'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        save()
    print(f'local-signin: PASS; evidence {out}', flush=True)


if __name__ == '__main__': main()
