#!/usr/bin/env python3
"""Bounded Linux usbmon capture of Edimax initialization registers only.
Run as root after modprobe usbmon. Observe a user-operated cold replug in Ubuntu;
never reset/unbind the adapter. This records host requests, not wire handshakes.
"""
import argparse
import json
import os
from pathlib import Path
import select
import time

REGISTERS = {0, 0x20, 0x80, 0x238, 0x730, 0x800, 0x9a0, 0x9a4,
             0x9a8, 0x9c4, 0xa6c, 0x1000, 0x1004}


def submission(line):
    p = line.split()
    if len(p) < 11 or p[2] != 'S' or p[4] != 's':
        return None
    try:
        direction, bus, address, endpoint = p[3].split(':')
        typ, req, val, index, length = (int(x, 16) for x in p[5:10])
        if direction not in ('Ci', 'Co') or int(endpoint) != 0:
            return None
        allowed = (typ in (0x40, 0xc0) and req in (6, 7) and
                   val == 0 and index in REGISTERS and length == 4)
        allowed |= typ == 0x40 and req == 1 and val == 1 and index == 0 and length == 0
        if not allowed:
            return None
        return dict(tag=p[0], start_us=int(p[1]), bus=int(bus), address=int(address),
                    type=typ, request=req, value=val, index=index, length=length,
                    data=payload(p, 11), status=None, actual=None, elapsed_us=None)
    except (ValueError, IndexError):
        return None


def payload(parts, pos):
    if len(parts) <= pos or parts[pos] != '=':
        return None
    text = ''.join(parts[pos+1:])
    try:
        return bytes.fromhex(text)[:4].hex()
    except ValueError:
        return None


def completion(line, pending):
    p = line.split()
    if len(p) < 6 or p[2] not in ('C', 'E') or p[0] not in pending:
        return
    r = pending[p[0]]
    try:
        direction, bus, addr, ep = p[3].split(':')
        if (int(bus), int(addr), int(ep)) != (r['bus'], r['address'], 0):
            return
        if direction != ('Ci' if r['type'] & 0x80 else 'Co'):
            return
        r.update(status=int(p[4]), actual=int(p[5]), elapsed_us=int(p[1])-r['start_us'])
        if r['type'] & 0x80:
            r['data'] = payload(p, 6)
        pending.pop(p[0])
    except (ValueError, IndexError):
        return


def identity(port):
    try:
        if tuple((port/x).read_text().strip() for x in ('idVendor','idProduct','bcdDevice')) != ('7392','a711','0100'):
            return None
        return int((port/'busnum').read_text()), int((port/'devnum').read_text())
    except (OSError, ValueError):
        return None


def capture(port, output, seconds):
    first = identity(port)
    if first is None:
        raise RuntimeError('Expected Edimax 7392:a711 revision 0100 at selected port')
    output.mkdir(parents=True, exist_ok=False)
    fd = os.open(f'/sys/kernel/debug/usb/usbmon/{first[0]}u', os.O_RDONLY | os.O_NONBLOCK)
    records, pending, addresses = [], {}, {first[1]}
    absent = False
    reconnected = None
    started = time.monotonic()
    reason = 'deadline'
    buffer = b''
    (output/'ready.json').write_text(json.dumps(dict(bus=first[0], initial_address=first[1], seconds=seconds))+'\n')
    print('READY: bounded register capture is active; no device changes are performed.', flush=True)
    try:
        while time.monotonic()-started < seconds:
            current = identity(port)
            if current is None:
                absent = True
            elif current[0] == first[0]:
                addresses.add(current[1])
                if absent and current[1] != first[1] and reconnected is None:
                    reconnected = time.monotonic()
            if reconnected is not None and time.monotonic()-reconnected >= 20:
                reason = 'reconnected observation complete'
                break
            if len(records) >= 20000:
                reason = 'record limit; incomplete'
                break
            if not select.select([fd], [], [], .005)[0]:
                continue
            try:
                chunk = os.read(fd, 65536)
            except BlockingIOError:
                continue
            if not chunk:
                reason = 'monitor EOF; incomplete'
                break
            buffer += chunk
            while b'\n' in buffer:
                line, buffer = buffer.split(b'\n', 1)
                line = line.decode('ascii', errors='replace')
                r = submission(line)
                if r is not None:
                    records.append(r)
                    pending[r['tag']] = r
                else:
                    completion(line, pending)
    finally:
        os.close(fd)
        # Address discovery may lag the first probe requests. Keep candidates in
        # bounded memory, then persist only addresses verified on this port.
        selected = [r for r in records if r['bus']==first[0] and r['address'] in addresses]
        report = dict(kernel=os.uname().release, port=port.name, reason=reason,
                      observed_disconnect=absent, observed_reconnect=reconnected is not None,
                      initial_address=first[1], addresses=sorted(addresses),
                      note='Filtered host URBs, not wire evidence; missing records are not proof of absence.',
                      records=selected)
        (output/'capture.json').write_text(json.dumps(report, indent=2)+'\n')
        os.sync()
    print(f'Saved {len(selected)} register requests: {output}/capture.json', flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--seconds', type=int, default=240)
    a = p.parse_args()
    if not 30 <= a.seconds <= 600 or '/' in a.port or '..' in a.port:
        p.error('Use a USB port name and a 30–600 second bound')
    capture(Path('/sys/bus/usb/devices')/a.port, a.output, a.seconds)
