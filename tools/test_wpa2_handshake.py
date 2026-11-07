#!/usr/bin/env python3
"""Optional host crypto differential (cryptography required);synthetic data only."""
import ctypes as C
import hashlib
import hmac
from pathlib import Path
import random
import sys
from cryptography.hazmat.primitives.keywrap import aes_key_wrap
from wpa2_handshake_fixture import AA, SPA, PMK, AN, SN, RSNE, GTK, PTK, M1, M2, M3, M4, packet, plaintext, header

U8 = C.c_ubyte
B6, B16, B32 = U8 * 6, U8 * 16, U8 * 32


class Peer(C.Structure):
    _fields_ = [('aa', B6), ('spa', B6), ('rsne', U8 * 64), ('rsne_size', C.c_size_t)]


class State(C.Structure):
    _fields_ = [('phase', C.c_int), ('generation', C.c_uint64), ('replay', C.c_uint64), ('key_owner', C.c_size_t),
                ('peer', Peer), ('emissions', C.c_uint), ('install_handoffs', C.c_uint),
                ('keys_may_live', C.c_bool), ('unsafe', C.c_bool), ('version', U8),
                ('group_id', U8), ('group_rsc', B6), ('pmk', B32), ('snonce', B32),
                ('anonce', B32), ('ptk', U8 * 48), ('gtk', B16), ('scratch', U8 * 1016)]


class Keys(C.Structure):
    _fields_ = [('pairwise', B16), ('group', B16), ('group_rsc', B6),
                ('group_id', U8), ('generation', C.c_uint64)]


def buf(data):
    return (U8 * len(data)).from_buffer_copy(data)


lib = C.CDLL(str(Path(sys.argv[1]).resolve()))
for name, result, args in (
    ('wpa2_hs_start', C.c_bool, [C.c_void_p] * 4 + [C.c_uint64]),
    ('wpa2_hs_receive', C.c_bool, [C.c_void_p, C.c_uint64] + [C.c_void_p] * 3 + [C.c_size_t]),
    ('wpa2_hs_emit', C.c_size_t, [C.c_void_p, C.c_uint64, C.c_void_p, C.c_size_t]),
    ('wpa2_hs_tx_result', C.c_bool, [C.c_void_p, C.c_uint64, C.c_int]),
    ('wpa2_hs_take_keys', C.c_bool, [C.c_void_p, C.c_uint64, C.c_void_p]),
    ('wpa2_hs_claim_keys', C.c_bool, [C.c_void_p, C.c_uint64, C.c_size_t]),
    ('wpa2_hs_install_result', C.c_bool, [C.c_void_p, C.c_uint64, C.c_int]),
    ('wpa2_hs_retire', C.c_bool, [C.c_void_p, C.c_uint64, C.c_bool]),
    ('wpa2_key_unwrap', C.c_size_t, [C.c_void_p, C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t]),
):
    f = getattr(lib, name)
    f.restype, f.argtypes = result, args
for name, shape in (('state', State), ('peer', Peer), ('keys', Keys)):
    f = getattr(lib, 'wpa2_probe_' + name + '_size')
    f.restype = C.c_size_t
    assert f() == C.sizeof(shape)
assert header() == (Path(__file__).parent / 'fixtures/wpa2-handshake-synthetic.h').read_text()
p = Peer(aa=B6(*AA), spa=B6(*SPA), rsne=(U8 * 64)(*RSNE), rsne_size=len(RSNE))


def start():
    s = State()
    assert lib.wpa2_hs_start(C.byref(s), C.byref(p), buf(PMK), buf(SN), 9)
    return s


def receive(s, frame):
    return lib.wpa2_hs_receive(C.byref(s), 9, buf(AA), buf(SPA), buf(frame), len(frame))


def emit(s, want):
    out = (U8 * 121)()
    assert lib.wpa2_hs_emit(C.byref(s), 9, out, 121) == len(want)
    assert bytes(out)[:len(want)] == want
    assert lib.wpa2_hs_tx_result(C.byref(s), 9, 0)


def before_m3():
    s = start()
    assert receive(s, M1)
    emit(s, M2)
    return s


def clone(s):
    return State.from_buffer_copy(bytes(s))


def erased(s):
    for field in ('pmk', 'snonce', 'anonce', 'ptk', 'gtk', 'scratch'):
        assert not any(getattr(s, field)), field


def refused(s, frame):
    assert not receive(s, frame)
    assert s.phase == 10 and s.install_handoffs == 0 and not s.keys_may_live
    erased(s)


m1base, m3base = start(), before_m3()
# Exhaust every flag word;all non-profile bits and every wrong descriptor refuse.
for word in range(65536):
    s = clone(m1base)
    frame = bytearray(M1)
    frame[5:7] = word.to_bytes(2, 'big')
    if word == 0x008A:
        assert receive(s, frame)
    else:
        refused(s, frame)
    s = clone(m3base)
    frame = packet(word, 8, data=M3[99:], rsc=M3[65:73])
    if word == 0x13CA:
        assert receive(s, frame)
    else:
        refused(s, frame)

for replay in (0, 1, 7, 8, 9, 2**32, 2**64 - 1):
    s = clone(m3base)
    frame = packet(0x13CA, replay, data=M3[99:], rsc=M3[65:73])
    if replay > 7:
        assert receive(s, frame)
        assert s.replay == replay
    else:
        refused(s, frame)
# M1 counter0 is legal;max has no monotonic successor in this one-shot profile.
for nonce in (bytes(32), bytes([255]) * 32, SN):
    refused(start(), packet(0x008A, 7, nonce=nonce))

