#!/usr/bin/env python3
"""Reconstruct a recorded JSC patch stack and compare full source fingerprints."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
from browser_probe import ARCHIVE, DIGEST, LIMIT, ROOT, SIZE, sha

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('run', type=Path)
a = p.parse_args()
base = (ROOT / 'build/browser-jsc').resolve()
run = a.run.resolve()
if run.parent != base or not run.name.startswith('run-') or not run.is_dir():
    p.error('expected a retained run directory under build/browser-jsc')
port = ROOT / 'ports/srvros/webkit'
state = json.loads((run / 'source-state.json').read_text())
started = time.monotonic()
receipt = {'result': 'failed', 'patches': state['patches'], 'source_state_sha256': sha(run/'source-state.json'),
           'tool_sha256': sha(Path(__file__).resolve())}

def tree(path):
    h = hashlib.sha256()
    for entry in sorted(path.rglob('*')):
        if entry.is_symlink(): raise ValueError('unexpected source symlink: '+str(entry))
        if entry.is_file():
            h.update(str(entry.relative_to(path)).encode()+b'\0')
            h.update(str(entry.stat().st_mode & 0o777).encode()+b'\0')
            h.update(bytes.fromhex(sha(entry)))
    return h.hexdigest()

try:
    if not ARCHIVE.is_file() or ARCHIVE.stat().st_size != SIZE or sha(ARCHIVE) != DIGEST:
        raise ValueError('pinned archive missing or invalid')
    with tempfile.TemporaryDirectory(prefix='replay-', dir=base) as temporary:
        temp = Path(temporary); total = 0
        with tarfile.open(ARCHIVE, 'r:xz') as archive:
            for member in archive:
                name = PurePosixPath(member.name)
                if name.is_absolute() or '..' in name.parts: raise ValueError('unsafe archive path')
                if member.issym() or member.islnk(): continue
                if not (member.isdir() or member.isfile()): raise ValueError('special archive member')
                total += member.size
                if total > LIMIT or shutil.disk_usage(base).free < 2 * 1024**3: raise ValueError('resource limit')
                dest = temp.joinpath(*name.parts)
                if member.isdir(): dest.mkdir(parents=True, exist_ok=True)
                else:
                    dest.parent.mkdir(parents=True, exist_ok=True)
                    with archive.extractfile(member) as src, dest.open('wb') as dst:
                        for block in iter(lambda: src.read(1024*1024), b''):
                            if shutil.disk_usage(base).free < 2 * 1024**3: raise ValueError('disk reserve')
                            dst.write(block)
                    dest.chmod(member.mode & 0o777)
        source = temp / 'wpewebkit-2.54.2'
        logpath = run / ('replay-' + str(time.time_ns()) + '.log')
        with logpath.open('w') as log:
            for name, expected in state['patches'].items():
                if Path(name).name != name or sha(port/name) != expected: raise ValueError('patch mismatch: '+name)
                command = ['patch','--batch','--fuzz=0','-p1','-d',str(source),'-i',str(port/name)]
                log.write(json.dumps(command)+'\n'); log.flush()
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
        receipt.update(archive_sha256=DIGEST, extracted_bytes=total, patch_log=str(logpath.relative_to(ROOT)),
                       replay_sha256=tree(source), retained_sha256=tree(run/'wpewebkit-2.54.2'),
                       recorded_sha256=state['source_sha256'])
        if not receipt['replay_sha256'] == receipt['retained_sha256'] == receipt['recorded_sha256']:
            raise ValueError('source fingerprints differ')
        receipt['result'] = 'matched'
except (OSError, ValueError, subprocess.SubprocessError, tarfile.TarError) as error:
    receipt['error'] = str(error)
finally:
    receipt['seconds'] = round(time.monotonic()-started,3)
    path = run / ('replay-' + str(time.time_ns()) + '.json')
    path.write_text(json.dumps(receipt,indent=2)+'\n')
    print('Replay receipt:',path,'result:',receipt['result'])
if receipt['result'] != 'matched': sys.exit(1)
