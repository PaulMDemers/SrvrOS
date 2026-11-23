#!/usr/bin/env python3
"""Validate target IPv4 broadcast frames on an inherited private QEMU link.

The inherited AF_UNIX stream carries QEMU socket-netdev length-framed Ethernet.
This peer answers ARP only for 10.0.2.3 and opens no host network sockets.
"""
import argparse
import json
from pathlib import Path
import socket
import struct
import time

from ipv4_ttl_test_peer import BROADCAST, GUEST_IP, PEER_IP, PEER_MAC, checksum

LIMITED_BROADCAST = bytes((255, 255, 255, 255))
DIRECTED_BROADCAST = bytes((10, 0, 2, 255))
UDP_PORT = 47771
EXPECTED = {
    b'broadcast-limited': (LIMITED_BROADCAST, BROADCAST),
    b'broadcast-connected': (LIMITED_BROADCAST, BROADCAST),
    b'broadcast-native': (LIMITED_BROADCAST, BROADCAST),
    b'broadcast-directed': (DIRECTED_BROADCAST, BROADCAST),
    b'broadcast-unicast': (PEER_IP, PEER_MAC),
}


class Peer:
    def __init__(self):
        self.stats = {
            'result': 'failed',
            'frames': 0,
            'ignored_frames': 0,
            'arp_requests': 0,
            'broadcast_arp_requests': 0,
            'ipv4_checksums_verified': 0,
            'udp_checksums_verified': 0,
            'udp_zero_checksums': 0,
            'observations': {key.decode(): [] for key in EXPECTED},
        }

    def frame(self, frame):
        if len(frame) < 14:
            raise ValueError('truncated Ethernet header')
        destination, source, kind = frame[:6], frame[6:12], frame[12:14]
        if kind == b'\x08\x06':
            if len(frame) < 42:
                raise ValueError('truncated ARP frame')
            header = struct.unpack_from('!HHBBH', frame, 14)
            sender_mac, sender_ip, target_ip = frame[22:28], frame[28:32], frame[38:42]
            if header == (1, 0x0800, 6, 4, 1) and target_ip in (LIMITED_BROADCAST, DIRECTED_BROADCAST):
                self.stats['broadcast_arp_requests'] += 1
                raise ValueError(f'ARP requested for broadcast {socket.inet_ntoa(target_ip)}')
            if header != (1, 0x0800, 6, 4, 1) or sender_ip != GUEST_IP or target_ip != PEER_IP:
                self.stats['ignored_frames'] += 1
                return []
            if sender_mac != source:
                raise ValueError('ARP sender MAC differs from Ethernet source')
            self.stats['arp_requests'] += 1
            arp = struct.pack('!HHBBH', 1, 0x0800, 6, 4, 2)
            arp += PEER_MAC + PEER_IP + sender_mac + sender_ip
            return [sender_mac + PEER_MAC + b'\x08\x06' + arp]

        if kind != b'\x08\x00':
            self.stats['ignored_frames'] += 1
            return []
        ip = frame[14:]
        if len(ip) < 20 or ip[0] >> 4 != 4:
            raise ValueError('invalid IPv4 header')
        ihl = (ip[0] & 15) * 4
        total = struct.unpack_from('!H', ip, 2)[0]
        fragments = struct.unpack_from('!H', ip, 6)[0]
        if ihl < 20 or total < ihl or total > len(ip) or fragments & 0x3fff:
            raise ValueError('invalid or fragmented IPv4 frame')
        if checksum(ip[:ihl]) != 0:
            raise ValueError('invalid IPv4 header checksum')
        self.stats['ipv4_checksums_verified'] += 1
        if ip[9] != 17 or ip[12:16] != GUEST_IP:
            self.stats['ignored_frames'] += 1
            return []
        udp = ip[ihl:total]
        if len(udp) < 8:
            raise ValueError('truncated UDP header')
        src_port, dst_port, udp_len, udp_sum = struct.unpack_from('!HHHH', udp)
        if udp_len != len(udp) or not src_port:
            raise ValueError('invalid UDP length or source port')
        if udp_sum:
            pseudo = ip[12:20] + b'\x00\x11' + struct.pack('!H', udp_len)
            if checksum(pseudo + udp) != 0:
                raise ValueError('invalid UDP checksum')
            self.stats['udp_checksums_verified'] += 1
        else:
            self.stats['udp_zero_checksums'] += 1
        if dst_port != UDP_PORT:
            self.stats['ignored_frames'] += 1
            return []
        payload = udp[8:]
        if payload not in EXPECTED:
            raise ValueError(f'unexpected probe payload {payload!r}')
        expected_ip, expected_mac = EXPECTED[payload]
        if ip[16:20] != expected_ip or destination != expected_mac:
            raise ValueError(f'incorrect destination for {payload.decode()}')
        observations = self.stats['observations'][payload.decode()]
        observations.append({
            'destination_ip': socket.inet_ntoa(ip[16:20]),
            'destination_mac': ':'.join(f'{octet:02x}' for octet in destination),
            'ttl': ip[8],
            'source_port': src_port,
            'udp_checksum': 'verified' if udp_sum else 'zero',
        })
        if len(observations) > 2:
            raise ValueError(f'duplicate probe payload {payload.decode()}')
        return []

    def finish(self):
        for label, observations in self.stats['observations'].items():
            if len(observations) != 2:
                raise ValueError(f'{label} appeared {len(observations)} times, expected 2')
        self.stats['result'] = 'passed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fd', type=int, required=True, help='inherited connected AF_UNIX stream')
    parser.add_argument('--stats', type=Path, required=True, help='new output JSON file')
    parser.add_argument('--timeout', type=float, default=150)
    parser.add_argument('--max-frames', type=int, default=4096)
    args = parser.parse_args()
    if not 0 < args.timeout <= 180 or not 0 < args.max_frames <= 100000:
        parser.error('invalid timeout or frame limit')

    peer = Peer()
    sock = None
    try:
        sock = socket.socket(fileno=args.fd)
        if sock.family != socket.AF_UNIX or sock.getsockopt(socket.SOL_SOCKET, socket.SO_TYPE) != socket.SOCK_STREAM:
            raise ValueError('fd must be an AF_UNIX SOCK_STREAM')
        sock.getpeername()
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
                    raise ValueError('peer closed with a partial frame')
                peer.finish()
                break
            pending.extend(chunk)
            while len(pending) >= 4:
                length = struct.unpack_from('!I', pending)[0]
                if not 14 <= length <= 65535:
                    raise ValueError('invalid framed Ethernet length')
                if len(pending) < 4 + length:
                    break
                frame = bytes(pending[4:4 + length])
                del pending[:4 + length]
                peer.stats['frames'] += 1
                if peer.stats['frames'] > args.max_frames:
                    raise ValueError('frame limit exceeded')
                for reply in peer.frame(frame):
                    sock.sendall(struct.pack('!I', len(reply)) + reply)
    except Exception as exc:
        peer.stats['error'] = f'{type(exc).__name__}: {exc}'
    finally:
        if sock is not None:
            sock.close()
        with args.stats.open('x', encoding='utf-8') as output:
            json.dump(peer.stats, output, indent=2, sort_keys=True)
            output.write('\n')
    return 0 if peer.stats['result'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
