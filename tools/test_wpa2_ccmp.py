#!/usr/bin/env python3
"""Synthetic-key differential;never loads host credentials or transmits frames."""
import ctypes as C
from pathlib import Path
import random
import sys
from wpa2_handshake_fixture import AA, SPA, PMK, SN, M1, M2, M3, M4, RSNE, GTK, PTK
from wpa2_ccmp_fixture import DATA, RSC, header, encrypt, fixture_header

U8 = C.c_ubyte
class Peer(C.Structure):
    _fields_ = [('aa', U8*6), ('spa', U8*6), ('rsne', U8*64), ('rsne_size', C.c_size_t)]
class HS(C.Structure):
    _fields_ = [('phase', C.c_int), ('generation', C.c_uint64), ('replay', C.c_uint64), ('key_owner', C.c_size_t), ('peer', Peer), ('emissions', C.c_uint), ('install_handoffs', C.c_uint), ('keys_may_live', C.c_bool), ('unsafe', C.c_bool), ('version', U8), ('group_id', U8), ('group_rsc', U8*6), ('pmk', U8*32), ('snonce', U8*32), ('anonce', U8*32), ('ptk', U8*48), ('gtk', U8*16), ('scratch', U8*1016)]
class Keys(C.Structure):
    _fields_ = [('pairwise', U8*16), ('group', U8*16), ('group_rsc', U8*6), ('group_id', U8), ('generation', C.c_uint64)]
class CCMP(C.Structure):
    _fields_ = [('phase', C.c_int), ('generation', C.c_uint64), ('tx_pn', C.c_uint64), ('rx_pairwise', C.c_uint64*17), ('rx_group', C.c_uint64*17), ('peer', Peer), ('tk', U8*16), ('gtk', U8*16), ('group_id', U8), ('scratch', U8*2304)]
def buf(v):return (U8*len(v)).from_buffer_copy(v)
lib = C.CDLL(str(Path(sys.argv[1]).resolve()))
V, G, N = C.c_void_p, C.c_uint64, C.c_size_t
for name, result, args in (
 ('wpa2_hs_start', C.c_bool, [V,V,V,V,G]), ('wpa2_hs_receive', C.c_bool, [V,G,V,V,V,N]),
 ('wpa2_hs_emit', N, [V,G,V,N]), ('wpa2_hs_tx_result', C.c_bool, [V,G,C.c_int]),
 ('wpa2_hs_take_keys', C.c_bool, [V,G,V]), ('wpa2_hs_install_result', C.c_bool, [V,G,C.c_int]),
 ('wpa2_ccmp_install', C.c_bool, [V,V,V]), ('wpa2_ccmp_activate', C.c_bool, [V,V]),
 ('wpa2_ccmp_seal', N, [V,G,V,V,N,V,N]), ('wpa2_ccmp_open', N, [V,G,V,N,V,N]),
 ('wpa2_ccmp_retire', C.c_bool, [V,G]), ('wpa2_ccmp_erased', C.c_bool, [V,G]),
):
 f=getattr(lib,name);f.restype,f.argtypes=result,args
for name,shape in (('state',HS),('peer',Peer),('keys',Keys),('ccmp',CCMP)):
 f=getattr(lib,'wpa2_probe_'+name+'_size');f.restype=N;assert f()==C.sizeof(shape)
assert fixture_header()==(Path(__file__).parent/'fixtures/wpa2-ccmp-synthetic.h').read_text()
p=Peer(aa=(U8*6)(*AA),spa=(U8*6)(*SPA),rsne=(U8*64)(*RSNE),rsne_size=len(RSNE))
h,s=HS(),CCMP()
assert lib.wpa2_hs_start(C.byref(h),C.byref(p),buf(PMK),buf(SN),9)
def receive(frame):assert lib.wpa2_hs_receive(C.byref(h),9,buf(AA),buf(SPA),buf(frame),len(frame))
def emit(want):
 b=(U8*121)();assert lib.wpa2_hs_emit(C.byref(h),9,b,121)==len(want);assert bytes(b)[:len(want)]==want;assert lib.wpa2_hs_tx_result(C.byref(h),9,0)
receive(M1);emit(M2);receive(M3);k=Keys();assert lib.wpa2_hs_take_keys(C.byref(h),9,C.byref(k))
assert lib.wpa2_ccmp_install(C.byref(s),C.byref(h),C.byref(k));assert h.key_owner==C.addressof(s)
assert not lib.wpa2_ccmp_install(C.byref(CCMP()),C.byref(h),C.byref(k))
assert lib.wpa2_hs_install_result(C.byref(h),9,0);emit(M4);assert lib.wpa2_ccmp_activate(C.byref(s),C.byref(h))
# Cloning a public synthetic-key fixture is test-only;native owners must never
# clone/restart a live key context or reclaim a reserved PN.
base=bytes(s)
def fresh():return CCMP.from_buffer_copy(base)
def open_packet(ctx,frame,expected=None,cap=2304):
 out=(U8*cap)(*([0xa5]*cap));oldpn=(list(ctx.rx_pairwise),list(ctx.rx_group));n=lib.wpa2_ccmp_open(C.byref(ctx),9,buf(frame),len(frame),out,cap)
 if expected is None:
  assert n==0 and not any(out) and oldpn==(list(ctx.rx_pairwise),list(ctx.rx_group))
 else:
  assert n==len(expected) and bytes(out)[:n]==expected and not any(out[n:])
 assert not any(ctx.scratch)

