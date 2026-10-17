#!/usr/bin/env python3
"""Repair only the six known metadata records from the first USB test failure.

Requires an unmounted USB volume and a prior read-only recovery.json. Refuses
changed evidence/entries. Saves and fsyncs original/replacement records before
writing; never alters allocation, file contents, lengths or unrelated entries.
"""
import argparse
import json
import os
from pathlib import Path
import stat
import struct
import subprocess

import hardware_test as test
import inspect_hardware_test as recovery


def plan(evidence, metadata):
    if evidence['run'] != metadata['run'] or evidence['volume'] != metadata['volume']:
        raise ValueError('Recovery identity mismatch')
    files = evidence['files']; prefix = 'srvros-tests/'+metadata['run']
    report = bytes.fromhex(files[prefix+'.txt']['data_hex'])
    done = bytes.fromhex(files[prefix+'.done']['data_hex'])
    test.validate_result(metadata, report, done)
    if bytes.fromhex(files[prefix+'.probe']['data_hex']) != test.PROBE_BYTES:
        raise ValueError('Cannot repair without a verified persisted probe')
    allowed = {prefix+s for s in ('.req', '.txt', '.done', '.probe')} | {'.srvros', '.srvros/meta'}
    changes = []
    for path, item in evidence['entries'].items():
        if path not in allowed: continue
        old = bytes.fromhex(item['raw_hex']); new = bytearray(old)
        if len(old) < 96 or len(old) != (old[1]+1)*32 or old[0] != 0x85 or old[32] != 0xc0:
            raise ValueError('Unexpected entry format')
        if old[33] & ~3: raise ValueError('Unexpected flags')
        name = path.rsplit('/',1)[-1]
        name_bytes = b''.join(old[i+2:i+32] for i in range(64,len(old),32) if old[i] == 0xc1)
        if name_bytes[:old[35]*2].decode('utf-16le') != name: raise ValueError('Entry/name mismatch')
        new[33] |= 1
        struct.pack_into('<H', new, 36, recovery.checksum(name.upper().encode('utf-16le')))
        struct.pack_into('<H', new, 2, recovery.checksum(new, (2,3)))
        if new != old:
            changes.append({'path': path, 'offset': item['offset'], 'before_hex': old.hex(), 'after_hex': new.hex()})
    if not changes: raise ValueError('No matching malformed entries to repair')
    return {'run': metadata['run'], 'volume': metadata['volume'], 'changes': changes}


def apply(stream, proposed):
    # Validate ALL original bytes before making the first write. A rerun after
    # partial completion can finish, but unrelated changes always cause refusal.
    for change in proposed['changes']:
        old, new = bytes.fromhex(change['before_hex']), bytes.fromhex(change['after_hex'])
        stream.seek(change['offset']); actual = stream.read(len(old))
        if actual not in (old, new): raise ValueError('Directory entry changed since capture')
    for change in proposed['changes']:
        data = bytes.fromhex(change['after_hex'])
        stream.seek(change['offset'])
        if stream.write(data) != len(data): raise OSError('Short repair write')
    stream.flush(); os.fsync(stream.fileno())
    for change in proposed['changes']:
        data = bytes.fromhex(change['after_hex'])
        stream.seek(change['offset'])
        if stream.read(len(data)) != data: raise OSError('Repair read-back failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', required=True)
    parser.add_argument('--apply', action='store_true', help='Write the backed-up metadata corrections; otherwise show plan')
    args = parser.parse_args()
    metadata = test.load_run(args.run)
    folder = test.RUNS / metadata['run']
    evidence = json.loads((folder/'recovery.json').read_text())
    proposed = plan(evidence, metadata)
    if not args.apply:
        print(json.dumps(proposed, indent=2)); return
    uuid = metadata['volume'].upper()
    device = Path('/dev/disk/by-uuid')/(uuid[:4]+'-'+uuid[4:])
    inventory = json.loads(subprocess.check_output(['lsblk', '--json', '--paths', '--output',
        'NAME,TYPE,TRAN,FSTYPE,UUID,MOUNTPOINTS'], text=True))
    matches = []
    def visit(node, transport=None):
        transport = node.get('tran') or transport
        if (node.get('uuid') or '').replace('-','').lower() == metadata['volume']:
            matches.append((node,transport))
        for child in node.get('children',[]): visit(child,transport)
    for node in inventory['blockdevices']: visit(node)
    if len(matches) != 1 or matches[0][1] != 'usb' or matches[0][0].get('fstype') != 'exfat':
        raise ValueError('Volume is not an unambiguous USB exFAT device')
    if any(matches[0][0].get('mountpoints', [])):
        raise ValueError('Unmount the USB volume before repair')
    # O_EXCL prevents opening a mounted block device, including a mount racing
    # the inventory check. Only this explicitly selected removable partition.
    fd = os.open(device, os.O_RDWR | os.O_EXCL)
    with os.fdopen(fd, 'r+b', buffering=0) as stream:
        if not stat.S_ISBLK(os.fstat(stream.fileno()).st_mode): raise ValueError('Expected block device')
        current = recovery.inspect(stream, metadata)
        for path, item in evidence['entries'].items():
            now = current['entries'].get(path)
            if not now or now['offset'] != item['offset']: raise ValueError('Directory layout changed')
        if current['files'] != evidence['files']: raise ValueError('File contents changed since recovery')
        backup = folder/'repair-plan.json'
        encoded = (json.dumps(proposed, indent=2)+'\n').encode()
        if backup.exists():
            if backup.read_bytes() != encoded: raise ValueError('Existing repair backup differs')
        else:
            test.sync_file(backup, encoded, exclusive=True)
        parent = os.open(folder, os.O_RDONLY | os.O_DIRECTORY)
        try: os.fsync(parent)
        finally: os.close(parent)
        apply(stream, proposed)
    print(f'Repaired {len(proposed["changes"])} directory records; originals saved in {backup}')


if __name__ == '__main__': main()
