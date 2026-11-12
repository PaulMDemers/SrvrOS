#!/usr/bin/env python3
"""Prove an order-only miss with real import, persistent fields and clean media."""
import argparse
import json
import re
import subprocess
from pathlib import Path
from exfat_test_image import ExfatFixture,check_clean_image
from test_storage_leases import empty_image
from sidecar_equality import sidecar_equality

p=argparse.ArgumentParser();p.add_argument('--driver',type=Path,required=True)
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--output',type=Path,default=Path('build/review/exfat-sidecar-remount'))
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);results=[]
for shift in (3,8):
    runs={}
    for label,driver in [('legacy',a.baseline),('ordered',a.driver)]:
        prefix=a.output/f'{shift}-{label}';image=prefix.with_suffix('.exfat')
        image.write_bytes(empty_image(16*1024*1024,cluster_shift=shift).data)
        proc=subprocess.run([str(driver.resolve()),str(image),str(prefix)],capture_output=True,text=True)
        prefix.with_suffix('.log').write_text(proc.stdout+proc.stderr)
        assert proc.returncode==0,proc.stdout+proc.stderr
        phases={}
        for line in proc.stdout.splitlines():
            if not line.startswith('row '):continue
            _,phase,path,*values=line.split()
            row=(path,list(map(int,values)))
            if phase=='synced':continue
            phases.setdefault(phase,[]).append(row)
        assert dict(phases['before'])==dict(phases['imported']), 'Remount changed actual fields'
        assert phases['before']!=phases['imported'], 'Missing import-order reproduction'
        assert dict(phases['changed'])==dict(phases['restored']), 'Changed metadata did not persist'
        before=dict(phases['before']);changed=dict(phases['changed'])
        assert before.keys()==changed.keys()
        assert changed['/vol/z'][1]&0o777==0o640
        assert changed['/vol/z'][5]==before['/vol/z'][5]+2, 'Read atime lost'
        assert changed['/vol/z'][7]==before['/vol/z'][7]+2, 'ctime lost'
        assert dict(phases['renamed'])['/vol/one/renamed']==changed['/vol/one/c']
        before_bytes=Path(str(prefix)+'-before.meta').read_bytes()
        after_bytes=Path(str(prefix)+'-after.meta').read_bytes()
        def records(raw):
            assert raw.startswith(b'SRVROSMETA1\n')
            rows=[line.split(b'\t') for line in raw.splitlines()[1:]]
            assert all(len(row)==9 for row in rows)
            return {row[0]:row[1:] for row in rows}
        assert records(before_bytes)==records(after_bytes)
        noops=[dict(zip(['round','writes','flushes','bytes'],map(int,m))) for m in
               re.findall(r'noop round=(\d+) writes=(\d+) flushes=(\d+) bytes=(\d+)',proc.stdout)]
        assert len(noops)==4
        diffs={}
        for line in proc.stdout.splitlines():
            if line.startswith('exfat-sidecar-diff:'):
                fields=dict(x.split('=') for x in line.split()[1:]);diffs[int(fields['snapshot_us'])]={k:int(v) for k,v in fields.items()}
        # Parse every paired snapshot through the production strict reader.
        sidecar_equality([proc.stdout.encode()],True,require_equal=False)
        first_path=diffs[1001]['path']-diffs[1000]['path']
        if label=='ordered':
            assert before_bytes==after_bytes, 'Canonical sidecar changed on no-op remount'
            assert [l.split(b'\t')[0] for l in before_bytes.splitlines()[1:]]==sorted(records(before_bytes))
            assert all(x['writes']==2 and x['bytes']==4 and x['flushes']==4 for x in noops)
            assert first_path==0
        else:
            assert before_bytes!=after_bytes, 'Legacy order did not rewrite'
            assert noops[0]['writes']>2 and noops[0]['bytes']>4 and noops[0]['flushes']>4
            assert first_path==1, 'First differing field did not identify the order-only rewrite'
        for suffix in ('changed','renamed'):
            assert records(Path(str(prefix)+f'-{suffix}.meta').read_bytes())!=records(before_bytes)
        check_clean_image(image)
        fs=ExfatFixture(image.read_bytes());assert fs.read('two/reused')==b'kept'
        allocation=subprocess.run(['build/tests/exfat-driver',str(image),'allocation-check'],capture_output=True,text=True)
        assert allocation.returncode==0,allocation.stdout+allocation.stderr
        runs[label]=dict(noops=noops,fields_preserved=True,clean_allocation=True)
    assert runs['ordered']['noops'][0]['writes']<runs['legacy']['noops'][0]['writes']
    results.append(dict(cluster_shift=shift,**runs))
(a.output/'result.json').write_text(json.dumps(dict(status='PASS',cases=results),indent=2)+'\n')
print('sidecar-remount: PASS order-only reproduction, canonical bytes, four remounts, live atime/mode/ctime and rename persistence, full files/allocation')
