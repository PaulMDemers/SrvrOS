#!/usr/bin/env python3
"""Real producer/BOT/fs/lease and strict collector on disposable media."""
import copy
import json
import hashlib
import re
from pathlib import Path
import subprocess
from exfat_test_image import ExfatFixture,check_clean_image
from test_storage_leases import empty_image
from storage_only_result import capture_result,payload
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/review/storage-only';OUT.mkdir(parents=True,exist_ok=True)
results=[]
for shift,cluster_shift in ((9,3),(9,8),(12,0)):
    baseline=empty_image(16*1024*1024,0x5a1b2c3d,shift,cluster_shift)
    baseline.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 5a1b2c3d\n','anchor.txt':b'preserve this anchor\n'})
    for mode in ('workload','unsampled','alias','partial','flush','mismatch','clock','deadline','busy','held','timer','existing'):
        fixture=copy.deepcopy(baseline)
        if mode=='existing':
            fixture=empty_image(16*1024*1024,0x5a1b2c3d,shift,cluster_shift)
            fixture.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 5a1b2c3d\n','anchor.txt':b'preserve this anchor\n','storage-compare-00000000000186a0.bin':b'never replace me'})
        control_image=OUT/'control.exfat';control_image.write_bytes(fixture.data)
        control=subprocess.run([str(ROOT/'build/tests/workspace-compare-legacy'),str(control_image),mode],capture_output=True,text=True,timeout=30)
        (OUT/f'{shift}-{cluster_shift}-{mode}-legacy.log').write_text(control.stdout+control.stderr)
        assert control.returncode==0,(mode,control.stdout+control.stderr)
        control_trace=re.search(r'workspace-control-trace: calls=(\d+) hash=([0-9a-f]{16}) clocks=(\d+)',control.stdout)
        assert control_trace
        reference=(control_trace.groups(),hashlib.sha256(control_image.read_bytes()).hexdigest())
        for version in (1,2):
            image=OUT/'candidate.exfat';image.write_bytes(fixture.data)
            p=subprocess.run([str(ROOT/'build/tests/workspace-compare-host'),str(image),mode]+(['roles'] if version==2 else []),capture_output=True,text=True,timeout=30)
            log=p.stdout+p.stderr;name=f'{shift}-{cluster_shift}-{mode}-v{version}'
            (OUT/(name+'.log')).write_text(log)
            assert p.returncode==0,(name,log[-2000:])
            actual=ExfatFixture(image.read_bytes())
            assert actual.read('srvros-work/anchor.txt')==b'preserve this anchor\n'
            assert actual.read('srvros-work/.workspace')==b'SRVROS-WORKSPACE-V1 5a1b2c3d\n'
            proof=capture_result(log,False)
            if mode in ('workload','unsampled'):
                assert proof is not None and proof['version']==version,name
                expected=payload(12)
                assert len({expected[i:i+512] for i in range(0,32768,512)})==64
                audio=log+'\nstorage-only-audio: volume=1 cues=2 stopped=yes quiet=yes clean=yes\nlaptop-capture: completed=yes\n'
                assert capture_result(audio) is not None
                assert capture_result(audio+'storage-only-audio: volume=1 cues=0 stopped=NO quiet=NO clean=NO\n') is None
                assert actual.read(proof['path'].removeprefix('/work/'))==payload(12)
                check_clean_image(image)
                check=subprocess.run([str(ROOT/'build/tests/exfat-driver'),str(image),'allocation-check'],capture_output=True,text=True)
                assert check.returncode==0,(name,check.stdout+check.stderr)
                # Receipt mutations must never turn incomplete evidence into PASS.
                lines=log.splitlines()
                for prefix in ('storage-only: begin','storage-only: complete','storage-only-sample:','storage-only-command:','storage-only-service:','storage-only-lease:'):
                    index=next(i for i,l in enumerate(lines) if l.startswith(prefix))
                    assert capture_result('\n'.join(lines[:index]+lines[index+1:]),False) is None
                    assert capture_result('\n'.join(lines[:index]+[lines[index]]+lines[index:]),False) is None
                for old,new in (('valid=1','valid=0'),('ok=1','ok=0'),('retired=1','retired=0'),('claimed=1','claimed=0'),('policy=unchanged','policy=changed'),('actual=13','actual=12'),('complete=yes','complete=NO'),('seq=1 ','seq=99 ')):
                    assert capture_result(log.replace(old,new,1),False) is None,(old,new)
                if version==2:
                    assert capture_result(log,False,require_sidecar=True) is not None
                    # Missing/incorrect first-difference accounting is not a
                    # valid new candidate, even when every legacy row remains.
                    without='\n'.join(l for l in log.splitlines() if not l.startswith('exfat-sidecar-diff:'))+'\n'
                    assert capture_result(without,False,require_sidecar=True) is None
                    assert capture_result(log.replace('exfat-sidecar-diff: version=1','exfat-sidecar-diff: version=0'),False,True) is None
                    for prefix in ('storage-only-roles:','storage-only-role:','storage-only-wire:'):
                        index=next(i for i,l in enumerate(lines) if l.startswith(prefix))
                        assert capture_result('\n'.join(lines[:index]+lines[index+1:])+'\n',False) is None
                        assert capture_result('\n'.join(lines[:index]+[lines[index]]+lines[index:])+'\n',False) is None
                    def mutate_roles(old,new):
                        edited=log.replace(old,new,1)
                        size=sum(len(l) for l in edited.splitlines(keepends=True) if any(l.startswith(p+':') for p in ('storage-only-sample','storage-only-command','storage-only-service','storage-only-roles','storage-only-role')))
                        return re.sub(r'row_bytes=\d+',f'row_bytes={size}',edited)
                    for old,new in [('roles=7','roles=6'),('writes512=0','writes512=999'),('failed=0','failed=1'),('other=0','other=1'),('mask=64','mask=65')]:
                        assert capture_result(mutate_roles(old,new),False) is None,(name,old)
                (OUT/(name+'.json')).write_text(json.dumps(proof,indent=2)+'\n')
            else:
                assert proof is None
                if mode in ('busy','held','timer','existing'):
                    assert actual.data==fixture.data,name
                if mode=='existing':assert actual.read('srvros-work/storage-compare-00000000000186a0.bin')==b'never replace me'
            results.append(dict(case=name,status='PASS'));print(name,'PASS',flush=True)
            trace=re.search(r'workspace-control-trace: calls=(\d+) hash=([0-9a-f]{16}) clocks=(\d+)',log)
            assert trace
            signature=(trace.groups(),hashlib.sha256(actual.data).hexdigest())
            assert signature==reference,(name,'observer changed ordered BOT data, clocks or media')
(OUT/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print('storage-only: PASS real producer, exact containment, failures, retirement, parser negatives')
