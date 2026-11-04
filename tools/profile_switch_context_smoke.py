#!/usr/bin/env python3
"""Supplement a completed graphical switch proof with real shell environments.

The prior receipt remains unchanged, including its failed capture audit. This
run uses independent copies of its stopped disks and records complementary proof.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import shutil
import stat
import tempfile

import profile_switch_smoke as s
import profile_desktop_smoke as d
import profile_chooser_smoke as c
import profile_transfer_smoke as t
from exfat_test_image import ExfatFixture
from pack_runtime_elf import runtime_elf
from uefi_usb_smoke import default_ovmf_path

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(data)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prior-receipt', type=Path, required=True)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    prior_path = args.prior_receipt.resolve()
    prior = json.loads(prior_path.read_text())
    assert prior['status'] == 'FAIL' and prior['error'] == 'StopIteration: '
    assert prior['sessions']['same_boot_a_b_a']
    assert prior['small_fixture']['keyboard_cancel'] and prior['small_fixture']['keyboard_switch_to_b']
    assert prior['accepted_io']['close_wait_and_retirement_before_commit']
    build = d.verified_build(ROOT)
    assert build['hashes'] == prior['build']['hashes'], 'guest payload changed since graphical proof'
    base = (ROOT / 'build/review/profile-switch-context').resolve()
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'Context supplement evidence: {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'build': build,
               'prior_receipt': str(prior_path), 'prior_receipt_sha256': digest(prior_path),
               'prior_status_preserved': 'FAIL', 'sources': {},
               'firmware_sha256': digest(Path(args.ovmf)),
               'firmware_vars_sha256': digest(Path(args.ovmf_vars)),
               'scope': 'Complementary real shell environment/cwd proof; no relabeling of prior run.'}
    d.save_json(out / 'receipt.json', receipt)
    disks = {}
    try:
        required = sum(Path(v['disk']).stat().st_size for v in prior['installations'].values()) * 2 + 1024**3
        assert shutil.disk_usage(out).free >= required, 'insufficient copy/audit space and reserve'
        for name, entry in prior['installations'].items():
            src = Path(entry['disk']).resolve()
            meta = src.stat()
            assert stat.S_ISREG(meta.st_mode) and meta.st_nlink == 1
            before = digest(src)
            bounds = d.partitions(src)[1]
            assert d.outside_data(src, *bounds) == entry['protected_regions']
            data = d.data_image(src, *bounds)
            assert data == gzip.decompress((prior_path.parent / f'data-{name}-final.exfat.gz').read_bytes())
            if name == 'system':
                fixture = ExfatFixture(data)
                for binary in ('deskd', 'desktools', 'deskterm', 'desksketch', 'deskclock',
                               'lifetimeprobe', 'profile', 'deskprofiles', 'env', 'pwd'):
                    assert fixture.read('bin/' + binary) == runtime_elf((ROOT / f'build/userspace/{binary}.elf').read_bytes()), binary
            target = out / f'{name}.img'
            shutil.copyfile(src, target)
            assert target.stat().st_nlink == 1 and digest(target) == before and digest(src) == before
            receipt['sources'][name] = {'path': str(src), 'sha256': before,
                                        'protected_regions': entry['protected_regions']}
            disks[name] = target
        d.save_json(out / 'receipt.json', receipt)
        with c.VM(disks['system'], disks['transport'], out, 'contexts', args) as vm:
            vm_pid = vm.vm.pid
            start = len(vm.output)
            vm.launch('--profile=' + s.A + ' --apps-trace --scale=1')
            root = s.observe(vm, rb'session: phase=started token=(\d+) pid=(\d+)', start)
            token, pid = map(int, root.groups())
            sessions = []
            for index, (label, home, identity) in enumerate((('a', s.A, s.ID_A), ('b', s.B, s.ID_B), ('a-return', s.A, s.ID_A))):
                s.observe(vm, rb'deskd: profile id=' + identity.encode() + rb' home=' + home.encode(), start)
                s.observe(vm, rb'deskd: commit', start)
                terminal, _, _, _ = s.launch(vm, 'Terminal', rb'deskterm: ready shell=\d+')
                # One shell line ensures both external commands finish before
                # exit confirms the capture; no timing-based handoff assumption.
                s.type_shell(vm, '/fat/bin/pwd > /work/ctx-' + label + '-pwd; /fat/bin/env > /work/ctx-' + label + '-env; exit')
                s.observe(vm, rb'deskterm: shell exited status=0', terminal)
                vm.key('ctrl-shift-w')
                s.observe(vm, rb'deskterm: exited failure=0', terminal)
                sessions.append({'label': label, 'token': token, 'root_pid': pid})
                if index < 2:
                    ending = s.switch_request(vm)
                    token, pid, start = s.transition(vm, ending, token, pid)
                    next_home, next_id = (s.B, s.ID_B) if index == 0 else (s.A, s.ID_A)
                    s.select_profile(vm, next_home, next_id, start)
                else:
                    s.menu_cancel_console(vm, token, pid)
            assert vm.vm.pid == vm_pid
            vm.finish()
        receipt['sessions'] = sessions
        receipt['qemu_pid'] = vm_pid
        receipt['same_boot_a_b_a'] = True
        transport = t.check_allocation(disks['transport'], out, 'transport-final')
        t.check_allocation(disks['system'], out, 'system-final')
        receipt['contexts'] = {}
        for label, home, identity in (('a', s.A, s.ID_A), ('b', s.B, s.ID_B), ('a-return', s.A, s.ID_A)):
            assert transport.read('ctx-' + label + '-pwd') == (home + '\n').encode()
            receipt['contexts'][label] = s.check_context(transport.read('ctx-' + label + '-env'), home, identity)
        for name, entry in receipt['sources'].items():
            assert digest(Path(entry['path'])) == entry['sha256'], 'original evidence changed'
            assert d.outside_data(disks[name], *d.partitions(disks[name])[1]) == entry['protected_regions']
        receipt['status'] = 'PASS'
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        d.save_json(out / 'receipt.json', receipt)
    print(f'profile-switch-context: PASS; evidence {out}', flush=True)


if __name__ == '__main__':
    main()
