#!/usr/bin/env python3
"""Growth beyond the old table limits on disposable, independently checked exFAT."""
from pathlib import Path
import subprocess,tempfile,time,struct,hashlib
from exfat_test_image import ExfatFixture,check_clean_image
ROOT=Path(__file__).resolve().parents[1]
DRIVER=ROOT/'build/tests/exfat-driver'
def run(image,mode,*args):
    start=time.monotonic()
    r=subprocess.run([str(DRIVER),str(image),mode,*map(str,args)],capture_output=True,text=True)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    print(r.stdout.strip(),f'seconds={time.monotonic()-start:.3f}',flush=True)
def format_image(path):
    with path.open('wb') as f:f.truncate(128*1024*1024)
    subprocess.run(['/usr/sbin/mkfs.exfat','-c','128K',str(path)],check=True,stdout=subprocess.DEVNULL)
def main():
    with tempfile.TemporaryDirectory(prefix='srvros-scale-') as tmp:
        image=Path(tmp)/'scale.img';format_image(image)
        run(image,'memory-refusal');check_clean_image(image)
        format_image(image)
        fs=ExfatFixture(image.read_bytes())
        fs.add_directory_files('bulk',{**{f'f{i:04d}':b'' for i in range(5000)},'anchor.txt':b'anchor'})
        image.write_bytes(fs.data);check_clean_image(image)
        before=hashlib.sha256(image.read_bytes()).hexdigest()
        # Mount-time directory buffers, file blocks and VFS blocks must fail cleanly.
        for boundary in (1,2,3,4,8,16,32,64):
            run(image,'memory-mount',boundary)
            assert hashlib.sha256(image.read_bytes()).hexdigest()==before
        run(image,'scale');print(check_clean_image(image).strip())
        fs=ExfatFixture(image.read_bytes());entries=dict(fs.entries(fs.read('bulk')))
        expected={f'f{i:04d}' for i in range(5000)}-{'f0042'}|{'renamed','anchor.txt'}|{f'new-{i:04d}' for i in range(96)}
        assert set(entries)==expected
        for name,entry in entries.items():
            size=struct.unpack_from('<Q',entry,56)[0];first=struct.unpack_from('<I',entry,52)[0]
            expected_bytes=b'anchor' if name=='anchor.txt' else b'edited' if name=='f0043' else (b'new' if int(name[4:])%2 else b'reused') if name.startswith('new-') else b''
            assert fs.chain(first,size,bool(entry[33]&2))==expected_bytes,name
        root=dict(fs.entries(fs.chain(fs.root)))
        assert {f'dir-{i:04d}' for i in range(320)}<=root.keys()
        for i in range(320):assert not list(fs.entries(fs.read(f'dir-{i:04d}')))
        print('exfat-scale: PASS exact 5000 imported files, 96 growth/reuse files, 320 directories, identity, rename/edit/remount and allocation failures',flush=True)
if __name__=='__main__':main()
