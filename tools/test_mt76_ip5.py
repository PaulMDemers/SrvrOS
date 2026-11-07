#!/usr/bin/env python3
"""Independent synthetic Ethernet server for the disposable IP diagnostic."""
import ctypes as C
from pathlib import Path
import struct as S
import sys

U8=C.c_ubyte;V=C.c_void_p;N=C.c_size_t;G=C.c_uint64
class State(C.Structure):
    _fields_=[(n,C.c_bool) for n in ('attempted','complete','unsafe','discover','offer','request','ack','arp_dns','dns','arp_router','echo','exhausted')]+[(n,C.c_uint) for n in ('phase','sends','polls','ignored')]+[('arp_class',C.c_uint*9)]+[(n,C.c_uint32) for n in ('xid','address','server','mask','router','resolver','answer')]+[(n,U8*6) for n in ('station','dns_mac','router_mac')]+[('tx',U8*1514),('rx',U8*1514)]
Owned=C.CFUNCTYPE(C.c_bool,V);Send=C.CFUNCTYPE(C.c_int,V,V,N);Poll=C.CFUNCTYPE(C.c_bool,V,V,N,C.POINTER(N),C.POINTER(C.c_bool));Delay=C.CFUNCTYPE(C.c_bool,V,C.c_uint);Now=C.CFUNCTYPE(G,V)
Exhausted=C.CFUNCTYPE(C.c_bool,V)
class IO(C.Structure):
    _fields_=[('context',V),('owned',Owned),('send',Send),('poll',Poll),('delay',Delay),('now',Now),('exhausted',Exhausted)]
