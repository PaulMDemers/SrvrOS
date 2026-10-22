#!/usr/bin/env python3
"""VM-only owned SMBus event dispatch; no host disk or native enable option."""
import argparse,json,os,re,subprocess,tempfile,time
from pathlib import Path
from desktop_foundation_smoke import QMP
from displayd_frame_smoke import stop_process
from displayd_resolution_smoke import build_iso,read_for
from serial_records import complete_lines
from vm_sockets import connect_unix

CASES={'completion':62,'alarm-darwin':63,'alarm-other':64,'unknown':65,'busy':66,
       'error':67,'read-timeout':68,'ack-timeout':69,'bad-hid':70,'bad-bank':71,
       'native-refusal':62,'outside-study':62,'late-alarm':72}

def check(data,case):
    assert case in CASES
    data=complete_lines(data.replace(b'\r\n',b'\n'))
    if case=='native-refusal':
        assert data.count(b'battery-smbus-study: REFUSED VM-only idle study')==1
        assert b'battery-native:' not in data and b'battery-service:' not in data
        return dict(status='PASS',case=case,native_admission='refused')
    if case=='outside-study':
        assert b'battery-query-policy: policy=2 code=16 class=5; dispatch=refused' in data
        assert b'battery: HALTED query policy ' in data
        assert b'battery-smbus: event=' not in data and b'battery-service: retired ' not in data
        return dict(status='PASS',case=case,scope='unchanged ordinary dispatch')
    if case in ('busy','error','read-timeout','ack-timeout','bad-hid','bad-bank'):
        reason=b'namespace/EC connection' if case in ('bad-hid','bad-bank') else b'SMBus event'
        assert b'battery: HALTED '+reason+b' ' in data
        for forbidden in (b'battery-smbus-study: PASS',b'battery-service: retired ',b'contained fault; returning'):
            assert forbidden not in data
        if case not in ('bad-hid','bad-bank'):
            rows=re.findall(rb'^battery-smbus: FAILED phase=(\d+) status=(\d+) ack_attempted=(\d+) acknowledged=0$',data,re.M)
            assert len(rows)==data.count(b'battery-smbus: FAILED ')==1
            expected={'busy':(0,0,0),'error':(1,216,0),'read-timeout':(3,192,0),'ack-timeout':(7,192,1)}
            assert tuple(map(int,rows[0]))==expected[case]
        return dict(status='PASS',case=case,outcome='expected containment',runtime_retired=False)
    rows=re.findall(rb'^battery-smbus: event=(\d+) generation=(\d+) status=(\d+) alarm=([01]) address=(\d+) data=(\d+) acknowledged=([01]) refresh=([01]); aml=0 serialized=1$',data,re.M)
    assert len(rows)==data.count(b'battery-smbus: event=')>=3
    rows=[tuple(map(int,r)) for r in rows]
    summary=re.findall(rb'^battery-smbus-study: PASS events=(\d+) alarms=(\d+) acknowledgements=(\d+) unknown=(\d+) generation=(\d+); fresh_before_stop=1 retired=yes native=disabled$',data,re.M)
    assert len(summary)==data.count(b'battery-smbus-study: PASS ')==1
    events,alarms,acks,unknown,generation=map(int,summary[0]);assert events==len(rows) and generation>=3
    assert [r[0] for r in rows]==list(range(1,events+1))
    assert [r[1] for r in rows]==sorted(r[1] for r in rows)
    expected_alarm=case!='completion';assert alarms==acks==int(expected_alarm) and unknown==int(case=='unknown')
    assert sum(r[3] for r in rows)==alarms and sum(r[6] for r in rows)==acks
    for _,gen,status,alarm,address,payload,ack,refresh in rows:
        assert gen<=generation
        if alarm:assert status==192 and address==(48 if case=='unknown' else 11) and payload==4660 and ack==1 and refresh==int(case!='unknown')
        else:assert (status,address,payload,ack,refresh)==(128,0,0,0,0)
    invalidations=re.findall(rb'^battery-smbus: invalidate generation=(\d+) cached=(\d+) valid=([01]) fresh=0$',data,re.M)
    assert len(invalidations)==data.count(b'battery-smbus: invalidate ')==alarms-unknown
    if case=='late-alarm':
        assert len(invalidations)==1
        invalid,cached,valid=map(int,invalidations[0]);assert invalid==2 and 0<cached<=invalid and valid==1
        assert generation>invalid and sum(r[3] and r[1]==invalid for r in rows)==1
    elif invalidations:assert tuple(map(int,invalidations[0]))==(0,0,0)
    assert data.count(b'battery-recovery-resources:')==1
    assert b'battery-recovery-resources: known=1 lease=0 ec_phase=6 poison=0 sample=0 waiting=0 smbus=0 api=0 aml=0 mutex=0 pending=0 facs=0 blocked=0' in data
    retired=re.findall(rb'^battery-service: retired epoch=1 attempts=(\d+) rejected=0 queries=(\d+) notifications=1 attached=1 detached=1 heap=0 errors=0 denied=0$',data,re.M)
    assert len(retired)==data.count(b'battery-service: retired ')==1 and int(retired[0][0])>=3 and int(retired[0][1])==events
    for bad in (b'battery: HALTED',b'battery: FAILED',b'battery-smbus: FAILED',b'battery-query-policy:'):
        assert bad not in data
    return dict(status='PASS',case=case,events=events,alarms=alarms,acknowledgements=acks,unknown=unknown,generation=generation,runtime_retired=True,native_acceptance='not tested')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--case',choices=CASES,default='completion');args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];out=root/'build/review'/('battery-smbus-'+args.case);out.mkdir(parents=True,exist_ok=True)
    (out/'result.json').unlink(missing_ok=True)
    output=bytearray()
    with tempfile.TemporaryDirectory(prefix='srvros-smbus-') as folder:
        tmp=Path(folder);(tmp/'battery.asl').write_text('#define BATTERY_FAULT '+str(CASES[args.case])+'\n'+(root/'tools/fixtures/acpi/battery-runtime.asl').read_text())
        subprocess.run(['iasl','-p',str(tmp/'battery'),str(tmp/'battery.asl')],check=True,stdout=subprocess.DEVNULL)
        kernel=root/'build/acpi-battery'/('srvros.elf' if args.case=='native-refusal' else 'vm.elf')
        iso=build_iso(str(root),str(tmp),1280,800,os.environ.copy(),kernel,root/'build/initramfs.tar')
        cmd=['qemu-system-x86_64','-M','q35','-m','512M','-cdrom',iso,'-boot','d','-nic','none',
             '-acpitable',f'file={tmp}/battery.aml','-serial',f'unix:{tmp}/serial,server=on,wait=off',
             '-qmp',f'unix:{tmp}/qmp,server=on,wait=off','-display','none','-monitor','none','-no-reboot']
        with (out/'qemu.txt').open('w') as errors:
            process=subprocess.Popen(cmd,stdout=errors,stderr=errors)
            try:
                serial=connect_unix(tmp/'serial',20);serial.settimeout(.05);qmp=QMP(tmp/'qmp');start=time.monotonic()
                while b'srv> ' not in output and time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));qmp.boot_if_menu_stalled(output,time.monotonic()-start)
                assert b'srv> ' in output
                command=b'battery-service' if args.case=='outside-study' else b'battery-smbus-study'
                serial.sendall(b'acpiruntime '+command+b'\n');start=time.monotonic()
                while time.monotonic()-start<60:
                    output.extend(read_for(serial,.1));(out/'serial.log').write_bytes(output)
                    ready=complete_lines(output)
                    if any(marker in ready for marker in (b'battery-smbus-study: PASS ',b'battery: HALTED ',b'battery-smbus-study: REFUSED ')):break
                result=check(bytes(output).replace(b'\r\n',b'\n'),args.case)
                assert process.poll() is None
            finally:stop_process(process)
        import hashlib
        result['kernel_sha256']=hashlib.sha256(kernel.read_bytes()).hexdigest()
        (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