for replay in (0, 2**64 - 2, 2**64 - 1):
    s = clone(m1base)
    if replay == 2**64 - 1:
        refused(s, packet(0x008A, replay))
    else:
        assert receive(s, packet(0x008A, replay))

# M1 PMKID is an untrusted hint;only the exact optional KDE shape is admitted.
pmkid = bytes.fromhex('dd14000fac04') + bytes(range(16))
s = start()
assert receive(s, packet(0x008A, 7, data=pmkid))
for i in range(6):
    bad = bytearray(pmkid)
    bad[i] ^= 1
    refused(start(), packet(0x008A, 7, data=bad))
for data in (pmkid + pmkid, pmkid[:-1], b'\x00', RSNE):
    refused(start(), packet(0x008A, 7, data=data))

# Every fixed field must still fail when the attacker/test authenticator signs it.
for offset in (0, 1, 4, 7, 8, 17, 48, 49, 64, 71, 72, 73, 80, 97, 98):
    frame = bytearray(M3)
    frame[offset] ^= 1
    frame[81:97] = bytes(16)
    frame[81:97] = hmac.digest(PTK[:16], frame, 'sha1')[:16]
    refused(clone(m3base), frame)

# Authenticated ciphertext can fail wrap integrity;MIC is not unwrap authority.
bad_cipher = bytearray(M3[99:])
for i in range(len(bad_cipher)):
    bad_cipher[i] ^= 1
    refused(clone(m3base), packet(0x13CA, 8, data=bad_cipher))
    bad_cipher[i] ^= 1

# Valid MIC + RFC3394 integrity still must not admit changed RSNE,bad or duplicate
# GTK/KDE,unknown IEs,or noncanonical padding. All inputs are independently wrapped.
def padded(data):
    pad = (-len(data)) % 8
    return data + (b'\xdd' + bytes(pad - 1) if pad else b'')


gtk_kde = bytes.fromhex('dd16000fac010100') + GTK
bad_data = [padded(RSNE), padded(gtk_kde), padded(RSNE + RSNE + gtk_kde),
            padded(RSNE + gtk_kde + gtk_kde), padded(RSNE + gtk_kde + b'\x00\x00'),
            plaintext()[:-1] + b'\x01', plaintext() + bytes(8)]
for offset in range(len(RSNE)):
    bad = bytearray(RSNE)
    bad[offset] ^= 1
    bad_data.append(padded(bad + gtk_kde))
for offset in (0, 1, 2, 3, 4, 5, 7):
    bad = bytearray(gtk_kde)
    bad[offset] ^= 1
    bad_data.append(padded(RSNE + bad))
for info in range(8, 256):
    bad = bytearray(gtk_kde)
    bad[6] = info
    bad_data.append(padded(RSNE + bad))
for data in bad_data:
    refused(clone(m3base), packet(0x13CA, 8, data=aes_key_wrap(PTK[16:32], data)))

# Legal GTK indices/ignored TX bit,RSC,IE order,and EAPOL1 all retain one handoff.
for info in range(8):
    kde = bytearray(gtk_kde)
    kde[6] = info
    for data in (padded(RSNE + kde), padded(kde + RSNE)):
        s = clone(m3base)
        assert receive(s, packet(0x13CA, 8, data=aes_key_wrap(PTK[16:32], data), rsc=M3[65:73]))
        keys = Keys()
        assert lib.wpa2_hs_take_keys(C.byref(s), 9, C.byref(keys))
        assert keys.group_id == (info & 3) and bytes(keys.group) == GTK
        assert bytes(keys.pairwise) == PTK[32:] and bytes(keys.group_rsc) == M3[65:71]
        assert lib.wpa2_hs_claim_keys(C.byref(s), 9, 0x1234)
        assert lib.wpa2_hs_install_result(C.byref(s), 9, 0)
        emit(s, M4)
        assert s.phase == 9 and s.install_handoffs == 1 and s.keys_may_live
        erased(s)
        assert not receive(s, M3) and s.install_handoffs == 1 and s.keys_may_live
        assert not lib.wpa2_hs_retire(C.byref(s), 9, False)
        assert lib.wpa2_hs_retire(C.byref(s), 9, True)

s = start()
assert receive(s, packet(0x008A, 7, version=1))
emit(s, packet(0x010A, 7, SN, RSNE, version=1, key_length=0))
assert receive(s, packet(0x13CA, 8, data=M3[99:], version=1))
keys = Keys()
assert lib.wpa2_hs_take_keys(C.byref(s), 9, C.byref(keys))
assert lib.wpa2_hs_claim_keys(C.byref(s), 9, 0x1234)
assert lib.wpa2_hs_install_result(C.byref(s), 9, 0)
emit(s, packet(0x030A, 8, bytes(32), version=1, key_length=0))

r = random.Random(0x3394)
lengths = [16, 24, 32, 40, 48, 1016] + [8 * r.randrange(2, 17) for _ in range(100)]
for n in lengths:
    kek, data = r.randbytes(16), r.randbytes(n)
    wrapped = aes_key_wrap(kek, data)
    out = (U8 * 1016)()
    assert lib.wpa2_key_unwrap(buf(kek), buf(wrapped), len(wrapped), out, 1016) == n
    assert bytes(out)[:n] == data and not any(out[n:])
    bad = bytearray(kek)
    bad[r.randrange(16)] ^= 1
    assert not lib.wpa2_key_unwrap(buf(bad), buf(wrapped), len(wrapped), out, 1016)
    assert not any(out)

print('wpa2-handshake differential: PASS 131072 flag cases,independent M2/M4 and encrypted M3,replay/fixed/KDE/padding/RSNE refusals,one-time install/retirement,106 AES unwraps through1024-byte ciphertext;synthetic inputs only')
