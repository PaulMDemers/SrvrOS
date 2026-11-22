#!/usr/bin/env python3
"""Provision one private PMK into an explicit disposable WPA5 candidate only.
Never print credential values;the input is SSID followed by passphrase,with
blank lines separating entries. Output is not an evidence/credential erasure
claim:the private staged initramfs remains a persistent credential store.
"""
import argparse
import hashlib
import io
import os
from pathlib import Path
import stat
import tarfile
MAGIC=b'SRWPA5\0\1'
NAME='cfg/wifi-wpa5.pmk'

def validate(blob):
    if len(blob)!=73 or blob[:8]!=MAGIC or not 1<=blob[8]<=32 or b'\0' in blob[9:9+blob[8]] or any(blob[9+blob[8]:41]):
        raise ValueError('invalid private PMK record')
    return blob

def derive(credentials,ssid):
    try:
        data=credentials.read_bytes()
        if len(data)>8192:raise ValueError()
        lines=[x for x in data.decode('utf-8').splitlines() if x.strip()]
        target=ssid.encode('utf-8')
        if not 1<=len(target)<=32 or b'\0' in target or len(lines)%2:raise ValueError()
        pairs=list(zip(lines[::2],lines[1::2]))
        selected=[password for name,password in pairs if name==ssid]
        if len(selected)!=1 or len({name for name,_ in pairs})!=len(pairs):raise ValueError()
        password=selected[0].encode('utf-8')
        if len(password)==64 and all(x in b'0123456789abcdefABCDEF' for x in password):pmk=bytes.fromhex(password.decode('ascii'))
        elif 8<=len(password)<=63 and b'\0' not in password:pmk=hashlib.pbkdf2_hmac('sha1',password,target,4096,32)
        else:raise ValueError()
        return validate(MAGIC+bytes([len(target)])+target.ljust(32,b'\0')+pmk)
    except (OSError,UnicodeError,ValueError):
        raise ValueError('credential file unavailable or invalid for selected SSID') from None

def provision(credentials,ssid,output):
    blob=derive(credentials,ssid)
    fd=os.open(output,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
    with os.fdopen(fd,'wb') as f:
        f.write(blob);f.flush();os.fsync(f.fileno())

def read_private(path):
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW)
    with os.fdopen(fd,'rb') as f:
        st=os.fstat(f.fileno())
        if not stat.S_ISREG(st.st_mode) or stat.S_IMODE(st.st_mode)!=0o600 or st.st_size!=73:
            raise ValueError('PMK source requires a regular 0600 record')
        return validate(f.read(74))

def add_to_initramfs(path,archive):
    blob=read_private(path)
    with tarfile.open(archive,'a',format=tarfile.USTAR_FORMAT) as tar:
        if any(m.name.lstrip('./')==NAME for m in tar.getmembers()):raise ValueError('private PMK entry already exists')
        info=tarfile.TarInfo(NAME);info.size=73;info.mode=0o600;info.mtime=0
        tar.addfile(info,io.BytesIO(blob))
    os.chmod(archive,0o600)

def verify_initramfs(archive,required):
    with tarfile.open(archive,'r:') as tar:
        members=[m for m in tar.getmembers() if m.name.lstrip('./')==NAME]
        if not required:
            if members:raise ValueError('private PMK forbidden outside explicit WPA5 stage')
            return
        if len(members)!=1:raise ValueError('missing or duplicate private PMK')
        m=members[0]
        if m.name!=NAME or not m.isfile() or m.size!=73 or m.mode!=0o600:raise ValueError('invalid private PMK tar entry')
        validate(tar.extractfile(m).read(74))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--credentials',type=Path,required=True)
    p.add_argument('--ssid',required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    try:provision(a.credentials,a.ssid,a.output)
    except (OSError,ValueError):p.exit(1,'private PMK provisioning refused;check input format and exclusive output\n')
    print('Private PMK record prepared;no credential values logged')
