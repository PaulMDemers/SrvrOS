#!/usr/bin/env python3
"""Prepare an opt-in persistent USB workspace and stage its RAM-root boot profile."""
import argparse
import json
from pathlib import Path
import os
import hardware_boot as boot
import hardware_test as host

README = b'''SrvrOS persistent USB workspace

At the monitor: workspace shell
History is saved on shell exit; use history -w and sync to save it earlier.
Settings are read from .profile in this directory.
Try: sh welcome.sh
Save documents/scripts here. /fat remains in RAM.
Before removing the drive: sync, exit, then workspace unmount at the monitor.
The whole USB volume is visible at /work; other files there are writable too.
'''
PROFILE = b'# Workspace shell settings; loaded only by explicit workspace startup.\nHISTSIZE=128\n'
WELCOME = b'echo "Hello from the persistent USB workspace"\n'


def prepare(uuid):
    volume = host.volume_id(uuid)
    mount = host.volume_mount(volume)
    from hardware_log_archive import require_headroom
    require_headroom(mount)
    folder = mount/'srvros-work'
    if folder.is_symlink() or (folder.exists() and not folder.is_dir()):
        raise ValueError('Workspace path is not a regular directory')
    marker = folder/'.workspace'
    expected = f'SRVROS-WORKSPACE-V1 {volume}\n'.encode()
    if marker.is_symlink() or (marker.exists() and (not marker.is_file() or marker.stat().st_size != len(expected) or marker.read_bytes() != expected)):
        raise ValueError('Existing workspace marker is invalid; nothing was replaced')
    # Verify build inputs before changing the selected USB directory.
    release = boot.stage(workspace_volume=volume)
    folder.mkdir(exist_ok=True)
    if not marker.exists(): host.sync_file(marker, expected, exclusive=True)
    for name, data in (('README.txt',README),('welcome.sh',WELCOME),('.profile',PROFILE)):
        path = folder/name
        if path.is_symlink(): raise ValueError('Symlink in workspace preparation path')
        if not path.exists(): host.sync_file(path,data,exclusive=True)
    os.sync()
    destination = boot.ROOT/'build/workspace'
    destination.mkdir(parents=True,exist_ok=True)
    metadata = {'volume':volume,'release':release,'boot_manifest':boot.verify(boot.STAGE/release)}
    host.sync_file(destination/'manifest.json',(json.dumps(metadata,indent=2)+'\n').encode())
    print(f'Prepared workspace: {folder}')
    print(f'Staged workspace release: {release}')
    print(f'Install: sudo python3 tools/hardware_boot.py install --release {release}')
    return metadata


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uuid',required=True)
    args=parser.parse_args()
    try: prepare(args.uuid)
    except (OSError,ValueError,RuntimeError) as error: parser.exit(1,f'workspace: {error}\n')
