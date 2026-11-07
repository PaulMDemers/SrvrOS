#!/usr/bin/env python3
"""Differential tests with synthetic secrets only;never read host credentials."""
import ctypes
import hashlib
import hmac
from pathlib import Path
import random
import sys

lib = ctypes.CDLL(str(Path(sys.argv[1]).resolve()))
U8 = ctypes.POINTER(ctypes.c_ubyte)
for name, args in (
    ('wpa2_psk_pmk', [U8, ctypes.c_size_t, U8, ctypes.c_size_t, U8]),
    ('wpa2_ccmp_ptk', [U8] * 6),
    ('wpa2_eapol_mic', [U8, U8, ctypes.c_size_t, U8]),
    ('wpa2_eapol_mic_verify', [U8, U8, ctypes.c_size_t]),
):
    f = getattr(lib, name)
    f.argtypes = args
    f.restype = ctypes.c_bool


def buf(data):
    return (ctypes.c_ubyte * len(data)).from_buffer_copy(data)


def blank(n):
    return buf(bytes([0xA5]) * n)


r = random.Random(0x80211)
for i in range(100):
    # Include spaces,maximum/minimum lengths,and binary SSID octets/NULs.
    pn = (8, 63, r.randint(8, 63))[i % 3]
    sn = (1, 32, r.randint(1, 32))[i % 3]
    password = bytes(r.randint(32, 126) for _ in range(pn))
    ssid = r.randbytes(sn)
    out = blank(32)
    assert lib.wpa2_psk_pmk(buf(password), pn, buf(ssid), sn, out)
    assert bytes(out) == hashlib.pbkdf2_hmac('sha1', password, ssid, 4096, 32)

for i in range(1000):
    pmk = r.randbytes(32)
    aa, spa = bytearray(r.randbytes(6)), bytearray(r.randbytes(6))
    aa[0] = (aa[0] & 0xFE) | 2
    spa[0] = (spa[0] & 0xFE) | 2
    aa, spa = bytes(aa), bytes(spa)
    an, sn = r.randbytes(32), r.randbytes(32)
    context = min(aa, spa) + max(aa, spa) + min(an, sn) + max(an, sn)
    expected = b''.join(hmac.digest(pmk, b'Pairwise key expansion\0' + context + bytes([c]), 'sha1') for c in range(3))[:48]
    for a, s, x, y in ((aa, spa, an, sn), (spa, aa, an, sn), (aa, spa, sn, an), (spa, aa, sn, an)):
        out = blank(48)
        assert lib.wpa2_ccmp_ptk(buf(pmk), buf(a), buf(s), buf(x), buf(y), out)
        assert bytes(out) == expected

for i in range(2000):
    n = (99, 1123, r.randint(99, 1123))[i % 3]
    key = r.randbytes(16)
    frame = bytearray(r.randbytes(n))
    frame[0] = 1 + (i % 2)
    frame[1] = 3
    frame[2:4] = (n - 4).to_bytes(2, 'big')
    frame[4] = 2
    # Other flags are intentionally outside the primitive's authority.
    frame[5:7] = ((r.getrandbits(16) & ~7) | 0x102).to_bytes(2, 'big')
    frame[97:99] = (n - 99).to_bytes(2, 'big')
    zeroed = frame[:81] + bytes(16) + frame[97:]
    expected = hmac.digest(key, zeroed, 'sha1')[:16]
    before = bytes(frame)
    cframe, out = buf(frame), blank(16)
    assert lib.wpa2_eapol_mic(buf(key), cframe, n, out)
    assert bytes(out) == expected and bytes(cframe) == before
    frame[81:97] = expected
    cframe = buf(frame)
    assert lib.wpa2_eapol_mic_verify(buf(key), cframe, n)
    # Every MIC bit and independently chosen key/data bit must fail.
    for j in range(16):
        for bit in range(8):
            frame[81 + j] ^= 1 << bit
            assert not lib.wpa2_eapol_mic_verify(buf(key), buf(frame), n)
            frame[81 + j] ^= 1 << bit
    wrong_key = bytearray(key)
    wrong_key[i % 16] ^= 1 << (i % 8)
    assert not lib.wpa2_eapol_mic_verify(buf(wrong_key), cframe, n)
    if n > 99:
        frame[r.randrange(99, n)] ^= 1 << (i % 8)
        assert not lib.wpa2_eapol_mic_verify(buf(key), buf(frame), n)

print('wpa2-crypto differential: PASS 100 PMKs,4000 ordered PTKs,2000 full EAPOL MICs and256000 MIC-bit refusals;synthetic inputs only')