lib=C.CDLL(str(Path(sys.argv[1]).resolve()));lib.mt76_ip5_probe_size.restype=N
assert lib.mt76_ip5_probe_size()==C.sizeof(State)
lib.mt76_ip5_run.restype=C.c_bool;lib.mt76_ip5_run.argtypes=[V,V,C.c_uint32,V]
station=bytes.fromhex('02060708090a');gateway=bytes.fromhex('021122334455');dns_mac=bytes.fromhex('025566778899')
client=bytes([192,0,2,22]);server=bytes([192,0,2,1]);mask=bytes([255,255,255,0]);router=server
xid=0x13572468
def p16(n):return S.pack('!H',n)
def p32(n):return S.pack('!I',n)
def u16(p):return S.unpack('!H',p)[0]
def check(p):
    if len(p)%2:p+=bytes(1)
    value=sum(S.unpack('!'+str(len(p)//2)+'H',p))
    while value>>16:value=(value&65535)+(value>>16)
    return (~value)&65535
def ethernet(payload,proto=0x800,src=gateway,dst=station):return dst+src+p16(proto)+payload
def ip_packet(payload,protocol,src,dst):
    h=bytes([0x45,0])+p16(20+len(payload))+p16(123)+p16(0x4000)+bytes([64,protocol,0,0])+src+dst
    return h[:10]+p16(check(h))+h[12:]+payload
def udp_packet(payload,sport,dport,src,dst):
    udp=p16(sport)+p16(dport)+p16(len(payload)+8)+bytes(2)+payload
    checksum=check(src+dst+bytes([0,17])+p16(len(udp))+udp) or 65535
    return udp[:6]+p16(checksum)+udp[8:]
def wrap_udp(payload,sport,dport,src,dst):return ethernet(ip_packet(udp_packet(payload,sport,dport,src,dst),17,src,dst))
def options(packet):
    result={};at=240
    while at<len(packet):
        code=packet[at];at+=1
        if code==255:assert not any(packet[at:]);break
        if not code:continue
        n=packet[at];at+=1;assert code not in result and at+n<=len(packet);result[code]=packet[at:at+n];at+=n
    return result
def bootp(kind,resolver):
    p=bytearray(240);p[:3]=bytes([2,1,6]);p[4:8]=p32(xid);p[16:20]=client;p[28:34]=station;p[236:240]=bytes.fromhex('63825363')
    return bytes(p)+bytes([53,1,kind,54,4])+server+bytes([1,4])+mask+bytes([3,4])+router+bytes([6,4])+resolver+bytes([51,4])+p32(3600)+bytes([255])+bytes(17)
def arp_reply(query):
    target=query[38:42];source=gateway if target==router else dns_mac
    return ethernet(bytes.fromhex('0001080006040002')+source+target+station+client,0x806,source)
def dns_reply(query,resolver):
    request=query[42:];assert request[2:12]==bytes.fromhex('01000001000000000000')
    assert request[12:]==b'\x07example\x03com\0\0\x01\0\x01'
    payload=request[:2]+bytes.fromhex('81800001000100000000')+request[12:]+bytes.fromhex('c00c000100010000003c0004cb007107')
    return wrap_udp(payload,53,49161,resolver,client)
def echo_reply(query):
    icmp=bytearray(query[34:]);assert len(icmp)==264 and not check(bytes(icmp)) and icmp[:2]==bytes([8,0])
    assert icmp[4:8]==p16(xid&65535)+p16(1) and icmp[8:]==bytes((i*73+0x51)&255 for i in range(256))
    icmp[:4]=bytes(4);icmp[2:4]=p16(check(bytes(icmp)));return ethernet(ip_packet(bytes(icmp),1,router,client))

def trial(*,finite=False,finite_loss=False,resolver=bytes([192,0,2,53]),fail_send=0,send_result=1,lose=0,fail_poll=0,fail_delay=False,empty=False,backward=False,late=False,mutate=None,bad_span=False,capture=None):
    state=State();time=1000;checks=0;sent=[];queued=None;errors=[];lost=False;polls=0
    def guard(function,default):
        def f(*args):
            try:return function(*args)
            except BaseException as e:errors.append(e);return default
        return f
    def owned(_):
        nonlocal checks,lost
        checks+=1
        if checks==lose:lost=True
        return not lost
    def send(_,data,n):
        nonlocal queued
        assert not lost and n<=1514
        p=C.string_at(data,n);sent.append(p);phase=state.phase;assert phase==len(sent)-1
        assert p[6:12]==station
        if p[12:14]==bytes.fromhex('0800'):
            h=p[14:34];assert len(p)==14+u16(h[2:4]) and h[0]==0x45 and not check(h)
            if h[9]==17:
                u=p[34:];assert u16(u[4:6])==len(u) and not check(h[12:20]+bytes([0,17])+p16(len(u))+u)
        if phase<2:
            assert p[:6]==bytes([255])*6 and p[26:34]==bytes(4)+bytes([255])*4 and p[34:38]==p16(68)+p16(67)
            b=p[42:];assert len(b)==300 and b[:3]==bytes([1,1,6]) and b[4:8]==p32(xid) and b[10:12]==bytes.fromhex('8000') and b[28:34]==station
            o=options(b);assert o[53]==bytes([3 if phase else 1]) and o[61]==bytes([1])+station and o[55]==bytes([1,3,6,51,54])
            if phase:assert o[50]==client and o[54]==server
            else:assert 50 not in o and 54 not in o
            queued=wrap_udp(bootp(5 if phase else 2,resolver),67,68,server,bytes([255])*4)
        elif phase in (2,4):
            assert p[12:22]==bytes.fromhex('08060001080006040001') and p[22:28]==station and p[28:32]==client and not any(p[32:38])
            expected=router if phase==4 or resolver[:3]!=client[:3] else resolver
            assert p[38:42]==expected;queued=arp_reply(p)
        elif phase==3:
            expected_mac=gateway if resolver[:3]!=client[:3] else dns_mac
            assert p[:6]==expected_mac and p[26:34]==client+resolver and p[34:38]==p16(49161)+p16(53)
            queued=dns_reply(p,resolver)
        elif phase==5:
            assert p[:6]==gateway and p[26:34]==client+router;queued=echo_reply(p)
        if capture is not None:capture.append((p,queued))
        if mutate:queued=mutate(phase,queued)
        return send_result if len(sent)==fail_send else 0
    def poll(_,data,cap,count,present):
        nonlocal queued,time,polls
        polls+=1;assert cap==1514 and not lost;count[0]=0;present[0]=False
        if polls==fail_poll:return False
        if bad_span:count[0]=1515;present[0]=True;return True
        if queued and not empty:
            C.memmove(data,queued,len(queued));count[0]=len(queued);present[0]=True;queued=None
            if late:time+=2000000
            if backward:time=0
        else:time+=100000
        return True
    def delay(_,us):
        nonlocal time
        assert us==100;time+=us;return not fail_delay
    def exhausted(_):
        nonlocal lost
        if finite_loss and polls:lost=True
        return finite and bool(polls)
    callbacks=(Owned(guard(owned,False)),Send(guard(send,2)),Poll(guard(poll,False)),Delay(guard(delay,False)),Now(lambda _:time),Exhausted(guard(exhausted,False)) if finite or finite_loss else Exhausted())
    io=IO(None,*callbacks);mac=(U8*6).from_buffer_copy(station)
    ok=lib.mt76_ip5_run(C.byref(state),mac,xid,C.byref(io))
    if errors:raise errors[0]
    assert not any(state.tx) and not any(state.rx) and state.sends==len(sent) and state.polls==polls
    return ok,state,checks

ok,s,checks=trial();assert ok and s.sends==6 and s.polls==6 and s.answer==0xcb007107 and s.echo
ok,s,_=trial(empty=True,finite=True);assert not ok and s.exhausted and not s.unsafe and s.polls==1 and s.sends==1
ok,s,_=trial(finite=True);assert not ok and s.exhausted and not s.unsafe and s.phase==1 and s.polls==1 and s.sends==1
ok,s,_=trial(empty=True,finite_loss=True);assert not ok and s.unsafe and not s.exhausted and s.polls==1
assert trial(resolver=bytes([203,0,113,53]))[0]
for i in range(1,checks+1):
    ok,s,_=trial(lose=i);assert not ok and s.unsafe
for i in range(1,7):
    for result in (1,2,99):
        ok,s,_=trial(fail_send=i,send_result=result);assert not ok and s.sends==i and s.unsafe==(result!=1)
    ok,s,_=trial(fail_poll=i);assert not ok and s.unsafe
for kwargs in ({'empty':True},{'late':True},{'backward':True},{'bad_span':True},{'empty':True,'fail_delay':True}):assert not trial(**kwargs)[0]

def changed(phase,offset,value):
    def f(at,p):
        if at!=phase:return p
        b=bytearray(p);b[offset]^=value
        # Recompute IP/UDP checksums so payload tests reach DHCP/DNS/ICMP.
        if b[12:14]==bytes.fromhex('0800'):
            if b[23]==17:
                b[40:42]=bytes(2);b[40:42]=p16(check(bytes(b[26:34])+bytes([0,17])+p16(len(b)-34)+bytes(b[34:])) or 65535)
            elif b[23]==1:b[36:38]=bytes(2);b[36:38]=p16(check(bytes(b[34:])))
            b[24:26]=bytes(2);b[24:26]=p16(check(bytes(b[14:34])))
        return bytes(b)
    return f
for phase in (0,1):
    for offset,value in [(42,1),(43,1),(44,1),(46,1),(70,1),(278,1),(282,1),(284,1)]+([(58,1),(287,1)] if phase else []):
        assert not trial(mutate=changed(phase,offset,value))[0],(phase,offset)
for phase in (2,4):
    for offset in (0,6,14,16,18,19,20,22,28,32,38):assert not trial(mutate=changed(phase,offset,1))[0],(phase,offset)
for phase in (3,5):
    for offset in (0,6,14,20,23,26,30,34,35,38):assert not trial(mutate=changed(phase,offset,1))[0],(phase,offset)
for phase in range(6):
    def truncated(at,p):return p[:len(p)//2] if at==phase else p
    assert not trial(mutate=truncated)[0]
for offset in range(42,298):assert not trial(mutate=changed(5,offset,1))[0]
# ARP classification is aggregate,first refusal only,and never relaxes binding.
for phase in (2,4):
    for reason,offset in ((1,12),(2,14),(3,21),(4,0),(5,28),(6,38)):
        ok,state,*_=trial(mutate=changed(phase,offset,1))
        assert not ok and state.arp_class[reason]==1,(phase,reason,list(state.arp_class))
        assert sum(state.arp_class[:8])==(1 if phase==2 else 2)
    ok,state,*_=trial(mutate=lambda at,p:p[:20] if at==phase else p)
    assert not ok and state.arp_class[0]==1
def request_for_us(at,p):
    if at!=2:return p
    b=bytearray(p);b[20:22]=p16(1);b[:6]=bytes([255])*6;return bytes(b)
ok,state,*_=trial(mutate=request_for_us)
assert not ok and state.arp_class[3]==1 and state.arp_class[8]==1
ok,state,*_=trial(mutate=lambda at,p:p+bytes(10) if at in (2,4) else p)
assert ok and state.arp_class[7]==2 and sum(state.arp_class)==2
print('mt76-ip5: PASS independent synthetic DHCP DORA,local/routed DNS ARP,shared strict DNS parser,256-byte echoed payload;all ownership/send/poll/time bounds and directed wire/checksum/span/payload refusals;no native IP acceptance')
