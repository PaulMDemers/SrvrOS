#!/usr/bin/env python3
"""Exercise CDC ECM through QEMU's USB gadget, without a PCI network device."""
import os
import argparse
from pathlib import Path
import re
import shutil
import socket
import subprocess
import struct
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import time
import displayd_resolution_smoke as boot
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--soak-seconds',type=int,default=0)
    parser.add_argument('--second-usb',action='store_true',help='Also enumerate an inactive ECM adapter on a separate network')
    parser.add_argument('--device-checks',action='store_true',help='Exercise attended script gates with explicit synthetic observations, EOF and replay refusal')
    args=parser.parse_args()
    if not 0<=args.soak_seconds<=3600:parser.error('soak duration must be 0..3600 seconds')
    root=Path(__file__).resolve().parents[1]
    out=root/'build/review'/('usb-ecm-device-checks' if args.device_checks else 'usb-ecm-multiple' if args.second_usb else 'usb-ecm-soak' if args.soak_seconds else 'usb-ecm');out.mkdir(parents=True,exist_ok=True)
    original=boot.make_limine_conf
    boot.make_limine_conf=lambda w,h:original(w,h)+'    cmdline: srvros.usb-network=ecm\n'
    output=b'';consumed=0
    udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);udp.bind(('127.0.0.1',0));udp.settimeout(.2)
    udp_port=udp.getsockname()[1];stop=threading.Event();messages=[]
    def echo():
        while not stop.is_set():
            try:
                data,peer=udp.recvfrom(2048);messages.append(data);udp.sendto(data,peer)
            except socket.timeout:pass
    body=b'SrvrOS ECM outbound HTTP fixture\n'+bytes(range(32,127))*80
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            self.send_response(200);self.send_header('Content-Length',str(len(body)))
            self.end_headers();self.wfile.write(body)
        def log_message(self,*args):pass
    http=ThreadingHTTPServer(('127.0.0.1',0),Handler)
    http_worker=threading.Thread(target=http.serve_forever,daemon=True);http_worker.start()
    worker=threading.Thread(target=echo,daemon=True);worker.start()
    try:
        with tempfile.TemporaryDirectory(prefix='srvros-ecm-') as directory:
            tmp=Path(directory);iso=boot.build_iso(str(root),str(tmp),1280,800,os.environ.copy())
            shutil.copyfile(root/'build/srvros.exfat',tmp/'disk.exfat')
            port=boot.choose_port(30001,38000);http_port=boot.choose_port(38001,45000)
            command=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d',
                '-serial',f'tcp:127.0.0.1:{port},server=on,wait=off',
                '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot',
                '-drive',f'if=none,id=disk,file={tmp}/disk.exfat,format=raw',
                '-device','ich9-ahci,id=ahci','-device','ide-hd,drive=disk,bus=ahci.0',
                '-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0',
                '-nic','none','-netdev',f'user,id=net0,hostfwd=tcp:127.0.0.1:{http_port}-10.0.2.15:80',
                '-device','usb-net,netdev=net0,bus=xhci.0,id=usbnet',
                '-object',f'filter-dump,id=packets,netdev=net0,file={out}/traffic.pcap']
            if args.second_usb:
                command+=['-netdev','user,id=net1,net=10.0.3.0/24','-device','usb-net,netdev=net1,bus=xhci.0,id=usbnet2,mac=52:54:00:12:34:57']
            with (out/'qemu.txt').open('w') as log:
                process=subprocess.Popen(command,stdout=log,stderr=log)
                try:
                    q=QMP(tmp/'qmp');serial=boot.connect_serial(port,15);serial.settimeout(.2)
                    def wait(marker,timeout=40):
                        nonlocal output,consumed
                        start=time.monotonic()
                        while marker not in output[consumed:]:
                            if not consumed:q.boot_if_menu_stalled(output,time.monotonic()-start)
                            if time.monotonic()-start>timeout or process.poll() is not None:
                                raise RuntimeError(f'Missing {marker!r}; see {out}/serial.txt')
                            try:
                                chunk=serial.recv(65536)
                                if not chunk:raise RuntimeError('serial disconnected')
                                output+=chunk;(out/'serial.txt').write_bytes(output)
                            except socket.timeout:pass
                        consumed=output.index(marker,consumed)+len(marker)
                    def command_line(line,marker=b' $ ',timeout=40):
                        serial.sendall(line.encode()+b'\n');wait(marker,timeout)
                    wait(b'srv> ',100)
                    assert b'net: provider=usb-ecm' in output,output[-8000:].decode(errors='replace')
                    assert b'ECM ready slot=' in output and b'configuration=1 control=0 data=1 alternate=1' in output
                    command_line('run /fat/bin/sh');command_line('dhcp')
                    assert b'dhcp: 10.0.2.15' in output
                    command_line('ping 10.0.2.2');assert b'sent=4 received=4 lost=0' in output
                    command_line('ping 10.0.2.99',timeout=15)
                    assert b'sent=4 received=0 lost=4' in output
                    command_line('dns');command_line('echo dns-usage-status=$?')
                    assert b'dns-usage-status=2' in output
                    command_line('host localhost')
                    assert b'localhost has address' in output
                    # Exercise outbound TCP and file output without a public HTTP service.
                    command_line(f'httpget 10.0.2.2 / {http.server_port} > /fat/ecm-http.txt')
                    command_line('cat /fat/ecm-http.txt')
                    assert b'200 OK' in output and body in output.replace(b'\r',b'')
                    command_line(f'sh /fat/share/examples/network-check.sh localhost 10.0.2.2 {http.server_port} > /fat/network-check.txt')
                    start=len(output);command_line('cat /fat/network-check.txt')
                    report=output[start:].replace(b'\r',b'')
                    for marker in (b'dhcp-status=0',b'dns-status=0',b'http-status=0',b'Network check complete',body):
                        assert marker in report,marker
                    command_line('export HOME=/fat')
                    serial.sendall(f'sh /fat/share/examples/network-trial.sh localhost 10.0.2.2 {http.server_port}\n'.encode())
                    wait(b'UNPLUG the Ethernet cable ONLY, then press Enter here.')
                    time.sleep(2)
                    q.call('set_link',{'name':'usbnet','up':False})
                    serial.sendall(b'\n')
                    wait(b'RECONNECT the Ethernet cable, then press Enter here.',60)
                    time.sleep(2)
                    q.call('set_link',{'name':'usbnet','up':True})
                    serial.sendall(b'\n')
                    wait(b'Then type reboot and press Enter. There is no timer.',60)
                    wait(b' $ ')
                    if args.device_checks:
                        serial.sendall(f'sh /fat/share/examples/device-checks.sh localhost 10.0.2.2 {http.server_port}\n'.encode())
                        gates=[
                            (b'Return here and press Enter for the left-only', b'\n'),
                            (b'Describe speaker channels and clarity', b'vm-speakers-skipped\n'),
                            (b'Keep volume at 10 percent. Speakers must stay silent.', b'\n'),
                            (b'Describe left/right/both, jack detection', b'vm-headphones-skipped\n'),
                            (b'Then press Enter here to replay', b'\n'),
                            (b'Describe volume, Stop/recovery', b'vm-controls-skipped\n'),
                        ]
                        for marker,answer in gates:
                            wait(marker,40);serial.sendall(answer)
                        wait(b'UNPLUG the Ethernet cable ONLY, then press Enter here.')
                        time.sleep(2);q.call('set_link',{'name':'usbnet','up':False});serial.sendall(b'\n')
                        wait(b'RECONNECT the Ethernet cable, then press Enter here.',60)
                        time.sleep(2);q.call('set_link',{'name':'usbnet','up':True});serial.sendall(b'\n')
                        for marker,answer in [
                            (b'Optionally run DNS example.com',b'vm-network-ui-skipped\n'),
                            (b'Describe physical secondary press',b'vm-press-skipped tap-skipped\n'),
                            (b'Describe primary drag',b'vm-drag-skipped cancel-skipped\n'),
                            (b'Leave the screenshot notification',b'\n')]:
                            wait(marker,60);serial.sendall(answer)
                        wait(b'Checks recorded. Completion is NOT a pass');wait(b' $ ')
                        command_line('sh /fat/share/examples/device-checks.sh')
                        start=len(output);command_line('echo replay-status=$?')
                        assert b'replay-status=1' in output[start:]
                        command_line('mkdir /fat/device-eof')
                        command_line('touch /fat/device-empty.txt')
                        command_line('HOME=/fat/device-eof sh /fat/share/examples/device-checks.sh < /fat/device-empty.txt')
                        start=len(output);command_line('echo checklist-eof-status=$?')
                        assert b'checklist-eof-status=1' in output[start:]
                    command_line('touch /fat/network-empty.txt')
                    command_line("sh -c 'read EOF_GATE' < /fat/network-empty.txt")
                    start=len(output);command_line('echo eof-status=$?')
                    assert b'eof-status=1' in output[start:]
                    for invalid in ('0','65536','12x','999999999999999'):
                        start=len(output);command_line(f'httpget 10.0.2.2 / {invalid}')
                        assert b'usage: httpget' in output[start:]
                    for cycle in range(3):
                        q.call('set_link',{'name':'usbnet','up':False})
                        # QEMU usb-net lacks link_status_changed: traffic stops,
                        # but its CDC notification remains connected. Real cable
                        # notifications are covered by the xHCI host fixture.
                        start=len(output);command_line('ifconfig')
                        assert b'role=active-net0 link=up' in output[start:]
                        command_line('ping 10.0.2.2',timeout=15)
                        assert b'sent=4 received=0 lost=4' in output[start:]
                        start=len(output);command_line('dhcp');command_line('echo dhcp-outage-status=$?')
                        assert b'dhcp: failed' in output[start:] and b'dhcp-outage-status=1' in output[start:]
                        command_line('dns localhost');command_line('echo dns-outage-status=$?')
                        assert b'dns-outage-status=1' in output[start:]
                        q.call('set_link',{'name':'usbnet','up':True})
                        start=len(output)
                        command_line('dhcp');command_line('ping 10.0.2.2')
                        assert b'dhcp: 10.0.2.15' in output[start:]
                        assert b'sent=4 received=4 lost=0' in output[start:]
                        command_line('host localhost')
                        assert b'localhost has address' in output[start:]
                        command_line(f'httpget 10.0.2.2 / {http.server_port} > /fat/ecm-http.txt')
                        start=len(output);command_line('cat /fat/ecm-http.txt')
                        assert body in output[start:].replace(b'\r',b'')
                    # 22 + Ethernet/IP/UDP headers (42) = exactly 64 bytes.
                    # Alternate this with 65-byte frames, across repeated TRB wraps.
                    started=time.monotonic();i=0
                    while i<160 or time.monotonic()-started<args.soak_seconds:
                        message=f'ecm-{i:04d}-'+('x'*(13+(i%2)))
                        assert len(message) in (22,23)
                        command_line(f'udpecho client 10.0.2.2 {udp_port} {message}')
                        assert f'udpecho: reply {message}'.encode() in output
                        i+=1
                        if not i%100:print(f'usb-ecm: completed {i} echoes in {time.monotonic()-started:.1f}s',flush=True)
                    rounds=i;assert len(messages)==rounds
                    command_line('ifconfig');assert b'net0: flags=UP,RUNNING dhcp=yes' in output
                    if args.second_usb:
                        assert b'adapter 1: driver=usb-ecm role=active-net0' in output
                        assert b'adapter 2: driver=usb-ecm role=inactive' in output
                        assert b'mac=52:54:00:12:34:57' in output
                    command_line('service webd status');assert b'webd background pid' in output
                    for _ in range(8):
                        with socket.create_connection(('127.0.0.1',http_port),timeout=10) as client:
                            client.settimeout(10);client.sendall(b'GET / HTTP/1.0\r\nHost: srvros\r\nConnection: close\r\n\r\n')
                            response=b''
                            while chunk:=client.recv(65536):response+=chunk
                        assert b'200 OK' in response and b'Server: srvros-webd' in response,response[:500]
                    # The keyboard and USB networking share the same event ring.
                    command_line('exit',b'\nsrv> ')
                    q.key('h');q.key('e');q.key('l');q.key('p');q.key('ret');wait(b'commands: help');wait(b'\nsrv> ')
                    command_line('xhci',b'xhci-network: slot=');wait(b'\nsrv> ')
                    rows=re.findall(rb'xhci-network: slot=\d+ ready=(\d+) link=(\w+) tx=(\d+) rx=(\d+) drops=(\d+) errors=(\d+) notifications=(\d+) queued=(\d+)',output)
                    if args.second_usb:
                        assert rows[-1][2:4]==(b'0',b'0'),rows
                        rows=rows[:-1]
                    assert rows and rows[-1][0:2]==(b'1',b'up') and rows[-1][5]==b'0',rows
                    assert int(rows[-1][2])>=170 and int(rows[-1][3])>=170,rows
                    keyboards=re.findall(rb'keyboard=1 mouse=0 absolute=0 dci=3 control=\d+ intr=\d+ reports=(\d+)',output)
                    assert keyboards and int(keyboards[-1])>=8,keyboards
                    # Physical USB removal must stop traffic without reusing live DMA.
                    q.call('device_del',{'id':'usbnet'});wait(b'quarantined reason=disconnected')
                    command_line('xhci',b'xhci-network: slot=');wait(b'\nsrv> ')
                    command_line('run /fat/bin/sh');command_line('ifconfig')
                    tail=output[output.rfind(b'adapter 1:'):]
                    assert b'role=active-net0 link=down' in tail,tail
                    if args.second_usb: assert b'adapter 2: driver=usb-ecm role=inactive' in tail
                    assert b'exception:' not in output and b'kernel panic' not in output
                    q.call('quit');process.wait(timeout=10)
                finally:
                    stop_process(process);(out/'serial.txt').write_bytes(output)
                from exfat_test_image import ExfatFixture
                fixture=ExfatFixture((tmp/'disk.exfat').read_bytes())
                assert fixture.read('ecm-http.txt').endswith(body)
                report=fixture.read('network-check.txt')
                assert body in report and b'http-status=0' in report
                (out/'network-check.txt').write_bytes(report)
                for name in ('network-before.txt','network-after.txt'):
                    report=fixture.read(name)
                    for marker in (b'dhcp-status=0',b'dns-status=0',b'http-status=0',body):assert marker in report
                    (out/name).write_bytes(report)
                assert b'disconnected-dhcp-status=1' in fixture.read('network-down.txt')
                assert fixture.read('network-trial.txt')==b'started\ncompleted\n'
                if args.device_checks:
                    assert fixture.read('device-checks/sequence.txt')==b'started\ncompleted\n'
                    observed=fixture.read('device-checks/observations.txt')
                    for marker in (b'vm-speakers-skipped',b'vm-headphones-skipped',b'vm-controls-skipped',b'vm-network-ui-skipped',b'vm-press-skipped tap-skipped',b'vm-drag-skipped cancel-skipped'):assert marker in observed
                    assert fixture.read('device-eof/device-checks/sequence.txt')==b'started\n'
                    for name in ('network-before.txt','network-after.txt'):
                        report=fixture.read('device-checks/'+name)
                        for marker in (b'dhcp-status=0',b'dns-status=0',b'http-status=0',body):assert marker in report
                    assert b'disconnected-dhcp-status=1' in fixture.read('device-checks/network-down.txt')
                    for name in ('sequence.txt','observations.txt','network-before.txt','network-down.txt','network-after.txt','speakers.txt','headphones.txt','headphone-stop.txt'):
                        (out/('device-'+name)).write_bytes(fixture.read('device-checks/'+name))
                    print('device-checks: PASS guest prompts/reports, nested cable checks, EOF and replay refusal; human audio/gesture acceptance NOT TESTED')


    finally:
        stop.set();worker.join(timeout=2);udp.close()
        http.shutdown();http.server_close();http_worker.join(timeout=2)
    # Check actual wire framing, not only the application echo result.
    raw=(out/'traffic.pcap').read_bytes();assert raw[:4]==b'\xd4\xc3\xb2\xa1'
    cursor=24;sizes=[]
    while cursor<len(raw):
        _,_,captured,_=struct.unpack_from('<IIII',raw,cursor);cursor+=16
        frame=raw[cursor:cursor+captured];cursor+=captured
        if len(frame)>=42 and frame[12:14]==b'\x08\x00' and frame[23]==17 and frame[36:38]==udp_port.to_bytes(2,'big'):
            sizes.append(len(frame))
    assert sizes.count(64)==(rounds+1)//2 and sizes.count(65)==rounds//2,sizes
    print(f'usb-ecm: PASS second configuration selection, DHCP, DNS, outbound HTTP, three traffic outage/recovery cycles, ICMP, {rounds} UDP echoes including exact-64-byte ZLP frames, repeated ring wraps, TCP web serving, shared USB keyboard and disconnect quarantine')

if __name__=='__main__':main()
