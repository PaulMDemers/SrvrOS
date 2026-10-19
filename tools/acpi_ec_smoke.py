#!/usr/bin/env python3
"""Synthetic EC through real uACPI regions, query methods and SBS transport."""
import argparse
import os
import re
from pathlib import Path
from vm_sockets import connect_unix
from serial_records import search_complete, complete_lines
import subprocess
import tempfile
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--disconnect-timeout', action='store_true',
                        help='leave disconnect AML asleep past the finite drain deadline')
    parser.add_argument('--query-fault', choices=('storm', 'zero-storm', 'slow'),
                        help='verify query-budget failure halts with live state retained')
    namespace = parser.add_mutually_exclusive_group()
    namespace.add_argument('--namespace-init', action='store_true',
                        help='initialize the namespace after the EC handler connects')
    namespace.add_argument('--namespace-denied', action='store_true',
                           help='verify generated Q35 firmware initialization is refused without wider IO permissions')
    namespace.add_argument('--early-load', action='store_true', help='bind EC before module-level AML on its worker')
    namespace.add_argument('--early-denied', action='store_true', help='refuse module-level EC IO and verify cleanup')
    namespace.add_argument('--early-timeout', action='store_true', help='retain state when namespace worker misses deadline')
    namespace.add_argument('--early-fail', action='store_true', help='contain interpreter self-reset from namespace worker')
    args = parser.parse_args()
    if args.query_fault and (args.disconnect_timeout or args.namespace_denied or
                            args.early_denied or args.early_timeout or args.early_fail):
        parser.error('--query-fault requires a successful runtime setup')
    early = args.early_load or args.early_denied or args.early_timeout or args.early_fail
    if early:
        args.namespace_init = True
    root = Path(__file__).resolve().parents[1]
    out = root / 'build/review' / ('acpi-ec-initialize-denied' if args.namespace_denied else 'acpi-ec-initialize' if args.namespace_init else 'acpi-ec-disconnect-timeout' if args.disconnect_timeout else 'acpi-ec')
    if early:
        out = root / 'build/review' / ('acpi-ec-early-denied' if args.early_denied else 'acpi-ec-early-timeout' if args.early_timeout else 'acpi-ec-early-fail' if args.early_fail else 'acpi-ec-early-load')
    if args.query_fault:
        out = root / 'build/review' / ('acpi-ec-query-' + args.query_fault)
    out.mkdir(parents=True, exist_ok=True)
    output = bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-ec-') as directory:
        tmp = Path(directory)
        subprocess.run(['iasl', '-p', str(tmp/'worker'), str(root/'tools/fixtures/acpi/runtime-worker.asl')],
                       check=True, stdout=subprocess.DEVNULL)
        subprocess.run(['iasl'] + (['-D', 'SRVROS_EARLY_LOAD'] if early else []) + (['-D', 'SRVROS_EARLY_SLOW'] if args.early_timeout else []) + ['-p', str(tmp/'ec'), str(root/'tools/fixtures/acpi/runtime-ec.asl')], check=True, stdout=subprocess.DEVNULL)
        extra_tables = []
        if args.namespace_init:
            subprocess.run(['iasl', '-p', str(tmp/'init'), str(root/'tools/fixtures/acpi/runtime-init.asl')], check=True, stdout=subprocess.DEVNULL)
            extra_tables = ['-acpitable', f'file={tmp}/init.aml']
        iso = boot.build_iso(str(root), str(tmp), 1280, 800, os.environ.copy(), root/'build/acpi-runtime/srvros.elf')

        command = ['qemu-system-x86_64', '-M', 'q35', '-m', '512M', '-cdrom', iso,
                   '-boot', 'd', '-nic', 'none', '-acpitable', f'file={tmp}/worker.aml', '-acpitable', f'file={tmp}/ec.aml',
                   '-serial', f'unix:{tmp}/serial,server=on,wait=off', '-qmp',
                   f'unix:{tmp}/qmp,server=on,wait=off', '-display', 'none',
                   '-monitor', 'none', '-no-reboot'] + extra_tables
        with (out / 'qemu.txt').open('w') as errors:
            process = subprocess.Popen(command, stdout=errors, stderr=errors)
            try:
                serial = connect_unix(tmp / 'serial', 20)
                serial.settimeout(.05)
                qmp = QMP(tmp / 'qmp')
                start = time.monotonic()
                while b'srv> ' not in output and time.monotonic() - start < 60:
                    output.extend(boot.read_for(serial, .2))
                    qmp.boot_if_menu_stalled(output, time.monotonic() - start)
                assert b'srv> ' in output

                def request(text, pattern, timeout=8):
                    serial.sendall(text.encode() + b'\n')
                    part = bytearray()
                    until = time.monotonic() + timeout
                    match = None
                    while not match and time.monotonic() < until:
                        part.extend(boot.read_for(serial, .02))
                        match = search_complete(pattern, part)
                        if b'HALTED' in complete_lines(part) or b'panic' in complete_lines(part):
                            break
                    output.extend(part)
                    (out / 'serial.txt').write_bytes(output)
                    assert match, bytes(part)
                    return match

                def snapshot(queries, notes, transactions, generation=1, current=1, failed=0, poisoned=0):
                    until = time.monotonic() + 8
                    while time.monotonic() < until:
                        match = request('acpiruntime ec status', rb'acpi-ec: ready=(\d+) queries=(\d+) notify=(\d+) queued_in_sample=(\d+) errors=(\d+) foreign=(\d+) attached=(\d+) detached=(\d+) pending=(\d+)\r?\nacpi-ec: generation=(\d+) current=(\d+) soc=(\d+) voltage_uv=(\d+) transactions=(\d+) failed=(\d+) poisoned=(\d+)')
                        v = tuple(map(int, match.groups()))
                        assert v[4] == 0 and v[1] <= queries and v[2] <= notes, v
                        if v[0] == 1 and v[1:3] == (queries, notes) and v[8] == 0 and v[9] == generation and v[14] == failed:
                            assert v[6:8] == (1, 0), v
                            if generation:
                                assert v[3:6] == (2, 0, 1) and v[9:] == (generation, current, 86, 12000000, transactions, failed, poisoned), v
                            return
                    raise AssertionError(('EC did not complete', v))

                if args.early_denied:
                    request('acpiruntime start-early-denied', rb'acpi-runtime: initialization failed status=the requested action has been denied cleanup=yes\r?\nacpi-runtime: REFUSED')
                    state = request('acpiruntime status', rb'acpi-runtime: active=0 irq=0 events=0 work=0 value=0 errors=(\d+) denied=0 heap=0')
                    assert int(state[1]) > 0
                    assert b'acpi-runtime: READY' not in output
                    assert b'acpi-ec: retired=yes attached=1 detached=1' in complete_lines(output)
                    qmp.call('quit'); process.wait(timeout=10)
                    print('acpi-ec: PASS module-level EC denial prevents readiness and cleans partial load with balanced detach/zero heap')
                    return
                if args.early_timeout or args.early_fail:
                    started = time.monotonic()
                    reason = b'namespace load deadline; state retained' if args.early_timeout else b'namespace load reset requires lifecycle owner'
                    halt = request('acpiruntime start-early' if args.early_timeout else 'acpiruntime start-early-fail',
                                   rb'acpi-runtime: HALTED unsafe teardown: ' + reason + rb' heap=(\d+)')
                    assert int(halt[1]) > 0
                    if args.early_timeout:
                        assert 4.5 <= time.monotonic() - started < 8
                    assert b'acpi-runtime: READY' not in output and b'cleanup=yes' not in output
                    # Pass the seven-second AML sleep: an interrupt-enabled hlt
                    # could let the worker continue after the printed halt.
                    output.extend(boot.read_for(serial, 3.5 if args.early_timeout else .3))
                    assert b'namespace worker returned' not in output
                    registers = qmp.call('human-monitor-command', {'command-line': 'info registers'})
                    flags = re.search(r'RFL=([0-9a-fA-F]+)', registers)
                    assert flags and not (int(flags[1], 16) & 0x200), registers
                    qmp.call('quit'); process.wait(timeout=10)
                    print('acpi-ec: PASS early-load ' + ('deadline' if args.early_timeout else 'self-reset') + ' halted visibly with live state retained')
                    return

                if args.namespace_denied:
                    request('acpiruntime start', rb'acpi-runtime: READY')
                    failure = request('acpiruntime ec initialize', rb'acpi-runtime: namespace=FAILED status=0 errors=(\d+) denied=(\d+)')
                    errors, denied = map(int, failure.groups())
                    assert errors > 0 and denied > 0
                    # The API returned success, but unsupported region accesses
                    # must not publish a ready EC/battery service.
                    until = time.monotonic() + 5
                    while True:
                        state = request('acpiruntime ec status', rb'acpi-ec: ready=(\d+) queries=0 notify=0 queued_in_sample=0 errors=(\d+) foreign=0 attached=1 detached=0 pending=(\d+)')
                        ready, failures, pending = map(int, state.groups())
                        assert ready == 0
                        if failures == 1 and pending == 0:
                            break
                        assert time.monotonic() < until
                    request('acpiruntime ec sample', rb'acpi-ec: sample=REFUSED')
                    stopped = request('acpiruntime stop', rb'acpi-runtime: stopped=yes heap=0 errors=(\d+) denied=(\d+)')
                    assert tuple(map(int, stopped.groups())) == (errors, denied)
                    qmp.call('quit')
                    process.wait(timeout=10)
                    print('acpi-ec: PASS refused namespace initialization despite API success; no ready service or sampling, clean bounded teardown, no widened permissions')
                    return

                if args.query_fault:
                    request('acpiruntime start-early' if early else 'acpiruntime start-fixture' if args.namespace_init else 'acpiruntime start', rb'acpi-runtime: READY')
                    request('acpiruntime ec initialize' if args.namespace_init else 'acpiruntime ec start', rb'acpi-ec: setup=queued')
                    snapshot(0, 0, 0, generation=0, current=0)
                    start = time.monotonic()
                    # Inspect both records in one request: the worker can halt
                    # immediately after the command without another serial turn.
                    halted = request('acpiruntime ec query-' + args.query_fault,
                                     rb'acpi-runtime: HALTED unsafe teardown: EC query drain failed heap=(\d+)')
                    assert int(halted[1]) > 0
                    fault = search_complete(rb'acpi-ec: query-failed reason=(\w+) attempts=(\d+) queries=(\d+) active=1', output)
                    assert fault, bytes(output)
                    reason, attempts, queries = fault.groups()
                    expected = (b'deadline', 1, 1) if args.query_fault == 'slow' else (b'limit', 128, 0 if args.query_fault == 'zero-storm' else 128)
                    assert (reason, int(attempts), int(queries)) == expected, fault.groups()
                    if args.query_fault == 'slow':
                        assert 2.5 <= time.monotonic() - start < 8
                    tail = boot.read_for(serial, .3)
                    output.extend(tail)
                    assert b'retired=yes' not in output and b'stopped=yes' not in output
                    assert b'query-failed' not in tail  # No recurring worker behind the halt.
                    registers = qmp.call('human-monitor-command', {'command-line': 'info registers'})
                    flags = re.search(r'RFL=([0-9a-fA-F]+)', registers)
                    assert flags and not (int(flags[1], 16) & 0x200), registers
                    qmp.call('quit'); process.wait(timeout=10)
                    print('acpi-ec: PASS query ' + args.query_fault + ' bounded across worker batches; halted with interrupts disabled and live state retained')
                    return

                for _ in range(2):
                    request('acpiruntime start-early' if early else 'acpiruntime start-fixture' if args.namespace_init else 'acpiruntime start', rb'acpi-runtime: READY')
                    request('acpiruntime ec initialize' if args.namespace_init else 'acpiruntime ec start', rb'acpi-ec: setup=queued')
                    snapshot(0, 0, 0, generation=0, current=0)
                    request('acpiruntime ec foreign', rb'acpi-ec: foreign=REFUSED')
                    request('acpiruntime ec sample', rb'acpi-ec: sample=queued')
                    snapshot(2, 1, 16)
                    qmp.call('system_powerdown')
                    snapshot(3, 2, 16)
                    request('acpiruntime ec queries', rb'acpi-ec: queries=queued')
                    snapshot(35, 18, 32)
                    request('acpiruntime stop', rb'acpi-ec: retired=yes attached=1 detached=1\r?\nacpi-runtime: stopped=yes heap=0 errors=0 denied=0')
                request('acpiruntime start-early' if early else 'acpiruntime start-fixture' if args.namespace_init else 'acpiruntime start', rb'acpi-runtime: READY')
                request('acpiruntime ec start-slow', rb'acpi-ec: setup=queued')
                request('acpiruntime stop', rb'acpi-ec: retired=yes attached=1 detached=1\r?\nacpi-runtime: stopped=yes heap=0 errors=0 denied=0')
                # A failed disconnect must retain the namespace, not report a
                # clean reset after upstream swallowed an AML _REG failure.
                request('acpiruntime start-early' if early else 'acpiruntime start-fixture' if args.namespace_init else 'acpiruntime start', rb'acpi-runtime: READY')
                request('acpiruntime ec initialize' if args.namespace_init else 'acpiruntime ec start', rb'acpi-ec: setup=queued')
                snapshot(0, 0, 0, generation=0, current=0)
                request('acpiruntime ec foreign', rb'acpi-ec: foreign=REFUSED')
                request('acpiruntime ec sample', rb'acpi-ec: sample=queued')
                snapshot(2, 1, 16)
                if args.disconnect_timeout:
                    request('acpiruntime ec disconnect-slow', rb'acpi-ec: disconnect-delay=armed')
                else:
                    request('acpiruntime ec poison', rb'acpi-ec: sample=queued')
                    snapshot(2, 1, 16, current=0, failed=1, poisoned=1)
                    request('acpiruntime ec sample', rb'acpi-ec: sample=REFUSED')
                stop_started = time.monotonic()
                stop_offset = len(output)
                halted = request('acpiruntime stop', rb'acpi-runtime: HALTED unsafe teardown: EC disconnect failed heap=(\d+)')
                assert int(halted[1]) > 0
                if args.disconnect_timeout:
                    assert 4.5 <= time.monotonic() - stop_started < 8, time.monotonic() - stop_started
                tail = boot.read_for(serial, .3)
                output.extend(tail)
                assert b'stopped=yes' not in output[stop_offset:] and b'retired=yes' not in output[stop_offset:]
                qmp.call('quit')
                process.wait(timeout=10)
            finally:
                stop_process(process)
                (out / 'serial.txt').write_bytes(output)
    if args.early_load:
        assert complete_lines(output).count(b'early-load verified read=66 write=60 reg=0 worker=yes') == 4
        print('acpi-ec: PASS module-level read/write before _REG, inherited early handler and worker ownership')
    if args.namespace_init:
        assert complete_lines(output).count(b'namespace=initialized status=0 errors=0 denied=0') == 3
        print('acpi-ec: PASS worker-owned namespace initialization, root/SB/EC ordering, _STA/_INI EC reads and sleep')
    print('acpi-ec: negative teardown=' + ('AML timeout' if args.disconnect_timeout else 'poisoned transport'))
    print('acpi-ec: PASS two lifetimes, _REG read/write, single-worker ownership, SCI-triggered queries/Notify, 32-query batches, deferred queries during full SBS samples, worker-owned disconnect IO, delayed-setup teardown, detached/leak-free reset and failed disconnect halt with state retained')


if __name__ == '__main__':
    main()
