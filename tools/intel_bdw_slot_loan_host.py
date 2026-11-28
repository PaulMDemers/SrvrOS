"""Observe a private i915 GGTT rebind on Air7,2. The GPU client runs as the invoking user."""
import argparse
from collections import Counter
from contextlib import ExitStack
import mmap
import os
from pathlib import Path
import pwd
import re
import select
import struct
import subprocess
import tempfile
import time

FIRMWARE_SLOT = 8704
INDEPENDENT_SLOT = 12291
BIND = re.compile(r'i915_vma_bind: obj=(\w+), offset=(0x[\da-f]+) size=(0x[\da-f]+).* vm=(\w+)')
TRANSITION = re.compile(r'i915_vma_(?:un)?bind: obj=\w+, offset=(0x[\da-f]+) size=(0x[\da-f]+).* vm=(\w+)')
PCI = Path('/sys/bus/pci/devices/0000:00:02.0')


def text_console(stack, console):
    if console is None:
        return
    previous = Path('/sys/class/tty/tty0/active').read_text().strip()
    if not re.fullmatch(r'tty[1-9][0-9]*', previous):
        raise ValueError('Cannot identify the recovery VT')
    stack.callback(subprocess.run, ['chvt', previous[3:]], check=True)
    subprocess.run(['chvt', str(console)], check=True)
    time.sleep(0.5)


def observe_hold(stack, client, console, power, messages, footprint):
    if not select.select([client.stdout], [], [], 5)[0] or client.stdout.readline() != 'READY\n':
        raise ValueError('GPU client missed the baseline handshake')
    text_console(stack, console)
    baseline = footprint()
    if (not baseline[0] & (1 << 31) or baseline[0] == 0xffffffff or
            baseline[1] & ~8 != baseline[2] or baseline[2] & 4095 or baseline[2] >= FIRMWARE_SLOT * 4096 or
            any(not entry & 1 or entry == 2**64 - 1 for entry in baseline[3:])):
        raise ValueError(f'Invalid baseline {baseline}')
    samples = []
    now = lambda: time.monotonic_ns() // 1000

    def record():
        begin = now()
        request = struct.unpack_from('<I', power, 0xd0)[0]
        response = struct.unpack_from('<I', messages, 0x28)[0]
        reset_before = struct.unpack_from('<I', messages, 0x108)[0]
        l3 = struct.unpack_from('<I', messages, 0x38)[0]
        engines = tuple(struct.unpack_from('<I', messages, i * 4)[0] for i in range(5))
        widi = struct.unpack_from('<I', messages, 0x14)[0]
        reset_after = struct.unpack_from('<I', messages, 0x108)[0]
        response_after = struct.unpack_from('<I', messages, 0x28)[0]
        request_after = struct.unpack_from('<I', power, 0xd0)[0]
        if 0xffffffff in (request, response, l3, *engines, widi, reset_before, reset_after, response_after, request_after):
            raise ValueError('Floating traffic register')
        state = (request, response, *(v & 0x31 for v in engines), widi & 0x11,
                 l3 & 3, reset_before & 0x400, reset_after & 0x400, response_after, request_after)
        if footprint() != baseline:
            raise ValueError('Original plane or sampled GGTT entry changed')
        samples.append((begin, now(), state))

    record()
    print(1, file=client.stdin, flush=True)
    deadline = now() + 3_000_000
    while client.poll() is None:
        if now() >= deadline or len(samples) >= 100_000:
            raise RuntimeError('GPU hold trace exceeded the capture budget')
        record()
    record()
    output = client.stdout.read()
    print(output, end='')
    matches = re.findall(r'^hold_times (\d+) (\d+) (\d+) (\d+)$', output, re.M)
    if client.returncode or len(matches) != 1:
        raise ValueError('GPU client did not finish the five-engine transaction')
    times = list(map(int, matches[0]))
    if not (samples[0][1] <= times[0] < times[1] < times[2] < times[3] <= samples[-1][0]):
        raise ValueError('Observer and GPU client intervals disagree')
    print('plane_control=%#x programmed=%#x live=%#x slot8704=%#x slot12291=%#x' % baseline)
    for phase, start, stop in zip(('acquire', 'held', 'release'), times, times[1:]):
        inside = [(a, b, state) for a, b, state in samples if start <= a and b <= stop]
        if not inside:
            raise ValueError(f'Missing complete register sweep in {phase}')
        # Includes the sweep on either side of an event, conservative for each register.
        begins = [a for a, _, _ in inside]
        ends = [b for _, b, _ in inside]
        gap = max(b-a for a, b in zip([start, *begins], [*ends, stop]))
        print(f'phase={phase} start_us={start} end_us={stop} sweeps={len(inside)} max_gap_us={gap}')
        for state, count in sorted(Counter(state for _, _, state in inside).items()):
            print('  req resp rcs vcs0 vcs1 bcs vecs widi l3 widi_reset_begin widi_reset_end resp_end req_end =',
                  *(hex(word) for word in state), f'samples={count}')
    # Boundary sweeps can contain transient replies too. Include them in the
    # overall histogram even when they cannot be attributed to a single phase.
    observed = Counter(state for _, _, state in samples)
    print(f'all_sweeps={len(samples)}')
    for state, count in sorted(observed.items()):
        print('  all', *(hex(word) for word in state), f'samples={count}')