r=random.Random(0xcc128)
lengths=[1,15,16,17,31,32,33,1500,2304]+[r.randint(1,2304) for _ in range(64)]
count=0
for i,n in enumerate(lengths):
 data=r.randbytes(n);ctx=fresh();hh=header(tx=True,seq=i%4096);out=(U8*4096)()
 got=lib.wpa2_ccmp_seal(C.byref(ctx),9,buf(hh),buf(data),n,out,4096)
 assert bytes(out)[:got]==encrypt(hh,data,PTK[32:],1) and got==40+n and ctx.tx_pn==1 and not any(out[got:])
 # Discarding a successful seal still consumes PN1;no publication is assumed.
 got=lib.wpa2_ccmp_seal(C.byref(ctx),9,buf(hh),buf(data),n,out,4096)
 assert bytes(out)[:got]==encrypt(hh,data,PTK[32:],2) and ctx.tx_pn==2
 for group,tid in ((False,None),(False,0),(False,7),(True,None),(True,0),(True,3)):
  ctx=fresh();pn=(RSC if group else 0)+i+1;key=GTK if group else PTK[32:];hh=header(group=group,tid=tid,seq=i%4096,flags=(0,0x800,0x1000,0x2000,0x3800)[i%5])
  packet=encrypt(hh,data,key,pn,1 if group else 0);open_packet(ctx,packet,data);open_packet(ctx,packet)
  # Authenticated frame at a newer PN,then corrupted MIC must not advance it.
  newer=encrypt(hh,data,key,pn+1,1 if group else 0);bad=bytearray(newer);bad[-1]^=1;open_packet(ctx,bad);open_packet(ctx,newer,data);count+=1
# Each QoS priority and the non-QoS slot are independent within each key.
for group in (False,True):
 ctx=fresh();pn=RSC+1 if group else 1
 for tid in list(range(8))+[None]:open_packet(ctx,encrypt(header(group=group,tid=tid),DATA,GTK if group else PTK[32:],pn,1 if group else 0),DATA)
 for tid in list(range(8))+[None]:open_packet(ctx,encrypt(header(group=group,tid=tid),DATA,GTK if group else PTK[32:],pn,1 if group else 0))
# Group-key RSC floor,wrong key/index,zero PN and unsupported headers fail even
# when the independent oracle supplies a valid AESCCM tag.
for pn in (0,1,RSC-1,RSC):open_packet(fresh(),encrypt(header(group=True),DATA,GTK,pn,1))
for keyid in (0,2,3):open_packet(fresh(),encrypt(header(group=True),DATA,GTK,RSC+1,keyid))
open_packet(fresh(),encrypt(header(),DATA,GTK,1))
for tid in range(8,16):open_packet(fresh(),encrypt(header(tid=tid),DATA,PTK[32:],1))
for flags in (0x100,0x400,0x8000):open_packet(fresh(),encrypt(header(flags=flags),DATA,PTK[32:],1))
# Every tag bit at an unconsumed PN refuses without plaintext/replay publication.
packet=encrypt(header(),DATA,PTK[32:],1)
for i in range(8):
 for bit in range(8):
  bad=bytearray(packet);bad[-8+i]^=1<<bit;open_packet(fresh(),bad)
for cap in (1,len(DATA)-1):open_packet(fresh(),packet,cap=cap)
ctx=fresh();ctx.tx_pn=(1<<48)-2;out=(U8*4096)();hh=header(tx=True)
assert lib.wpa2_ccmp_seal(C.byref(ctx),9,buf(hh),buf(DATA),len(DATA),out,4096)
assert bytes(out)[:40+len(DATA)]==encrypt(hh,DATA,PTK[32:],(1<<48)-1)
assert not lib.wpa2_ccmp_seal(C.byref(ctx),9,buf(hh),buf(DATA),len(DATA),out,4096) and not any(out)
assert lib.wpa2_ccmp_retire(C.byref(ctx),9) and lib.wpa2_ccmp_erased(C.byref(ctx),9)
assert not any(ctx.tk) and not any(ctx.gtk) and not any(ctx.scratch)
print(f'wpa2-ccmp differential: PASS {len(lengths)*2} TX seals,{count} RX/replay/MIC triplets,per-key/TID floors,group RSC,nonce/AAD/header/key-ID/capacity refusal,48-bit exhaustion and software erasure;synthetic keys only')
