#!/usr/bin/env python3
"""Portable prefix-write/flush interruption checks on synthetic disposable exFAT.

This models persisted write prefixes (including one half-write), not controller
reordering, physical sector atomicity, exfatprogs repair or crash-atomic rename.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from test_storage_leases import empty_image
from exfat_test_image import ExfatFixture
ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver', type=Path, default=ROOT/'build/tests/exfat-driver')
    parser.add_argument('--output', type=Path, default=ROOT/'build/review/rename-sidecar/crash')
    args = parser.parse_args()
    out = args.output.resolve();out.mkdir(parents=True, exist_ok=True)
    driver = args.driver.resolve();image = out/'candidate.exfat';results = []

    def run(mode, args=(), expected=(0,)):
        result = subprocess.run([str(driver), str(image), mode, *map(str, args)],
                                capture_output=True, text=True, timeout=30)
        results.append(dict(mode=mode, args=list(args), exit=result.returncode,
                            stdout=result.stdout, stderr=result.stderr))
        (out/'result.json').write_text(json.dumps(dict(cases=results,
            driver_sha256=hashlib.sha256(driver.read_bytes()).hexdigest()), indent=2)+'\n')
        assert result.returncode in expected, result.stdout+result.stderr
        return result.stdout

    image.write_bytes(empty_image(4*1024*1024).data)
    run('seed');seed=image.read_bytes()
    run('seed-temp');temporary=image.read_bytes()
    counts={}
    for operation in ('rename', 'replace', 'recover'):
        baseline = temporary if operation=='recover' else seed
        image.write_bytes(baseline)
        text=run(operation)
        writes, flushes=map(int,re.findall(r'complete writes=(\d+) flushes=(\d+)',text)[0])
        assert writes and flushes
        if operation!='recover':run('verify-'+operation)
        else:
            fs=ExfatFixture(image.read_bytes())
            assert fs.read('.srvros/meta')==ExfatFixture(seed).read('.srvros/meta')
            assert 'meta.tmp' not in dict(fs.entries(fs.read('.srvros')))
        counts[operation]=dict(writes=writes, flushes=flushes)
        for fault, count in (('write-failure',writes),('write-cut',writes),
                             ('flush-failure',flushes),('flush-cut',flushes),('half-write',writes)):
            for boundary in range(1,count+1):
                image.write_bytes(baseline)
                values=[0]*5
                values[{'write-failure':0,'write-cut':1,'flush-failure':2,'flush-cut':3,'half-write':4}[fault]]=boundary
                text=run(operation,values,expected=(77,) if fault=='write-cut' else (78,) if fault=='flush-cut' else (79,) if fault=='half-write' else (0,))
                if operation!='recover' and fault.endswith('failure'):
                    assert 'operation=FAIL' in text
                data=image.read_bytes();fs=ExfatFixture(data)
                # An unrelated file is preserved even when a torn entry makes
                # the strict kernel importer refuse the complete namespace.
                assert fs.read('anchor.txt')==b'unchanged'
                # Promotion must never delete the committed sidecar first.
                if fault!='half-write':assert 'meta' in dict(fs.entries(fs.read('.srvros')))
                if struct.unpack_from('<H',data,106)[0]&6:
                    run('inspect-cut')
                    assert image.read_bytes()==data
                else:
                    # Clean cuts can only precede mutation or follow all data
                    # and sidecar writes. Validate the allocation graph.
                    run('allocation-check')
                    if fault in ('write-cut','flush-cut','half-write') and operation!='recover':
                        run('verify-'+operation)
            print(operation, fault, count, 'boundaries PASS', flush=True)
    (out/'counts.json').write_text(json.dumps(counts,indent=2)+'\n')


if __name__=='__main__':main()
