#!/usr/bin/env python3
"""Prepare, boot and collect a physical trackpad capture; the guest always halts."""
import argparse
import binascii
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import subprocess

import hardware_boot as boot
import hardware_test as host
from desktop_io_result import validate as validate_io, validate_timing, timing_gate, validate_text, text_gate, validate_timeline

RUNS = boot.ROOT/'build/trackpad-tests'
NUMBERS = ('found','ready','slot','reports','contact_reports','button_reports','lifts','max_contacts',
           'malformed','errors','request_bytes','last_bytes','last_completion',
           'keyboard_reports_before','keyboard_reports_after','keyboard_dropped','elapsed_us')
SAMPLES = ('sample_first','sample_touch','sample_multi','sample_button','sample_malformed')


def folder_for(volume):
    folder = host.volume_mount(volume)/'srvros-work'
    marker = folder/'.workspace'
    if folder.is_symlink() or not folder.is_dir() or marker.is_symlink() or not marker.is_file() or \
            marker.read_bytes() != f'SRVROS-WORKSPACE-V1 {volume}\n'.encode():
        raise ValueError('A prepared USB workspace is required; its existing files were not changed')
    return folder


def prepare(uuid,pointer=False,gestures=False,desktop_io=False,tap_typing=False):
    if tap_typing and desktop_io: raise ValueError("Tap/typing and desktop I/O are separate trials")
    gestures=gestures or desktop_io or tap_typing
    pointer=pointer or gestures
    volume = host.volume_id(uuid)
    folder = folder_for(volume)
    run = secrets.token_hex(8)
    request = folder/f'trackpad-{run}.req'
    if request.exists() or request.is_symlink() or any((folder/f'trackpad-{run}{suffix}').exists() or
            (folder/f'trackpad-{run}{suffix}').is_symlink() for suffix in ('.txt','.done')):
        raise ValueError('Run identity collision')
    release = boot.stage(workspace_volume=volume, trackpad_run=run,pointer=pointer,gestures=gestures,desktop_io=desktop_io,tap_typing=tap_typing)
    manifest = boot.verify(boot.STAGE/release)
    host.sync_file(request,f'SRVROS-TRACKPAD-V1 {run}\n'.encode(),exclusive=True)
    target = RUNS/run; target.mkdir(parents=True)
    metadata = {'run':run,'volume':volume,'release':release,'boot_manifest':manifest,'tap_typing':tap_typing,'pointer':pointer,'gestures':gestures,'desktop_io':desktop_io,'desktop_timing':desktop_io,'desktop_text':desktop_io,'desktop_timeline':desktop_io}
    host.sync_file(target/'manifest.json',(json.dumps(metadata,indent=2)+'\n').encode())
    host.sync_file(RUNS/'latest',(run+'\n').encode())
    os.sync()
    print(f'Prepared trackpad capture {run}, release {release}')
    if desktop_io: print('Desktop waits for Space before starting USB work; follow the on-screen checklist.')
    elif pointer: print('Desktop trial opens first: move, physical click/drag, type; Escape returns to save checks.')
    print('The guest waits for gestures and AaZz09 + Enter, saves the report, then halts.')
    return metadata


def load(run):
    run = run or (RUNS/'latest').read_text().strip()
    if not host.RUN_RE.fullmatch(run): raise ValueError('Invalid run ID')
    metadata = json.loads((RUNS/run/'manifest.json').read_text())
    if metadata['run'] != run or not boot.RELEASE_RE.fullmatch(metadata['release']):
        raise ValueError('Invalid run metadata')
    host.volume_id(metadata['volume'])
    return metadata


