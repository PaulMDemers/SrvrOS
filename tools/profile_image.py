#!/usr/bin/env python3
"""Provision a new offline exFAT image copy from a validated profile bundle.

The source must be a disposable, offline regular image, never an installed or
mounted image. The driver writes only an unpublished private staging copy.
"""
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tempfile

import profile_bundle as p


def validate_request(bundle, home, alias, uid, gid, machine_preferences=None):
    payloads = p.validate_bundle(bundle)
    local = p.binding(alias, uid, gid)
    p.require(type(home) is str and len(home) <= 95 and
              re.fullmatch(r'/fat/home/[A-Za-z0-9][A-Za-z0-9_-]{0,63}', home) is not None,
              'invalid local profile home')
    if machine_preferences is not None:
        p.require(type(machine_preferences) is bytes, 'invalid machine preferences')
        p.desktop_projection(machine_preferences)
    files = {
        home + '/profile.id': b'srvros-profile-v1\n' + bundle['profile']['id'].encode('ascii') + b'\n',
        home + '/profile-local.json': p.encode_json(local),
        home + '/' + p.PREF_PATH: p.encode_json(bundle['preferences']),
    }
    files.update({home + '/' + name: data for name, data in payloads.items()})
    directories = {home, home + '/Documents', home + '/.config', home + '/.config/srvros',
                   home + '/.local', home + '/.local/share', home + '/.cache'}
    for name in files:
        parent = name.rsplit('/', 1)[0]
        while parent.startswith(home + '/'):
            directories.add(parent)
            parent = parent.rsplit('/', 1)[0]
    # Reserve space in each document directory for an application save temp.
    for name in [*files, *directories]:
        p.require(len(name.encode('ascii')) <= 159, 'expanded path exceeds guest limit')
    for name in files:
        p.require(len(name.rsplit('/', 1)[0].encode('ascii')) + 40 <= 159,
                  'directory leaves no room for save temporary')
    return {'profile_id': bundle['profile']['id'], 'home': home, **local}, files, directories


def provision(source_exfat, target_exfat, bundle, home, alias, uid, gid, driver,
              machine_preferences=None):
    metadata, files, directories = validate_request(bundle, home, alias, uid, gid, machine_preferences)
    source = Path(os.path.abspath(source_exfat))
    target = Path(os.path.abspath(target_exfat))
    for parent in (source.parent, target.parent):
        p.require(all(stat.S_ISDIR(path.lstat().st_mode) for path in (parent, *parent.parents)),
                  'invalid image parent')
    p.require(not os.path.lexists(target), 'target already exists')
    source_fd = os.open(source, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(source_fd)
        p.require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1 and before.st_size > 0 and before.st_size % 512 == 0,
                  'source must be a single-link regular exFAT image')
        with tempfile.TemporaryDirectory(prefix='.profile-image-', dir=target.parent) as temp:
            stage = Path(temp) / 'image'
            with os.fdopen(os.dup(source_fd), 'rb') as src, open(stage, 'xb') as dst:
                shutil.copyfileobj(src, dst, 1024 * 1024)
                dst.flush()
                os.fsync(dst.fileno())
            after = os.fstat(source_fd)
            p.require((before.st_size, before.st_mtime_ns, before.st_ctime_ns) ==
                      (after.st_size, after.st_mtime_ns, after.st_ctime_ns), 'source changed during copy')
            def run(operation, path, data=None):
                args = [os.fspath(driver), os.fspath(stage), operation, path]
                if data is not None:
                    content = Path(temp) / 'payload'
                    with open(content, 'wb') as stream:
                        stream.write(data)
                    args.append(os.fspath(content))
                subprocess.run(args, check=True)
            run('ensure-home', '/fat/home')
            for directory in sorted(directories, key=lambda value: (value.count('/'), value)):
                run('mkdir', directory)
            for name, data in files.items():
                run('write', name, data)
            if machine_preferences is not None:
                run('write-desktop', '/fat/home/desktop.conf', machine_preferences)
            run('check-clean', '/fat/home')
            os.link(stage, target)  # Atomic, exclusive publication on the same filesystem.
    finally:
        os.close(source_fd)
    return {'target': str(target), **metadata}
