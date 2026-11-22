"""Exact bounded DNS peer and capture proof for the isolated HTTP guest only."""
import ipaddress,json,socket,struct,subprocess,threading
from pathlib import Path
QUESTION=b'\x07httpbin\x03org\0\0\x01\0\x01'
ANSWER=socket.inet_aton('10.0.2.2')
def dns_response(query):
 assert len(query)==12+len(QUESTION),'unexpected DNS length'
 ident,flags,qd,an,ns,ar=struct.unpack('!6H',query[:12])
 assert (flags,qd,an,ns,ar)==(0x100,1,0,0,0) and query[12:]==QUESTION,'unexpected DNS question'
 return struct.pack('!6H',ident,0x8180,1,1,0,0)+QUESTION+b'\xc0\x0c\0\x01\0\x01\0\0\0\x3c\0\x04'+ANSWER

def configured_resolver(path=Path('/etc/resolv.conf')):
 for line in path.read_text().splitlines():
  words=line.split('#',1)[0].split()
  if len(words)>=2 and words[0]=='nameserver':
   try:address=ipaddress.ip_address(words[1])
   except ValueError:continue
   if address.version==4:
    assert not address.is_unspecified and not address.is_multicast and int(address)!=0xffffffff,'unsafe resolver address'
    return str(address)
 raise AssertionError('no configured IPv4 nameserver; fixture refuses fallback')

def verify_local_routes():
 assert [name for _,name in socket.if_nameindex()]==['lo'],'non-loopback interface'
 result={}
 for family in ('-4','-6'):
  assert json.loads(subprocess.check_output(['ip','-j',family,'route','show','table','main'],text=True))==[],'main route appeared'
  rows=json.loads(subprocess.check_output(['ip','-j',family,'route','show','table','all'],text=True))
  assert all(row.get('dev')=='lo' and row.get('type') in ('local','broadcast','multicast') for row in rows),'nonlocal route appeared'
  result[family]=rows
 return result

def configure_private_resolver(namespace):
 # Caller must have already proved this exact new mapped rootless namespace.
 # Recheck identities before ANY address mutation; do not trust --inside alone.
 import os
 assert namespace['only_loopback'] and not namespace['external_routes']
 assert os.readlink('/proc/self/ns/net')==namespace['network_namespace']!=namespace['parent_network_namespace']
 assert os.readlink('/proc/self/ns/user')==namespace['user_namespace']!=namespace['parent_user_namespace']
 assert os.getuid()==os.geteuid()==0 and Path('/proc/self/uid_map').read_text().split()==list(map(str,namespace['uid_map']))
 before=verify_local_routes();address=configured_resolver()
 rows=json.loads(subprocess.check_output(['ip','-j','-4','address','show','dev','lo'],text=True))
 present=any(ipaddress.ip_address(address) in ipaddress.ip_network(f"{a['local']}/{a['prefixlen']}",strict=False) for row in rows for a in row.get('addr_info',[]) if a['family']=='inet')
 if not present:
  subprocess.run(['ip','-4','address','add',address+'/32','dev','lo','scope','host','noprefixroute'],check=True)
 after=verify_local_routes()
 return {'configured_ipv4_resolver':address,'private_loopback_alias_added':not present,'alias_prefix':32 if not present else None,'routes_before':before,'routes_after':after,'host_resolver_modified':False,'external_route_added':False}

class DNSPeer:
 def __init__(self,address,port=53):
  self.socket=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
  try:self.socket.bind((address,port));self.socket.settimeout(.2)
  except BaseException:self.socket.close();raise
  self.address=address;self.port=self.socket.getsockname()[1];self.records=[];self.error=None;self.stop=threading.Event();self.thread=threading.Thread(target=self.run,daemon=True)
 def start(self):self.thread.start();return self
 def run(self):
  try:
   while not self.stop.is_set():
    try:query,source=self.socket.recvfrom(513)
    except socket.timeout:continue
    assert len(self.records)<3,'DNS query budget exceeded'
    response=dns_response(query)
    assert self.socket.sendto(response,source)==len(response)
    self.records.append({'query_hex':query.hex(),'response_hex':response.hex(),'question':'httpbin.org','qtype':1,'qclass':1,'answer':'10.0.2.2'})
  except BaseException as error:
   if not self.stop.is_set():self.error=repr(error)
 def close(self):
  self.stop.set();self.thread.join(timeout=1);self.socket.close();assert not self.thread.is_alive(),'DNS peer did not stop'
 def finish(self):
  self.close();assert self.error is None,self.error;assert 1<=len(self.records)<=3,'no exact DNS exchange'
  return {'bind_address':self.address,'port':self.port,'queries':len(self.records),'records':self.records,'external_forwarding':False}

