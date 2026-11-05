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

import profile_bundle as v1

MAGIC = b'SRVROS-PROFILE2\n'
HEADER = struct.Struct('<16sII')
MAX_MANIFEST = 65536
MAX_FILE = 16 * 1024 * 1024
MAX_TOTAL = 64 * 1024 * 1024
BUFFER = 65536
MAX_BUNDLE = HEADER.size + MAX_MANIFEST + MAX_TOTAL
require = v1.require
ProfileError = v1.ProfileError


def _stamp(fd):
    s = os.fstat(fd)
    require(stat.S_ISREG(s.st_mode) and s.st_nlink == 1, 'only single-link regular files supported')
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
    v1.exact(bundle, ('format', 'schema_version', 'profile', 'preferences', 'documents'), 'bundle')
    require(bundle['format'] == v1.FORMAT and type(bundle['schema_version']) is int and bundle['schema_version'] == 2, 'unsupported profile schema')
    v1.identity(bundle['profile'])
    v1.preferences(bundle['preferences'])
    entries = bundle['documents']
    require(type(entries) is list and len(entries) <= v1.MAX_FILES, 'too many documents')
    paths, aliases, total = set(), {}, 0
    for entry in entries:
        v1.exact(entry, ('path', 'size', 'sha256'), 'document')
        path = entry['path']
        parts = v1.document_path(path)
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
    def __init__(self, stream, stamp, path):
        self.stream, self.stamp, self.path = stream, stamp, Path(os.path.abspath(path))
        require(stamp[2] <= MAX_BUNDLE, 'bundle exceeds size limit')
        prefix = _read_exact(stream, min(len(MAGIC), stamp[2]))
        stream.seek(0)
        self.legacy = prefix != MAGIC
        if self.legacy:
            require(stamp[2] <= v1.MAX_BUNDLE, 'legacy bundle exceeds size limit')
            data = _read_exact(stream, stamp[2])
            self.metadata = v1.decode_json(data)
            self.payloads = v1.validate_bundle(self.metadata)
            self.offsets = {}
        else:
            magic, length, reserved = HEADER.unpack(_read_exact(stream, HEADER.size))
            require(magic == MAGIC and reserved == 0 and 0 < length <= MAX_MANIFEST, 'invalid stream header')
            self.metadata = v1.decode_json(_read_exact(stream, length))
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
        current = self.path.lstat()
        require(stat.S_ISREG(current.st_mode) and (current.st_dev, current.st_ino) == self.stamp[:2], 'source pathname changed')
        require(_stamp(self.stream.fileno()) == self.stamp, 'source changed')
        self.stream.seek(0)
        digest = _hash_range(self.stream, self.stamp[2])
        require(not self.stream.read(1) and _stamp(self.stream.fileno()) == self.stamp, 'source changed')
        return digest

    def copy_document(self, entry, output):
        require(not self.legacy, 'legacy bundle has no streaming offsets')
        self.stream.seek(self.offsets[entry['path']])
        require(_hash_range(self.stream, entry['size'], output) == entry['sha256'], 'document changed during copy')


@contextmanager
def open_bundle(path):
    stream, stamp = _open(path)
    try:
        yield Bundle(stream, stamp, path)
    finally:
        stream.close()


