#!/usr/bin/env python3
"""Real exFAT/VFS operations and exhaustive write-boundary failures on disposable images."""
from pathlib import Path
import re
import struct
import subprocess
import tempfile
from exfat_test_image import ExfatFixture, check_clean_image as clean
from hardware_workload import verify_tree

ROOT = Path(__file__).resolve().parents[1]
DRIVER = ROOT/'build/tests/exfat-driver'


def command(*args, codes=(0,)):
    result = subprocess.run([str(a) for a in args], capture_output=True, text=True)
    if result.returncode not in codes:
        raise RuntimeError(f'{args}: exit {result.returncode}\n{result.stdout}{result.stderr}')
    return result.stdout+result.stderr


def format_image(path, cluster):
    with path.open('wb') as stream: stream.truncate(64*1024*1024)
    command('/usr/sbin/mkfs.exfat','-c',cluster,path)
    clean(path)


def sparse_copy(source,target):
    with source.open('rb') as src,target.open('wb') as dst:
        while True:
            block=src.read(1024*1024)
            if not block:break
            if block.strip(b'\0'):dst.write(block)
            else:dst.seek(len(block),1)
        dst.truncate(source.stat().st_size)


def main():
    with tempfile.TemporaryDirectory(prefix='srvros-exfat-fault-') as tmp:
        directory = Path(tmp)
        for cluster in ('4K','128K'):
            workload = directory/'workload.img'
            format_image(workload,cluster)
            print(command(DRIVER,workload,'workload').strip(),flush=True)
            clean(workload)
            fs=ExfatFixture(workload.read_bytes())
            base='srvros-tests/0123456789abcdef.files'
            def listing(relative):
                path=base+('/'+relative if relative else '')
                return {name: 'directory' if entry[4]&0x10 else 'file'
                        for name,entry in fs.entries(fs.read(path))}
            verify_tree(lambda name,size:fs.read(base+'/'+name),listing)
            image = directory/'ops.img'
            format_image(image,cluster)
            print(command(DRIVER,image,'ops').strip(),flush=True)
            clean(image)
            fs = ExfatFixture(image.read_bytes())
            assert fs.read('empty') == b'' and fs.read('truncated') == b''
            assert fs.read('A-much-longer-renamed-file.txt') == bytes((i*17+3)&255 for i in range(9))
            print(f'operations {cluster}: create/grow/shrink/empty/rename/move/delete PASS',flush=True)
        image=directory/'fragmented.img'
        format_image(image,'4K'); command(DRIVER,image,'fragmented'); clean(image)
        fs=ExfatFixture(image.read_bytes())
        entry=next(e for name,e in fs.entries(fs.chain(fs.root)) if name=='fragmented')
        assert entry[33]&2==0, 'Fragmentation fixture did not exercise a FAT chain'
        assert fs.read('fragmented')==bytes([0x5a])*(fs.cluster*24)
        print('forced fragmented allocation and readback PASS',flush=True)
        seed, work = directory/'seed.img', directory/'work.img'
        format_image(seed,'4K'); command(DRIVER,seed,'seed'); clean(seed)
        for mode in ('noflush','foreign'):
            sparse_copy(seed,work)
            before=work.read_bytes(); command(DRIVER,work,mode)
            assert work.read_bytes()==before
        for flag in (2,4):
            sparse_copy(seed,work)
            with work.open('r+b') as stream:
                stream.seek(106); stream.write(struct.pack('<H',flag))
            before=work.read_bytes(); command(DRIVER,work,'readonly')
            assert work.read_bytes()==before
        print('dirty/media-error/no-flush/foreign-owner refusal PASS',flush=True)
        for operation in ('overwrite','create','delete','rename'):
            sparse_copy(seed,work)
            output=command(DRIVER,work,operation); clean(work)
            total=int(re.search(r'operation=PASS writes=(\d+)',output)[1])
            flush_total=int(re.search(r'operation=PASS writes=\d+ flushes=(\d+)',output)[1])
            for mode,count in (('flush-failure',flush_total),('failure',total),('power-cut',total)):
                for boundary in range(1,count+1):
                    sparse_copy(seed,work)
                    output=command(DRIVER,work,operation,boundary if mode=='failure' else 0,
                                   boundary if mode=='power-cut' else 0,boundary if mode=='flush-failure' else 0,
                                   codes=(77,) if mode=='power-cut' else (0,))
                    with work.open('rb') as stream:
                        stream.seek(106); dirty=struct.unpack('<H',stream.read(2))[0]&2
                    if mode!='power-cut' and 'operation=FAIL' not in output: raise RuntimeError(output)
                    if mode!='flush-failure' and boundary>1 and boundary<total and not dirty:
                        raise RuntimeError(f'{operation} {mode} {boundary}: interrupted metadata declared clean')
                    if dirty:
                        before=work.read_bytes(); command(DRIVER,work,'readonly')
                        assert work.read_bytes()==before, 'Dirty remount changed media'
                        command('/usr/sbin/fsck.exfat','-y',work,codes=(0,1))
                    clean(work)
                    fs=ExfatFixture(work.read_bytes())
                    assert fs.read('anchor.txt')==b'unchanged'
                    names={name for name,_ in fs.entries(fs.chain(fs.root))}
                    if operation=='overwrite':assert fs.read('keep.txt') in (b'original',b'replacement-data')
                    if operation=='create':
                        assert fs.read('keep.txt')==b'original'
                        if 'new.txt' in names:assert fs.read('new.txt')==b'new-file-data'
                    if operation=='delete' and 'keep.txt' in names:assert fs.read('keep.txt')==b'original'
                    if operation=='rename':
                        present=[name for name in ('keep.txt','renamed-with-a-longer-name.txt') if name in names]
                        assert present and any(fs.read(name)==b'original' for name in present)
                print(f'{operation} {mode}: all {count} boundaries, readonly remount and offline recovery PASS',flush=True)
    print('exfat-hardening: PASS',flush=True)


if __name__ == '__main__': main()