def _sum(data):
 if len(data)%2:data+=b'\0'
 value=sum(struct.unpack('!%dH'%(len(data)//2),data))
 while value>>16:value=(value&65535)+(value>>16)
 return value

def capture_proof(path,dns,request,response):
 raw=Path(path).read_bytes();assert 24<=len(raw)<=4*1024*1024
 endian={b'\xd4\xc3\xb2\xa1':'<',b'\xa1\xb2\xc3\xd4':'>'}.get(raw[:4]);assert endian,'unsupported pcap magic'
 assert struct.unpack_from(endian+'I',raw,20)[0]==1,'non-Ethernet capture'
 client=socket.inet_aton('10.0.2.15');server=ANSWER;resolver=socket.inet_aton('10.0.2.3');zero=b'\0'*4;broadcast=b'\xff'*4
 at=24;dhcp=[];dns_queries=[];dns_answers=[];arp=[];tcp=[[],[]];frames=0;ports=None;dhcp_xid=None;station=None
 while at<len(raw):
  assert at+16<=len(raw),'truncated pcap header'
  _,_,n,original=struct.unpack_from(endian+'4I',raw,at);at+=16
  assert n==original and 14<=n<=65536 and at+n<=len(raw),'truncated pcap frame'
  frame=raw[at:at+n];at+=n;frames+=1;kind=frame[12:14]
  if kind==b'\x08\x06':
   a=frame[14:];assert len(a)>=28 and a[:6]==b'\0\1\x08\0\6\4','ARP layout'
   op=int.from_bytes(a[6:8],'big');source,target=a[14:18],a[24:28]
   assert op in (1,2) and source in (client,server,resolver,zero) and target in (client,server,resolver,zero),'foreign ARP'
   arp.append((op,source,target));continue
  assert kind==b'\x08\x00','foreign Ethernet protocol'
  ip=frame[14:];assert len(ip)>=20;ihl=(ip[0]&15)*4;size=int.from_bytes(ip[2:4],'big')
  assert ip[0]>>4==4 and ihl>=20 and ihl<=size<=len(ip) and not int.from_bytes(ip[6:8],'big')&0x3fff
  assert _sum(ip[:ihl])==65535,'IPv4 checksum';body=ip[ihl:size];source,target=ip[12:16],ip[16:20]
  pseudo=source+target+bytes((0,ip[9]))+len(body).to_bytes(2,'big')
  if ip[9]==17:
   assert len(body)>=8;sport,dport,usize,check=struct.unpack('!4H',body[:8]);assert usize==len(body)
   assert not check or _sum(pseudo+body)==65535,'UDP checksum'
   payload=body[8:]
   if (sport,dport) in ((68,67),(67,68)):
    outgoing=sport==68
    assert source==(zero if outgoing else server) and target==broadcast,'foreign DHCP destination'
    assert len(payload)>=240 and payload[:3]==bytes((1 if outgoing else 2,1,6)) and payload[236:240]==b'\x63\x82\x53\x63','DHCP layout'
    if dhcp_xid is None:dhcp_xid=payload[4:8];station=payload[28:34]
    assert payload[4:8]==dhcp_xid and payload[28:34]==station,'foreign DHCP transaction/station'
    opts={};i=240;ended=False
    while i<len(payload):
     key=payload[i];i+=1
     if key==255:ended=True;break
     if key==0:continue
     assert i<len(payload);length=payload[i];i+=1;assert i+length<=len(payload) and key not in opts,'duplicate/truncated DHCP option'
     opts[key]=payload[i:i+length];i+=length
    assert ended and 53 in opts and len(opts[53])==1;message=opts[53][0]
    assert message in ((1,3) if outgoing else (2,5)),'foreign DHCP message'
    if not outgoing:
     assert {1,3,6,51,53,54}<=opts.keys() and payload[16:20]==client,'incomplete DHCP configuration'
     assert opts[1]==b'\xff\xff\xff\0' and opts[3]==server and opts[6]==resolver and opts[54]==server,'wrong DHCP route/DNS/server'
     assert len(opts[51])==4 and int.from_bytes(opts[51],'big')>0,'invalid DHCP lease'
    dhcp.append(message);continue
   assert (source,target,sport,dport) in ((client,resolver,53000,53),(resolver,client,53,53000)),'foreign UDP/DNS traffic'
   if dport==53:dns_response(payload);dns_queries.append(payload.hex())
   else:dns_answers.append(payload.hex())
   continue
  assert ip[9]==6 and (source,target) in ((client,server),(server,client)),'foreign IP traffic/destination'
  assert len(body)>=20 and _sum(pseudo+body)==65535,'TCP checksum'
  sport,dport,seq,ack=struct.unpack('!HHII',body[:12]);header=(body[12]>>4)*4;assert 20<=header<=len(body)
  direction=int(source==server);pair=(dport,sport) if direction else (sport,dport)
  if ports is None:ports=pair
  assert pair==ports and pair[1]==80 and 1024<=pair[0]<=65535,'foreign TCP port pair'
  flags=body[13];assert not flags&4,'TCP RST'
  tcp[direction].append((frames,seq,ack,flags,body[header:]))
 assert dhcp==[1,2,3,5],('nonexact DHCP DORA; retries not accepted by this fixture',dhcp)
 assert dns_queries and dns_answers and len(dns_queries)==len(dns_answers)==dns['queries']
 assert sorted(dns_queries)==sorted(row['query_hex'] for row in dns['records'])
 assert sorted(dns_answers)==sorted(row['response_hex'] for row in dns['records'])
 assert any(op==1 and source==client and target==server for op,source,target in arp) and any(op==2 and source==server and target==client for op,source,target in arp),'route ARP missing'
 fins=[]
 for direction,expected in enumerate((request,response)):
  stream={};syn=[seq for index,seq,ack,flags,data in tcp[direction] if flags&2];ends=[];fin_indices=[]
  assert syn and set(syn)=={syn[0]};start=(syn[0]+1)&0xffffffff
  for index,seq,ack,flags,data in tcp[direction]:
   offset=(seq-start)&0xffffffff
   if flags&2:assert not data;continue
   assert offset<=len(expected) or (not data and not flags&1 and flags&16 and offset==len(expected)+1 and ends),'TCP frontier beyond exact stream'
   for i,byte in enumerate(data):
    key=offset+i;assert key<len(expected) and (key not in stream or stream[key]==byte),'conflicting/extra TCP data';stream[key]=byte
   if flags&1:ends.append(offset+len(data));fin_indices.append(index)
  assert len(stream)==len(expected) and bytes(stream[i] for i in range(len(expected)))==expected,'TCP stream mismatch/gap'
  assert ends and set(ends)=={len(expected)},'FIN did not follow exact stream';fins.append(((start+len(expected)+1)&0xffffffff,min(fin_indices)))
 # A numerically matching ACK is proof only after the opposite terminal FIN
 # appeared in this capture. Use the first matching FIN so later retransmits
 # do not invalidate a legitimate earlier acknowledgement.
 assert all(any(flags&16 and ack==fins[1-d][0] and index>fins[1-d][1]
  for index,seq,ack,flags,data in tcp[d]) for d in (0,1)),'FIN not acknowledged after observed terminal FIN'
 return {'frames':frames,'dhcp_types':dhcp,'dns_queries':len(dns_queries),'dns_answers':len(dns_answers),'exact_dns_peer_capture_match':True,'route_arp':True,'request_exact':True,'response_exact':True,'bidirectional_FIN_ACK':True,'FIN_ACK_capture_order':True,'IPv4_UDP_TCP_checksums':True,'only_private_expected_destinations':True,'DHCP_retries_allowed':False}
