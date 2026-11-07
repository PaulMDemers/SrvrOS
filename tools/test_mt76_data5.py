#!/usr/bin/env python3
"""Independent AESCCM plus USB envelope oracle;synthetic credentials only."""
import ctypes as C
import random
import struct
# Reuse the handshake/CCMP ABI probes and validated synthetic installed base.
# Its own independent differential suite also runs against this same library.
from test_wpa2_ccmp import lib, fresh, buf, U8, V, G, N, AA, SPA, PTK, GTK, RSC, header, encrypt

class Scratch(C.Structure):
    _fields_ = [('plain', U8*2304), ('mpdu', U8*1554)]

for name, args in (
    ('mt76_data5_tx', [V,V,G,V,N,C.c_uint,V,N]),
    ('mt76_data5_rx', [V,V,G,V,N,V,N]),
):
    f = getattr(lib, name)
    f.restype, f.argtypes = N, args

r = random.Random(0xdada5161)
lan = bytes.fromhex('02aabbccddee')
lengths = [0,1,7,15,16,17,31,32,33,1499,1500] + [r.randint(0,1500) for _ in range(40)]
count = 0
for index, length in enumerate(lengths):
    payload = r.randbytes(length)
    proto = bytes.fromhex('0806' if index%2 else '0800')
    eth = lan+SPA+proto+payload
    plain = bytes.fromhex('aaaa03000000')+proto+payload
    k, scratch, output = fresh(), Scratch(), (U8*4096)()
    for pn in (1,2):
        seq = 1+index
        got = lib.mt76_data5_tx(C.byref(scratch), C.byref(k), 9, buf(eth), len(eth), seq, output, len(output))
        hh = bytearray(header(tx=True, seq=seq));hh[2:4] = bytes([60,0])
        mpdu = encrypt(bytes(hh),plain,PTK[32:],pn)
        dma = (20+len(mpdu)+3)&~3
        info = dma | (1<<19) | (1<<24) | (2<<25)
        wi = struct.pack('<HHBBH',0,0x2000,1,255,len(mpdu))+bytes(11)+bytes([128])
        expected = struct.pack('<I',info)+wi+mpdu+bytes(dma-20-len(mpdu))+bytes(4)
        assert got == len(expected) and bytes(output)[:got] == expected
        assert not any(output[got:]) and not any(bytes(scratch)) and k.tx_pn == pn
    for group in (False,True):
        for tid in (None,0,1,7):
            for pad in (0,2):
                k = fresh();pn = RSC+1 if group else 1
                hh = header(group=group,tid=tid,seq=seq,flags=(0,0x800,0x3800)[index%3])
                mpdu = encrypt(hh,plain,GTK if group else PTK[32:],pn,1 if group else 0)
                hs = len(hh);dma = (32+len(mpdu)+pad+3)&~3
                wi = struct.pack('<II',0x2092 | (0x4000 if pad else 0),len(mpdu)<<16)+bytes(24)
                usb = struct.pack('<I',dma)+wi+mpdu[:hs]+bytes(pad)+mpdu[hs:]+bytes(dma-32-len(mpdu)-pad)
                want = hh[4:10]+hh[16:22]+proto+payload
                out = (U8*1514)()
                got = lib.mt76_data5_rx(C.byref(scratch),C.byref(k),9,buf(usb),len(usb),out,len(out))
                assert got == len(want) and bytes(out)[:got] == want and not any(out[got:])
                assert not any(bytes(scratch))
                old = bytes(k)
                assert not lib.mt76_data5_rx(C.byref(scratch),C.byref(k),9,buf(usb),len(usb),out,len(out))
                assert not any(out) and bytes(k) == old
                # A new PN with a bad MIC may not consume replay state.
                mpdu2 = encrypt(hh,plain,GTK if group else PTK[32:],pn+1,1 if group else 0)
                bad = bytearray(usb);bad[36:36+hs] = mpdu2[:hs];bad[36+hs+pad:36+pad+len(mpdu2)] = mpdu2[hs:];bad[36+pad+len(mpdu2)-1] ^= 1
                assert not lib.mt76_data5_rx(C.byref(scratch),C.byref(k),9,buf(bad),len(bad),out,len(out))
                assert bytes(k) == old and not any(out) and not any(bytes(scratch))
                count += 1
print(f'mt76-data5 differential: PASS {2*len(lengths)} full USB TX oracles,{count} protected non-QoS/QoS/group/pad RX/replay/MIC cases through1500-byte Ethernet payloads;synthetic credentials only')
