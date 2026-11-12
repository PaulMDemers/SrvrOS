#!/usr/bin/env python3
"""Real producer/BOT/fs/lease and strict collector on disposable media."""
import copy
import json
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
    for mode in ('workload','partial','flush','mismatch','clock','deadline','busy','held','timer','existing'):
        fixture=copy.deepcopy(baseline)
        if mode=='existing':
            fixture=empty_image(16*1024*1024,0x5a1b2c3d,shift,cluster_shift)
            fixture.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 5a1b2c3d\n','anchor.txt':b'preserve this anchor\n','storage-compare-00000000000186a0.bin':b'never replace me'})
        image=OUT/'candidate.exfat';image.write_bytes(fixture.data)
        p=subprocess.run([str(ROOT/'build/tests/workspace-compare-host'),str(image),mode],capture_output=True,text=True,timeout=30)
        log=p.stdout+p.stderr;name=f'{shift}-{cluster_shift}-{mode}'
        (OUT/(name+'.log')).write_text(log)
        assert p.returncode==0,(name,log[-2000:])
        actual=ExfatFixture(image.read_bytes())
        assert actual.read('srvros-work/anchor.txt')==b'preserve this anchor\n'
        assert actual.read('srvros-work/.workspace')==b'SRVROS-WORKSPACE-V1 5a1b2c3d\n'
        proof=capture_result(log,False)
        if mode=='workload':
            assert proof is not None,name
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
            (OUT/(name+'.json')).write_text(json.dumps(proof,indent=2)+'\n')
        else:
            assert proof is None
            if mode in ('busy','held','timer','existing'):
                assert actual.data==fixture.data,name
            if mode=='existing':assert actual.read('srvros-work/storage-compare-00000000000186a0.bin')==b'never replace me'
        results.append(dict(case=name,status='PASS'));print(name,'PASS',flush=True)
(OUT/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print('storage-only: PASS real producer, exact containment, failures, retirement, parser negatives')