def command_traffic_reader(mmio_page):
    # Map read-only BAR pages under the existing i915 forcewake.
    power, messages, reset, plane = (mmio_page(off) for off in (0x4000, 0x8000, 0x9000, 0x70000))

    def sample():
        def read(page, offset):
            value, = struct.unpack_from('<I', page, offset)
            if value == 0xffffffff:
                raise ValueError(f'Floating command-traffic register at page offset {offset:#x}')
            return value
        def panel():
            control, queued, live = (read(plane, off) for off in (0x180, 0x19c, 0x1ac))
            if not control & (1 << 31) or queued & ~8 != live or live & 4095:
                raise ValueError(f'Primary surface unavailable or changed: control={control:#x} queued={queued:#x} live={live:#x}')
            return control, live
        original = panel()
        values = {'gam_req': read(power, 0xd0), 'gam_reply': read(messages, 0x28),
                  'garb_mbc': read(messages, 0x1c), 'garb_gcp': read(messages, 0x24),
                  'reset_prepare': read(messages, 0x30), 'gdrst': read(reset, 0x41c),
                  'clk_req': read(messages, 0x104), 'clk_reply': read(messages, 0x2c),
                  'widi': read(messages, 0x14), 'l3': read(messages, 0x38),
                  'rcs': read(messages, 0), 'vcs0': read(messages, 4), 'vcs1': read(messages, 8),
                  'bcs': read(messages, 0xc), 'vecs': read(messages, 0x10),
                  'widi_reset': read(messages, 0x108),
                  'widi_tlb_ctl_before': read(power, 0x278),
                  # All ones is a possible fully populated valid vector.
                  'widi_tlb_valid0': struct.unpack_from('<I', power, 0xdc0)[0],
                  'widi_tlb_valid1': struct.unpack_from('<I', power, 0xdc4)[0],
                  'widi_tlb_ctl_after': read(power, 0x278)}
        if original != panel() or values['gam_req'] != read(power, 0xd0) or values['clk_req'] != read(messages, 0x104):
            raise ValueError('Unstable display or traffic request during checkpoint')
        return original, read(plane, 0x40), values
    return sample


