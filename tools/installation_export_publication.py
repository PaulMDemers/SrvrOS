"""Owned host publication of a fixed v1 export snapshot; no source recovery.

The private record is an integrity/ownership journal for cooperative offline
operators, not authentication. Inspect never writes or flushes. Reconcile may
publish/sync exact recorded bytes and retire their recorded temporary alias;
it never rereads the source or replaces an existing destination.
"""
from contextlib import contextmanager
import fcntl
import hashlib
import os
from pathlib import Path
import re
import stat
import uuid

import installation_image as image
import installation_inspection as inspection
import profile_bundle as p

FORMAT = 'srvros-export-publication-v1'
MAX_RECORD = 65536
CHECKPOINTS = ('recorded', 'partial-write', 'written', 'file-synced', 'linked',
               'unlinked', 'directory-synced', 'acknowledged')
FINISH_CHECKPOINTS = CHECKPOINTS[3:]
NAMES = {'.lock', 'intent.json', 'intent.sha256', 'bundle.partial', 'complete.json'}


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def identity(st):
    return {'device': st.st_dev, 'inode': st.st_ino}


def stable(st):
    return (st.st_dev, st.st_ino, st.st_size, st.st_nlink, st.st_mtime_ns, st.st_ctime_ns)


def valid_identity(value):
    p.exact(value, ('device', 'inode'), 'file identity')
    require(all(type(n) is int and 0 <= n < 2**64 for n in value.values()), 'invalid file identity')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def valid_hash(value):
    require(type(value) is str and re.fullmatch('[0-9a-f]{64}', value), 'invalid hash')


def named(fd, name):
    try:
        return os.stat(name, dir_fd=fd, follow_symlinks=False)
    except FileNotFoundError:
        return None


def write(fd, data):
    require(os.write(fd, data) == len(data), 'short publication write')


def write_new(fd, name, data):
    target = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                     0o600, dir_fd=fd)
    try:
        write(target, data)
        os.fsync(target)
    finally:
        os.close(target)


