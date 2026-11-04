#!/usr/bin/env python3
"""Read-only diagnosis of an offline exFAT workspace selected by GPT or explicit range; no admission."""
import argparse
import fcntl
import hashlib
import json
import os
import stat
import subprocess
import uuid
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

def partition_identity(value):
    native.require(type(value) is str,'invalid partition GUID')
    parsed=uuid.UUID(value)
    native.require(str(parsed)==value and parsed.int!=0,'partition GUID must be canonical lowercase and nonzero')
    return parsed.bytes_le.hex()

def selected_range(report,total,requested_guid_le=None):
    keys={'version','kind','primary_validated','backup_agrees','writable_admission','disk_guid_le','partition_guid_le','partitions','candidates','offset_bytes','size_bytes'}
    native.require(type(report) is dict and set(report)==keys and type(report['version']) is int and report['version']==1 and
                   report['kind']=='workspace-gpt-selection' and report['primary_validated'] is True and
                   report['backup_agrees'] is True and report['writable_admission'] is False,'invalid GPT selection')
    for key in ('disk_guid_le','partition_guid_le'):
        value=report[key]
        native.require(type(value) is str and len(value)==32 and all(c in '0123456789abcdef' for c in value) and int(value,16)>0,'invalid GPT identity')
    native.require(requested_guid_le is None or report['partition_guid_le']==requested_guid_le,'selected partition GUID differs from request')
    offset=report['offset_bytes'];length=report['size_bytes']
    native.require(type(report['partitions']) is int and 1<=report['partitions']<=32 and
                   type(report['candidates']) is int and report['candidates']==1 and
                   type(offset) is int and type(length) is int and offset>=512 and offset%512==0 and
                   length>0 and length%512==0 and length<=MAX_VOLUME and offset<=total and length<=total-offset,'invalid GPT range/count')
    return offset,length

def inspect_workspace(path,driver,offset=None,length=None,gpt_driver=None,partition_guid=None):
    fd=None;before=None;initial_hash=None;result=None
    try:
        use_gpt=offset is None and length is None
        native.require(use_gpt or (gpt_driver is None and partition_guid is None),'GPT selection cannot be combined with an explicit range')
        requested_guid_le=partition_identity(partition_guid) if partition_guid is not None else None
        native.require(use_gpt or (type(offset) is int and type(length) is int and offset>=0 and length>0 and
                       offset%512==0 and length%512==0 and length<=MAX_VOLUME),'invalid explicit volume range')
        path=image.checked_path(path);driver=image.checked_path(driver)
        driver_record=image.source(driver)
        gpt_record=None;selection=None
        if use_gpt:
            gpt_driver=image.checked_path(gpt_driver if gpt_driver is not None else driver.with_name('workspace-gpt-driver'+driver.suffix))
            gpt_record=image.source(gpt_driver)
        fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
        before=os.fstat(fd)
        native.require(stat.S_ISREG(before.st_mode) and before.st_nlink==1 and 0<before.st_size<=MAX_IMAGE and
                       (use_gpt or (offset<=before.st_size and length<=before.st_size-offset)),'invalid offline regular image/range')
        fcntl.flock(fd,fcntl.LOCK_SH|fcntl.LOCK_NB)
        initial_hash=digest(fd,before.st_size)
        if use_gpt:
            spec={'offset_bytes':0,'size_bytes':before.st_size}
            if requested_guid_le is None:
                selection=native.driver_output(gpt_driver,fd,spec)
            else:
                selection=native.driver_output(gpt_driver,fd,spec,extra_args=('--partition-guid-le',requested_guid_le))
            offset,length=selected_range(selection,before.st_size,requested_guid_le)
        diagnostic=validate(native.driver_output(driver,fd,{'offset_bytes':offset,'size_bytes':length},'--workspace-fd'))
        native.require(image.source(driver)==driver_record and (not use_gpt or image.source(gpt_driver)==gpt_record),'native driver changed')
        result={'state':'inspected','read_only':True,'admission':'not-authorized',
                'image_sha256':initial_hash,'image_size':before.st_size,'image_mode':stat.S_IMODE(before.st_mode),'image_mode_octal':f'{stat.S_IMODE(before.st_mode):04o}',
                'range':{'offset':offset,'length':length,'partition_table_validated':use_gpt},
                'driver_sha256':driver_record['sha256'],'diagnostic':diagnostic}
        if use_gpt:
            result['gpt_selection']=selection;result['gpt_driver_sha256']=gpt_record['sha256']
            if partition_guid is not None:result['requested_partition_guid']=partition_guid
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
    parser.add_argument('--offset',type=int);parser.add_argument('--length',type=int)
    parser.add_argument('--gpt-driver',help='native GPT adapter (default: beside --driver)')
    parser.add_argument('--partition-guid',help='canonical lowercase GPT partition GUID; requires validated matching Basic Data entry')
    args=parser.parse_args();result=inspect_workspace(args.image,args.driver,args.offset,args.length,args.gpt_driver,args.partition_guid)
    print(json.dumps(result,indent=2,sort_keys=True));return 0 if result['state']=='inspected' else 1

if __name__=='__main__':raise SystemExit(main())
