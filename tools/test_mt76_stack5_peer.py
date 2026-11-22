#!/usr/bin/env python3
"""Cross-check the test-only OpenSSL AP against independent Python AESCCM.

All vectors are public synthetic data. Builds/runs only in a private temporary
directory; no production crypto, shared build paths, hardware or host secrets.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

from cryptography.hazmat.primitives.ciphers.aead import AESCCM
from wpa2_handshake_fixture import AA, SPA, PTK, GTK

ROOT = Path(__file__).resolve().parents[1]


def protected(header, plain, pn, group=False):
    aad = bytes((header[0] & 0x8f, header[1] & 0xc7)) + header[4:22] + bytes((header[22] & 15, 0))
    nonce = bytes(1) + header[10:16] + pn.to_bytes(6, 'big')
    iv = pn.to_bytes(6, 'little')
    iv = iv[:2] + bytes((0, 0x60 if group else 0x20)) + iv[2:]
    return header + iv + AESCCM(GTK if group else PTK[32:], tag_length=8).encrypt(nonce, plain, aad)


def vectors():
    for index, size in enumerate((14, 15, 42, 60, 298, 511, 1500, 1513, 1514)):
        pn = (1, 255, 256, 65535, 65536, 0x123456789abc, 0xffffffffffff, 2, 7)[index]
        destination = bytes.fromhex('021122334455')
        protocol = bytes.fromhex('0806' if index % 2 else '0800')
        eth = destination + SPA + protocol + bytes((37 * x + index) & 255 for x in range(size - 14))
        plain = bytes.fromhex('aaaa03000000') + eth[12:]
        h = bytes.fromhex('08413c00') + AA + SPA + eth[:6] + struct.pack('<H', (index + 1) << 4)
        mpdu = protected(h, plain, pn)
        dma = (20 + len(mpdu) + 3) & ~3
        wi = struct.pack('<HHBBH', 0, 0x2000, 1, 255, len(mpdu)) + bytes(11) + bytes((128,))
        tx = struct.pack('<I', dma | 1 << 19 | 1 << 24 | 2 << 25) + wi + mpdu + bytes(dma - 20 - len(mpdu) + 4)
        for group in (False, True):
            reply = (bytes.fromhex('ffffffffffff') if group else SPA) + destination + eth[12:]
            rp = pn if not group else 0x060504030201 + index + 1
            h = bytes.fromhex('08420000') + reply[:6] + AA + reply[6:12] + struct.pack('<H', (rp & 4095) << 4)
            rx = protected(h, bytes.fromhex('aaaa03000000') + reply[12:], rp, group)
            yield tx, eth, pn, reply, rp, rx


def frozen_vectors():
    """Authenticate retained public vectors rather than regenerating their TX."""
    text = (ROOT / 'tools/fixtures/mt76-stream5-dhcp-synthetic.h').read_text()
    def array(kind, index):
        body = re.search(r'static const unsigned char stream5_dhcp_' + kind + str(index) + r'\[\]=\{([^}]*)\}', text)[1]
        return bytes(int(x, 0) for x in body.split(','))
    def plain(mpdu):
        h, iv = mpdu[:24], mpdu[24:32]
        pn = int.from_bytes(iv[:2] + iv[4:], 'little')
        aad = bytes((h[0] & 0x8f, h[1] & 0xc7)) + h[4:22] + bytes((h[22] & 15, 0))
        nonce = bytes(1) + h[10:16] + pn.to_bytes(6, 'big')
        key = GTK if iv[3] == 0x60 else PTK[32:]
        return AESCCM(key, tag_length=8).decrypt(nonce, mpdu[32:], aad), pn
    for i in range(71):
        tx = array('tx', i)
        h = tx[24:]
        body, pn = plain(h[:int.from_bytes(tx[10:12], 'little')])
        eth = h[16:22] + h[10:16] + body[6:]
        # Packet0 deliberately has no reply in this lost-first-Offer oracle.
        # Its peer sealing check uses the independently retained packet1 reply.
        rx = array('rx', max(1, i))
        body, rp = plain(rx)
        reply = rx[4:10] + rx[16:22] + body[6:]
        expected = rx[:22] + struct.pack('<H', (rp & 4095) << 4) + rx[24:]
        yield tx, eth, pn, reply, rp, expected


def authenticated_refusals():
    """Valid-MIC malformed envelopes cannot pass by merely decrypting."""
    base = bytes.fromhex('08413c00') + AA + SPA + bytes.fromhex('021122334455') + struct.pack('<H', 16)
    normal = bytes.fromhex('aaaa030000000800') + b'public malformed fixture'
    cases = [(base, bytes((0xab,)) + normal[1:], 1),
             (base, normal[:6] + bytes.fromhex('86dd') + normal[8:], 1),
             (base[:4] + bytes.fromhex('020000000099') + base[10:], normal, 1),
             (base[:10] + bytes.fromhex('020000000098') + base[16:], normal, 1),
             (base[:22] + bytes(2), normal, 1), (base, normal, 0)]
    for h, body, pn in cases:
        mpdu = protected(h, body, pn)
        dma = (20 + len(mpdu) + 3) & ~3
        wi = struct.pack('<HHBBH', 0, 0x2000, 1, 255, len(mpdu)) + bytes(11) + bytes((128,))
        yield struct.pack('<I', dma | 1 << 19 | 1 << 24 | 2 << 25) + wi + mpdu + bytes(dma - 20 - len(mpdu) + 4)


def render(items):
    lines = ['#include <assert.h>', '#include <stdint.h>', '#include <stdio.h>', '#include <string.h>',
             '#include "mt76-stack5-peer.h"']
    for i, item in enumerate(items):
        for name, value in zip(('tx', 'eth', 'pn', 'reply', 'rp', 'rx'), item):
            if isinstance(value, bytes):
                lines.append('static const uint8_t %s%d[]={%s};' % (name, i, ','.join(map(str, value))))
    refusals = list(authenticated_refusals())
    for i, packet in enumerate(refusals):
        lines.append('static const uint8_t badtx%d[]={%s};' % (i, ','.join(map(str, packet))))
    lines += ['int main(void) {', 'uint8_t out[1600], changed[1600]; size_t n; uint64_t pn;']
    for i, (_, _, pn, _, rp, _) in enumerate(items):
        lines += [f'memset(out,0xa5,sizeof out); n=123;pn=99; assert(stack5_peer_open(tx{i},sizeof tx{i},out,sizeof out,&n,&pn));',
                  f'assert(n==sizeof eth{i} && pn==UINT64_C({pn}) && !memcmp(out,eth{i},n));',
                  f'assert(stack5_peer_seal(reply{i},sizeof reply{i},UINT64_C({rp}),out,sizeof out,&n));',
                  f'assert(n==sizeof rx{i} && !memcmp(out,rx{i},n));']
    for i in range(len(refusals)):
        lines += [f'memset(out,0xa5,sizeof out);n=123;pn=99;assert(!stack5_peer_open(badtx{i},sizeof badtx{i},out,sizeof out,&n,&pn));assert(!n && !pn && out[0]==0xa5);']
    lines += [
        'for(size_t size=0;size<sizeof tx8;size++){ memset(out,0xa5,sizeof out);n=123;pn=99;assert(!stack5_peer_open(tx8,size,out,sizeof out,&n,&pn));assert(!n && !pn && out[0]==0xa5);}',
        'for(size_t cap=0;cap<sizeof eth8;cap++){ memset(out,0xa5,sizeof out);n=123;pn=99;assert(!stack5_peer_open(tx8,sizeof tx8,out,cap,&n,&pn));assert(!n && !pn && out[0]==0xa5);}',
        'for(size_t cap=0;cap<sizeof rx8;cap++){ memset(out,0xa5,sizeof out);n=123;assert(!stack5_peer_seal(reply8,sizeof reply8,5,out,cap,&n));assert(!n && out[0]==0xa5);}',
        # Every actual authenticated ciphertext/MIC byte must refuse corruption.
        'for(size_t byte=56;byte<sizeof tx8-4;byte++){memcpy(changed,tx8,sizeof tx8);changed[byte]^=1;memset(out,0xa5,sizeof out);n=123;pn=99;assert(!stack5_peer_open(changed,sizeof tx8,out,sizeof out,&n,&pn));assert(!n && !pn && out[0]==0xa5);}',
        # All envelope/header fields except valid arbitrary destination/sequence.
        'const unsigned bad[]={0,1,2,3,4,5,6,7,8,9,10,11,12,22,23,24,25,26,27,28,34,46,48,49,50,51};',
        'for(size_t i=0;i<sizeof bad/sizeof bad[0];i++){memcpy(changed,tx8,sizeof tx8);changed[bad[i]]^=1;n=123;pn=99;assert(!stack5_peer_open(changed,sizeof tx8,out,sizeof out,&n,&pn));assert(!n && !pn);}',
        'memcpy(changed,tx8,sizeof tx8);n=123;pn=99;assert(!stack5_peer_open(changed,sizeof tx8,changed,sizeof changed,&n,&pn));assert(!n && !pn);',
        'memcpy(changed,reply8,sizeof reply8);n=123;assert(!stack5_peer_seal(changed,sizeof reply8,1,changed,sizeof changed,&n));assert(!n);',
        'n=123;assert(!stack5_peer_seal(reply8,sizeof reply8,0,out,sizeof out,&n));assert(!n);',
        'n=123;assert(!stack5_peer_seal(reply8,sizeof reply8,UINT64_C(0x1000000000000),out,sizeof out,&n));assert(!n);',
        'for(size_t size=0;size<14;size++){n=123;assert(!stack5_peer_seal(reply8,size,1,out,sizeof out,&n));assert(!n);}',
        'n=123;assert(!stack5_peer_seal(reply8,1515,1,out,sizeof out,&n));assert(!n);',
        'memcpy(changed,reply8,sizeof reply8);changed[0]^=2;n=123;assert(!stack5_peer_seal(changed,sizeof reply8,1,out,sizeof out,&n));assert(!n);',
        'memcpy(changed,reply8,sizeof reply8);changed[6]|=1;n=123;assert(!stack5_peer_seal(changed,sizeof reply8,1,out,sizeof out,&n));assert(!n);',
        'memcpy(changed,reply8,sizeof reply8);changed[12]=0x86;changed[13]=0xdd;n=123;assert(!stack5_peer_seal(changed,sizeof reply8,1,out,sizeof out,&n));assert(!n);',
        'assert(!stack5_peer_open(NULL,80,out,sizeof out,&n,&pn));assert(!stack5_peer_open(tx8,sizeof tx8,NULL,sizeof out,&n,&pn));',
        'assert(!stack5_peer_open(tx8,sizeof tx8,out,sizeof out,NULL,&pn));assert(!stack5_peer_open(tx8,sizeof tx8,out,sizeof out,&n,NULL));',
        'assert(!stack5_peer_seal(NULL,42,1,out,sizeof out,&n));assert(!stack5_peer_seal(reply8,sizeof reply8,1,NULL,sizeof out,&n));',
        'assert(!stack5_peer_seal(reply8,sizeof reply8,1,out,sizeof out,NULL));',
        f'puts("stack5-peer: PASS independent Python AESCCM byte-exact{len(items)}cases including71frozenvectors;IPv4ARP/minimum+maximum/TKGTK/PNboundaries;truncation,capacity,MIC,AAD,peer,envelope,alias,invalidPN refusals");return 0;}}',
    ]
    return '\n'.join(lines) + '\n'


def main():
    destination = Path(tempfile.mkdtemp(prefix='srvros-stack5-peer-'))
    source = destination / 'test.c'
    items = list(vectors()) + list(frozen_vectors())
    source.write_text(render(items))
    inputs = (ROOT / 'tools/fixtures/mt76-stack5-peer.c', ROOT / 'tools/fixtures/mt76-stack5-peer.h',
              ROOT / 'tools/fixtures/wpa2-handshake-synthetic.h', Path(__file__).resolve(),
              ROOT / 'tools/fixtures/mt76-stream5-dhcp-synthetic.h', ROOT / 'tools/wpa2_handshake_fixture.py')
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    commands = []
    for fatal in (False, True):
        binary = destination / ('fatal' if fatal else 'normal')
        cmd = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-pedantic', '-g', '-O1',
               '-I' + str(ROOT / 'tools/fixtures'), str(source), str(inputs[0]), '-lcrypto', '-o', str(binary)]
        if fatal:
            cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
        subprocess.run(cmd, check=True, capture_output=True)
        env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1:abort_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
        result = subprocess.run([str(binary)], check=True, env=env, capture_output=True, text=True)
        (destination / (binary.name + '.log')).write_text(result.stdout + result.stderr)
        commands.append(dict(compile=cmd, run=[str(binary)], exit=result.returncode,
                             stderr_bytes=len(result.stderr.encode()), stdout=result.stdout))
    assert hashes == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    proof = dict(inputs=hashes, independent='Python cryptography AESCCM vs OpenSSL EVP CCM;public synthetic keys only',
                 cases=len(items), frozen_vectors=71, valid_mic_refusals=6,
                 generated_c_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), commands=commands,
                 scope='Host-fixture AP envelope only;no native or production stack acceptance')
    (destination / 'proof.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(destination / 'proof.json')


if __name__ == '__main__':
    main()
