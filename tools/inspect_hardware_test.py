#!/usr/bin/env python3
"""Read-only recovery of a test report whose exFAT directory metadata is invalid.

Reads only the root directory, the named run's four files, and SrvrOS metadata.
Never repairs or writes the volume. JSON goes to stdout for an unprivileged shell
redirect to a local evidence file. This does not certify the filesystem as clean.
"""
import argparse
import json
from pathlib import Path
import struct

import hardware_test as test


def checksum(data, skip=()):
    value = 0
    for i, byte in enumerate(data):
        if i not in skip:
            value = (((value >> 1) | ((value & 1) << 15)) + byte) & 65535
    return value


class Reader:
    def __init__(self, stream, volume):
        self.stream = stream
        self.size = 512
        boot = self.read(0, 512)
        if boot[3:11] != b'EXFAT   ' or boot[510:] != b'\x55\xaa':
            raise ValueError('Invalid exFAT boot sector')
        if f'{struct.unpack_from("<I", boot, 100)[0]:08x}' != volume:
            raise ValueError('Volume identity mismatch')
        if not 9 <= boot[108] <= 12 or boot[108] + boot[109] > 25:
            raise ValueError('Invalid sector/cluster geometry')
        self.sector = 1 << boot[108]
        self.cluster = self.sector << boot[109]
        self.size = struct.unpack_from('<Q', boot, 72)[0] * self.sector
        self.fat = struct.unpack_from('<I', boot, 80)[0] * self.sector
        self.heap = struct.unpack_from('<I', boot, 88)[0] * self.sector
        self.count, self.root = struct.unpack_from('<II', boot, 92)

    def read(self, offset, size):
        if offset < 0 or size < 0 or offset + size > self.size or size > 4*1024*1024:
            raise ValueError('Read outside bounded volume range')
        self.stream.seek(offset)
        data = self.stream.read(size)
        if len(data) != size: raise ValueError('Short read')
        return data

    def chunks(self, cluster, size=None, contiguous=False):
        seen, used = set(), 0
        if size is not None and size > 4*1024*1024: raise ValueError('Oversized stream')
        while cluster < 0xfffffff8 and (size is None or used < size):
            if not 2 <= cluster < self.count+2 or cluster in seen:
                raise ValueError('Invalid or looping cluster chain')
            seen.add(cluster)
            length = self.cluster if size is None else min(self.cluster, size-used)
            used += length
            if used > 4*1024*1024: raise ValueError('Directory exceeds recovery bound')
            offset = self.heap + (cluster-2)*self.cluster
            yield offset, self.read(offset, length)
            if size is not None and used == size: return
            cluster = cluster+1 if contiguous else struct.unpack('<I', self.read(self.fat+cluster*4, 4))[0]
        if size is not None and used != size: raise ValueError('Incomplete cluster chain')

    def directory(self, first, size=None, contiguous=False):
        for base, data in self.chunks(first, size, contiguous):
            offset = 0
            while offset + 32 <= len(data):
                if data[offset] == 0: return
                if data[offset] != 0x85:
                    offset += 32; continue
                length = (data[offset+1]+1)*32
                entry = data[offset:offset+length]
                if len(entry) != length or length < 96 or entry[32] != 0xc0:
                    raise ValueError('Unsupported or truncated entry set')
                names = b''.join(entry[i+2:i+32] for i in range(64, length, 32) if entry[i] == 0xc1)
                name = names[:entry[35]*2].decode('utf-16le')
                yield name, base+offset, entry
                offset += length

    @staticmethod
    def stream_info(entry):
        return struct.unpack_from('<I', entry, 52)[0], struct.unpack_from('<Q', entry, 56)[0], bool(entry[33]&2)


def inspect(stream, metadata):
    reader = Reader(stream, metadata['volume'])
    output = {'run': metadata['run'], 'volume': metadata['volume'],
              'filesystem_validation': 'NOT_PASSED', 'entries': {}, 'files': {}}
    def record(path, offset, entry):
        name = path.rsplit('/', 1)[-1]
        # Only ASCII entries are expected for these explicitly named files.
        expected_hash = checksum(name.upper().encode('utf-16le')) if name.isascii() else None
        output['entries'][path] = {
            'offset': offset, 'raw_hex': entry.hex(), 'flags': entry[33],
            'name_hash': struct.unpack_from('<H', entry, 36)[0], 'expected_name_hash': expected_hash,
            'set_checksum': struct.unpack_from('<H', entry, 2)[0],
            'expected_set_checksum': checksum(entry, (2,3))}
    targets = {metadata['run']+suffix for suffix in ('.req','.probe','.txt','.done')}
    for name, offset, entry in reader.directory(reader.root):
        if name not in ('srvros-tests', '.srvros'): continue
        if not struct.unpack_from('<H', entry, 4)[0] & 0x10: raise ValueError('Expected directory')
        record(name, offset, entry)
        for child, position, item in reader.directory(*reader.stream_info(entry)):
            if name == 'srvros-tests' and child not in targets: continue
            path = name+'/'+child
            record(path, position, item)
            first, size, contiguous = reader.stream_info(item)
            if struct.unpack_from('<H', item, 4)[0] & 0x10 or size > 65536: continue
            data = b''.join(data for _, data in reader.chunks(first, size, contiguous))
            output['files'][path] = {'data_hex': data.hex(), 'text': data.decode('ascii', errors='replace')}
    prefix = 'srvros-tests/'+metadata['run']
    try:
        report = bytes.fromhex(output['files'][prefix+'.txt']['data_hex'])
        done = bytes.fromhex(output['files'][prefix+'.done']['data_hex'])
        fields = test.validate_result(metadata, report, done)
        probe = bytes.fromhex(output['files'][prefix+'.probe']['data_hex'])
        if fields['usb_write_flush_read'] == 'PASS' and probe != test.PROBE_BYTES:
            raise ValueError('Probe mismatch')
        output['recovered_report'] = fields
        output['recovered_payload_checksum_valid'] = True
    except (KeyError, ValueError) as error:
        output['recovery_error'] = str(error)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', required=True)
    args = parser.parse_args()
    metadata = test.load_run(args.run)
    volume = metadata['volume'].upper()
    device = Path('/dev/disk/by-uuid') / (volume[:4]+'-'+volume[4:])
    # Python rb opens O_RDONLY. No device mutation or repair API is provided.
    with device.open('rb', buffering=0) as stream:
        result = inspect(stream, metadata)
    print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
