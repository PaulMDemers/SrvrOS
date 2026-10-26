#!/usr/bin/env python3
"""Run the real exFAT/VFS import contracts on disposable in-memory media."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shlex
import subprocess
ROOT=Path(__file__).resolve().parents[1]
MODES=('control','geometry-shifts','geometry-volume','geometry-fat','geometry-heap','geometry-format','geometry-valid','stream-invalid',
       'valid-read','valid-zero','valid-boundary','valid-chain','valid-invalid','valid-rename','valid-replace','valid-overwrite',
       'entry-checksum','entry-layout','entry-name-length','entry-name-hash','entry-unsafe-name',
       'entry-flags','entry-extra','entry-cleanup','entry-valid','dir-cycle','dir-invalid-next','dir-short','dir-chain')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/review/exfat-import')
    parser.add_argument('--sanitize', action='store_true', help='Enable address and undefined-behavior sanitizers')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    exe=out/'exfat-import'
    files=['tools/test_exfat_import.c','kernel/src/fs/exfat.c','kernel/src/vfs.c','tools/test_exfat_import.py']
    command=shlex.split(os.environ.get('CC','cc'))+['-DSRVROS_EXFAT_HOST_TEST','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-I',str(ROOT/'shared/include'),'-I',str(ROOT/'kernel/include'),
        str(ROOT/files[0]),str(ROOT/files[2]),'-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections','-o',str(exe)]
    if args.sanitize: command[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
    subprocess.run(command,check=True,cwd=ROOT)
    results=[]
    for mode in MODES:
        r=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=15)
        print(mode, 'PASS' if r.returncode==0 else 'FAIL',r.stdout+r.stderr,flush=True)
        results.append(dict(mode=mode,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
    code=int(any(r['exit'] for r in results))
    record=dict(source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),command=command,cases=results,exit=code,
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files})
    (out/'result.json').write_text(json.dumps(record,indent=2)+'\n');return code
if __name__=='__main__':raise SystemExit(main())
