#!/usr/bin/env python3
"""Actual ELF/IRQ/TCP/worker HTTP guest test against a bounded local peer.

VM-only numeric destination10.0.2.2 uses ordinary SLIRP TCP to a private
127.0.0.1:80 peer. QEMU and peer share a fresh unprivileged user+network namespace
with only loopback and no external routes. The host has no port80 listener.
Standalone --self-test retains an ordinary random unprivileged loopback port.
"""
import argparse,base64,hashlib,json,os,re,socket,subprocess,sys,tempfile,threading,time
from pathlib import Path
BODY=bytes(ord('A')+i%26 for i in range(3072))
REQUEST=b'GET /base64/'+base64.b64encode(BODY)+b' HTTP/1.0\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n'
RESPONSE=b'HTTP/1.0 200 OK\r\nContent-Length: 3072\r\nConnection: close\r\n\r\n'+BODY
assert len(REQUEST)==4159
class Peer:
 def __init__(self,port=0):
  self.listener=socket.socket();self.listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
  self.listener.bind(('127.0.0.1',port));self.listener.listen(1);self.listener.settimeout(180)
  self.port=self.listener.getsockname()[1];self.result=None;self.error=None;self.request_bytes=0;self.done=threading.Event()
  self.thread=threading.Thread(target=self.run,daemon=True)
 def start(self):self.thread.start();return self
 def run(self):
  try:
   with self.listener.accept()[0] as conn:
    # The private peer may start before guest boot. Keep the declared boot
    # budget before request data, then use the short data/close timeout.
    conn.settimeout(180);request=bytearray()
    while b'\r\n\r\n' not in request:
     block=conn.recv(min(1024,4160-len(request)));assert block,'EOF before complete request'
     request.extend(block);self.request_bytes=len(request);assert len(request)<=4159,'oversized request'
    assert bytes(request)==REQUEST,'request differs from exact4159-byte GET'
    conn.settimeout(12)
    # Different write sizes exercise stream handling without assuming packet
    # boundaries survive TCP/QEMU. Actual received EOF is still mandatory.
    at=0
    for size in (17,493,5,1024,11,len(RESPONSE)):
     block=RESPONSE[at:at+size]
     if block:conn.sendall(block);at+=len(block)
     if at==len(RESPONSE):break
    assert at==len(RESPONSE)
    conn.shutdown(socket.SHUT_WR)
    assert conn.recv(1)==b'','extra request bytes after exact GET'
    self.result={'request_exact':True,'request_bytes':len(request),'request_sha256':hashlib.sha256(request).hexdigest(),'body_bytes':len(BODY),'body_sha256':hashlib.sha256(BODY).hexdigest(),'response_bytes':len(RESPONSE),'server_write_eof':True,'client_read_eof':True,'connections':1}
  except BaseException as error:self.error=repr(error)
  finally:self.listener.close();self.done.set()
 def finish(self):
  assert self.done.wait(15),'peer did not observe client close'
  assert self.error is None,self.error
  assert self.result and self.result['request_exact']
  return self.result
 def close(self):self.listener.close()
def values(part,prefix):
 rows=[line[len(prefix):] for line in part.decode('utf8').splitlines() if line.startswith(prefix)]
 assert len(rows)==1,(prefix,len(rows));row=rows[0]
 fields=row.split();result={}
 for field in fields:
  key,value=field.split('=',1);assert key not in result and re.fullmatch(r'[0-9]+',value),field
  result[key]=int(value)
 return result
def check_receipts(part):
 w=values(part,'wifi-http5-workload: ')
 for k in ('complete','prepared','attempted','dhcp','route','handoff','go','child_observed','child_reaped','child_retired','tcp_empty','provider_drained','worker_finished','quiescent','refs_scrubbed','test_override','body_eof'):assert w[k]==1,(k,w[k])
 assert w['version']==1 and w['reason']==w['child_status']==w['dns']==w['retained']==0
 assert w['request_bytes']==4159 and w['body_bytes']==3072 and 0<w['elapsed_us']<15000000
 assert 0<w['worker_turns']<=w['worker_polls']<150000 and 0<w['bootstrap_polls']<=w['worker_polls'] and w['supervisor_turns']>0
 r=values(part,'http-network-vm: ')
 # The entry also emits progress/PASS lines with the same prefix. Select its
 # numeric receipt explicitly rather than interpreting prose as key/value data.
 return w,r
