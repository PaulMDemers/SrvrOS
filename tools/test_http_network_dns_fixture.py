import importlib.util,ipaddress,json,socket,struct,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import http_network_dns_fixture as d
import http_network_smoke as h

def packet(protocol,src,dst,body):
 a=socket.inet_aton(src);b=socket.inet_aton(dst)
 pseudo=a+b+bytes((0,protocol))+len(body).to_bytes(2,'big')
 if protocol==6:
  body=bytearray(body);body[16:18]=(65535-d._sum(pseudo+body)).to_bytes(2,'big');body=bytes(body)
 ip=bytearray(b'\x45\0'+(20+len(body)).to_bytes(2,'big')+b'\0\0\0\0\x40'+bytes((protocol,))+b'\0\0'+a+b)
 ip[10:12]=(65535-d._sum(ip)).to_bytes(2,'big')
 return b'\0'*12+b'\x08\0'+ip+body

def udp(src,dst,sport,dport,payload):return packet(17,src,dst,struct.pack('!4H',sport,dport,8+len(payload),0)+payload)
def tcp(out,seq,ack,flags,payload=b''):
 return packet(6,'10.0.2.15' if out else '10.0.2.2','10.0.2.2' if out else '10.0.2.15',struct.pack('!HHII',42000 if out else 80,80 if out else 42000,seq,ack)+b'\x50'+bytes((flags,))+b'\xff\xff\0\0\0\0'+payload)
def capture(frames):return struct.pack('<IHH4I',0xa1b2c3d4,2,4,0,0,65536,1)+b''.join(struct.pack('<4I',0,0,len(f),len(f))+f for f in frames)
def good_frames():
 frames=[]
 for kind in (1,2,3,5):
  out=kind in (1,3);body=bytearray(240);body[:3]=bytes((1 if out else 2,1,6));body[4:8]=b'\x12\x34\x56\x78';body[28:34]=b'\x52\x54\x00\x12\x34\x56';body[236:240]=b'\x63\x82\x53\x63';body+=bytes((53,1,kind))
  if not out:
   body[16:20]=socket.inet_aton('10.0.2.15')
   for key,value in ((1,b'\xff\xff\xff\0'),(3,d.ANSWER),(6,socket.inet_aton('10.0.2.3')),(51,(3600).to_bytes(4,'big')),(54,d.ANSWER)):body+=bytes((key,len(value)))+value
  body+=b'\xff'
  frames.append(udp('0.0.0.0' if out else '10.0.2.2','255.255.255.255',68 if out else 67,67 if out else 68,bytes(body)))
 query=b'\x12\x34\x01\x00\x00\x01'+b'\0'*6+d.QUESTION;answer=d.dns_response(query)
 frames.extend([udp('10.0.2.15','10.0.2.3',53000,53,query),udp('10.0.2.3','10.0.2.15',53,53000,answer)])
 for op in (1,2):
  frames.append(b'\0'*12+b'\x08\x06'+b'\0\1\x08\0\6\4'+op.to_bytes(2,'big')+b'\0'*6+socket.inet_aton('10.0.2.15' if op==1 else '10.0.2.2')+b'\0'*6+socket.inet_aton('10.0.2.2' if op==1 else '10.0.2.15'))
 c,s=100,10000
 frames.extend([tcp(True,c,0,2),tcp(False,s,c+1,18),tcp(True,c+1,s+1,16)])
 for start in range(0,len(h.REQUEST),900):frames.append(tcp(True,c+1+start,s+1,24,h.REQUEST[start:start+900]))
 for start in range(0,len(h.RESPONSE),1100):frames.append(tcp(False,s+1+start,c+1+len(h.REQUEST),24,h.RESPONSE[start:start+1100]))
 cf=c+1+len(h.REQUEST);sf=s+1+len(h.RESPONSE)
 frames.extend([tcp(False,sf,cf,17),tcp(True,cf,sf+1,17),tcp(False,sf+1,cf+1,16)])
 dns={'queries':1,'records':[{'query_hex':query.hex(),'response_hex':answer.hex()}]}
 return frames,dns

