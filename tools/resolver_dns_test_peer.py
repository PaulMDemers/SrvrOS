#!/usr/bin/env python3
"""Bounded, inherited-socket Ethernet/DNS peer for private resolver tests.

The caller owns QEMU and supplies one AF_UNIX stream endpoint via --fd. Frames
use QEMU socket-netdev's 32-bit network-order length prefix. This program never
opens an internet socket or starts another process.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import struct
import time

GUEST_IP = bytes((10, 0, 2, 15))
DNS_IP = bytes((10, 0, 2, 3))
DNS_MAC = bytes.fromhex('525400123503')
ETH_BROADCAST = b'\xff' * 6
MAX_FRAME = 65535

# These names are local fixture identifiers, not requests to a real resolver.
CASES = {
    'good.srvros.test': (16, 'good'),
    'large.srvros.test': (16, 'large'),
    'missing.srvros.test': (16, 'missing'),
    'nodata.srvros.test': (16, 'nodata'),
    'temporary.srvros.test': (16, 'temporary'),
    'truncated.srvros.test': (16, 'truncated'),
    'mismatch.srvros.test': (16, 'mismatch'),
    'malformed.srvros.test': (16, 'malformed'),
    'service.srvros.test': (33, 'service'),
    'legacy.srvros.test': (1, 'legacy'),
}


def checksum(data):
    if len(data) & 1:
        data += b'\0'
    total = sum(struct.unpack('!%dH' % (len(data) // 2), data))
    while total >> 16:
        total = (total & 0xffff) + (total >> 16)
    return (~total) & 0xffff


def qname(raw):
    labels = []
    offset = 12
    wire_length = 1
    while True:
        if offset >= len(raw):
            raise ValueError('missing DNS name')
        count = raw[offset]
        offset += 1
        if count == 0:
            break
        if count > 63 or offset + count > len(raw):
            raise ValueError('invalid DNS label')
        label = raw[offset:offset + count]
        if any(c < 33 or c > 126 or c == 46 for c in label):
            raise ValueError('invalid DNS label byte')
        labels.append(label.decode('ascii').lower())
        offset += count
        wire_length += count + 1
        if wire_length > 255:
            raise ValueError('DNS name too long')
    if offset + 4 > len(raw):
        raise ValueError('missing DNS question')
    qtype, qclass = struct.unpack_from('!HH', raw, offset)
    return '.'.join(labels), qtype, qclass, raw[12:offset + 4]


def dns_reply(query, question, kind, transaction_id=None):
    ident, flags = struct.unpack_from('!HH', query)
    if transaction_id is not None:
        ident = transaction_id
    rcode = {'missing': 3, 'temporary': 2}.get(kind, 0)
    flags = 0x8080 | (flags & 0x0100) | rcode
    if kind == 'truncated':
        flags |= 0x0200
    if kind == 'service':
        target = b'\x04host\x06srvros\x04test\x00'
        rdata = struct.pack('!HHH', 10, 5, 8080) + target
        target_offset = 12 + len(question) + 12 + 6
        if target_offset >= 0x4000:
            raise ValueError('SRV target compression offset too large')
        answer = b'\xc0\x0c' + struct.pack('!HHIH', 33, 1, 60, len(rdata)) + rdata
        additional = struct.pack('!H', 0xc000 | target_offset) + struct.pack('!HHIH', 1, 1, 60, 4) + bytes((192, 0, 2, 123))
        return struct.pack('!HHHHHH', ident, flags, 1, 1, 0, 1) + question + answer + additional
    rdata = None
    record_type = 16
    if kind in ('good', 'mismatch', 'malformed'):
        text = b'srvros valid after malformed' if kind == 'malformed' else b'srvros resolver fixture'
        rdata = bytes((len(text),)) + text
    elif kind == 'large':
        rdata = b''.join(bytes((220,)) + bytes((ord('a') + i,)) * 220 for i in range(5))
    elif kind == 'legacy':
        record_type = 1
        rdata = bytes((192, 0, 2, 123))
    header = struct.pack('!HHHHHH', ident, flags, 1, int(rdata is not None), 0, 0)
    if rdata is None:
        return header + question
    answer = b'\xc0\x0c' + struct.pack('!HHIH', record_type, 1, 60, len(rdata)) + rdata
    return header + question + answer


def ipv4_reply(guest_mac, source_port, payload):
    udp = struct.pack('!HHHH', 53, source_port, 8 + len(payload), 0) + payload
    header = struct.pack('!BBHHHBBH4s4s', 0x45, 0, 20 + len(udp), 0, 0, 64, 17, 0, DNS_IP, GUEST_IP)
    header = header[:10] + struct.pack('!H', checksum(header)) + header[12:]
    return guest_mac + DNS_MAC + b'\x08\x00' + header + udp


def frame_replies(frame, stats):
    if len(frame) < 14:
        stats['invalid_frames'] += 1
        return []
    destination, source, kind = frame[:6], frame[6:12], frame[12:14]
    if kind == b'\x08\x06':
        if len(frame) < 42:
            stats['invalid_frames'] += 1
            return []
        htype, ptype, hlen, plen, op = struct.unpack_from('!HHBBH', frame, 14)
        sender_mac, sender_ip, target_ip = frame[22:28], frame[28:32], frame[38:42]
        if (htype, ptype, hlen, plen, op) != (1, 0x0800, 6, 4, 1) or sender_ip != GUEST_IP or target_ip != DNS_IP or sender_mac != source:
            stats['ignored_frames'] += 1
            return []
        stats['arp_requests'] += 1
        arp = struct.pack('!HHBBH', 1, 0x0800, 6, 4, 2) + DNS_MAC + DNS_IP + sender_mac + sender_ip
        return [sender_mac + DNS_MAC + b'\x08\x06' + arp]
    if kind != b'\x08\x00' or destination not in (DNS_MAC, ETH_BROADCAST):
        stats['ignored_frames'] += 1
        return []
    ip = frame[14:]
    if len(ip) < 20 or ip[0] >> 4 != 4:
        stats['invalid_frames'] += 1
        return []
    ihl = (ip[0] & 15) * 4
    total = struct.unpack_from('!H', ip, 2)[0]
    fragment = struct.unpack_from('!H', ip, 6)[0]
    if ihl < 20 or total < ihl + 8 or total > len(ip) or fragment & 0x3fff or checksum(ip[:ihl]) != 0:
        stats['invalid_frames'] += 1
        return []
    if ip[9] != 17 or ip[12:16] != GUEST_IP or ip[16:20] != DNS_IP:
        stats['ignored_frames'] += 1
        return []
    udp = ip[ihl:total]
    source_port, dest_port, udp_len, udp_sum = struct.unpack_from('!HHHH', udp)
    if dest_port != 53:
        stats['ignored_frames'] += 1
        return []
    if not source_port or udp_len != len(udp) or udp_len < 20:
        stats['invalid_frames'] += 1
        return []
    if udp_sum and checksum(GUEST_IP + DNS_IP + b'\x00\x11' + struct.pack('!H', udp_len) + udp) != 0:
        stats['invalid_frames'] += 1
        return []
    query = udp[8:]
    try:
        ident, flags, qd, an, ns, ar = struct.unpack_from('!HHHHHH', query)
        if flags & 0xf800 or qd != 1 or an != 0 or ns != 0:
            raise ValueError('not a single-question DNS query')
        name, qtype, qclass, question = qname(query)
        if name not in CASES or (qtype, qclass) != (CASES[name][0], 1):
            raise ValueError('unexpected DNS question')
    except (ValueError, struct.error):
        stats['rejected_queries'] += 1
        return []
    case = CASES[name][1]
    stats['queries'][case] = stats['queries'].get(case, 0) + 1
    responses = []
    if case == 'mismatch':
        # A wrong-ID response and a wrong-question response precede the valid one.
        responses.append(dns_reply(query, question, case, transaction_id=ident ^ 1))
        wrong = bytes((5,)) + b'wrong' + question[1 + len('mismatch'):]
        responses.append(dns_reply(query, wrong, case))
        stats['deliberate_mismatch_replies'] += 2
    if case == 'malformed':
        malformed = dns_reply(query, question, case)
        rdlength_offset = 12 + len(question) + 2 + 2 + 2 + 4
        declared = struct.unpack_from('!H', malformed, rdlength_offset)[0]
        malformed = (malformed[:rdlength_offset] + struct.pack('!H', declared + 5)
                     + malformed[rdlength_offset + 2:])
        responses.append(malformed)
        stats['deliberate_malformed_replies'] += 1
    responses.append(dns_reply(query, question, case))
    stats['dns_responses'] += len(responses)
    return [ipv4_reply(source, source_port, response) for response in responses]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fd', required=True, type=int, help='inherited connected AF_UNIX SOCK_STREAM')
    parser.add_argument('--stats', required=True, type=Path, help='new output JSON path')
    parser.add_argument('--timeout', type=float, default=150, help='wall deadline, at most 180 seconds')
    parser.add_argument('--max-frames', type=int, default=4096)
    args = parser.parse_args()
    if not 0 < args.timeout <= 180 or not 0 < args.max_frames <= 100000:
        parser.error('invalid timeout or frame limit')
    stats = {'result': 'failed', 'frames': 0, 'arp_requests': 0, 'dns_responses': 0,
             'queries': {}, 'invalid_frames': 0, 'rejected_queries': 0, 'ignored_frames': 0,
             'deliberate_mismatch_replies': 0, 'deliberate_malformed_replies': 0}
    sock = None
    try:
        sock = socket.socket(fileno=args.fd)
        if sock.family != socket.AF_UNIX or sock.getsockopt(socket.SOL_SOCKET, socket.SO_TYPE) != socket.SOCK_STREAM:
            raise ValueError('fd must be an AF_UNIX SOCK_STREAM')
        sock.getpeername()  # Require a connected endpoint, never create a network socket.
        deadline = time.monotonic() + args.timeout
        pending = bytearray()
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                raise TimeoutError('peer deadline expired')
            sock.settimeout(min(left, 1.0))
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                if pending:
                    raise ValueError('peer closed with partial frame')
                stats['result'] = 'eof'
                break
            pending.extend(chunk)
            while len(pending) >= 4:
                length = struct.unpack_from('!I', pending)[0]
                if not 14 <= length <= MAX_FRAME:
                    raise ValueError('invalid framed Ethernet length')
                if len(pending) < 4 + length:
                    break
                frame = bytes(pending[4:4 + length])
                del pending[:4 + length]
                stats['frames'] += 1
                if stats['frames'] > args.max_frames:
                    raise ValueError('frame limit exceeded')
                for response in frame_replies(frame, stats):
                    sock.sendall(struct.pack('!I', len(response)) + response)
    except Exception as exc:
        stats['error'] = f'{type(exc).__name__}: {exc}'
    finally:
        if sock is not None:
            sock.close()
        # Exclusive creation preserves an earlier run's evidence.
        with args.stats.open('x', encoding='utf-8') as output:
            json.dump(stats, output, indent=2, sort_keys=True)
            output.write('\n')
    return 0 if stats['result'] == 'eof' else 1


if __name__ == '__main__':
    raise SystemExit(main())