def launch(run):
    if os.geteuid() != 0: raise RuntimeError('Run this action with sudo')
    metadata = load(run)
    release = metadata['release']; run = metadata['run']
    manifest = boot.verify(boot.STAGE/release)
    if manifest != metadata['boot_manifest'] or manifest['config_version'] != \
            f"ram-usb-v{50 if metadata.get('tap_typing') else 16 if metadata.get('desktop_timeline') else 15 if metadata.get('desktop_text') else 14 if metadata.get('desktop_timing') else 13 if metadata.get('desktop_io') else 12 if metadata.get('gestures') else 11 if metadata.get('pointer') else 10}-trackpad-{metadata['volume']}-{run}":
        raise ValueError('Staged payload does not match the capture request')
    folder = folder_for(metadata['volume'])
    request = folder/f'trackpad-{run}.req'
    result = folder/f'trackpad-{run}.txt'
    done = folder/f'trackpad-{run}.done'
    if request.is_symlink() or request.read_bytes()!=f'SRVROS-TRACKPAD-V1 {run}\n'.encode() or result.exists() or result.is_symlink() or done.exists() or done.is_symlink():
        raise ValueError('Request absent/consumed or result already exists')
    defaults = Path('/etc/default/grub').read_text()
    host.check_ubuntu_default(defaults,boot.GRUB_CFG.read_text())
    boot.install(release)
    host.check_ubuntu_default(defaults,boot.GRUB_CFG.read_text())
    state = {'boot_id_before':Path('/proc/sys/kernel/random/boot_id').read_text().strip()}
    host.sync_file(RUNS/run/'launch.json',(json.dumps(state)+'\n').encode())
    os.sync()
    subprocess.run(['umount',str(folder.parent)],check=True)
    try:
        subprocess.run(['grub-reboot','srvros-current'],check=True)
        os.sync()
        print('One-shot trial armed. Press Space to start USB work; follow the checklist, then save. Guest will HALT.' if metadata.get('desktop_timeline') else 'One-shot capture armed. Swipe, physically click, type AaZz09 and Enter. Guest will HALT.',flush=True)
        subprocess.run(['systemctl','reboot','-i'],check=True)
    except BaseException:
        subprocess.run(['grub-editenv','/boot/grub/grubenv','unset','next_entry'],check=False)
        raise


def validate_result(metadata,data,completion):
    if not data or len(data)>8192: raise ValueError('Missing/oversized report')
    if completion!=f"COMPLETE {metadata['run']} {binascii.crc32(data)&0xffffffff:08x}\n".encode():
        raise ValueError('Missing/invalid durable completion marker')
    fields = {}
    for line in data.decode('ascii').splitlines():
        key,value = line.split('=',1)
        if key in fields: raise ValueError('Duplicate field')
        fields[key] = value
    for key,expected in {'format':'srvros-trackpad-v1','run':metadata['run'],
                         'release':metadata['release'],'complete':metadata['run']}.items():
        if fields.get(key)!=expected: raise ValueError(f'Capture identity mismatch: {key}')
    tail = f"crc32={fields.get('crc32','')}\ncomplete={metadata['run']}\n".encode()
    if not data.endswith(tail) or not re.fullmatch(r'[0-9]{1,10}',fields.get('crc32','')) or \
            binascii.crc32(data[:-len(tail)])&0xffffffff != int(fields['crc32']):
        raise ValueError('Missing/invalid report checksum')
    for key in NUMBERS:
        if not re.fullmatch(r'[0-9]{1,20}',fields.get(key,'')): raise ValueError(f'Invalid measurement: {key}')
    n = {key:int(fields[key]) for key in NUMBERS}
    if any(n[key]>1 for key in ('found','ready')) or n['max_contacts']>16 or n['request_bytes']!=486 or n['last_bytes']>486:
        raise ValueError('Measurement out of range')
    for key in SAMPLES:
        value = fields.get(key)
        if value is None or len(value)>972 or len(value)%2 or not re.fullmatch('[0-9a-f]*',value):
            raise ValueError('Malformed packet sample')
    keyboard = fields.get('typed')=='AaZz09' and n['keyboard_reports_after']>n['keyboard_reports_before'] and not n['keyboard_dropped']
    covered = keyboard and all(n[k] for k in ('found','ready','reports','contact_reports','button_reports','lifts')) and \
        n['max_contacts']>=2 and not n['errors'] and not n['malformed']
    if fields.get('keyboard')!=('PASS' if keyboard else 'INCOMPLETE') or \
            fields.get('status')!=('CAPTURED' if covered else 'INCOMPLETE'):
        raise ValueError('Inconsistent capture coverage')
    if covered and any(not fields[k] for k in SAMPLES[:4]): raise ValueError('Missing coverage samples')
    if metadata.get('pointer') or 'pointer_trial' in fields:
        for key in ('pointer_motion','pointer_buttons','pointer_cancels','pointer_queue_overflows'):
            if not re.fullmatch(r'[0-9]{1,20}',fields.get(key,'')): raise ValueError('Invalid pointer measurement')
        if fields.get('desktop_exit') not in ('PASS','FAIL'): raise ValueError('Missing desktop result')
        exercised=covered and fields['desktop_exit']=='PASS' and int(fields['pointer_motion'])>0 and \
            int(fields['pointer_buttons'])>=2 and fields['pointer_cancels']=='0' and fields['pointer_queue_overflows']=='0'
        if fields.get('pointer_trial')!=('EXERCISED' if exercised else 'INCOMPLETE'):
            raise ValueError('Inconsistent pointer trial')
    if metadata.get('gestures') or 'gesture_trial' in fields:
        for key in ('gesture_scroll_events','gesture_typing_suppressed','gesture_palm_suppressed',
                    'gesture_max_touch_major','gesture_max_touch_minor'):
            if not re.fullmatch(r'[0-9]{1,20}',fields.get(key,'')): raise ValueError('Invalid gesture measurement')
        exercised=fields.get('pointer_trial')=='EXERCISED' and int(fields['gesture_scroll_events'])>0
        if fields.get('gesture_trial')!=('EXERCISED' if exercised else 'INCOMPLETE'):
            raise ValueError('Inconsistent gesture trial')
    if metadata.get('tap_typing') or 'tap_typing_trial' in fields:
        for key in ('tap_primary','tap_secondary','input_policy_options'):
            if not re.fullmatch(r'[0-9]{1,20}',fields.get(key,'')): raise ValueError('Invalid tap/typing measurement')
        exercised=fields.get('gesture_trial')=='EXERCISED' and fields['input_policy_options']=='14' and int(fields['tap_primary'])>0 and int(fields['gesture_typing_suppressed'])>0
        if fields.get('tap_typing_trial')!=('EXERCISED' if exercised else 'INCOMPLETE'):
            raise ValueError('Inconsistent tap/typing trial')
    return fields


