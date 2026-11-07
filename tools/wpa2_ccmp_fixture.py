#!/usr/bin/env python3
"""Independent AESCCM oracle and synthetic packet fixtures;no host secrets."""
from cryptography.hazmat.primitives.ciphers.aead import AESCCM
from wpa2_handshake_fixture import AA, SPA, GTK, PTK

LAN = bytes.fromhex('02aabbccddee')
DATA = bytes.fromhex('aaaa030000000800') + b'synthetic CCMP packet payload'
RSC = int.from_bytes(bytes.fromhex('010203040506'), 'little')


def header(tx=False, group=False, tid=None, seq=0x123, flags=0):
    fc = (0x0108 if tx else 0x4208) | (0x80 if tid is not None else 0) | flags
    ra = AA if tx else (bytes([255]) * 6 if group else SPA)
    ta = SPA if tx else AA
    return fc.to_bytes(2, 'little') + bytes(2) + ra + ta + LAN + (seq << 4).to_bytes(2, 'little') + (bytes([tid, 0]) if tid is not None else b'')


def encrypt(h, data, key, pn, keyid=0):
    qos = bool(h[0] & 0x80)
    priority = h[24] & 15 if qos else 0
    fc = int.from_bytes(h[:2], 'little') | 0x4000
    protected = fc.to_bytes(2, 'little') + h[2:]
    aad = bytes([protected[0] & 0x8F, protected[1] & 0xC7]) + protected[4:22] + bytes([protected[22] & 15, 0])
    if qos:
        aad += bytes([priority, 0])
    nonce = bytes([priority]) + h[10:16] + pn.to_bytes(6, 'big')
    p = pn.to_bytes(6, 'little')
    ccmp = p[:2] + bytes([0, 0x20 | keyid << 6]) + p[2:]
    return protected + ccmp + AESCCM(key, tag_length=8).encrypt(nonce, data, aad)


TX_HEADER = header(tx=True)
TX = encrypt(TX_HEADER, DATA, PTK[32:], 1)
RX = encrypt(header(), DATA, PTK[32:], 1)
GROUP = encrypt(header(group=True, tid=3), DATA, GTK, RSC + 1, 1)


def fixture_header():
    rows = ['/* Synthetic public CCMP fixtures;tools/wpa2_ccmp_fixture.py/AESCCM oracle. */']
    for name in ('DATA', 'TX_HEADER', 'TX', 'RX', 'GROUP'):
        rows.append('static const unsigned char ccmp_' + name.lower() + '[]={' + ','.join(f'0x{x:02x}' for x in globals()[name]) + '};')
    return '\n'.join(rows) + '\n'


if __name__ == '__main__':
    print(fixture_header(), end='')
