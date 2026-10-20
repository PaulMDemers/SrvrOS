#!/usr/bin/env python3
"""Exercise the real nested filesystem lease; prove all outside bytes survive."""
from pathlib import Path
import argparse
import re
import struct
import tempfile
from battery_filesystem_image import create, verify, SIZE, ANCHOR
from exfat_test_image import ExfatFixture, check_clean_image
from exfat_hardening_test import command, format_image, DRIVER
from exfat_scratch_test import checksum

ROOT = Path(__file__).resolve().parents[1]
IMAGE_DRIVER = ROOT/'build/tests/workspace-image-host'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--flush-only',action='store_true',help='run new flush faults after an unchanged write-boundary run')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='srvros-image-') as directory:
        tmp = Path(directory)
        inner = tmp/'inner.exfat'
        create(inner)
        original_inner = inner.read_bytes()
        # Exclusive creation refuses a pre-existing file, including symlinks.
        try:
            create(inner)
            raise AssertionError('replaced fixture')
        except FileExistsError:
            assert inner.read_bytes() == original_inner
        volume = tmp/'outer.exfat'
        format_image(volume, '128K')
        fs = ExfatFixture(volume.read_bytes())
        fs.add_directory_files('srvros-work', {'anchor.txt':b'unchanged outer file',
                                             'battery-fs.img':original_inner})
        baseline = bytes(fs.data)
        entry = dict(fs.entries(fs.read('srvros-work')))['battery-fs.img']
        start_cluster = struct.unpack_from('<I', entry, 52)[0]
        offset = fs.offset(start_cluster)
        entry_at = baseline.index(entry, fs.offset(fs.root))
        volume.write_bytes(baseline)
        extent_check=command(DRIVER,volume,'image-range')
        assert f'image-range: PASS offset={offset} bytes={SIZE}' in extent_check
        assert volume.read_bytes()==baseline
        output = command(IMAGE_DRIVER, volume, 'workload')
        total = int(re.search(r'writes=(\d+)', output)[1])
        total_flushes = int(re.search(r'flushes=(\d+)', output)[1])
        data = volume.read_bytes()
        assert data[:offset] == baseline[:offset] and data[offset+SIZE:] == baseline[offset+SIZE:]
        verify(data[offset:offset+SIZE], 1)
        check_clean_image(volume)
        command(DRIVER, volume, 'allocation-check')
        inner.write_bytes(data[offset:offset+SIZE]); check_clean_image(inner)
        command(DRIVER, inner, 'allocation-check')
        print(f'image workload: PASS, {total} write boundaries', flush=True)
        # Contiguous NoFatChain and a contiguous FAT chain are both valid.
        contiguous = bytearray(baseline); contiguous[entry_at+33] |= 2
        checksum(contiguous,entry_at); volume.write_bytes(contiguous)
        command(IMAGE_DRIVER,volume,'workload')
        # Refuse dirty state, invalid initialized length, aliases and fragmentation.
        for fault in ('outer-dirty','inner-dirty','valid-length','size','alias','fragmented','bitmap'):
            changed = bytearray(baseline)
            if fault == 'outer-dirty':struct.pack_into('<H',changed,106,2)
            elif fault == 'inner-dirty':struct.pack_into('<H',changed,offset+106,2)
            elif fault in ('valid-length','size'):
                struct.pack_into('<Q',changed,entry_at+(40 if fault=='valid-length' else 56),SIZE-512)
                checksum(changed,entry_at)
            elif fault == 'alias':
                anchor = dict(fs.entries(fs.read('srvros-work')))['anchor.txt']
                at = baseline.index(anchor,fs.offset(fs.root))
                changed[at+40:at+64] = changed[entry_at+40:entry_at+64]
                checksum(changed,at)
            elif fault == 'fragmented':
                # Same live clusters, valid chain, noncontiguous order.
                for cluster,nxt in ((start_cluster,start_cluster+2),(start_cluster+2,start_cluster+1),(start_cluster+1,start_cluster+3)):
                    struct.pack_into('<I',changed,fs.fat+cluster*4,nxt)
            else:
                root = fs.chain(fs.root)
                bitmap = next(root[i:i+32] for i in range(0,len(root),32) if root[i]==0x81)
                bit = start_cluster-2
                at = fs.offset(struct.unpack_from('<I',bitmap,20)[0])+bit//8
                changed[at] &= ~(1<<(bit%8))
            volume.write_bytes(changed)
            command(IMAGE_DRIVER,volume,'refuse')
            if fault!='inner-dirty':command(DRIVER,volume,'image-range',codes=(1,))
            assert volume.read_bytes() == changed,fault
        print('image admission: dirty/length/alias/fragmentation/bitmap refusals PASS',flush=True)
        for mode in (('flush-failure',) if args.flush_only else ('failure','power-cut','flush-failure')):
            boundaries=total_flushes if mode=='flush-failure' else total
            for boundary in range(1,boundaries+1):
                volume.write_bytes(baseline)
                command(IMAGE_DRIVER,volume,'workload',boundary if mode=='failure' else 0,
                        boundary if mode=='power-cut' else 0,boundary if mode=='flush-failure' else 0,codes=(77,) if mode=='power-cut' else (0,))
                data = volume.read_bytes()
                assert data[:offset]==baseline[:offset] and data[offset+SIZE:]==baseline[offset+SIZE:],(mode,boundary)
                # Outer allocation and file contents remain completely unchanged.
                check_clean_image(volume)
                command(DRIVER,volume,'allocation-check')
                inner.write_bytes(data[offset:offset+SIZE])
                command('/usr/sbin/fsck.exfat','-y','-s',inner,codes=(0,1))
                check_clean_image(inner)
                command(DRIVER,inner,'allocation-check')
                assert ExfatFixture(inner.read_bytes()).read('fixture/anchor.txt') == ANCHOR
            print(f'image {mode}: all {boundaries} boundaries confined; outer clean; inner recovery PASS',flush=True)
    print('workspace-image: PASS',flush=True)


if __name__ == '__main__':main()
