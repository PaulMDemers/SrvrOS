#!/usr/bin/env python3
"""Capture source identities and worktree changes, including nested submodules."""
import hashlib
import os
from pathlib import Path
import subprocess


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def capture(root):
    root = Path(root).resolve()
    records = {}

    def visit(repo, name):
        status = git(repo, 'status', '--porcelain=v1', '-z', '--ignore-submodules=none')
        untracked = {}
        for item in git(repo, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0'):
            if not item:
                continue
            relative = os.fsdecode(item)
            path = repo / relative
            # Hash symlink targets as links, rather than following them outside the tree.
            data = os.fsencode(os.readlink(path)) if path.is_symlink() else path.read_bytes()
            untracked[relative] = {'sha256': hashlib.sha256(data).hexdigest(),
                                   'symlink': path.is_symlink()}
        records[name] = {
            'head': git(repo, 'rev-parse', 'HEAD').decode().strip(),
            'dirty': bool(status),
            'status': os.fsdecode(status),
            'tracked_diff_sha256': hashlib.sha256(git(repo, 'diff', 'HEAD', '--binary',
                                                       '--ignore-submodules=none')).hexdigest(),
            'untracked': untracked,
        }
        for entry in git(repo, 'ls-files', '--stage', '-z').split(b'\0'):
            if not entry:
                continue
            meta, path = entry.split(b'\t', 1)
            mode, commit, stage = meta.split(b' ')
            if mode != b'160000' or stage != b'0':
                continue
            relative = os.fsdecode(path)
            child = repo / relative
            child_name = relative if name == '.' else f'{name}/{relative}'
            if not (child / '.git').exists():
                records[child_name] = {'initialized': False, 'recorded_head': commit.decode()}
            else:
                visit(child, child_name)

    visit(root, '.')
    return records
