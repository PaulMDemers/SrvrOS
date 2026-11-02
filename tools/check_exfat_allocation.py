#!/usr/bin/env python3
"""Run SrvrOS's allocation checker read-only on an unmounted USB exFAT partition.

Complements fsck.exfat -n: that command alone can report clean with orphan
allocations. No repair, mount, unmount or write operation is performed here.
The check uses SrvrOS's supported filesystem limits; refusal is not a pass.
Build first with: make build/tests/exfat-driver
"""
import argparse
import json
import os
from pathlib import Path
import stat
import subprocess

from hardware_test import volume_id
from inspect_hardware_test import Reader

ROOT = Path(__file__).resolve().parents[1]


def select_partition(inventory, volume):
    matches = []

    def visit(node, transport=None):
        transport = node.get('tran') or transport
        if (node.get('uuid') or '').replace('-', '').lower() == volume:
            matches.append((node, transport))
        for child in node.get('children', []):
            visit(child, transport)

    for node in inventory['blockdevices']:
        visit(node)
    if len(matches) != 1:
        raise ValueError('Expected exactly one matching UUID')
    node, transport = matches[0]
    if transport != 'usb' or node.get('fstype') != 'exfat' or node.get('type') != 'part':
        raise ValueError('Expected a USB exFAT partition')
    if any(node.get('mountpoints') or []):
        raise ValueError('Unmount the partition before allocation checking')
    return node['path']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uuid', required=True)
    parser.add_argument('--battery-image', action='store_true',
                        help='also require the exclusive contiguous 16 MiB battery-fs.img extent')
    parser.add_argument("--require-headroom",action="store_true",help="Also require 32 file and 8 directory slots plus metadata reserve")
    parser.add_argument('--require-writable', action='store_true',
                        help='also require the production importer to authorize writes, without writing')
    args = parser.parse_args()
    if sum((args.battery_image, args.require_headroom, args.require_writable)) > 1:
        parser.error("Use separate extent, headroom and writable checks")
    try:
        volume = volume_id(args.uuid)
        inventory = json.loads(subprocess.check_output([
            'lsblk', '--json', '--tree', '--output', 'PATH,TYPE,TRAN,UUID,FSTYPE,MOUNTPOINTS'], text=True))
        device = Path(select_partition(inventory, volume))
        driver = ROOT / 'build/tests/exfat-driver'
        if not driver.is_file():
            raise ValueError('Build first: make build/tests/exfat-driver')
        with device.open('rb', buffering=0) as stream:
            if not stat.S_ISBLK(os.fstat(stream.fileno()).st_mode):
                raise ValueError('Expected a block device')
            Reader(stream, volume)  # Independently verify on-disk UUID and geometry.
        return subprocess.run([str(driver), str(device),
                               'image-range' if args.battery_image else 'headroom-check' if args.require_headroom else 'writable-check' if args.require_writable else 'allocation-check']).returncode
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f'allocation-check: NOT CHECKED: {error}')
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