def read_regular(directory, name, limit, *, links=1, writable=False):
    flags = os.O_RDWR if writable else os.O_RDONLY
    fd = os.open(name, flags | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
    try:
        before = os.fstat(fd)
        require(stat.S_ISREG(before.st_mode) and before.st_uid == os.geteuid() and
                before.st_nlink == links and before.st_size <= limit, 'invalid publication file: ' + name)
        raw = bytearray()
        while len(raw) <= limit:
            chunk = os.read(fd, min(65536, limit + 1 - len(raw)))
            if not chunk:
                break
            raw.extend(chunk)
        require(len(raw) == before.st_size and stable(os.fstat(fd)) == stable(before) and
                stable(os.stat(name, dir_fd=directory, follow_symlinks=False)) == stable(before),
                'publication file changed: ' + name)
        return bytes(raw), before
    finally:
        os.close(fd)


@contextmanager
def locked(path, *, exclusive=False, create=False):
    path = image.checked_path(path)
    if create:
        path.mkdir(mode=0o700)
    root = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    lock = None
    try:
        st = os.fstat(root)
        require(stat.S_ISDIR(st.st_mode) and st.st_uid == os.geteuid() and
                stat.S_IMODE(st.st_mode) == 0o700, 'publication directory must be private and owned')
        flags = os.O_RDWR | os.O_CREAT | os.O_EXCL if create else os.O_RDONLY
        lock = os.open('.lock', flags | os.O_NOFOLLOW | os.O_NONBLOCK, 0o600, dir_fd=root)
        ls = os.fstat(lock)
        require(stat.S_ISREG(ls.st_mode) and ls.st_uid == os.geteuid() and
                ls.st_nlink == 1 and ls.st_size == 0, 'invalid publication lock')
        fcntl.flock(lock, (fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH) | fcntl.LOCK_NB)
        require(identity(path.stat()) == identity(st) and
                stable(os.stat('.lock', dir_fd=root, follow_symlinks=False)) == stable(ls),
                'publication directory or lock changed')
        yield path, root, identity(ls)
        image.checked_path(path)
        after = path.stat()
        require(identity(after) == identity(st) and after.st_uid == os.geteuid() and
                stat.S_IMODE(after.st_mode) == 0o700 and
                stable(os.stat('.lock', dir_fd=root, follow_symlinks=False)) == stable(ls),
                'publication directory or lock changed')
    finally:
        try:
            if lock is not None:
                os.close(lock)
        finally:
            os.close(root)


def projection(bundle):
    # Future portable formats need a separately qualified streaming publisher.
    require(type(bundle) is dict and bundle.get('format') == 'org.srvros.portable-profile' and
            type(bundle.get('schema_version')) is int and bundle['schema_version'] == 1,
            'publication recovery supports portable v1 only')
    p.validate_bundle(bundle)
    return {'profile': bundle['profile'], 'preferences': bundle['preferences'],
            'documents': [{k: entry[k] for k in ('path', 'size', 'sha256')}
                          for entry in bundle['documents']]}


def completion(record, intent_hash):
    return p.encode_json({'format': 'srvros-export-complete-v1', 'attempt': record['attempt'],
                          'intent_sha256': intent_hash, 'bundle_sha256': record['sha256'],
                          'bundle_identity': record['bundle_identity']})


def source_observation(source):
    p.exact(source, ('stage', 'plan_sha256', 'image_sha256', 'home'), 'source snapshot')
    # Historical path syntax only: no dependency on the source still existing,
    # or still having the same current mount, contents, or pathname type.
    require(type(source['stage']) is str and len(source['stage']) <= 4096 and
            '\0' not in source['stage'] and os.path.isabs(source['stage']) and
            os.path.abspath(source['stage']) == source['stage'] and
            inspection.guest_home(source['home']), 'invalid source observation')
    valid_hash(source['plan_sha256']); valid_hash(source['image_sha256'])


def load(path, root, lock):
    require(set(os.listdir(root)) <= NAMES, 'unrecognized publication artifact')
    raw, _ = read_regular(root, 'intent.json', MAX_RECORD)
    checksum, _ = read_regular(root, 'intent.sha256', 65)
    require(checksum == (digest(raw) + '\n').encode(), 'torn or changed publication record')
    record = p.decode_json(raw)
    p.exact(record, ('format', 'attempt', 'directory', 'directory_identity', 'lock_identity',
                     'output', 'parent_identity', 'bundle_identity', 'size', 'sha256',
                     'selection', 'source_snapshot'), 'export publication')
    require(record['format'] == FORMAT and type(record['attempt']) is str and
            str(uuid.UUID(record['attempt'])) == record['attempt'],
            'invalid publication format or attempt')
    for key in ('directory_identity', 'lock_identity', 'parent_identity', 'bundle_identity'):
        valid_identity(record[key])
    image.checked_path(path)
    require(record['directory'] == str(path) and record['directory_identity'] == identity(os.fstat(root))
            and record['directory_identity'] == identity(path.stat())
            and record['lock_identity'] == lock, 'publication identity mismatch')
    require(type(record['size']) is int and 0 < record['size'] <= p.MAX_BUNDLE, 'invalid bundle size')
    valid_hash(record['sha256'])
    p.exact(record['selection'], ('profile', 'preferences', 'documents'), 'recorded selection')
    p.identity(record['selection']['profile']); p.preferences(record['selection']['preferences'])
    output = image.checked_path(record['output'])
    require(str(output) == record['output'] and output != path and path not in output.parents,
            'invalid recorded output')
    source_observation(record['source_snapshot'])
    return record, digest(raw), output


@contextmanager
def state(path, root, lock):
    record, intent_hash, output = load(path, root, lock)
    parent = os.open(output.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        require(identity(os.fstat(parent)) == record['parent_identity'] and
                identity(output.parent.stat()) == record['parent_identity'], 'output parent identity mismatch')
        private, public = named(root, 'bundle.partial'), named(parent, output.name)
        found = [s for s in (private, public) if s is not None]
        require(found, 'recorded bundle is missing')
        require(all(stat.S_ISREG(s.st_mode) and s.st_uid == os.geteuid() and
                    identity(s) == record['bundle_identity'] and s.st_nlink == len(found) for s in found),
                'bundle identity or link count mismatch')
        complete = named(root, 'complete.json')
        if private and not public and private.st_size < record['size'] and not complete:
            result = {'state': 'incomplete', 'can_reconcile': False, 'reason': 'private bundle is incomplete'}
        else:
            require(all(s.st_size == record['size'] for s in found), 'bundle size mismatch')
            directory, name = (parent, output.name) if public else (root, 'bundle.partial')
            raw, _ = read_regular(directory, name, p.MAX_BUNDLE, links=len(found))
            require(digest(raw) == record['sha256'], 'bundle hash mismatch')
            bundle = p.decode_json(raw)
            require(p.encode_json(projection(bundle)) == p.encode_json(record['selection']) and p.encode_json(bundle) == raw,
                    'bundle selection or encoding mismatch')
            if complete:
                ack, _ = read_regular(root, 'complete.json', MAX_RECORD)
                require(public and not private and ack == completion(record, intent_hash),
                        'invalid or conflicting completion record')
            result = {'state': 'complete' if complete else ('visible-uncertain' if public else 'prepared'),
                      'can_reconcile': True}
        result.update(publication_dir=str(path), output=str(output), bundle_sha256=record['sha256'],
                      source_snapshot=record['source_snapshot'], source_snapshot_only=True,
                      visible=public is not None)
        yield result, record, intent_hash, parent, output, private, public
        require(identity(output.parent.stat()) == record['parent_identity'], 'output parent changed')
    finally:
        os.close(parent)


def inspect(publication_dir):
    try:
        with locked(publication_dir) as (path, root, lock):
            with state(path, root, lock) as (result, *_):
                result = dict(result, read_only=True)
        return result
    except (OSError, ValueError, KeyError, TypeError, OverflowError) as error:
        return {'state': 'refused', 'read_only': True, 'can_reconcile': False, 'reason': str(error)[:512]}


def finish(path, root, lock, checkpoint):
    with state(path, root, lock) as (result, record, intent_hash, parent, output, private, public):
        require(result['can_reconcile'], result.get('reason', 'incomplete publication'))
        directory, name = (parent, output.name) if public else (root, 'bundle.partial')
        fd = os.open(name, os.O_RDWR | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
        try:
            require(identity(os.fstat(fd)) == record['bundle_identity'], 'bundle changed before sync')
            os.fsync(fd)
        finally:
            os.close(fd)
        checkpoint('file-synced')
        with state(path, root, lock) as (current, *_):
            require(current['can_reconcile'], 'bundle changed before publication')
        if not public:
            os.link('bundle.partial', output.name, src_dir_fd=root, dst_dir_fd=parent, follow_symlinks=False)
            checkpoint('linked')
        # Revalidate both aliases before retiring the known private name. Any
        # extra link or changed destination blocks even this cleanup operation.
        with state(path, root, lock) as (_, _, _, _, _, private, public):
            require(public is not None, 'output disappeared')
            if private:
                os.unlink('bundle.partial', dir_fd=root)
                checkpoint('unlinked')
        os.fsync(parent)
        os.fsync(root)
        checkpoint('directory-synced')
        with state(path, root, lock) as (current, *_):
            require(current['can_reconcile'], 'bundle changed before acknowledgment')
        if named(root, 'complete.json') is None:
            write_new(root, 'complete.json', completion(record, intent_hash))
        else:
            ack = os.open('complete.json', os.O_RDWR | os.O_NOFOLLOW, dir_fd=root)
            try:
                os.fsync(ack)
            finally:
                os.close(ack)
        os.fsync(root)
        checkpoint('acknowledged')
    with state(path, root, lock) as (result, *_):
        require(result['state'] == 'complete', 'completion not verified')
        return dict(result, read_only=False)


def reconcile(publication_dir, checkpoint=None):
    try:
        with locked(publication_dir, exclusive=True) as (path, root, lock):
            result = finish(path, root, lock, checkpoint or (lambda _: None))
        return result
    except (OSError, ValueError, KeyError, TypeError, OverflowError) as error:
        return {'state': 'refused', 'read_only': False, 'can_reconcile': False,
                'publication_dir': str(publication_dir), 'reason': str(error)[:512]}


def publish(publication_dir, output, bundle, source_snapshot, checkpoint=None):
    checkpoint = checkpoint or (lambda _: None)
    visible = False
    try:
        selected = projection(bundle)
        source_observation(source_snapshot)
        data = p.encode_json(bundle)
        require(len(data) <= p.MAX_BUNDLE, 'encoded bundle exceeds limit')
        path, output = image.checked_path(publication_dir), image.checked_path(output)
        source = image.checked_path(source_snapshot['stage'])
        require(path != source and source not in path.parents and output != source and
                source not in output.parents and path != output and path not in output.parents,
                'publication/output overlaps source or publication directory')
        require(not os.path.lexists(output), 'output already exists')
        parent = output.parent.stat()
        require(stat.S_ISDIR(parent.st_mode) and path.parent.stat().st_dev == parent.st_dev,
                'publication and output must share a host filesystem')
        with locked(path, exclusive=True, create=True) as (path, root, lock):
            fd = os.open('bundle.partial', os.O_RDWR | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                         0o600, dir_fd=root)
            try:
                os.fsync(fd)
                record = {'format': FORMAT, 'attempt': str(uuid.uuid4()), 'directory': str(path),
                          'directory_identity': identity(os.fstat(root)), 'lock_identity': lock,
                          'output': str(output), 'parent_identity': identity(parent),
                          'bundle_identity': identity(os.fstat(fd)), 'size': len(data), 'sha256': digest(data),
                          'selection': selected, 'source_snapshot': source_snapshot}
                encoded = p.encode_json(record)
                require(len(encoded) <= MAX_RECORD, 'publication record exceeds limit')
                write_new(root, 'intent.json', encoded)
                write_new(root, 'intent.sha256', (digest(encoded) + '\n').encode())
                os.fsync(root)
                image.sync_dir(path.parent)
                checkpoint('recorded')
                half = len(data) // 2
                write(fd, data[:half]); checkpoint('partial-write')
                write(fd, data[half:]); checkpoint('written')
            finally:
                os.close(fd)
            result = finish(path, root, lock, checkpoint)
        return result
    except (OSError, ValueError, KeyError, TypeError, OverflowError) as error:
        if 'output' in locals():
            visible = os.path.lexists(output)
        return {'state': 'refused', 'can_reconcile': False, 'publication_dir': str(publication_dir),
                'visible_uncertain': visible, 'reason': str(error)[:512]}