def check_part(part):
 lines=part.splitlines();receipt=[x for x in lines if x.startswith(b'http-network-vm: complete=')]
 assert len(receipt)==1
 reduced=b'\n'.join(x for x in lines if x.startswith(b'wifi-http5-workload: '))+b'\n'+receipt[0]+b'\n'
 w,r=check_receipts(reduced)
 for k in ('complete','real_elf','real_sockets','clock_ready','worker_finished','rings_stable','provider_dma_retained'):assert r[k]==1,(k,r[k])
 assert r['pre_go_us']>=1000000 and r['pre_go_ticks']>=100
 assert r['tick_end']>r['tick_start'] and r['worker_identity'] and r['supervisor_identity'] and r['worker_identity']!=r['supervisor_identity']
 assert r['worker_turns']>0 and r['tx_delta']>0 and r['rx_delta']>0 and r['rx_irq_delta']>0
 for k in ('tcp_empty',):assert r[k]==1
 for k in ('tx_pending','rx_ready','rx_dispatching','tcp_slots','tcp_timewait','listeners','pending','udp','native_wifi'):assert r[k]==0,(k,r[k])
 assert b'http-network-vm: PASS actual ELF exact HTTP worker and terminal TCP proof; native Wi-Fi untested' in part
 assert b'http-network-vm: FAIL' not in part and b'exception:' not in part
 return {'workload':w,'guest':r}
def isolated_namespace():
 # Refuse host/root namespaces BEFORE link changes or binding. The explicit
 # inside flag alone grants no authority. A normal invocation self-execs unshare.
 captured={k:os.environ.pop('SRVROS_HTTP_PARENT_'+k,None) for k in ('NETNS','USERNS','UID','GID')}
 assert all(captured.values()),'missing mandatory parent identity'
 assert captured['UID'].isdigit() and captured['GID'].isdigit() and int(captured['UID'])>0 and int(captured['GID'])>0,'non-user parent identity'
 uid=Path('/proc/self/uid_map').read_text().split()
 gid=Path('/proc/self/gid_map').read_text().split()
 assert os.getuid()==os.geteuid()==0 and len(uid)==len(gid)==3,'not a mapped user namespace'
 assert uid==['0',captured['UID'],'1'] and gid==['0',captured['GID'],'1'],'map differs from exact captured parent user/group'
 network=os.readlink('/proc/self/ns/net');user=os.readlink('/proc/self/ns/user')
 assert network!=captured['NETNS'] and user!=captured['USERNS'],'network/user namespace was not replaced'
 interfaces=socket.if_nameindex();assert [name for _,name in interfaces]==['lo'],'non-loopback interface in namespace'
 def routes(family):
  value=json.loads(subprocess.check_output(['ip','-j',family,'route','show','table','main'],text=True))
  assert value==[],'external/main route in namespace'
  return value
 before4,before6=routes('-4'),routes('-6')
 parent=captured['NETNS']
 subprocess.run(['ip','link','set','lo','up'],check=True)
 assert [name for _,name in socket.if_nameindex()]==['lo']
 after4,after6=routes('-4'),routes('-6')
 return {'user_namespace':user,'network_namespace':network,'parent_network_namespace':parent,'parent_user_namespace':captured['USERNS'],'parent_uid':int(captured['UID']),'parent_gid':int(captured['GID']),'uid_map':list(map(int,uid)),'gid_map':list(map(int,gid)),'interfaces_before':interfaces,'interfaces_after':socket.if_nameindex(),'ipv4_main_routes_before':before4,'ipv6_main_routes_before':before6,'ipv4_main_routes_after':after4,'ipv6_main_routes_after':after6,'mapped_host_uid':int(uid[1]),'only_loopback':True,'external_routes':False,'host_port80_listener_created':False,'private_peer_port':80}
