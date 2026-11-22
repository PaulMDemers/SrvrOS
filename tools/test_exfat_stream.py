#!/usr/bin/env python3
"""Exercise the actual bounded exFAT backend on disposable synthetic media."""
import hashlib
import os
from pathlib import Path
import platform
import shutil
import shlex
import tempfile
import subprocess
from exfat_test_image import ExfatFixture, check_clean_image
from test_storage_leases import empty_image

ROOT=Path(__file__).resolve().parents[1]
override=os.environ.get('SRVROS_BACKEND_OUTPUT')
parent=Path(override) if override else ROOT/'build/tests'
parent.mkdir(parents=True,exist_ok=True)
OUT=Path(tempfile.mkdtemp(prefix='exfat-stream-',dir=parent))
print(f'exfat stream evidence: {OUT}',flush=True)
driver=OUT/'driver'
command=shlex.split(os.environ.get('CC','cc'))+['-DSRVROS_EXFAT_HOST_TEST','-std=c11','-O2','-Wall','-Wextra','-Werror',
         '-ffunction-sections','-fdata-sections','-I',str(ROOT/'shared/include'),'-I',str(ROOT/'kernel/include'),
         str(ROOT/'tools/exfat_stream_driver.c'),str(ROOT/'kernel/src/fs/exfat.c'),str(ROOT/'kernel/src/vfs.c'),
         str(ROOT/'kernel/src/autotest_storage.c'),'-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections',
         '-o',str(driver)]
subprocess.run(command,check=True)
seed=OUT/'seed.img';seed.write_bytes(empty_image(256*1024*1024,cluster_shift=7).data)
work=OUT/'work.img'
def run(size,mode):
    shutil.copyfile(seed,work)
    p=subprocess.run([str(driver),str(work),str(size),mode],capture_output=True,text=True,timeout=120)
    if p.returncode:raise RuntimeError(f'{size} {mode}: {p.stdout} {p.stderr}')
    if "fault=" not in p.stdout:check_clean_image(work)
    print(p.stdout.strip(),flush=True)
    return p.stdout
for size in (0,1,511,512,513,65535,65536,65537,16*1024*1024,24+65536+64*1024*1024):
    run(size,'ok')
    if size in (16*1024*1024,24+65536+64*1024*1024):
        data=ExfatFixture(work.read_bytes()).read('stream.bin')
        expected=bytes((i*29+(i>>13)+17)&255 for i in range(size))
        assert hashlib.sha256(data).digest()==hashlib.sha256(expected).digest()
        print('sha256',size,hashlib.sha256(data).hexdigest(),flush=True)
run(1024*1024,'fragment-write')
f=ExfatFixture(work.read_bytes())
entry=dict(f.entries(f.chain(f.root)))['stream.bin']
first=__import__('struct').unpack_from('<I',entry,52)[0]
chain=[first]
while len(chain)<16:
    chain.append(f.u32(f.fat+chain[-1]*4))
assert any(b!=a+1 for a,b in zip(chain,chain[1:])),chain
print('fragmented output allocation PASS',flush=True)
run(12345,'lowspace')
run(12345,'alloc-clean')
run(12345,'read-clean')
run(12345,'sync-fault')
run(12345,'close-fault')
run(65536,'alloc-dirty')
run(65536,'alloc-publication')
import re
probe=run(65536,'probe')
counts=re.search(r'append-writes=(\d+) append-flushes=(\d+)',probe)
assert counts
for kind,limit in zip(('write-fault','flush-fault'),map(int,counts.groups())):
    for n in range(1,limit+1):run(65536,kind+str(n))
# Construct a FAT chain whose second cluster is physically distant, then
# independently change valid length and add an exact extent alias.
import struct
f=empty_image(32*1024*1024,cluster_shift=3)
payload=bytes((i*29+(i>>13)+17)&255 for i in range(7000))
f.add_directory_files('fixture',{'input':payload})
root=dict(f.entries(f.chain(f.root)))['fixture']
directory=struct.unpack_from('<I',root,52)[0]
dir_off=f.offset(directory)
entry=dict(f.entries(f.chain(directory)))['input']
first=struct.unpack_from('<I',entry,52)[0];second=f.u32(f.fat+first*4)
spare=f.count+1
bitmap=f.offset(3)
assert not f.data[bitmap+(spare-2)//8] & (1<<((spare-2)%8))
f.data[f.offset(spare):f.offset(spare)+f.cluster]=f.data[f.offset(second):f.offset(second)+f.cluster]
f.data[bitmap+(spare-2)//8]|=1<<((spare-2)%8)
f.data[bitmap+(second-2)//8]&=~(1<<((second-2)%8))
struct.pack_into('<I',f.data,f.fat+first*4,spare)
struct.pack_into('<I',f.data,f.fat+spare*4,0xffffffff)
fragment=bytes(f.data)
def readcase(image,mode):
    work.write_bytes(image)
    p=subprocess.run([str(driver),str(work),'7000',mode],capture_output=True,text=True,timeout=30,
                     env={**os.environ,'STREAM_READ_FAULT_OFFSET':str(f.offset(first))})
    if p.returncode:raise RuntimeError(f'{mode}: {p.stdout} {p.stderr}')
    print(p.stdout.strip(),flush=True)
readcase(fragment,'read-fragment')
readcase(fragment,'read-nlink')
readcase(fragment,'read-fault')
# Recompute the entry-set checksum after reducing valid length.
modified=bytearray(fragment);e=bytearray(entry);struct.pack_into('<Q',e,40,1000)
checksum=0
for i,b in enumerate(e):
    if i not in (2,3):checksum=((((checksum&1)<<15)|(checksum>>1))+b)&0xffff
struct.pack_into('<H',e,2,checksum);modified[dir_off:dir_off+len(e)]=e
readcase(modified,'read-zero')
modified=bytearray(fragment);alias=ExfatFixture.checked_entry('alias',first,payload,contiguous=False)
modified[dir_off+len(entry):dir_off+len(entry)+len(alias)]=alias
readcase(modified,'read-alias')
