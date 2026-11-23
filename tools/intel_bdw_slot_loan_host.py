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
import time

FIRMWARE_SLOT = 8704
INDEPENDENT_SLOT = 12291
BIND = re.compile(r'i915_vma_bind: obj=(\w+), offset=(0x[\da-f]+) size=(0x[\da-f]+).* vm=(\w+)')
TRANSITION = re.compile(r'i915_vma_(?:un)?bind: obj=\w+, offset=(0x[\da-f]+) size=(0x[\da-f]+).* vm=(\w+)')
PCI = Path('/sys/bus/pci/devices/0000:00:02.0')


def trial(binary, user, instance, independent, console, bcs_queue=False, compete=False):
    slot = INDEPENDENT_SLOT if independent else FIRMWARE_SLOT
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
    offset = 0x800000 + slot * 8
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
        protected_offset = 0x800000 + FIRMWARE_SLOT * 8
        protected_page = mmio_page(protected_offset) if independent else None
        protected = None
        ggtt = None
        separator = None
        separator_bind = None

        def checked_trace():
            trace = (instance / 'trace').read_text()
            counts = re.search(r'entries-in-buffer/entries-written:\s*(\d+)/(\d+)', trace)
            if not counts or counts[1] != counts[2]:
                raise ValueError('Incomplete i915 trace')
            return trace

        def verify_protected():
            if not independent:
                return
            entry = struct.unpack_from('<Q', protected_page, protected_offset & 4095)[0]
            if not ggtt or entry != protected:
                raise ValueError('Protected GGTT entry changed during the target loan')
            rows = checked_trace().splitlines()
            if rows.count(separator_bind) != 1:
                raise ValueError('Missing original separator bind event')
            for row in rows[rows.index(separator_bind) + 1:]:
                if 'i915_vma_bind:' not in row and 'i915_vma_unbind:' not in row:
                    continue
                event = TRANSITION.search(row)
                if not event:
                    raise ValueError('Unrecognized i915 VMA event')
                if separator in row and f'vm={ggtt}' in row:
                    raise ValueError('Separator was unbound during the loan')
                begin, size, vm = event.groups()
                if vm == ggtt and 0 <= FIRMWARE_SLOT * 4096 - int(begin, 16) < int(size, 16):
                    raise ValueError('Binding change overlaps the protected slot')

        blitter = mmio_page(0x22234) if bcs_queue else None

        def parked_blitter():
            # The parked batch has not crossed its release semaphore. Bracket
            # the separate context ID DWORD with equal status observations.
            before = struct.unpack_from('<I', blitter, 0x234)[0]
            context = struct.unpack_from('<I', blitter, 0x238)[0]
            after = struct.unpack_from('<I', blitter, 0x234)[0]
            active = (before >> 14) & 3
            valid = (before >> 3) & 3
            current = before & 1
            print(f'bcs_status_before={before:#010x} context={context:#010x} '
                  f'bcs_status_after={after:#010x} active_element={active} valid_lists={valid:#x}', flush=True)
            if (before != after or context == 0xffffffff or active not in (1, 2)
                    or not valid & (1 if current else 2)):
                raise ValueError('No stable executing blitter list at the held checkpoint')
            return context

        plane = mmio_page(0x70180)
        cache = mmio_page(0xb118)
        render = mmio_page(0x21c4)
        power = mmio_page(0x40d0)
        messages = mmio_page(0x8038)
        slice_status = mmio_page(0x138064)
        slice_fuse = mmio_page(0x9120)
        steering = mmio_page(0xfdc)

        def primary():
            return tuple(struct.unpack_from('<I', plane, r)[0] for r in (0x180, 0x19c, 0x1ac))

        def sample():
            verify_protected()
            live = primary()
            if (live != original_plane or edp.read_text().strip() != 'enabled' or
                not live[0] & (1 << 31) or live[0] == 0xffffffff or
                (live[1] & ~8) != live[2] or live[2] >= FIRMWARE_SLOT * 4096):
                raise ValueError(f'Panel state changed or lies outside the admitted range: {original_plane} -> {live}')
            entry = struct.unpack_from('<Q', ptes, offset & 4095)[0]
            if not entry & 1 or entry == 2**64 - 1:
                raise ValueError('Invalid native GGTT entry')
            # Read the currently selected bank. This does not steer i915 or
            # assert that the policy on all physical banks is identical.
            before = struct.unpack_from('<I', slice_status, 0x64)[0]
            fuse = struct.unpack_from('<I', slice_fuse, 0x120)[0]
            selected = struct.unpack_from('<I', steering, 0xfdc)[0]
            l3, scratch = struct.unpack_from('<II', cache, 0x118)
            selected_after = struct.unpack_from('<I', steering, 0xfdc)[0]
            after = struct.unpack_from('<I', slice_status, 0x64)[0]
            if (selected != selected_after or 0xffffffff in (l3, scratch, before, after, fuse, selected)
                    or ((before | after) & 7) & ~((fuse >> 25) & 7)):
                raise ValueError('Invalid L3 policy read')
            print(f'slice_available={(fuse >> 25) & 7:#x} powered_before={before & 7:#x} '
                  f'powered_after={after & 7:#x} selector={selected:#010x}', flush=True)
            return entry, l3, scratch

        def traffic_messages():
            request = struct.unpack_from('<I', power, 0xd0)[0]
            engines = struct.unpack_from('<5I', messages, 0)
            widi = struct.unpack_from('<I', messages, 0x14)[0]
            response = struct.unpack_from('<I', messages, 0x28)[0]
            l3 = struct.unpack_from('<I', messages, 0x38)[0]
            if 0xffffffff in (request, response, *engines, widi, l3):
                raise ValueError('Invalid power-manager message read')
            words = ','.join(hex(word) for word in engines)
            return f'gam_request={request:#x} widi={widi:#x} l3_message={l3:#x} gam_response={response:#x} engines={words}'

        original_plane = None if independent else primary()
        client_mode = '--independent-slot' if independent else '--slot-loan'
        client = stack.enter_context(subprocess.Popen(
            [str(binary), '--competing-context' if compete else client_mode], user=user.pw_uid, group=user.pw_gid,
            extra_groups=os.getgrouplist(user.pw_name, user.pw_gid),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True))
        entries, bindings = [], []
        indirect_context = None
        bcs_context = None
        client_ready = False
        checked = 0
        pid = re.compile(r'-' + str(client.pid) + r'\s')
        previous_rows = 0
        try:
            for line in client.stdout:
                fields = line.split()
                if fields == ['READY']:
                    if not independent or client_ready:
                        raise ValueError('Unexpected start of mapping trial')
                    client_ready = True
                    if console is not None:
                        previous = Path('/sys/class/tty/tty0/active').read_text().strip()
                        if not re.fullmatch(r'tty[1-9][0-9]*', previous):
                            raise ValueError('Cannot identify the recovery VT')
                        stack.callback(subprocess.run, ['chvt', previous[3:]], check=True)
                        subprocess.run(['chvt', str(console)], check=True)
                        # Seat activation returns before fbcon's modeset and
                        # deferred unpins have necessarily reached the device.
                        time.sleep(0.5)
                    (instance / 'trace').write_text('\n')
                    print(1, file=client.stdin, flush=True)
                    continue
                if fields[:1] == ['PAD']:
                    if not client_ready or protected is not None:
                        raise ValueError('Unexpected separator allocation')
                    rows = [r for r in checked_trace().splitlines() if pid.search(r)]
                    matches = [(row, m) for row in rows[previous_rows:] if (m := BIND.search(row))
                               and int(m[3], 16) == int(fields[1])]
                    previous_rows = len(rows)
                    if len(matches) != 1:
                        raise ValueError('Expected one fresh separator binding')
                    bind_row, match = matches[0]
                    obj, base, size, vm = match.groups()
                    begin, end = int(base, 16), int(base, 16) + int(size, 16)
                    if begin <= slot * 4096 < end:
                        raise ValueError('i915 placed a separator on the target slot')
                    if end <= FIRMWARE_SLOT * 4096 or begin > slot * 4096:
                        print(0, file=client.stdin, flush=True)
                        continue
                    # This independent occupied interval prevents a contiguous
                    # target allocation from extending across slot 8704.
                    ggtt = vm
                    separator = f'i915_vma_unbind: obj={obj}, offset={base} '
                    separator_bind = bind_row
                    original_plane = primary()
                    protected = struct.unpack_from('<Q', protected_page, protected_offset & 4095)[0]
                    if not protected & 1 or protected == 2**64 - 1:
                        raise ValueError('Invalid original protected PTE')
                    print(f'protected_slot={FIRMWARE_SLOT} pte={protected:#018x} separator=[{begin:#x},{end:#x})', flush=True)
                    print(1, file=client.stdin, flush=True)
                    continue
                if fields[:1] == ['CHECK']:
                    phase = int(fields[1])
                    pte, l3, scratch = sample()
                    if independent and phase == checked == 3 and len(entries) == 3 and pte == entries[0]:
                        checked += 1
                        print(f'resumed=1 slot={slot} restored_pte={pte:#018x} protected_pte={protected:#018x}', flush=True)
                        print(phase, file=client.stdin, flush=True)
                        continue
                    if phase != checked or len(entries) != phase + 1 or entries[phase] != pte:
                        raise ValueError('GGTT binding changed before the end of the phase')
                    # Gen8 RCS INDIRECT_CTX names the driver-owned restore batch.
                    # Sample it only while the second render context is parked.
                    indirect = struct.unpack_from('<I', render, 0x1c4)[0]
                    if (indirect == 0xffffffff or not (indirect & 63) or
                            not (indirect & ~63) or
                            (indirect_context is not None and indirect != indirect_context)):
                        raise ValueError('Render indirect context missing or changed under the hold')
                    if bcs_queue:
                        context = parked_blitter()
                        if bcs_context is not None and bcs_context != context:
                            raise ValueError('Blitter context changed while the queue was parked')
                        bcs_context = context
                    indirect_context = indirect
                    checked += 1
                    print(f'checked={phase} slot={slot} pte={pte:#018x} '
                          f'rcs_indirect={indirect:#010x} l3sqcreg4={l3:#010x} scratch1={scratch:#010x} {traffic_messages()}', flush=True)
                    print(phase, file=client.stdin, flush=True)
                    continue
                if fields[:1] != ['BIND']:
                    print(line.rstrip(), flush=True)
                    continue
                _, phase, size = fields
                trace = checked_trace()
                rows = [r for r in trace.splitlines() if pid.search(r)]
                recent = rows[previous_rows:]
                previous_rows = len(rows)
                matches = [(i, m.groups()) for i, r in enumerate(recent)
                           if (m := BIND.search(r)) and int(m[3], 16) == int(size)]
                if len(matches) != 1 or int(phase) != len(entries) or checked != len(entries):
                    raise ValueError('Expected one fresh private GGTT binding for this phase')
                at, (obj, base, length, vm) = matches[0]
                if independent:
                    if protected is None or (ggtt is not None and vm != ggtt):
                        raise ValueError('Target binding does not belong to the trial')
                    if 0 <= FIRMWARE_SLOT * 4096 - int(base, 16) < int(length, 16):
                        raise ValueError(f'i915 allocated the target over slot {FIRMWARE_SLOT} at {base}')
                    ggtt = vm
                relative = slot * 4096 - int(base, 16)
                if not 0 <= relative <= int(length, 16) - 4096:
                    raise ValueError(f'Private i915 allocation at {base} does not cover slot {slot}')
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
                print(f'phase={phase} slot={slot} pte={pte:#018x} pipe_surface={original_plane[2]:#x}', flush=True)
                print(relative, file=client.stdin, flush=True)
        finally:
            client.stdin.close()
        if client.wait() or checked != (4 if independent else 3):
            raise RuntimeError('Private hardware transaction did not complete')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('binary', type=Path)
    ap.add_argument('--user', required=True)
    ap.add_argument('--independent-slot', action='store_true', help='Rebind slot 12291 while protecting slot 8704')
    ap.add_argument('--bcs-queue', action='store_true',
                    help='Verify the active blitter execlist and context across the held checkpoints')
    ap.add_argument('--competing-context', action='store_true',
                    help='Challenge the queue hold with five independent requests before changing slot 12291')
    ap.add_argument('--console', type=int, choices=range(1, 64), metavar='VT',
                    help='Use a text VT during the independent trial and return to the previous VT')
    args = ap.parse_args()
    if args.console is not None and not args.independent_slot:
        ap.error('--console requires --independent-slot')
    if args.competing_context and not args.independent_slot:
        ap.error('--competing-context requires --independent-slot')
    account = pwd.getpwnam(args.user)
    if os.geteuid() or account.pw_uid == 0:
        ap.error('Use a privileged trace observer and an unprivileged GPU client')
    inst = Path(tempfile.mkdtemp(prefix='srvros-loan-', dir='/sys/kernel/tracing/instances'))
    try:
        trial(args.binary.resolve(strict=True), account, inst, args.independent_slot, args.console, args.bcs_queue,
              args.competing_context)
    finally:
        (inst / 'tracing_on').write_text('0')
        inst.rmdir()