def command(iso,tmp,port,capture=None):
 assert port==80
 # Ordinary SLIRP preserves TCP FIN/EOF. guestfwd's character frontend has no
 # event callback and does not forward backend EOF. Namespace isolation replaces
 # restrict=on,whose DHCP intentionally omits required router/DNS options.
 cmd=['qemu-system-x86_64','-M','q35','-m','2G','-cdrom',str(iso),'-boot','d','-nic','none','-netdev','user,id=net0,ipv6=off','-device','e1000,netdev=net0','-serial',f'unix:{tmp}/serial,server=on,wait=off','-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot']
 if capture is not None:cmd+=['-object',f'filter-dump,id=httpcap,netdev=net0,file={capture}']
 return cmd
def self_test():
 peer=Peer().start()
 with socket.create_connection(('127.0.0.1',peer.port),3) as c:
  for at in range(0,len(REQUEST),97):c.sendall(REQUEST[at:at+97])
  response=bytearray()
  while block:=c.recv(211):response.extend(block)
  assert bytes(response)==RESPONSE
 assert peer.finish()['request_bytes']==4159
 bad=Peer().start()
 with socket.create_connection(('127.0.0.1',bad.port),3) as c:
  c.sendall(b'GET /wrong HTTP/1.0\r\n\r\n');assert not c.recv(1)
 assert bad.done.wait(3) and bad.error and bad.result is None
 assert command('image','/tmp/example',80)[12]=='user,id=net0,ipv6=off'
 assert not any('guestfwd' in x for x in command('image','/tmp/example',80))
 try:check_part(b'')
 except AssertionError:pass
 else:raise AssertionError('missing receipt accepted')
 print(json.dumps({'self_test':'PASS','exact_request_bytes':len(REQUEST),'body_bytes':len(BODY),'loopback_peer':True,'wrong_request_refused':True,'missing_receipt_refused':True,'vm_run':False}))
def namespace_self_test(namespace):
 peer=Peer(80).start()
 with socket.create_connection(('127.0.0.1',80),3) as conn:
  conn.sendall(REQUEST);response=bytearray()
  while block:=conn.recv(211):response.extend(block)
  assert bytes(response)==RESPONSE
 result=peer.finish()
 print(json.dumps({'namespace_self_test':'PASS','namespace':namespace,'peer':result,'vm_run':False,'physical_acceptance':False}))
