"""Observe a private i915 GGTT rebind on Air7,2. The GPU client runs as the invoking user."""
import argparse
from contextlib import ExitStack
import mmap
import os
from pathlib import Path
import pwd
import re
import struct
import subprocess
import tempfile

SLOT = 8704
BIND = re.compile(r'i915_vma_bind: obj=(\w+), offset=(0x[\da-f]+) size=(0x[\da-f]+).* vm=(\w+)')
PCI = Path('/sys/bus/pci/devices/0000:00:02.0')


def trial(binary, user, instance):
    panel = PCI / 'drm'
    edp, = panel.glob('card*/card*-eDP-1/enabled')
    if (Path('/sys/class/dmi/id/product_name').read_text().strip() != 'MacBookAir7,2' or
        (PCI / 'vendor').read_text().strip() != '0x8086' or
        (PCI / 'device').read_text().strip() != '0x1626' or
        (PCI / 'driver').resolve().name != 'i915' or edp.read_text().strip() != 'enabled'):
        raise ValueError('Requires the Air7,2 with an active i915 eDP panel')
    for event in ('i915_vma_bind', 'i915_vma_unbind'):
        (instance / 'events/i915' / event / 'enable').write_text('1')
    (instance / 'tracing_on').write_text('1')
    offset = 0x800000 + SLOT * 8
    card, = (PCI / 'drm').glob('card[0-9]*')
    if {p.name for p in (card / 'engine').iterdir()} != {'rcs0', 'bcs0', 'vcs0', 'vcs1', 'vecs0'}:
        raise ValueError('Unexpected engine topology')
    wake_path = Path('/sys/kernel/debug/dri') / card.name[4:] / 'i915_forcewake_user'
    if '0000:00:02.0' not in (wake_path.parent / 'name').read_text():
        raise ValueError('Forcewake node belongs to a different device')
    with ExitStack() as stack:
        stack.enter_context(wake_path.open('rb', buffering=0))
        bar = stack.enter_context((PCI / 'resource0').open('rb', buffering=0))

        def mmio_page(address):
            return stack.enter_context(mmap.mmap(bar.fileno(), 4096, flags=mmap.MAP_SHARED,
                                                 prot=mmap.PROT_READ, offset=address & ~4095))

        ptes = mmio_page(offset)
        plane = mmio_page(0x70180)
        cache = mmio_page(0xb118)
        power = mmio_page(0x40d0)
        messages = mmio_page(0x8038)

        def primary():
            return tuple(struct.unpack_from('<I', plane, r)[0] for r in (0x180, 0x19c, 0x1ac))

        def sample():
            live = primary()
            if (live != original_plane or edp.read_text().strip() != 'enabled' or
                not live[0] & (1 << 31) or live[0] == 0xffffffff or
                (live[1] & ~8) != live[2] or live[2] >= SLOT * 4096):
                raise ValueError('Original panel surface changed or is outside the admitted range')
            entry = struct.unpack_from('<Q', ptes, offset & 4095)[0]
            if not entry & 1 or entry == 2**64 - 1:
                raise ValueError('Invalid native GGTT entry')
            # Read the currently selected bank. This does not steer i915 or
            # assert that the policy on all physical banks is identical.
            l3, scratch = struct.unpack_from('<II', cache, 0x118)
            if 0xffffffff in (l3, scratch):
                raise ValueError('Invalid L3 policy read')
            return entry, l3, scratch

        def traffic_messages():
            request = struct.unpack_from('<I', power, 0xd0)[0]
            widi = struct.unpack_from('<I', messages, 0x14)[0]
            l3 = struct.unpack_from('<I', messages, 0x38)[0]
            if 0xffffffff in (request, widi, l3):
                raise ValueError('Invalid power-manager message read')
            return f'gam_request={request:#x} widi={widi:#x} l3_message={l3:#x}'

        original_plane = primary()
        client = stack.enter_context(subprocess.Popen(
            [str(binary), '--slot-loan'], user=user.pw_uid, group=user.pw_gid,
            extra_groups=os.getgrouplist(user.pw_name, user.pw_gid),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True))
        entries, bindings = [], []
        checked = 0
        pid = re.compile(r'-' + str(client.pid) + r'\s')
        previous_rows = 0
        try:
            for line in client.stdout:
                fields = line.split()
                if fields[:1] == ['CHECK']:
                    phase = int(fields[1])
                    pte, l3, scratch = sample()
                    if phase != checked or len(entries) != phase + 1 or entries[phase] != pte:
                        raise ValueError('GGTT binding changed before the end of the phase')
                    checked += 1
                    print(f'checked={phase} slot={SLOT} pte={pte:#018x} '
                          f'l3sqcreg4={l3:#010x} scratch1={scratch:#010x} {traffic_messages()}', flush=True)
                    print(phase, file=client.stdin, flush=True)
                    continue
                if fields[:1] != ['BIND']:
                    print(line.rstrip(), flush=True)
                    continue
                _, phase, size = fields
                trace = (instance / 'trace').read_text()
                counts = re.search(r'entries-in-buffer/entries-written:\s*(\d+)/(\d+)', trace)
                if not counts or counts[1] != counts[2]:
                    raise ValueError('Incomplete i915 trace')
                rows = [r for r in trace.splitlines() if pid.search(r)]
                recent = rows[previous_rows:]
                previous_rows = len(rows)
                matches = [(i, m.groups()) for i, r in enumerate(recent)
                           if (m := BIND.search(r)) and int(m[3], 16) == int(size)]
                if len(matches) != 1 or int(phase) != len(entries) or checked != len(entries):
                    raise ValueError('Expected one fresh private GGTT binding for this phase')
                at, (obj, base, length, vm) = matches[0]
                relative = SLOT * 4096 - int(base, 16)
                if not 0 <= relative <= int(length, 16) - 4096:
                    raise ValueError(f'Private i915 allocation at {base} does not cover slot {SLOT}')
                if bindings:
                    last_obj, last_base, last_vm = bindings[-1]
                    if (vm != last_vm or obj == last_obj or not any(
                        f'i915_vma_unbind: obj={last_obj}, offset={last_base} ' in r and f'vm={vm}' in r
                        for r in recent[:at])):
                        raise ValueError('Missing ordered unbind in the same GGTT')
                pte, _, _ = sample()
                if entries and (pte >> 12) == (entries[-1] >> 12):
                    raise ValueError('Replacement did not change the physical page')
                if len(entries) == 2 and ((obj, base, vm) != bindings[0] or pte != entries[0]):
                    raise ValueError('Original i915 binding or raw PTE did not return')
                bindings.append((obj, base, vm))
                entries.append(pte)
                print(f'phase={phase} slot={SLOT} pte={pte:#018x} pipe_surface={original_plane[2]:#x}', flush=True)
                print(relative, file=client.stdin, flush=True)
        finally:
            client.stdin.close()
        if client.wait() or checked != 3:
            raise RuntimeError('Private hardware transaction did not complete')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('binary', type=Path)
    ap.add_argument('--user', required=True)
    args = ap.parse_args()
    account = pwd.getpwnam(args.user)
    if os.geteuid() or account.pw_uid == 0:
        ap.error('Use a privileged trace observer and an unprivileged GPU client')
    inst = Path(tempfile.mkdtemp(prefix='srvros-loan-', dir='/sys/kernel/tracing/instances'))
    try:
        trial(args.binary.resolve(strict=True), account, inst)
    finally:
        (inst / 'tracing_on').write_text('0')
        inst.rmdir()