class Tests(unittest.TestCase):
 def test_exact_dns(self):
  q=b'\x12\x34\x01\x00\x00\x01'+b'\0'*6+d.QUESTION
  a=d.dns_response(q);self.assertEqual(a[:12],b'\x12\x34\x81\x80\x00\x01\x00\x01\0\0\0\0');self.assertEqual(a[-4:],d.ANSWER)
  for bad in (q[:-1],q+b'\0',q[:2]+b'\x00\0'+q[4:],q.replace(b'httpbin',b'example'),q[:-2]+b'\0\x02'):
   with self.subTest(bad=bad),self.assertRaises(AssertionError):d.dns_response(bad)
 def test_resolver(self):
  with tempfile.TemporaryDirectory() as folder:
   f=Path(folder)/'resolv.conf';f.write_text('nameserver ::1\nnameserver 192.0.2.1\nnameserver 192.0.2.2\n');self.assertEqual(d.configured_resolver(f),'192.0.2.1')
   for text in ('nameserver ::1\n','nameserver 0.0.0.0\n','nameserver 224.0.0.1\n'):
    f.write_text(text)
    with self.assertRaises(AssertionError):d.configured_resolver(f)
 def test_namespace_refusal_before_mutation(self):
  with patch.object(d.subprocess,'run') as run,patch.object(d.subprocess,'check_output') as read:
   with self.assertRaises(AssertionError):d.configure_private_resolver({'only_loopback':False,'external_routes':False})
   run.assert_not_called();read.assert_not_called()
 def test_private_alias_command(self):
  ns={"only_loopback":True,"external_routes":False,"network_namespace":"newnet","parent_network_namespace":"oldnet","user_namespace":"newuser","parent_user_namespace":"olduser","uid_map":[0,1000,1]}
  addresses=json.dumps([{"addr_info":[{"family":"inet","local":"127.0.0.1","prefixlen":8}]}])
  with patch("os.readlink",side_effect=lambda path:"newnet" if path.endswith("/net") else "newuser"),patch("os.getuid",return_value=0),patch("os.geteuid",return_value=0),patch.object(d.Path,"read_text",return_value="0 1000 1\n"),patch.object(d,"configured_resolver",return_value="192.0.2.53"),patch.object(d,"verify_local_routes",return_value={}) as routes,patch.object(d.subprocess,"check_output",return_value=addresses),patch.object(d.subprocess,"run") as run:
   result=d.configure_private_resolver(ns);self.assertTrue(result["private_loopback_alias_added"]);self.assertEqual(routes.call_count,2)
   run.assert_called_once_with(["ip","-4","address","add","192.0.2.53/32","dev","lo","scope","host","noprefixroute"],check=True)
 def test_route_refusal(self):
  with patch.object(d.socket,"if_nameindex",return_value=[(1,"lo"),(2,"eth0")]),self.assertRaises(AssertionError):d.verify_local_routes()
  with patch.object(d.socket,"if_nameindex",return_value=[(1,"lo")]),patch.object(d.subprocess,"check_output",side_effect=["[]",json.dumps([{"dev":"eth0","type":"local"}])]),self.assertRaises(AssertionError):d.verify_local_routes()
 def test_peer(self):
  p=d.DNSPeer('127.0.0.1',0).start();q=b'\x12\x34\x01\x00\x00\x01'+b'\0'*6+d.QUESTION
  try:
   with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as c:c.settimeout(2);c.sendto(q,('127.0.0.1',p.port));self.assertEqual(c.recv(512),d.dns_response(q))
   self.assertEqual(p.finish()['queries'],1)
  finally:p.close()
 def test_capture(self):
  frames,dns=good_frames()
  with tempfile.TemporaryDirectory() as folder:
   f=Path(folder)/'wire.pcap';f.write_bytes(capture(frames));r=d.capture_proof(f,dns,h.REQUEST,h.RESPONSE);self.assertTrue(r['bidirectional_FIN_ACK'])
   # ACKs after the first terminal FIN remain valid when either sender later
   # retransmits its identical FIN. Do not accidentally require ACK after last.
   f.write_bytes(capture(frames+[frames[-3],frames[-2]]))
   self.assertTrue(d.capture_proof(f,dns,h.REQUEST,h.RESPONSE)['FIN_ACK_capture_order'])
   # This client FIN+ACK now acknowledges the server FIN before that FIN is
   # observed. Merely finding the terminal acknowledgement number is not proof.
   reordered=frames.copy();reordered[-3],reordered[-2]=reordered[-2],reordered[-3]
   f.write_bytes(capture(reordered))
   with self.assertRaisesRegex(AssertionError,'after observed terminal FIN'):
    d.capture_proof(f,dns,h.REQUEST,h.RESPONSE)
   unknown_udp=udp('10.0.2.15','10.0.2.2',12345,12346,b'foreign')
   foreign_dns=udp('10.0.2.15','192.0.2.1',53000,53,bytes.fromhex(dns['records'][0]['query_hex']))
   unknown_tcp=packet(6,'10.0.2.15','10.0.2.2',struct.pack('!HHII',42001,80,1,0)+b'\x50\x10\xff\xff\0\0\0\0')
   unknown_eth=b'\0'*12+b'\x86\xdd'+b'\0'*40
   foreign_tcp=packet(6,'10.0.2.15','192.0.2.1',struct.pack('!HHII',42000,80,1,0)+b'\x50\x10\xff\xff\0\0\0\0')
   duplicate=frames[0][42:-1]+b'\x35\1\1\xff'
   malformed_dhcp=udp('0.0.0.0','255.255.255.255',68,67,duplicate)
   negatives=[capture(frames[:-3]),capture(frames[1:]),capture(frames)[:-1]]
   negatives += [capture(frames+[foreign]) for foreign in (unknown_udp,foreign_dns,unknown_tcp,unknown_eth,foreign_tcp,malformed_dhcp)]
   negatives += [capture(frames+[frames[0]])]
   for bad in negatives:
    f.write_bytes(bad)
    with self.assertRaises(AssertionError):d.capture_proof(f,dns,h.REQUEST,h.RESPONSE)
   f.write_bytes(capture(frames));altered={'queries':1,'records':[{'query_hex':dns['records'][0]['query_hex'],'response_hex':'00'}]}
   with self.assertRaises(AssertionError):d.capture_proof(f,altered,h.REQUEST,h.RESPONSE)
 def test_mode_receipts(self):
  w={k:1 for k in ("complete prepared attempted dhcp route handoff go child_observed child_reaped child_retired tcp_empty provider_drained worker_finished quiescent refs_scrubbed body_eof").split()}
  w.update(version=1,reason=0,child_status=0,retained=0,request_bytes=4159,body_bytes=3072,elapsed_us=800000,worker_turns=80,worker_polls=90,bootstrap_polls=10,supervisor_turns=80)
  for real in (False,True):
   w.update(dns=int(real),test_override=int(not real))
   part=("wifi-http5-workload: "+" ".join(f"{k}={v}" for k,v in w.items())+"\nhttp-network-vm: complete=1\n").encode()
   h.check_receipts(part,real)
   with self.assertRaises(AssertionError):h.check_receipts(part,not real)
 def test_numeric_default(self):
  self.assertEqual(h.output_directory(Path('/repo'),False),Path('/repo/build/review/http-network'));self.assertEqual(h.output_directory(Path('/repo'),True),Path('/repo/build/review/http-network-dns'))
  self.assertIn('user,id=net0,ipv6=off',h.command('image','/tmp/example',80))
if __name__=='__main__':unittest.main()
