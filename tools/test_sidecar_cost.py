#!/usr/bin/env python3
"""Real BOT/exFAT A/B: identical writes/barriers/media, less directory input."""
import json,os,re,subprocess,gzip,hashlib
from pathlib import Path
from test_storage_leases import empty_image
from exfat_test_image import ExfatFixture,check_clean_image
from storage_only_result import capture_result,payload
out=Path('build/review/sidecar-cost');out.mkdir(parents=True,exist_ok=True);results=[]
for shift,cluster in ((9,3),(9,8),(12,5)):
    f=empty_image(32*1024*1024,0x5a1b2c3d,shift,cluster)
    f.add_directory_files('archive',{f'Kept-{i:04}.txt':b'' for i in range(1100)})
    f.add_directory_files('srvros-work',{'.workspace':b'SRVROS-WORKSPACE-V1 5a1b2c3d\n','anchor.txt':b'preserve this anchor\n'})
    pair={};media={}
    for name,driver in [('old','workspace-compare-directory-old'),('new','workspace-compare-host')]:
        p=out/f'{shift}-{cluster}-{name}.exfat';p.write_bytes(f.data)
        q=subprocess.run(['build/tests/'+driver,str(p),'workload','metadata'],env={**os.environ,'SRVROS_METADATA_LOGICAL_CLOCK':'1'},capture_output=True,text=True,timeout=120)
        (out/f'{shift}-{cluster}-{name}.log').write_text(q.stdout+q.stderr);assert q.returncode==0,q.stdout+q.stderr
        proof=capture_result(q.stdout,False,True);assert proof and proof['version']==3
        assert ExfatFixture(p.read_bytes()).read(proof['path'].removeprefix('/work/'))==payload(12)
        check_clean_image(p)
        check=subprocess.run(['build/tests/exfat-driver',str(p),'allocation-check'],capture_output=True,text=True);assert check.returncode==0,check.stdout
        trace=re.search(r'workspace-mutation-trace: commands=(\d+) hash=([0-9a-f]{16})',q.stdout);assert trace
        pair[name]=dict(trace=trace.groups(),proof=proof);media[name]=p.read_bytes()
    assert media['old']==media['new'],'Directory optimization changed complete media'
    assert pair['old']['trace']==pair['new']['trace'],'Writes/flushes or ordering changed'
    # Complete field evidence is identical under a step-bound metadata clock.
    assert pair['old']['proof']['metadata']==pair['new']['proof']['metadata']
    counts={}
    for name,row in pair.items():
        role4=[r for g in row['proof']['roles'] for r in g['roles'] if r['role']==4]
        counts[name]={k:sum(r[k] for r in role4) for k in ('reads','read_bytes','writes','write_bytes','flushes','csws')}
    for k in ('writes','write_bytes','flushes'):assert counts['old'][k]==counts['new'][k]
    if (1<<shift)<<cluster>16384:assert counts['new']['read_bytes']<counts['old']['read_bytes'],counts
    else:assert counts['new']==counts['old'],counts
    results.append(dict(sector_shift=shift,cluster_shift=cluster,counts=counts,mutation_trace=pair['new']['trace'],same_media=True,same_complete_changes=True))
    print('sidecar-cost',shift,cluster,counts,flush=True)
    for name,raw in media.items():
        p=out/f'{shift}-{cluster}-{name}.exfat';archive=p.with_suffix('.exfat.gz');archive.write_bytes(gzip.compress(raw,mtime=0));assert gzip.decompress(archive.read_bytes())==raw;p.unlink()
(out/'result.json').write_text(json.dumps(dict(status='PASS',cases=results),indent=2)+'\n')
print('sidecar-cost: PASS full-field evidence, complete media, write/barrier equivalence and directory read reduction')