def run_vm(root,namespace):
 import sys
 sys.path.insert(0,str(root/'tools'))
 from desktop_foundation_smoke import QMP
 from displayd_frame_smoke import stop_process
 from displayd_resolution_smoke import build_iso,read_for
 from vm_sockets import connect_unix
 out=root/'build/review/http-network';out.mkdir(parents=True,exist_ok=True)
 for name in ('result.json','serial.log','qemu.log','failure.json','peer.json','network.pcap','namespace.json'):(out/name).unlink(missing_ok=True)
 (out/'namespace.json').write_text(json.dumps(namespace,indent=2)+'\n')
 output=bytearray();peer=Peer(80).start()
 try:
  with tempfile.TemporaryDirectory(prefix='srvros-http-network-vm-') as folder:
   tmp=Path(folder);iso=build_iso(str(root),str(tmp),1024,768,os.environ.copy(),root/'build/kernel/srvros.elf',root/'build/initramfs.tar',cmdline='srvros.http-child-preflight=1 srvros.http-network-vm=1 srvros.storage=ram')
   cmd=command(iso,tmp,peer.port,out/'network.pcap')
   with (out/'qemu.log').open('w') as errors:
    process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
    try:
     serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp');start=time.monotonic()
     while b'\nsrv> ' not in output and time.monotonic()-start<90:
      output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
     assert b'\nsrv> ' in output,'no monitor'
     assert b'http-child-boot: isolated_init=1 net_init_deferred=1 worker_deferred=1' in output
     assert b'init: readiness=ready' in output and b'init: startup complete' not in output
     before=len(output);serial.sendall(b'httpnetworktest\n');end=time.monotonic()+75
     while b'\nsrv> ' not in output[before:] and time.monotonic()<end:output.extend(read_for(serial,.1))
     part=bytes(output[before:]);assert b'\nsrv> ' in part,'HTTP command did not return'
     checks=check_part(part);host=peer.finish()
     before=len(output);serial.sendall(b'httpnetworktest\n');end=time.monotonic()+10
     while b'\nsrv> ' not in output[before:] and time.monotonic()<end:output.extend(read_for(serial,.1))
     assert b'http-network-vm: REFUSED explicit isolated RAM guest permit required or one-shot consumed' in output[before:]
     files=[root/'build/kernel/srvros.elf',root/'build/initramfs.tar',root/'build/userspace/httpget.elf',root/'build/userspace/init.elf']
     result={'status':'PASS','source':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=root)),'actual_guest_ELF_sockets_IRQ_clocks_worker':True,'exact_request_and_pattern_response':host,'guest_receipts':checks,'qemu_command':cmd,'isolated_namespace':namespace,'numeric_vm_target':'10.0.2.2:80','numeric_vm_override':True,'external_DNS_or_HTTP_used':False,'provider_DMA_retained_until_VM_exit':True,'native_wifi':False,'physical_acceptance':False,'payload_sha256':{str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in files}}
     (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
    finally:(out/'serial.log').write_bytes(output);stop_process(process)
 except BaseException as error:
  (out/'failure.json').write_text(json.dumps({'status':'FAILED_OR_INCOMPLETE','error_type':type(error).__name__,'error':str(error),'native_wifi':False},indent=2)+'\n')
  raise
 finally:
  # Preserve peer observations independently even if boot, receipt validation,
  # or the server fails. An unfinished peer is incomplete evidence, not PASS.
  peer.close();peer.thread.join(timeout=1)
  (out/'serial.log').write_bytes(output)
  (out/'peer.json').write_text(json.dumps({'done':peer.done.is_set(),'request_bytes':peer.request_bytes,'result':peer.result,'error':peer.error},indent=2)+'\n')
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--self-test',action='store_true');parser.add_argument('--namespace-self-test',action='store_true');parser.add_argument('--isolated-netns',action='store_true');parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);args=parser.parse_args()
 if args.self_test:
  assert not args.namespace_self_test and not args.isolated_netns
  self_test();return
 if not args.isolated_netns:
  captured={'NETNS':os.readlink('/proc/self/ns/net'),'USERNS':os.readlink('/proc/self/ns/user'),'UID':str(os.getuid()),'GID':str(os.getgid())}
  assert int(captured['UID'])>0 and int(captured['GID'])>0,'launch requires an unprivileged host user'
  for key,value in captured.items():os.environ['SRVROS_HTTP_PARENT_'+key]=value
  cmd=['unshare','--user','--map-root-user','--net','--',sys.executable,'-B',str(Path(__file__).resolve()),*sys.argv[1:],'--isolated-netns']
  if not args.namespace_self_test:
   out=args.root.resolve()/'build/review/http-network';out.mkdir(parents=True,exist_ok=True)
   for name in ('result.json','serial.log','qemu.log','failure.json','peer.json','network.pcap','namespace.json','namespace-launch.json'):(out/name).unlink(missing_ok=True)
   (out/'namespace-launch.json').write_text(json.dumps({'status':'INCOMPLETE_UNTIL_CHILD_EVIDENCE','parent':captured,'command':cmd,'native_wifi':False},indent=2)+'\n')
  os.execvpe(cmd[0],cmd,os.environ)

 try:namespace=isolated_namespace()
 except BaseException as error:
  if not args.namespace_self_test:
   out=args.root.resolve()/'build/review/http-network';out.mkdir(parents=True,exist_ok=True);(out/'result.json').unlink(missing_ok=True)
   (out/'failure.json').write_text(json.dumps({'status':'FAILED_OR_INCOMPLETE','phase':'isolated-netns','error_type':type(error).__name__,'error':str(error),'native_wifi':False},indent=2)+'\n')
  raise
 if args.namespace_self_test:namespace_self_test(namespace)
 else:run_vm(args.root.resolve(),namespace)
if __name__=='__main__':main()