def observe_marker_csb(binary, user, blitter, traffic=None):
    def snapshot():
        # The registers require separate DWORD loads for status and context ID.
        word = lambda offset: struct.unpack_from('<I', blitter, offset)[0]
        ptr = word(0x3a0)
        entries = [(word(0x370+8*i), word(0x374+8*i)) for i in range(6)]
        if word(0x3a0) != ptr or (ptr & 15) >= 6 or word(0x234) & 0xc01c:
            raise ValueError('Unstable or busy BCS queue')
        return ptr, entries

    with subprocess.Popen([str(binary), '--marker-csb'], user=user.pw_uid, group=user.pw_gid,
                          extra_groups=os.getgrouplist(user.pw_name, user.pw_gid),
                          stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as client:
        try:
            snapshots = []
            traffic_samples = []
            for phase in range(2):
                ready, _, _ = select.select([client.stdout], [], [], 5)
                if not ready or client.stdout.readline().strip() != f'CHECK {phase}':
                    raise RuntimeError('Missing blitter marker checkpoint')
                snapshots.append(snapshot())
                if traffic:
                    traffic_samples.append(traffic())
                print(phase, file=client.stdin, flush=True)
            output, _ = client.communicate(timeout=5)
        finally:
            if client.poll() is None:
                client.kill()
        if client.returncode:
            raise RuntimeError('Device marker comparison failed')
    before, _ = snapshots[0]
    after, entries = snapshots[1]
    first = ((before & 15)+1) % 6
    last = (first+1) % 6
    enter, _ = entries[first]
    done, context = entries[last]
    if ((after & 15) != last or (before >> 8) != (after >> 8) or
            (enter & 0x801f) != 1 or (done & 0x801f) != 0x18 or not context):
        raise ValueError(f'Unexpected CSB events {before:#x} -> {after:#x} {entries}')
    print(output, end='')
    print(f'BCS CSB {before:#x} -> {after:#x}, enter={enter:#x} complete={done:#x} context={context:#x}')

    if traffic:
        if traffic_samples[0][0] != traffic_samples[1][0]:
            raise ValueError('Different primary surfaces across the BCS command')
        for label, (surface, frame, values) in zip(('before', 'after'), traffic_samples):
            registers = ' '.join(f'{name}={value:#x}' for name, value in values.items())
            print(f'{label} plane={surface[0]:#x} surface={surface[1]:#x} frame={frame} {registers}')


def trial(binary, user, instance, independent, console, bcs_queue=False, compete=False, observe=False, marker_csb=False, command_traffic=False):
    slot = INDEPENDENT_SLOT if independent or observe else FIRMWARE_SLOT
    panel = PCI / 'drm'
    edp, = panel.glob('card*/card*-eDP-1/enabled')
    if (Path('/sys/class/dmi/id/product_name').read_text().strip() != 'MacBookAir7,2' or
        (PCI / 'vendor').read_text().strip() != '0x8086' or
        (PCI / 'device').read_text().strip() != '0x1626' or
        (PCI / 'driver').resolve().name != 'i915' or edp.read_text().strip() != 'enabled'):
        raise ValueError('Requires the Air7,2 with an active i915 eDP panel')
    if not observe and not marker_csb:
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

        if marker_csb:
            return observe_marker_csb(binary, user, mmio_page(0x22000),
                                      command_traffic_reader(mmio_page) if command_traffic else None)

        ptes = mmio_page(offset)
        protected_offset = 0x800000 + FIRMWARE_SLOT * 8
        protected_page = mmio_page(protected_offset) if independent or observe else None
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

        if observe:
            def footprint():
                return (*primary(), struct.unpack_from('<Q', protected_page, protected_offset & 4095)[0],
                        struct.unpack_from('<Q', ptes, offset & 4095)[0])
            with subprocess.Popen([str(binary), '--observe-hold'], user=user.pw_uid, group=user.pw_gid,
                                  extra_groups=os.getgrouplist(user.pw_name, user.pw_gid),
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True) as client:
                try:
                    observe_hold(stack, client, console, power, messages, footprint)
                finally:
                    if client.poll() is None:
                        client.kill()
            return

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
            reset_before = struct.unpack_from('<I', messages, 0x108)[0]
            engines = struct.unpack_from('<5I', messages, 0)
            widi = struct.unpack_from('<I', messages, 0x14)[0]
            response = struct.unpack_from('<I', messages, 0x28)[0]
            l3 = struct.unpack_from('<I', messages, 0x38)[0]
            reset_after = struct.unpack_from('<I', messages, 0x108)[0]
            if 0xffffffff in (request, response, *engines, widi, l3, reset_before, reset_after):
                raise ValueError('Invalid power-manager message read')
            words = ','.join(hex(word) for word in engines)
            return (f'gam_request={request:#x} widi={widi:#x} l3_message={l3:#x} gam_response={response:#x} '
                    f'engines={words} reset_flow_before={reset_before:#x} reset_flow_after={reset_after:#x}')

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
                    text_console(stack, console)
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
                        print(f'resumed=1 slot={slot} restored_pte={pte:#018x} protected_pte={protected:#018x} '
                              f'{traffic_messages()}', flush=True)
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
    ap.add_argument('--observe-hold', action='store_true',
                    help='Trace acquisition and release of five private queues with both slots left in place')
    ap.add_argument('--command-traffic', action='store_true',
                    help='Bracket a blitter request with WIN clocks, GAM, flush and primary-plane samples')
    ap.add_argument('--marker-csb', action='store_true', help='Read fresh blitter status events around one request')
    ap.add_argument('--bcs-queue', action='store_true',
                    help='Verify the active blitter execlist and context across the held checkpoints')
    ap.add_argument('--competing-context', action='store_true',
                    help='Challenge the queue hold with five independent requests before changing slot 12291')
    ap.add_argument('--console', type=int, choices=range(1, 64), metavar='VT',
                    help='Use a text VT during the independent trial and return to the previous VT')
    args = ap.parse_args()
    args.marker_csb |= args.command_traffic
    if args.marker_csb and (args.observe_hold or args.independent_slot or args.bcs_queue or args.competing_context or args.console):
        ap.error('--marker-csb runs a standalone request')
    if args.observe_hold and (args.independent_slot or args.competing_context or args.bcs_queue):
        ap.error('--observe-hold is independent of slot loans')
    if args.console is not None and not (args.independent_slot or args.observe_hold):
        ap.error('--console requires --independent-slot or --observe-hold')
    if args.competing_context and not args.independent_slot:
        ap.error('--competing-context requires --independent-slot')
    account = pwd.getpwnam(args.user)
    if os.geteuid() or account.pw_uid == 0:
        ap.error('Use a privileged trace observer and an unprivileged GPU client')
    inst = None if args.observe_hold or args.marker_csb else Path(tempfile.mkdtemp(prefix='srvros-loan-', dir='/sys/kernel/tracing/instances'))
    try:
        trial(args.binary.resolve(strict=True), account, inst, args.independent_slot, args.console, args.bcs_queue,
              args.competing_context, args.observe_hold, args.marker_csb, args.command_traffic)
    finally:
        if inst is not None:
            (inst / 'tracing_on').write_text('0')
            inst.rmdir()