def collect(run):
    metadata = load(run); folder = folder_for(metadata['volume'])
    run = metadata['run']; path = folder/f'trackpad-{run}.txt'
    if path.is_symlink() or not path.is_file() or path.stat().st_size>8192: raise ValueError('No complete capture report')
    done=folder/f'trackpad-{run}.done'
    if done.is_symlink() or not done.is_file() or done.stat().st_size>128: raise ValueError('Missing completion marker')
    data = path.read_bytes(); fields = validate_result(metadata,data,done.read_bytes())
    launch_file = RUNS/run/'launch.json'
    if not launch_file.exists() or json.loads(launch_file.read_text())['boot_id_before']==Path('/proc/sys/kernel/random/boot_id').read_text().strip():
        raise ValueError('No recorded host reboot for this capture')
    host.sync_file(RUNS/run/'report.txt',data)
    result = {'sha256':hashlib.sha256(data).hexdigest(),'fields':fields,'reboot_observed':True}
    if metadata.get('desktop_io'):
        directory=folder/f'desktop-{run}'
        if directory.is_symlink() or not directory.is_dir(): raise ValueError('Missing desktop I/O results')
        blobs=[]
        for name,limit in (('result.txt',2048),('ui.txt',2048),('payload.bin',256*1024)):
            item=directory/name
            if item.is_symlink() or not item.is_file() or item.stat().st_size>limit:
                raise ValueError(f'Missing/oversized desktop I/O {name}')
            blob=item.read_bytes(); blobs.append(blob)
            host.sync_file(RUNS/run/f'desktop-{name}',blob)
        result['desktop_io']=validate_io(run,*blobs)
        if metadata.get('desktop_timing'):
            item=directory/'timing.txt'
            if item.is_symlink() or not item.is_file() or item.stat().st_size>2048:
                raise ValueError('Missing/oversized desktop timing')
            timing=item.read_bytes(); host.sync_file(RUNS/run/'desktop-timing.txt',timing)
            result['desktop_timing']=validate_timing(run,timing)
            for key in ('io_clock','io_owner_count','io_owner_max_us','io_owner_p95_us','io_gate_count',
                        'io_gate_max_us','io_gate_p95_us','io_gate_syscall','io_usb_commands','io_usb_bytes','io_usb_flushes'):
                if not re.fullmatch(r'[0-9]{1,20}',fields.get(key,'')):
                    raise ValueError(f'Missing/invalid timing field: {key}')
            if fields['io_clock']!='1': raise ValueError('Missing kernel reference clock')
            t=result['desktop_timing']
            result['desktop_timing_gate']='MET' if timing_gate(t) else 'NEEDS_WORK'
            print(f"Desktop timing: {result['desktop_timing_gate']}; {t}")
        if metadata.get('desktop_text'):
            item=directory/'text.txt'
            if item.is_symlink() or not item.is_file() or item.stat().st_size>2048:
                raise ValueError('Missing/oversized desktop text timing')
            text=item.read_bytes(); host.sync_file(RUNS/run/'desktop-text.txt',text)
            t=validate_text(run,text); result['desktop_text']=t
            result['desktop_text_gate']='MET' if text_gate(t) else 'NEEDS_WORK'
            print(f"Desktop text: {result['desktop_text_gate']}; {t}")
        if metadata.get('desktop_timeline'):
            blobs=[]
            for name in ('work-clock.txt','input-clock.txt'):
                item=directory/name
                if item.is_symlink() or not item.is_file() or item.stat().st_size>2048:
                    raise ValueError(f'Missing/oversized desktop {name}')
                blob=item.read_bytes(); blobs.append(blob)
                host.sync_file(RUNS/run/f'desktop-{name}',blob)
            timeline=validate_timeline(run,*blobs); result['desktop_timeline']=timeline
            print(f"Workload duration: {timeline['workload_seconds']:.3f}s; first key: {timeline['first_key_phase']} workload; first pointer: {timeline['first_pointer_phase']} workload")
        print(f"Desktop + file work: {result['desktop_io']['status']}; {result['desktop_io']['worker']}; {result['desktop_io']['input_during_workload']}")
    host.sync_file(RUNS/run/'result.json',(json.dumps(result,indent=2)+'\n').encode())
    print(f"Trackpad capture {run}: {fields['status']}; keyboard={fields['keyboard']}")
    print(' '.join(f'{key}={fields[key]}' for key in ('reports','max_contacts','button_reports','lifts','malformed','errors')))
    print(f'Evidence: {RUNS/run}')
    if metadata.get('pointer'): print(f"Pointer trial: {fields['pointer_trial']} (physical feel still needs user feedback)")
    if metadata.get('gestures'): print(f"Gesture trial: {fields['gesture_trial']}; scroll events={fields['gesture_scroll_events']}; typing suppressed={fields['gesture_typing_suppressed']}; broad-contact suppressed={fields['gesture_palm_suppressed']}")
    if metadata.get('tap_typing'): print(f"Tap/typing trial: {fields['tap_typing_trial']}; primary taps={fields['tap_primary']}; secondary taps={fields['tap_secondary']}")
    return (not metadata.get('tap_typing') or fields['tap_typing_trial']=='EXERCISED') and fields['status']=='CAPTURED' and (not metadata.get('pointer') or fields['pointer_trial']=='EXERCISED') and (not metadata.get('gestures') or fields['gesture_trial']=='EXERCISED') and (not metadata.get('desktop_io') or result['desktop_io']['status']=='EXERCISED') and (not metadata.get('desktop_timing') or result['desktop_timing_gate']=='MET') and (not metadata.get('desktop_text') or result['desktop_text_gate']=='MET')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action',choices=('prepare','run','collect'))
    parser.add_argument('--desktop-io',action='store_true',help='Run desktop input with bounded USB file work')
    parser.add_argument('--tap-typing',action='store_true',help='Explicit desktop tap-to-click and typing suppression trial')
    parser.add_argument('--gestures',action='store_true',help='Prepare scrolling and accidental-touch trial')
    parser.add_argument('--pointer',action='store_true',help='Prepare an interactive desktop pointer trial')
    parser.add_argument('--uuid'); parser.add_argument('--run')
    args=parser.parse_args()
    try:
        if args.action=='prepare':
            if not args.uuid: parser.error('prepare requires --uuid')
            prepare(args.uuid,args.pointer,args.gestures,args.desktop_io,args.tap_typing)
        elif args.action=='run': launch(args.run)
        elif not collect(args.run): raise SystemExit(2)
    except (OSError,ValueError,RuntimeError,KeyError,subprocess.CalledProcessError) as error:
        parser.exit(1,f'trackpad-test: {error}\n')
