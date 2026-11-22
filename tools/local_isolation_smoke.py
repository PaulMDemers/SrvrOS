#!/usr/bin/env python3
"""Two-boot local-credentials proof on newly created disposable installations.

Requires matched built artifacts and never opens or modifies physical media.
Run alone: the build and test helpers use shared paths.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

from exfat_test_image import ExfatFixture
import installation_image
import installed_disk_smoke as proof
from pack_runtime_elf import runtime_elf
import profile_chooser_smoke as chooser
from uefi_usb_smoke import default_ovmf_path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qemu', default='qemu-system-x86_64')
    parser.add_argument('--ovmf', default=default_ovmf_path())
    parser.add_argument('--ovmf-vars', required=True)
    parser.add_argument('--boot-wait', type=int, default=120)
    args = parser.parse_args()
    for path in (args.ovmf, args.ovmf_vars):
        if not Path(path).is_file() or ',' in path:
            parser.error('firmware templates must be files without commas in their paths')
    if not 10 <= args.boot_wait <= 240:
        parser.error('--boot-wait must be 10..240')
    build = proof.verified_build(ROOT)
    base = ROOT / 'build/review/local-isolation'
    base.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix='proof-', dir=base))
    print(f'local-isolation: evidence {out}', flush=True)
    receipt = {'status': 'INCOMPLETE', 'build': build, 'boots': [],
               'scope': 'new disposable QEMU disks; no physical media or profile edits',
               'firmware_sha256': proof.digest(Path(args.ovmf).read_bytes()),
               'firmware_vars_sha256': proof.digest(Path(args.ovmf_vars).read_bytes()),
               'qemu_version': subprocess.check_output([args.qemu, '--version'], text=True).splitlines()[0]}
    def save():
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    save()
    try:
        probe = runtime_elf((ROOT / 'build/userspace/test_local_isolation.elf').read_bytes())
        receipt['probe_sha256'] = hashlib.sha256(probe).hexdigest()
        fixture = ExfatFixture((ROOT / 'build/srvros.exfat').read_bytes())
        fixture.add_directory_files('isolation-test', {'probe': probe})
        volume = out / 'target.exfat'; volume.write_bytes(fixture.data)
        disks = []
        for name, source in (('target', volume), ('transport', ROOT / 'build/srvros.exfat')):
            stage = out / name
            installation_image.prepare(stage, ROOT / 'build/kernel/srvros.elf',
                ROOT / 'build/limine-binary/BOOTX64.EFI', source,
                ROOT / 'build/userspace/init.elf', identity_bound=True)
            receipt[name] = installation_image.apply(stage)
            disks.append(stage / 'image.img')
        target, transport = disks
        if receipt['target']['gpt']['disk_guid'] == receipt['transport']['gpt']['disk_guid']:
            raise RuntimeError('generated installation GUID collision')
        _, (offset, size) = proof.partitions(target)
        protected = proof.outside_data(target, offset, size)
        transport_hash = proof.digest(transport.read_bytes())
        for phase in ('setup', 'readback'):
            with chooser.VM(target, transport, out, phase, args) as vm:
                if phase == 'setup':
                    vm.run('/fat/bin/chmod 755 /fat/isolation-test/probe')
                start = len(vm.output)
                vm.run('/fat/isolation-test/probe ' + phase)
                for marker in (b'local-isolation: user-1001 PASS', b'local-isolation: user-1002 PASS',
                               ('local-isolation: ' + phase + ' PASS').encode()):
                    if marker not in vm.output[start:]:
                        raise RuntimeError(f'{phase}: missing {marker!r}')
                if b'local-isolation: FAIL' in vm.output[start:]:
                    raise RuntimeError(f'{phase}: guest reported failure')
                vm.finish()
            if proof.outside_data(target, offset, size) != protected:
                raise RuntimeError('target protected regions changed')
            if proof.digest(transport.read_bytes()) != transport_hash:
                raise RuntimeError('transport disk changed')
            receipt['boots'].append({'phase': phase, 'status': 'PASS',
                                     'target_sha256': proof.digest(target.read_bytes())})
            save()
        receipt.update(status='PASS', protected_regions=protected,
                       limitations=['disposable VM only', 'no physical power-loss qualification'])
    except Exception as error:
        receipt.update(status='FAIL', error=f'{type(error).__name__}: {error}')
        raise
    finally:
        save()
    print(f'local-isolation: PASS; evidence {out}', flush=True)


if __name__ == '__main__':
    main()