def import_bundle(root, path, alias, uid, gid, checkpoint=lambda _: None):
    with open_bundle(path) as source:
        if source.legacy:
            require(source.recheck() == source.sha256, 'source changed')
            return v1.import_bundle(root, source.metadata, alias, uid, gid, checkpoint)
        local = v1.binding(alias, uid, gid)
        bundle = source.metadata
        identifier = bundle['profile']['id']
        with v1.store_lock(root) as root:
            require(not any((root / 'staging').iterdir()), 'interrupted import; run recover before retry')
            destination = root / 'profiles' / identifier
            require(not os.path.lexists(destination), 'profile identity already exists; nothing replaced')
            for path in (root / 'profiles').iterdir():
                v1.profile_id(path.name)
                require(stat.S_ISDIR(path.lstat().st_mode), 'invalid profile store entry')
                old = v1.local_binding(path)
                require(old['alias'] != alias, 'local account name already exists')
                require(old['uid'] != uid, 'local UID already assigned')
            stage = Path(tempfile.mkdtemp(prefix='import-', dir=root / 'staging'))
            checkpoint('stage')
            home = stage / 'home'
            for directory in ('Documents', '.config/srvros', '.local/share', '.cache'):
                (home / directory).mkdir(mode=0o700, parents=True, exist_ok=True)
            v1.write_new(stage / 'identity.json', v1.encode_json(bundle['profile']))
            v1.write_new(stage / 'binding.json', v1.encode_json(local))
            v1.write_new(home / v1.PREF_PATH, v1.encode_json(bundle['preferences']))
            checkpoint('metadata')
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
            checkpoint('before-publish')
            require(source.recheck() == source.sha256, 'source changed')
            require(not os.path.lexists(destination), 'profile identity already exists')
            os.rename(stage, destination)
            checkpoint('after-publish')
            return {'profile_id': identifier, 'home': str(destination / 'home'), **local}


def export_bundle(root, identifier, documents, output, legacy_desktop=False):
    require(len(documents) <= v1.MAX_FILES and len(set(documents)) == len(documents), 'duplicate or excessive selection')
    with v1.store_lock(root) as root, ExitStack() as stack:
        directory = v1.profile_directory(root, identifier)
        who = v1.identity(v1.decode_json(v1.read_regular(directory / 'identity.json', 4096)))
        require(who['id'] == identifier, 'stored profile identity mismatch')
        home = directory / 'home'
        prefs = (v1.desktop_projection(v1.read_regular(home / 'desktop.conf', 512)) if legacy_desktop else v1.preferences(v1.decode_json(v1.read_regular(home / v1.PREF_PATH, 4096))))
        entries, sources = [], []
        for path in documents:
            v1.document_path(path)
            stream, stamp = _open(home / path)
            stack.enter_context(stream)
            require(stamp[2] <= MAX_FILE, 'invalid document size')
            digest = _hash_range(stream, stamp[2])
            require(_stamp(stream.fileno()) == stamp, 'source changed')
            entries.append({'path': path, 'size': stamp[2], 'sha256': digest})
            sources.append((stream, stamp))
        bundle = {'format': v1.FORMAT, 'schema_version': 2, 'profile': who, 'preferences': prefs, 'documents': entries}
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
    exp.add_argument('--document', action='append', default=[]); exp.add_argument('--legacy', action='store_true'); exp.add_argument('--legacy-desktop', action='store_true')
    args = parser.parse_args(argv)
    try:
        if args.command == 'inspect':
            with open_bundle(args.bundle) as source:
                result = {'profile': source.metadata['profile'], 'preferences': source.metadata['preferences'], 'documents': [e['path'] for e in source.metadata['documents']], 'bytes': sum(e['size'] for e in source.metadata['documents']), 'sha256': source.sha256, 'bundle_version': 1 if source.legacy else 2}
        elif args.command == 'import':
            result = import_bundle(args.root, args.bundle, args.alias, args.uid, args.gid)
        else:
            if args.legacy:
                bundle = v1.export_bundle(args.root, args.profile_id, args.document, args.legacy_desktop)
                v1.publish_bundle(args.output, bundle)
            else:
                export_bundle(args.root, args.profile_id, args.document, args.output, args.legacy_desktop)
            result = {'exported': args.output, 'documents': args.document}
        print(json.dumps(result, indent=2, sort_keys=True)); return 0
    except (ProfileError, OSError, struct.error) as error:
        print('profile: ' + str(error), file=sys.stderr); return 1


if __name__ == '__main__':
    sys.exit(main())
