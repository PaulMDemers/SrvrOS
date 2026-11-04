#!/usr/bin/env python3
"""Read-only diagnosis of an explicit offline exFAT workspace range; no admission."""
import argparse
import fcntl
import hashlib
import json
import os
import stat
import subprocess
import installation_image as image
import installation_inspection as native

MAX_IMAGE=8*1024**3
MAX_VOLUME=2*1024**3

def stamp(st):
    # atime may change on a read; mode/ownership/identity must not.
    return (st.st_dev,st.st_ino,st.st_mode,st.st_nlink,st.st_uid,st.st_gid,st.st_size,st.st_mtime_ns,st.st_ctime_ns)

def digest(fd,size):
    h=hashlib.sha256();native.fd_hash_range(fd,0,size,h);return h.hexdigest()

def validate(report):
    native.require(type(report) is dict and set(report)=={'version','kind','writable_admission','sidecar_loaded','metadata_origin','marker_uuid','marker_binding_authenticated','filesystem','entries'} and type(report.get('version')) is int and report['version']==1 and report.get('kind')=='workspace-diagnostic','invalid native diagnostic')
    native.require(report.get('writable_admission') is False and report.get('sidecar_loaded') is False and
                   report.get('metadata_origin')=='directory_entries_only' and report.get('marker_binding_authenticated') is False,'unexpected native capabilities')
    uuid=report['marker_uuid']
    native.require(type(uuid) is str and len(uuid)==8 and all(c in '0123456789abcdef' for c in uuid),'invalid marker syntax')
    fs=report['filesystem'];entries=report['entries']
    native.require(type(fs) is dict and set(fs)=={'volume_flags','dirty','allocation_consistent','allocation','writes','flushes'},'unexpected filesystem content')
    counts=fs['allocation']
    native.require(type(counts) is dict and set(counts)=={'files','clusters','leaked','fat_bitmap_mismatches','errors'},'unexpected allocation content')
    native.require(type(fs['volume_flags']) is int and 0<=fs['volume_flags']<=65535 and
                   type(fs['dirty']) is bool and fs['dirty']==bool(fs['volume_flags']&2) and type(fs['allocation_consistent']) is bool and
                   type(fs['writes']) is int and fs['writes']==0 and type(fs['flushes']) is int and fs['flushes']==0,'invalid filesystem diagnostic')
    native.require(all(type(counts[k]) is int and counts[k]>=0 for k in ('files','clusters','leaked','fat_bitmap_mismatches','errors')),'invalid allocation counters')
    native.require(type(entries) is list and len(entries)<=4096,'invalid entry count')
    seen=set()
    for entry in entries:
        native.require(type(entry) is dict and set(entry)=={'path','type','size'},'unexpected entry content')
        path=entry['path']
        native.require(type(path) is str and len(path.encode('utf-8'))<160 and
                       (path=='/fat/srvros-work' or path.startswith('/fat/srvros-work/')) and
                       all(p not in ('','.','..') for p in path.split('/')[1:]) and path not in seen and
                       entry['type'] in ('file','directory') and type(entry['size']) is int and entry['size']>=0,'invalid committed entry')
        seen.add(path)
    return report

def inspect_workspace(path,driver,offset,length):
    fd=None;before=None;initial_hash=None;result=None
    try:
        native.require(type(offset) is int and type(length) is int and offset>=0 and length>0 and
                       offset%512==0 and length%512==0 and length<=MAX_VOLUME,'invalid explicit volume range')
        path=image.checked_path(path);driver=image.checked_path(driver)
        driver_record=image.source(driver)
        fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
        before=os.fstat(fd)
        native.require(stat.S_ISREG(before.st_mode) and before.st_nlink==1 and 0<before.st_size<=MAX_IMAGE and
                       offset<=before.st_size and length<=before.st_size-offset,'invalid offline regular image/range')
        fcntl.flock(fd,fcntl.LOCK_SH|fcntl.LOCK_NB)
        initial_hash=digest(fd,before.st_size)
        diagnostic=validate(native.driver_output(driver,fd,{'offset_bytes':offset,'size_bytes':length},'--workspace-fd'))
        native.require(image.source(driver)==driver_record,'native driver changed')
        result={'state':'inspected','read_only':True,'admission':'not-authorized',
                'image_sha256':initial_hash,'image_size':before.st_size,'image_mode':stat.S_IMODE(before.st_mode),'image_mode_octal':f'{stat.S_IMODE(before.st_mode):04o}',
                'range':{'offset':offset,'length':length,'partition_table_validated':False},
                'driver_sha256':driver_record['sha256'],'diagnostic':diagnostic}
    except (OSError,ValueError,KeyError,TypeError,UnicodeError,RuntimeError,subprocess.SubprocessError) as error:
        result={'state':'refused','read_only':True,'admission':'not-authorized','reason':str(error)}
    finally:
        if fd is not None:
            try:
                if before is not None and initial_hash is not None:
                    native.require(digest(fd,before.st_size)==initial_hash and stamp(os.fstat(fd))==stamp(before) and
                                   stamp(os.stat(path,follow_symlinks=False))==stamp(before),'image bytes/permissions/identity changed')
                    result['image_unchanged']=True
            except (OSError,ValueError) as error:
                result={'state':'refused','read_only':True,'admission':'not-authorized','reason':str(error),'image_unchanged':False}
            os.close(fd)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image');parser.add_argument('--driver',required=True)
    parser.add_argument('--offset',type=int,required=True);parser.add_argument('--length',type=int,required=True)
    args=parser.parse_args();result=inspect_workspace(args.image,args.driver,args.offset,args.length)
    print(json.dumps(result,indent=2,sort_keys=True));return 0 if result['state']=='inspected' else 1

if __name__=='__main__':raise SystemExit(main())
