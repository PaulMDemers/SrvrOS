#!/usr/bin/env python3
"""Public synthetic70-frame oracle;Python AESCCM,never native seal/real keys."""
import argparse,hashlib,json,re,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from wpa2_handshake_fixture import AA,SPA,SN,PTK
from wpa2_ccmp_fixture import encrypt
known=(root/'tools/fixtures/mt76-data5-synthetic.h').read_text()
def old(kind,index):
 text=re.search(r'static const unsigned char data5_'+kind+str(index)+r'\[\]=\{([^}]*)\}',known)[1]
 return bytes(int(x,16) for x in text.split(','))
def check(p):
 if len(p)%2:p+=b'\0'
 total=sum(struct.unpack('!'+str(len(p)//2)+'H',p))
 while total>>16:total=(total&65535)+(total>>16)
 return (~total)&65535
p16=lambda n:struct.pack('!H',n)
p32=lambda n:struct.pack('!I',n)
xid=int.from_bytes(SN[:4],'big');gateway=bytes.fromhex('021122334455');client=bytes([192,0,2,22]);router=bytes([192,0,2,1])
def ipv4(body,src,dst,ident):
 h=b'\x45\0'+p16(20+len(body))+p16(ident)+p16(0x4000)+bytes([64,1,0,0])+src+dst
 return h[:10]+p16(check(h))+h[12:]+body

def make(index):
 body=p32(0x53525354)+p32(xid)+p32(index)+bytes(((xid>>(8*(i%4)))+73*i+29*index+0x51)&255 for i in range(12,256))
 h=bytes([8,0,0,0])+p16(((xid>>16)^0x5354)&65535)+p16(0x4000+index)+body
 icmp=h[:2]+p16(check(h))+h[4:]
 tx=gateway+SPA+p16(0x800)+ipv4(icmp,client,router,(xid+0x100+index)&65535)
 h=bytes(4)+icmp[4:];icmp=h[:2]+p16(check(h))+h[4:]
 rx=SPA+gateway+p16(0x800)+ipv4(icmp,router,client,123)
 packet=index+6
 wh=bytes.fromhex('08013c00')+AA+SPA+tx[:6]+((9+packet)<<4).to_bytes(2,'little')
 mpdu=encrypt(wh,bytes.fromhex('aaaa03000000')+tx[12:],PTK[32:],packet+1)
 dma=(20+len(mpdu)+3)&~3
 wi=struct.pack('<HHBBH',0,0x2000,1,255,len(mpdu))+bytes(11)+bytes([128])
 usb=struct.pack('<I',dma|(1<<19)|(1<<24)|(2<<25))+wi+mpdu+bytes(dma-20-len(mpdu))+bytes(4)
 wh=bytes.fromhex('08420000')+rx[:6]+AA+rx[6:12]+((100+packet)<<4).to_bytes(2,'little')
 received=encrypt(wh,bytes.fromhex('aaaa03000000')+rx[12:],PTK[32:],packet+1)
 assert len(tx)==len(rx)==298 and check(tx[14:34])==check(tx[34:])==check(rx[14:34])==check(rx[34:])==0
 return usb,received
pairs=[(old('tx',i),old('rx',i)) for i in range(6)]+[make(i) for i in range(64)]
rows=['/* Public synthetic70-frame stream fixture;independent Python AESCCM oracle. */']
for i,pair in enumerate(pairs):
 for kind,value in zip(('tx','rx'),pair):rows.append('static const unsigned char stream5_'+kind+str(i)+'[]={'+','.join('0x%02x'%x for x in value)+'};')
rows.append('static const struct {const unsigned char *tx,*rx;unsigned tx_size,rx_size;} stream5_fixture[]={')
for i in range(70):rows.append(f' {{stream5_tx{i},stream5_rx{i},sizeof stream5_tx{i},sizeof stream5_rx{i}}},')
rows.append('};')
value='\n'.join(rows)+'\n'
def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--output',required=True,type=Path,help='generated public C fixture header')
 parser.add_argument('--json',type=Path,help='optional synthetic provenance and input hashes')
 parser.add_argument('--check',action='store_true',help='compare existing output,without writing')
 args=parser.parse_args()
 encoded=value.encode('ascii')
 record=dict(synthetic=True,public_keys_only=True,packets=len(pairs),
  xid_source='public SN first4',setup_source='existing independentAESCCM public6-framegolden',
  extra_source='independentPythonwire+AESCCM',header_bytes=len(encoded),
  header_sha256=hashlib.sha256(encoded).hexdigest(),tx_pn_first=1,tx_pn_last=70,
  scope='synthetic expected TXUSB/RXMPDU bytes;no native ownership or hardware acceptance',
  inputs={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in
   ('tools/fixtures/mt76-data5-synthetic.h','tools/wpa2_handshake_fixture.py','tools/wpa2_ccmp_fixture.py','tools/make_mt76_stream5_oracle.py')})
 if args.check:
  if not args.output.is_file() or args.output.read_bytes()!=encoded:
   parser.exit(1,'stream5-oracle: output differs\n')
  if args.json and (not args.json.is_file() or json.loads(args.json.read_text())!=record):
   parser.exit(1,'stream5-oracle: JSON differs\n')
 else:
  if args.output.is_symlink() or (args.json and args.json.is_symlink()):
   parser.exit(1,'stream5-oracle: output symlink refused\n')
  args.output.write_bytes(encoded)
  if args.json:args.json.write_text(json.dumps(record,indent=2,sort_keys=True)+'\n')
 print('stream5-oracle:70 public TXUSB/RXMPDU pairs;independent AESCCM;synthetic only')

if __name__=='__main__':main()
