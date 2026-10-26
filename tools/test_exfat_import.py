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
MODES=('writeback-reserve','writeback-clean','writeback-entry','writeback-begin','writeback-clean-write','writeback-clean-flush','writeback-invalid','retire-first-sidecar','retire-overwrite-full','retire-truncate','retire-shrink','retire-delete','retire-directory','retire-chmod-full','retire-utime-full','growth-stale-chain','growth-stale-tail','growth-full','growth-oom','growth-size','growth-width','growth-root','growth-contiguous','boot-header-snapshot','teardown-statfs','boot-main-shift','boot-backup-stale','boot-read-main','boot-read-backup','system-unknown-critical','system-nested','boot-main','boot-backup','boot-both','boot-repeat','boot-fields','boot-extended','boot-disagree','boot-flags','system-bitmap-missing','system-upcase-missing','system-bitmap-duplicate','system-upcase-duplicate','system-bitmap-id','system-bitmap-length','system-bitmap-chain','system-upcase-checksum','system-upcase-ascii','system-upcase-short','system-upcase-chain','system-fragmented','chmod-failure','utime-failure','metadata-temp-read','metadata-partial','metadata-scope','metadata-overflow','metadata-truncated','metadata-nul','metadata-dirty','metadata-valid','metadata-temp','teardown-foreign','teardown-read','teardown-write','teardown-check','teardown-mount','teardown-range','teardown-admission','teardown-busy','teardown-admit-race','teardown-boot','device-alias','device-lease','device-delegate','device-rollback','control','geometry-shifts','geometry-volume','geometry-fat','geometry-heap','geometry-format','geometry-valid','stream-invalid',
       'valid-read','valid-zero','valid-boundary','valid-chain','valid-invalid','valid-rename','valid-replace','valid-overwrite',
       'entry-checksum','entry-layout','entry-name-length','entry-name-hash','entry-unsafe-name',
       'entry-flags','entry-extra','entry-cleanup','entry-valid','dir-cycle','dir-invalid-next','dir-short','dir-chain',
       'identity-duplicates','mount-overlap','mount-existing','mount-paths',
       'alias-delete','alias-overwrite','alias-truncate','alias-replace','alias-chain',
       'ownership-overlap','ownership-bitmap','ownership-leak','ownership-alias-flags',
       'alias-overwrite-failure','ownership-read-failure','ownership-oom','allocation-faults','alias-write-quarantine')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/review/exfat-import')
    parser.add_argument('--limine-include',type=Path,default=ROOT/'build/limine-protocol/include')
    parser.add_argument('--process',action='store_true',help='Link real process content writeback (x86_64 host/Rosetta on macOS)')
    parser.add_argument('--sanitize', action='store_true', help='Enable address and undefined-behavior sanitizers')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    exe=out/'exfat-import'
    files=['tools/test_exfat_import.c','kernel/src/fs/exfat.c','kernel/src/vfs.c','tools/test_exfat_import.py','kernel/src/block_claim.c','kernel/include/srvros/block.h','kernel/include/srvros/exfat.h']
    compiler=shlex.split(os.environ.get('CC','cc'))
    command=compiler+['-DSRVROS_EXFAT_HOST_TEST','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-I',str(ROOT/'shared/include'),'-I',str(ROOT/'kernel/include'),
        str(ROOT/files[0]),str(ROOT/files[2]),'-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections','-o',str(exe)]
    if args.process:
        files+=['tools/test_process_writeback.c','kernel/src/process.c']
        command[len(compiler):len(compiler)]=(['-arch','x86_64'] if platform.system()=='Darwin' else [])+['-DSRVROS_EXFAT_PROCESS_TEST','-I',str(args.limine_include.resolve()),str(ROOT/'tools/test_process_writeback.c')]
    if args.sanitize: command[len(compiler):len(compiler)]=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
    subprocess.run(command,check=True,cwd=ROOT)
    results=[]
    for mode in (('writeback-process',) if args.process else MODES):
        r=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=15)
        print(mode, 'PASS' if r.returncode==0 else 'FAIL',r.stdout+r.stderr,flush=True)
        results.append(dict(mode=mode,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
    code=int(any(r['exit'] for r in results))
    record=dict(source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),command=command,cases=results,exit=code,
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files})
    (out/'result.json').write_text(json.dumps(record,indent=2)+'\n');return code
if __name__=='__main__':raise SystemExit(main())
