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
NAMESPACE_MODES=tuple('namespace-'+case for case in ('paths','bounds','bindings','lifecycle','identity','persistence','rename','failure','workspace','system'))
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/review/exfat-import')
    parser.add_argument('--limine-include',type=Path,default=ROOT/'build/limine-protocol/include')
    selection=parser.add_mutually_exclusive_group()
    selection.add_argument('--process',action='store_true',help='Link real process content writeback (x86_64 host/Rosetta on macOS)')
    selection.add_argument('--workspace-writeback',action='store_true',help='Real process/exFAT/VFS/workspace failure matrix')
    selection.add_argument('--workspace-salvage',action='store_true',help='Copy retained failed-work data to a healthy volume')
    selection.add_argument('--namespace',action='store_true',help='Run only the explicit namespace view contracts')
    parser.add_argument('--sanitize', action='store_true', help='Enable address and undefined-behavior sanitizers')
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    exe=out/'exfat-import'
    files=['tools/test_exfat_import.c','kernel/src/fs/exfat.c','kernel/src/vfs.c','tools/test_exfat_import.py','kernel/src/block_claim.c','kernel/include/srvros/block.h','kernel/include/srvros/exfat.h']
    files+=['kernel/src/vfs_namespace.c','kernel/include/srvros/vfs_namespace.h','tools/test_vfs_namespace.inc']
    compiler=shlex.split(os.environ.get('CC','cc'))
    command=compiler+['-DSRVROS_EXFAT_HOST_TEST','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-I',str(ROOT/'shared/include'),'-I',str(ROOT/'kernel/include'),
        str(ROOT/files[0]),str(ROOT/files[2]),str(ROOT/'kernel/src/vfs_namespace.c'),'-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections','-o',str(exe)]
    if args.process or args.workspace_writeback or args.workspace_salvage:
        files+=['tools/test_process_writeback.c','kernel/src/process.c','kernel/src/workspace.c','tools/test_workspace_writeback.inc','tools/test_workspace_salvage.inc','tools/test_process_salvage.inc']
        command[len(compiler):len(compiler)]=(['-arch','x86_64'] if platform.system()=='Darwin' else [])+['-DSRVROS_EXFAT_PROCESS_TEST','-I',str(args.limine_include.resolve()),str(ROOT/'tools/test_process_writeback.c')]
    if args.sanitize: command[len(compiler):len(compiler)]=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
    subprocess.run(command,check=True,cwd=ROOT)
    modes=('writeback-process',) if args.process else NAMESPACE_MODES if args.namespace else MODES+NAMESPACE_MODES
    if args.workspace_writeback:
        baseline=subprocess.run([str(exe),'workspace-control'],capture_output=True,text=True)
        (out/'baseline.log').write_text(baseline.stdout+baseline.stderr)
        baseline.check_returncode()
        import re
        counts=re.search(r'workspace-ops writes=(\d+) flushes=(\d+)',baseline.stdout)
        if not counts or any(int(n)<=0 for n in counts.groups()):
            raise RuntimeError('Missing actual writeback operation counts')
        modes=['workspace-control','workspace-close-control','workspace-barrier','workspace-clean']
        for close in ('','close-'):
            for kind,total in zip(('write','flush'),map(int,counts.groups())):
                modes += [f'workspace-{close}{kind}-{i}' for i in range(1,total+1)]
    if args.workspace_salvage:
        baseline=subprocess.run([str(exe),'salvage-control'],capture_output=True,text=True)
        (out/'baseline.log').write_text(baseline.stdout+baseline.stderr)
        baseline.check_returncode()
        import re
        counts=re.search(r'salvage-ops writes=(\d+) flushes=(\d+)',baseline.stdout)
        if not counts or any(int(n)<=0 for n in counts.groups()):
            raise RuntimeError('Missing actual salvage operation counts')
        modes=['salvage-control','salvage-conflict','salvage-oom']
        for kind,total in zip(('write','flush'),map(int,counts.groups())):
            modes += [f'salvage-{kind}-{i}' for i in range(1,total+1)]
    results=[]
    for mode in modes:
        r=subprocess.run([str(exe),mode],capture_output=True,text=True,timeout=15)
        print(mode, 'PASS' if r.returncode==0 else 'FAIL',r.stdout+r.stderr,flush=True)
        results.append(dict(mode=mode,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
    code=int(any(r['exit'] for r in results))
    record=dict(source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),command=command,cases=results,exit=code,
        sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in files})
    (out/'result.json').write_text(json.dumps(record,indent=2)+'\n');return code
if __name__=='__main__':raise SystemExit(main())
