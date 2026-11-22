#!/usr/bin/env python3
"""Streaming portable profile v2 codec for explicitly initialized host fixtures."""
import argparse
from contextlib import contextmanager, ExitStack
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import sys
import tempfile

import profile_bundle as p

MAGIC = b'SRVROS-PROFILE2\n'
HEADER = struct.Struct('<16sII')
MAX_MANIFEST = 65536
MAX_FILE = 16 * 1024 * 1024
MAX_TOTAL = 64 * 1024 * 1024
BUFFER = 65536
MAX_BUNDLE = HEADER.size + MAX_MANIFEST + MAX_TOTAL
require = p.require
ProfileError = p.ProfileError


def _stamp(fd, expected_links=1):
    s = os.fstat(fd)
    message = 'only single-link regular files supported' if expected_links == 1 else 'invalid regular file link count'
    require(stat.S_ISREG(s.st_mode) and s.st_nlink == expected_links, message)
    return (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)


def _open(path):
    path = Path(os.path.abspath(path))
    for parent in reversed(path.parents):
        require(stat.S_ISDIR(parent.lstat().st_mode), 'symlink or non-directory parent')
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        stamp = _stamp(fd)
        return os.fdopen(fd, 'rb', buffering=0), stamp
    except BaseException:
        os.close(fd)
        raise


def _read_exact(stream, count):
    chunks = []
    while count:
        chunk = stream.read(min(BUFFER, count))
        require(chunk, 'truncated bundle')
        chunks.append(chunk)
        count -= len(chunk)
    return b''.join(chunks)


def _write_all(stream, data):
    view = memoryview(data)
    while view:
        written = stream.write(view)
        require(type(written) is int and 0 < written <= len(view), 'short or failed write')
        view = view[written:]


def _hash_range(stream, count, output=None):
    digest = hashlib.sha256()
    while count:
        data = stream.read(min(BUFFER, count))
        require(data, 'truncated bundle')
        digest.update(data)
        if output is not None:
            _write_all(output, data)
        count -= len(data)
    return digest.hexdigest()


def validate_manifest(bundle):
    p.exact(bundle, ('format', 'schema_version', 'profile', 'preferences', 'documents'), 'bundle')
    require(bundle['format'] == p.FORMAT and type(bundle['schema_version']) is int and bundle['schema_version'] == 2, 'unsupported profile schema')
    p.identity(bundle['profile'])
    p.preferences(bundle['preferences'])
    entries = bundle['documents']
    require(type(entries) is list and len(entries) <= p.MAX_FILES, 'too many documents')
    paths, aliases, total = set(), {}, 0
    for entry in entries:
        p.exact(entry, ('path', 'size', 'sha256'), 'document')
        path = entry['path']
        parts = p.document_path(path)
        require(path not in paths, 'duplicate document')
        paths.add(path)
        for n in range(1, len(parts) + 1):
            prefix = '/'.join(parts[:n]); folded = prefix.lower()
            require(folded not in aliases or aliases[folded] == prefix, 'case collision')
            aliases[folded] = prefix
        require(type(entry['size']) is int and 0 <= entry['size'] <= MAX_FILE, 'invalid document size')
        require(type(entry['sha256']) is str and re.fullmatch(r'[0-9a-f]{64}', entry['sha256']) is not None, 'invalid document digest')
        total += entry['size']
        require(total <= MAX_TOTAL, 'profile exceeds total size limit')
    for path in paths:
        require(not any('/'.join(path.split('/')[:n]) in paths for n in range(1, len(path.split('/')))), 'file/directory collision')
    return total


class Bundle:
    """Validated owned source; offsets and sha256 describe the exact open file."""
    def __init__(self, stream, stamp, path, expected_links=1, require_canonical=False):
        self.stream, self.stamp = stream, stamp
        self.path = Path(os.path.abspath(path)) if path is not None else None
        self.expected_links = expected_links
        self.require_canonical = require_canonical
        require(stamp[2] <= MAX_BUNDLE, 'bundle exceeds size limit')
        stream.seek(0)
        magic, length, reserved = HEADER.unpack(_read_exact(stream, HEADER.size))
        require(magic == MAGIC and reserved == 0 and 0 < length <= MAX_MANIFEST, 'invalid stream header')
        manifest = _read_exact(stream, length)
        self.metadata = p.decode_json(manifest)
        if self.require_canonical:
            canonical = json.dumps(self.metadata, sort_keys=True, separators=(',', ':'),
                                   ensure_ascii=True).encode('utf-8')
            require(manifest == canonical, 'noncanonical stream manifest')
        total = validate_manifest(self.metadata)
        require(stamp[2] == HEADER.size + length + total, 'bundle length mismatch')
        self.offsets = {}
        offset = HEADER.size + length
        for entry in self.metadata['documents']:
            self.offsets[entry['path']] = offset
            require(_hash_range(stream, entry['size']) == entry['sha256'], 'document integrity mismatch')
            offset += entry['size']
        require(not stream.read(1), 'trailing bundle data')
        self.sha256 = self.recheck()

    def recheck(self):
        if self.path is not None:
            current = self.path.lstat()
            require(stat.S_ISREG(current.st_mode) and (current.st_dev, current.st_ino) == self.stamp[:2], 'source pathname changed')
        require(_stamp(self.stream.fileno(), self.expected_links) == self.stamp, 'source changed')
        self.stream.seek(0)
        digest = _hash_range(self.stream, self.stamp[2])
        require(not self.stream.read(1) and _stamp(self.stream.fileno(), self.expected_links) == self.stamp, 'source changed')
        return digest

    def copy_document(self, entry, output):
        self.stream.seek(self.offsets[entry['path']])
        require(_hash_range(self.stream, entry['size'], output) == entry['sha256'], 'document changed during copy')


