#!/usr/bin/env python3
"""Private Ethernet peer for the target IPv4 TTL guest test.

The inherited AF_UNIX stream carries QEMU socket-netdev length-framed Ethernet.
The peer supplies ARP and two minimal TCP conversations, with no host network
socket. It validates packet checksums and records TTL observations by probe.
"""
import argparse
import json
from pathlib import Path
import socket
import struct
import time

GUEST_IP = bytes((10, 0, 2, 15))
PEER_IP = bytes((10, 0, 2, 3))
PEER_MAC = bytes.fromhex('525400123503')
BROADCAST = b'\xff' * 6
UDP_PORT = 47770
TCP_OUT_PORT = 47771
TCP_IN_PORT = 47772
FIRST_PEER_PORT = 47773
UDP_TTLS = {
    b'udp-default': 64,
    b'udp-changed': 37,
    b'udp-isolated': 64,
    b'udp-reset': 64,
    b'udp-raw': 53,
    b'tcp-listener-ready': 64,
}
TCP_TTLS = {
    b'tcp-out': 43,
    b'tcp-inherited': 47,
    b'tcp-accepted': 49,
}
SYN = 0x02
ACK = 0x10
FIN = 0x01
RST = 0x04


def checksum(data):
    if len(data) & 1:
        data += b'\0'
    total = sum(struct.unpack('!%dH' % (len(data) // 2), data))
    while total >> 16:
        total = (total & 0xffff) + (total >> 16)
    return (~total) & 0xffff


def ipv4_tcp(guest_mac, peer_port, guest_port, seq, ack, flags):
    tcp = struct.pack('!HHIIBBHHH', peer_port, guest_port, seq, ack,
                      5 << 4, flags, 65535, 0, 0)
    pseudo = PEER_IP + GUEST_IP + b'\x00\x06' + struct.pack('!H', len(tcp))
    tcp = tcp[:16] + struct.pack('!H', checksum(pseudo + tcp)) + tcp[18:]
    ip = struct.pack('!BBHHHBBH4s4s', 0x45, 0, 20 + len(tcp),
                     0, 0x4000, 64, 6, 0, PEER_IP, GUEST_IP)
    ip = ip[:10] + struct.pack('!H', checksum(ip)) + ip[12:]
    return guest_mac + PEER_MAC + b'\x08\x00' + ip + tcp


class Peer:
    def __init__(self):
        self.stats = {
            'result': 'failed', 'frames': 0, 'arp_requests': 0,
            'invalid_frames': 0, 'ignored_frames': 0,
            'checksum_verified_ipv4': 0, 'checksum_verified_tcp': 0,
            'udp': {key.decode(): [] for key in UDP_TTLS},
            'tcp_syn': [], 'tcp_syn_retries': [], 'tcp_synack': [],
            'tcp_payloads': {key.decode(): [] for key in TCP_TTLS},
        }
        self.outgoing = {}
        self.incoming = {}
        self.incoming_started = 0
        self.guest_mac = None

    def observe(self, group, key, ttl, expected):
        if ttl != expected:
            raise ValueError(f'{group} {key!r} TTL {ttl}, expected {expected}')
        self.stats[group][key.decode()].append(ttl)

    def new_incoming(self):
        if self.incoming_started >= 2:
            raise ValueError('too many inbound listener probes')
        peer_port = FIRST_PEER_PORT + self.incoming_started
        initial_seq = 0x16000000 + self.incoming_started * 0x10000
        self.incoming_started += 1
        conn = {'peer_port': peer_port, 'guest_port': TCP_IN_PORT,
                'peer_next': initial_seq + 1, 'guest_next': None,
                'stream': bytearray(), 'synack_recorded': False}
        self.incoming[peer_port] = conn
        return ipv4_tcp(self.guest_mac, peer_port, TCP_IN_PORT,
                        initial_seq, 0, SYN)

    def arp_reply(self, frame):
        if len(frame) < 42:
            raise ValueError('truncated ARP')
        header = struct.unpack_from('!HHBBH', frame, 14)
        sender_mac, sender_ip, target_ip = frame[22:28], frame[28:32], frame[38:42]
        if header != (1, 0x0800, 6, 4, 1) or sender_ip != GUEST_IP or target_ip != PEER_IP:
            self.stats['ignored_frames'] += 1
            return []
        if sender_mac != frame[6:12]:
            raise ValueError('ARP sender MAC mismatch')
        self.guest_mac = sender_mac
        self.stats['arp_requests'] += 1
        arp = struct.pack('!HHBBH', 1, 0x0800, 6, 4, 2)
        arp += PEER_MAC + PEER_IP + sender_mac + sender_ip
        return [sender_mac + PEER_MAC + b'\x08\x06' + arp]

    def tcp_reply(self, conn, flags=ACK):
        return ipv4_tcp(self.guest_mac, conn['peer_port'], conn['guest_port'],
                        conn['peer_next'], conn['guest_next'], flags)

    def check_tcp_payload(self, conn, payload, ttl, outgoing):
        stream = conn['stream']
        expected_stream = b'tcp-out' if outgoing else b'tcp-inheritedtcp-accepted'
        start = len(stream)
        end = start + len(payload)
        if end > len(expected_stream) or payload != expected_stream[start:end]:
            raise ValueError(f'unexpected TCP stream bytes {payload!r} at {start}')
        segments = [(b'tcp-out', 0)] if outgoing else [
            (b'tcp-inherited', 0), (b'tcp-accepted', len(b'tcp-inherited'))]
        for label, offset in segments:
            stop = offset + len(label)
            if start < stop and end > offset:
                expected = TCP_TTLS[label]
                if ttl != expected:
                    raise ValueError(f'TCP {label!r} TTL {ttl}, expected {expected}')
        stream.extend(payload)
        for label, offset in segments:
            if start < offset + len(label) <= end:
                self.observe('tcp_payloads', label, ttl, TCP_TTLS[label])

    def tcp(self, frame, ip, ihl, total):
        tcp = ip[ihl:total]
        if len(tcp) < 20:
            raise ValueError('truncated TCP')
        src, dst, seq, ack, offset_byte, flags, _, _, _ = struct.unpack_from('!HHIIBBHHH', tcp)
        header_len = (offset_byte >> 4) * 4
        if header_len < 20 or header_len > len(tcp):
            raise ValueError('invalid TCP header length')
        pseudo = GUEST_IP + PEER_IP + b'\x00\x06' + struct.pack('!H', len(tcp))
        if checksum(pseudo + tcp) != 0:
            raise ValueError('invalid TCP checksum')
        self.stats['checksum_verified_tcp'] += 1
        payload = tcp[header_len:]
        ttl = ip[8]
        if flags & RST:
            raise ValueError(f'guest reset TCP flow {src}->{dst}')
        if dst == TCP_OUT_PORT:
            conn = self.outgoing.get(src)
            if flags & SYN and not flags & ACK:
                if conn is None:
                    if ttl != 41:
                        raise ValueError(f'outgoing SYN TTL {ttl}, expected 41')
                    initial_seq = 0x12000000 + len(self.outgoing) * 0x10000
                    conn = {'peer_port': TCP_OUT_PORT, 'guest_port': src,
                            'peer_next': initial_seq + 1, 'guest_next': (seq + 1) & 0xffffffff,
                            'stream': bytearray(), 'guest_syn': seq}
                    self.outgoing[src] = conn
                    self.stats['tcp_syn'].append(ttl)
                    # Let the real TCP retransmit timer emit the second SYN.
                    return []
                elif seq != conn['guest_syn'] or ttl != 41:
                    raise ValueError('unexpected outgoing SYN retransmission')
                self.stats['tcp_syn_retries'].append(ttl)
                return [ipv4_tcp(self.guest_mac, TCP_OUT_PORT, src,
                                 conn['peer_next'] - 1, conn['guest_next'], SYN | ACK)]
            if conn is None:
                raise ValueError('outgoing TCP data without SYN')
            outgoing = True
        elif src == TCP_IN_PORT and dst in self.incoming:
            conn = self.incoming[dst]
            if flags & SYN and flags & ACK:
                if ack != conn['peer_next'] or ttl != 47:
                    raise ValueError('invalid inbound SYN-ACK sequence or TTL')
                if not conn['synack_recorded']:
                    conn['guest_next'] = (seq + 1) & 0xffffffff
                    conn['synack_recorded'] = True
                    self.stats['tcp_synack'].append(ttl)
                elif conn['guest_next'] != (seq + 1) & 0xffffffff:
                    raise ValueError('unexpected inbound SYN-ACK retransmission')
                return [self.tcp_reply(conn)]
            if not conn['synack_recorded']:
                raise ValueError('inbound TCP data before handshake')
            outgoing = False
        else:
            self.stats['ignored_frames'] += 1
            return []

        if payload or flags & FIN:
            if seq == conn['guest_next']:
                if payload:
                    self.check_tcp_payload(conn, payload, ttl, outgoing)
                    conn['guest_next'] = (conn['guest_next'] + len(payload)) & 0xffffffff
                if flags & FIN:
                    conn['guest_next'] = (conn['guest_next'] + 1) & 0xffffffff
                return [self.tcp_reply(conn)]
            if ((conn['guest_next'] - seq) & 0xffffffff) <= len(payload) + bool(flags & FIN):
                return [self.tcp_reply(conn)]
            raise ValueError('unexpected TCP sequence')
        return []

    def frame(self, frame):
        if len(frame) < 14:
            raise ValueError('truncated Ethernet frame')
        kind = frame[12:14]
        if kind == b'\x08\x06':
            return self.arp_reply(frame)
        if kind != b'\x08\x00' or frame[:6] not in (PEER_MAC, BROADCAST):
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
            raise ValueError('invalid IPv4 checksum')
        self.stats['checksum_verified_ipv4'] += 1
        if ip[12:16] != GUEST_IP or ip[16:20] != PEER_IP:
            self.stats['ignored_frames'] += 1
            return []
        self.guest_mac = frame[6:12]
        if ip[9] == 6:
            return self.tcp(frame, ip, ihl, total)
        if ip[9] != 17:
            self.stats['ignored_frames'] += 1
            return []
        udp = ip[ihl:total]
        if len(udp) < 8:
            raise ValueError('truncated UDP')
        _, dst, udp_len, udp_sum = struct.unpack_from('!HHHH', udp)
        if udp_len != len(udp):
            raise ValueError('invalid UDP length')
        if udp_sum and checksum(GUEST_IP + PEER_IP + b'\x00\x11' +
                                struct.pack('!H', udp_len) + udp) != 0:
            raise ValueError('invalid UDP checksum')
        if dst != UDP_PORT:
            self.stats['ignored_frames'] += 1
            return []
        payload = udp[8:]
        if payload not in UDP_TTLS:
            raise ValueError(f'unexpected UDP test payload {payload!r}')
        self.observe('udp', payload, ip[8], UDP_TTLS[payload])
        return [self.new_incoming()] if payload == b'tcp-listener-ready' else []

    def finish(self):
        for group in ('udp', 'tcp_payloads'):
            for name, values in self.stats[group].items():
                if len(values) != 2:
                    raise ValueError(f'{group} {name} appeared {len(values)} times, expected 2')
        if len(self.stats['tcp_syn']) != 2 or len(self.stats['tcp_synack']) != 2:
            raise ValueError('expected two TCP SYN and SYN-ACK observations')
        if len(self.stats['tcp_syn_retries']) < 2:
            raise ValueError('expected one SYN retransmission per active open')
        self.stats['result'] = 'passed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fd', required=True, type=int)
    parser.add_argument('--stats', required=True, type=Path)
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
                    raise ValueError('peer closed with partial Ethernet frame')
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
