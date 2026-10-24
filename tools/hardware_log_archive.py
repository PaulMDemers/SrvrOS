#!/usr/bin/env python3
"""Check mounted workspace node headroom; archive bounded old sessions before pruning."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat

ROOT = Path(__file__).resolve().parents[1]
SESSION = re.compile(r'session-[0-9a-f]{16}-[0-9a-f]{2}')
LOG_NAMES = {'boot.log', 'exit.log', 'session-a.log', 'session-b.log'}

def limits():
    text = (ROOT/'kernel/src/fs/exfat.c').read_text()
    def number(name):
        values = re.findall(r'^#define '+name+r' (\d+)$', text, re.M)
        if len(values) != 1: raise ValueError('Cannot determine driver node limits')
        return int(values[0])
    return number('EXFAT_MAX_FILES'), number('EXFAT_MAX_DIRS')

def inventory(mount):
    mount = Path(mount)
    if mount.is_symlink() or not mount.is_dir(): raise ValueError('Unsafe volume root')
    files, dirs = 0, 1
    device = mount.stat().st_dev
    def walk_error(error): raise error
    for parent, children, names in os.walk(mount,onerror=walk_error):
        for name in children + names:
            path = Path(parent)/name
            st = path.lstat()
            if st.st_dev != device or not (stat.S_ISREG(st.st_mode) or stat.S_ISDIR(st.st_mode)):
                raise ValueError('Unsupported link, nested mount or special file: '+str(path))
            if stat.S_ISDIR(st.st_mode): dirs += 1
            else: files += 1
    return files, dirs

def headroom(mount, files=32, dirs=8):
    if files < 0 or dirs < 0: raise ValueError('Negative reserve')
    used_files, used_dirs = inventory(mount)
    max_files, max_dirs = limits()
    # New operations also need the metadata replacement slot and, on a fresh
    # volume, the metadata file/directory. Existing files are already counted.
    extra_files = sum(not (Path(mount)/'.srvros'/n).exists() for n in ('meta', 'meta.tmp'))
    extra_dirs = int(not (Path(mount)/'.srvros').exists())
    result = dict(files=used_files, directories=used_dirs, file_limit=max_files,
                  directory_limit=max_dirs, requested_files=files, requested_directories=dirs,
                  metadata_files=extra_files, metadata_directories=extra_dirs,
                  available_files=max(0,max_files-used_files-extra_files),
                  available_directories=max(0,max_dirs-used_dirs-extra_dirs))
    result['ok'] = used_files+extra_files+files <= max_files and used_dirs+extra_dirs+dirs <= max_dirs
    return result

def require_headroom(mount, files=32, dirs=8):
    result = headroom(mount,files,dirs)
    if not result['ok']:
        raise RuntimeError('Insufficient SrvrOS file/directory slots; archive old logs before testing: '+json.dumps(result))
    return result

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''): h.update(block)
    return h.hexdigest()

def snapshot(folder):
    if folder.is_symlink() or not folder.is_dir(): raise ValueError('Unsafe session directory')
    result={}
    for p in folder.iterdir():
        st=p.lstat()
        if p.name not in LOG_NAMES or not stat.S_ISREG(st.st_mode) or st.st_size>2*1024*1024:
            raise ValueError('Unrecognized/oversized session contents')
        result[p.name]=dict(bytes=st.st_size,sha256=sha(p))
    return result

def plan(mount, keep=32, batch=32):
    if keep<8 or not 1<=batch<=32: raise ValueError('Keep at least 8 recent sessions; batch must be 1..32')
    logs=Path(mount)/'srvros-work/Logs'
    if logs.is_symlink() or not logs.is_dir(): raise ValueError('Unsafe/missing Logs directory')
    sessions=sorted((p for p in logs.iterdir() if SESSION.fullmatch(p.name)),key=lambda p:p.name)
    chosen=[]
    for p in sessions[:-keep]:
        try: records=snapshot(p)
        except ValueError: continue
        if not {'boot.log','exit.log'}<=records.keys(): continue
        if b'workspace-log: snapshot=exit.log' not in (p/'exit.log').read_bytes(): continue
        chosen.append(dict(session=p.name,files=records))
        if len(chosen)==batch: break
    return chosen

def sync_dir(path):
    fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY)
    try: os.fsync(fd)
    finally: os.close(fd)

def save_json(path,data):
    with path.open('w') as stream:
        json.dump(data,stream,indent=2);stream.write('\n');stream.flush();os.fsync(stream.fileno())
    sync_dir(path.parent)

def archive_location(mount,parent):
    return parent.stat().st_dev!=mount.stat().st_dev and not parent.is_relative_to(ROOT.resolve())

def archive(mount,destination,chosen,prune=False):
    inventory(mount)
    mount=Path(mount).resolve();destination=Path(destination)
    if not chosen: raise ValueError('No eligible completed sessions to archive')
    # A durable archive must be on another device, outside the checkout/build.
    parent=destination.parent.resolve(strict=True)
    if not archive_location(mount,parent):
        raise ValueError('Archive must be outside the repository and on a different device')
    if destination.exists() or destination.is_symlink(): raise ValueError('Archive already exists; use a fresh destination')
    destination.mkdir(mode=0o700)
    sync_dir(parent)
    receipt=dict(format='srvros-log-archive-v1',created=datetime.now(timezone.utc).isoformat(),
                 volume_path=str(mount),sessions=chosen,state='copying',pruned=[])
    save_json(destination/'manifest.json',receipt)
    logs=mount/'srvros-work/Logs'
    for item in chosen:
        name=item['session']
        if not SESSION.fullmatch(name): raise ValueError('Invalid session name')
        source=logs/name
        if snapshot(source)!=item['files']: raise ValueError('Source changed since plan')
        shutil.copytree(source,destination/name)
        if snapshot(destination/name)!=item['files']: raise ValueError('Archive verification failed')
        for p in (destination/name).iterdir():
            with p.open('rb') as stream: os.fsync(stream.fileno())
        sync_dir(destination/name)
    receipt['state']='verified';save_json(destination/'manifest.json',receipt)
    if prune:
        # Recheck the entire batch before the first unlink. No source deletion
        # is permitted without both a durable manifest and a verified copy.
        for item in chosen:
            if snapshot(logs/item['session'])!=item['files'] or snapshot(destination/item['session'])!=item['files']:
                raise ValueError('Source or archive changed before pruning')
        for item in chosen:
            source=logs/item['session']
            if snapshot(source)!=item['files']: raise ValueError('Session changed during pruning')
            for name in item['files']: (source/name).unlink()
            source.rmdir();sync_dir(logs)
            receipt['pruned'].append(item['session']);save_json(destination/'manifest.json',receipt)
        receipt['state']='pruned';save_json(destination/'manifest.json',receipt)
    return receipt

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=('check','plan','archive'))
    p.add_argument('--uuid',required=True)
    p.add_argument('--destination',type=Path)
    p.add_argument('--keep',type=int,default=32)
    p.add_argument('--batch',type=int,default=32)
    p.add_argument('--prune',action='store_true',help='Remove only verified archived sessions')
    args=p.parse_args()
    try:
        from hardware_test import volume_id,volume_mount
        volume=volume_id(args.uuid);mount=volume_mount(volume)
        marker=mount/'srvros-work/.workspace'
        inventory(mount)
        if marker.read_text()!=f'SRVROS-WORKSPACE-V1 {volume}\n': raise ValueError('Workspace marker mismatch')
        if args.action=='check': result=require_headroom(mount)
        elif args.action=='plan': result=dict(headroom=headroom(mount),sessions=plan(mount,args.keep,args.batch))
        else:
            if args.destination is None: raise ValueError('--destination required')
            result=archive(mount,args.destination,plan(mount,args.keep,args.batch),args.prune)
        print(json.dumps(result,indent=2));return 0
    except (OSError,ValueError,RuntimeError) as error:
        print('workspace-capacity: '+str(error));return 1

if __name__=='__main__': raise SystemExit(main())
