#!/usr/bin/env python3
"""Passive Linux MCU timing capture; never resets/unbinds a device.

Uses usbmon GETX (64-byte headers) and per-reader loss statistics. Persist only
the selected adapter's endpoint8 OUT/endpoint5 IN metadata. Firmware calibration
IDs/arguments and response headers are retained; arbitrary payload bytes are not.
Host/HCD observations are not independent wire evidence.
"""
import argparse
import ctypes
import errno
import fcntl
import json
import os
from pathlib import Path
import select
import struct
import time

GETX = 0x4018920A  # _IOW(0x92,10,{void *hdr,void *data,size_t alloc}) on x86_64
STATS = 0x80089203  # _IOR(0x92,3,{u32 queued,u32 dropped})
HEADER = struct.Struct('<QBBBBHccqiiII8siiii')


def event(header, data, bus, address):
    if len(header) != 64:
        raise ValueError('usbmon header must be 64 bytes')
    tag, kind, transfer, ep, addr, b, _, flag, sec, usec, status, length, cap, *_ = HEADER.unpack(header)
    if (b, addr) != (bus, address) or transfer != 3 or ep not in (0x08, 0x85):
        return None
    if kind not in b'SCE' or not 0 <= usec < 1000000 or cap > len(data):
        raise ValueError('invalid selected usbmon record')
    r = dict(tag=tag, kind=chr(kind), endpoint=ep, timestamp_us=sec*1000000+usec,
             status=status, length=length, captured=cap)
    # GETX may omit payload; length alone never permits decoding.
    payload = data[:cap] if flag == b'\0' else b''
    if len(payload) >= 4 and ((ep == 8 and kind == ord('S')) or
                              (ep == 0x85 and kind == ord('C') and status == 0)):
        h = int.from_bytes(payload[:4], 'little')
        if ep == 8 and h >> 30 == 1 and (h >> 27) & 7 == 2:
            r.update(command=(h >> 20) & 127, sequence=(h >> 16) & 15,
                     wire_length=h & 65535)
            if r['command'] == 31 and r['wire_length'] == 8 and length == 16 and len(payload) >= 12:
                r.update(calibration_id=int.from_bytes(payload[4:8], 'little'),
                         argument=int.from_bytes(payload[8:12], 'little'))
        elif ep == 0x85 and h >> 30 == 1:
            r.update(sequence=(h >> 16) & 15, event=(h >> 20) & 15,
                     wire_length=h & 0x3fff)
    return r


class Correlator:
    def __init__(self):
        self.pending = {}
        self.records = []
        self.orphans = 0
        self.reused = 0

    def consume(self, r):
        if r is None:
            return
        key = r['tag']
        if r['kind'] == 'S':
            old = self.pending.pop(key, None)
            if old is not None:
                old['incomplete'] = 'tag reused'
                self.reused += 1
            item = dict(r)
            self.pending[key] = item
            self.records.append(item)
            return
        item = self.pending.get(key)
        if item is None or item['endpoint'] != r['endpoint']:
            self.orphans += 1
            return
        del self.pending[key]
        elapsed = r['timestamp_us'] - item['timestamp_us']
        item.update(completed_us=r['timestamp_us'], elapsed_us=elapsed,
                    completion_status=r['status'], actual=r['length'])
        if elapsed < 0:
            item['incomplete'] = 'clock reversed'
        if item['endpoint'] == 0x85:
            for k in ('sequence', 'event', 'wire_length'):
                if k in r:
                    item['reply_' + k] = r[k]

    def calibrations(self):
        result = []
        # Serialize by submission, preserve failed/missing OUT and IN. Matching
        # is bounded by the next command, so a reused four-bit sequence cannot
        # steal a later reply. Nonmatching responses remain visible in attempts.
        commands = [r for r in self.records if r['endpoint'] == 8 and 'command' in r]
        for i, cmd in enumerate(commands):
            if cmd['command'] != 31:
                continue
            end = commands[i+1]['timestamp_us'] if i+1 < len(commands) else float('inf')
            attempts = [r for r in self.records if r['endpoint'] == 0x85 and
                        cmd['timestamp_us'] <= r['timestamp_us'] < end]
            matches = [r for r in attempts if r.get('completion_status') == 0 and
                       r.get('reply_sequence') == cmd['sequence'] and r.get('reply_event') == 0
                       and r.get('actual', 0) >= 8 and r.get('elapsed_us', -1) >= 0
                       and r.get('completed_us', end) <= end]
            item = dict(command=cmd, receive_attempts=attempts, matching_replies=len(matches))
            if len(matches) == 1 and cmd.get('completion_status') == 0 and cmd.get('elapsed_us', -1) >= 0:
                reply = matches[0]
                item.update(command_to_reply_us=reply['completed_us']-cmd['timestamp_us'],
                            out_completion_to_reply_us=reply['completed_us']-cmd['completed_us'],
                            in_wait_us=reply['elapsed_us'])
            result.append(item)
        return result


def identity(port):
    wanted = ('7392', 'a711', '0100')
    if tuple((port / n).read_text().strip() for n in ('idVendor', 'idProduct', 'bcdDevice')) != wanted:
        raise ValueError('wrong adapter identity')
    return int((port/'busnum').read_text()), int((port/'devnum').read_text())


def capture(port, output, seconds):
    if struct.calcsize('P') != 8:
        raise ValueError('GETX layout is qualified only for 64-bit Linux')
    bus, address = identity(port)
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    fd = os.open(f'/dev/usbmon{bus}', os.O_RDONLY | os.O_NONBLOCK)
    header = ctypes.create_string_buffer(64)
    data = ctypes.create_string_buffer(65536)
    request = struct.pack('QQQ', ctypes.addressof(header), ctypes.addressof(data), len(data))
    reader, total, reason = Correlator(), 0, 'deadline'
    started = time.monotonic()
    try:
        (output/'ready.json').write_text(json.dumps(dict(bus=bus, address=address, seconds=seconds))+'\n')
        while time.monotonic()-started < seconds:
            if not select.select([fd], [], [], .05)[0]:
                continue
            try:
                fcntl.ioctl(fd, GETX, request)
            except OSError as e:
                if e.errno == errno.EAGAIN:
                    continue
                raise
            total += 1
            reader.consume(event(header.raw, data.raw, bus, address))
            if len(reader.records) >= 30000:
                reason = 'selected record limit'
                break
        stats = bytearray(8)
        fcntl.ioctl(fd, STATS, stats, True)
        queued, dropped = struct.unpack('II', stats)
        if identity(port) != (bus, address):
            reason = 'identity/address changed'
        report = dict(kernel=os.uname().release, port=port.name, bus=bus, address=address,
                      reason=reason, total_events=total, queued=queued, dropped=dropped,
                      orphan_completions=reader.orphans, tag_reuse=reader.reused,
                      pending=len(reader.pending), records=reader.records,
                      calibrations=reader.calibrations(),
                      note='Filtered Linux HCD observations; no arbitrary packet payload retained.')
        (output/'capture.json').write_text(json.dumps(report, indent=2)+'\n')
        print(json.dumps({k:v for k,v in report.items() if k not in ('records','calibrations')}), flush=True)
    finally:
        os.close(fd)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--seconds', type=int, default=40)
    args = p.parse_args()
    if not 5 <= args.seconds <= 120 or '/' in args.port or '..' in args.port:
        p.error('Use a USB port name and a 5–120 second bound')
    capture(Path('/sys/bus/usb/devices')/args.port, args.output, args.seconds)