@contextmanager
def open_bundle(path, require_canonical=False):
    stream, stamp = _open(path)
    try:
        yield Bundle(stream, stamp, path, 1, require_canonical)
    finally:
        stream.close()


@contextmanager
def open_bundle_fd(fd, expected_links=1, require_canonical=False):
    """Validate an already pinned, no-follow regular file descriptor."""
    owned = os.fdopen(os.dup(fd), 'rb', buffering=0)
    try:
        owned.seek(0)
        stamp = _stamp(owned.fileno(), expected_links)
        yield Bundle(owned, stamp, None, expected_links, require_canonical)
    finally:
        owned.close()


def import_bundle(root, path, alias, uid, gid, checkpoint=lambda _: None):
    with open_bundle(path) as source:
        local = p.binding(alias, uid, gid)
        bundle = source.metadata
        def populate(home):
            for entry in bundle['documents']:
                target = home / entry['path']
                target.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
                with open(target, 'xb') as out:
                    os.chmod(target, 0o600)
                    source.copy_document(entry, out)
                    out.flush(); os.fsync(out.fileno())
                with open(target, 'rb', buffering=0) as copied:
                    require(_stamp(copied.fileno())[2] == entry['size'] and
                            _hash_range(copied, entry['size']) == entry['sha256'] and
                            not copied.read(1), 'copied document integrity mismatch')
                checkpoint('document:' + entry['path'])
        return p.install_profile(root, bundle['profile'], bundle['preferences'], local,
                                  populate, checkpoint,
                                  verify_source=lambda: require(source.recheck() == source.sha256, 'source changed'))


def export_bundle(root, identifier, documents, output, legacy_desktop=False):
    require(len(documents) <= p.MAX_FILES and len(set(documents)) == len(documents), 'duplicate or excessive selection')
    with p.store_lock(root) as root, ExitStack() as stack:
        directory = p.profile_directory(root, identifier)
        who = p.identity(p.decode_json(p.read_regular(directory / 'identity.json', 4096)))
        require(who['id'] == identifier, 'stored profile identity mismatch')
        home = directory / 'home'
        prefs = (p.desktop_projection(p.read_regular(home / 'desktop.conf', 512)) if legacy_desktop else p.preferences(p.decode_json(p.read_regular(home / p.PREF_PATH, 4096))))
        entries, sources = [], []
        for path in documents:
            p.document_path(path)
            stream, stamp = _open(home / path)
            stack.enter_context(stream)
            require(stamp[2] <= MAX_FILE, 'invalid document size')
            digest = _hash_range(stream, stamp[2])
            require(_stamp(stream.fileno()) == stamp, 'source changed')
            entries.append({'path': path, 'size': stamp[2], 'sha256': digest})
            sources.append((stream, stamp))
        bundle = {'format': p.FORMAT, 'schema_version': 2, 'profile': who, 'preferences': prefs, 'documents': entries}
        validate_manifest(bundle)
        manifest = json.dumps(bundle, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode('utf-8')
        require(len(manifest) <= MAX_MANIFEST, 'manifest exceeds size limit')
        with open(output, 'xb') as out:
            os.chmod(output, 0o600)
            _write_all(out, HEADER.pack(MAGIC, len(manifest), 0)); _write_all(out, manifest)
            for entry, (stream, stamp) in zip(entries, sources):
                require(_stamp(stream.fileno()) == stamp, 'source changed')
                stream.seek(0)
                require(_hash_range(stream, entry['size'], out) == entry['sha256'] and _stamp(stream.fileno()) == stamp, 'source changed')
            for entry, (stream, stamp) in zip(entries, sources):
                require(_stamp(stream.fileno()) == stamp, 'source changed')
                stream.seek(0)
                require(_hash_range(stream, entry['size']) == entry['sha256'] and
                        _stamp(stream.fileno()) == stamp, 'source changed')
            out.flush(); os.fsync(out.fileno())
    return bundle


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    inspect = commands.add_parser('inspect'); inspect.add_argument('bundle')
    imp = commands.add_parser('import'); imp.add_argument('root'); imp.add_argument('bundle')
    for option in ('alias', 'uid', 'gid'):
        imp.add_argument('--' + option, required=True, type=int if option != 'alias' else str)
    exp = commands.add_parser('export'); exp.add_argument('root'); exp.add_argument('profile_id'); exp.add_argument('output')
    exp.add_argument('--document', action='append', default=[]); exp.add_argument('--legacy-desktop', action='store_true')
    args = parser.parse_args(argv)
    try:
        if args.command == 'inspect':
            with open_bundle(args.bundle) as source:
                result = {'profile': source.metadata['profile'], 'preferences': source.metadata['preferences'], 'documents': [e['path'] for e in source.metadata['documents']], 'bytes': sum(e['size'] for e in source.metadata['documents']), 'sha256': source.sha256, 'bundle_version': 2}
        elif args.command == 'import':
            result = import_bundle(args.root, args.bundle, args.alias, args.uid, args.gid)
        else:
            export_bundle(args.root, args.profile_id, args.document, args.output, args.legacy_desktop)
            result = {'exported': args.output, 'documents': args.document}
        print(json.dumps(result, indent=2, sort_keys=True)); return 0
    except (ProfileError, OSError, struct.error) as error:
        print('profile: ' + str(error), file=sys.stderr); return 1


if __name__ == '__main__':
    sys.exit(main())
