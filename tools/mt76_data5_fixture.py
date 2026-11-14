#!/usr/bin/env python3
"""Public synthetic native-window AP fixtures,independent AESCCM cipher."""
import contextlib
import io
import struct
from wpa2_handshake_fixture import AA, SPA, SN, GTK, PTK
from wpa2_ccmp_fixture import encrypt, RSC
with contextlib.redirect_stdout(io.StringIO()):
    import test_mt76_ip5 as wire

def fixture_header():
    wire.xid=int.from_bytes(SN[:4],'big')
    exchange=[]
    with contextlib.redirect_stdout(io.StringIO()):
        assert wire.trial(capture=exchange)[0]
    rows=['/* Public synthetic DHCP/DNS/echo AP fixtures;independent AESCCM oracle. */']
    for i,(tx,rx) in enumerate(exchange):
        if i<2:rx=bytes([255])*6+rx[6:] # Exercise the installed GTK/RSC floor.
        h=bytes.fromhex('08013c00')+AA+SPA+tx[:6]+((9+i)<<4).to_bytes(2,'little')
        data=bytes.fromhex('aaaa03000000')+tx[12:]
        mpdu=encrypt(h,data,PTK[32:],i+1)
        dma=(20+len(mpdu)+3)&~3
        wi=struct.pack('<HHBBH',0,0x2000,1,255,len(mpdu))+bytes(11)+bytes([128])
        usb=struct.pack('<I',dma|(1<<19)|(1<<24)|(2<<25))+wi+mpdu+bytes(dma-20-len(mpdu))+bytes(4)
        group=bool(rx[0]&1)
        h=bytes.fromhex('08420000')+rx[:6]+AA+rx[6:12]+((100+i)<<4).to_bytes(2,'little')
        data=bytes.fromhex('aaaa03000000')+rx[12:]
        cipher=encrypt(h,data,GTK if group else PTK[32:],RSC+i+1 if group else i+1,1 if group else 0)
        for name,value in ((f'data5_tx{i}',usb),(f'data5_rx{i}',cipher)):
            rows.append('static const unsigned char '+name+'[]={'+','.join(f'0x{x:02x}' for x in value)+'};')
    rows.append('static const struct {const unsigned char *tx,*rx;unsigned tx_size,rx_size;} data5_fixture[]={')
    for i in range(6):rows.append(f' {{data5_tx{i},data5_rx{i},sizeof data5_tx{i},sizeof data5_rx{i}}},')
    rows.append('};')
    return '\n'.join(rows)+'\n'

if __name__=='__main__':
    import sys
    from pathlib import Path
    value=fixture_header()
    if '--check' in sys.argv:
        assert value==(Path(__file__).parent/'fixtures/mt76-data5-synthetic.h').read_text()
        print('mt76-data5: independent encrypted native AP fixture equality passed')
    else:print(value,end='')
