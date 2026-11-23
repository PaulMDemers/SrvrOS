#!/usr/bin/env python3
"""Shared metadata and local-store helpers for portable profile streams.

No authentication, chown, guest installation, automatic secret discovery, or
power-loss durability is claimed. Documents are always explicitly selected.
"""
import argparse
from contextlib import contextmanager
import fcntl
import json
import os
from pathlib import Path
import re
import shutil
import stat
import sys
import tempfile
import uuid

FORMAT = 'org.srvros.portable-profile'
VERSION = 2
MAX_FILE = 16 * 1024 * 1024
MAX_TOTAL = 64 * 1024 * 1024
MAX_JSON = 12 * 1024 * 1024
MAX_FILES = 64
MARKER = b'srvros-profile-fixture-v1\n'
PREF_PATH = '.config/srvros/portable.json'
# Names/ranges match desktop_preferences.h and desk_preferences.c. Device,
# display, timezone and session settings are deliberately absent.
PREF_RANGES = {'text_size': (16, 20), 'clock_flags': range(4),
               'wallpaper': range(3), 'theme_accent': range(3),
               'glass_level': range(3), 'theme_style': range(3)}
LOCAL_RANGES = {'utc_offset_minutes': range(-720, 841, 15),
                'pointer_speed': range(1, 6), 'pointer_acceleration': range(2),
                'disable_while_typing': range(2), 'tap_to_click': range(2),
                'two_finger_tap': range(2), 'display_scale': (1, 2, 125, 150, 175),
                'display_width': range(0, 3841), 'display_height': range(0, 2161)}


class ProfileError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise ProfileError(message)


def exact(value, keys, what):
    require(type(value) is dict and set(value) == set(keys), 'invalid ' + what + ' fields')


def unique_object(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, 'duplicate JSON key')
        out[key] = value
    return out


def bounded_integer(text):
    require(len(text) <= 20, 'JSON integer exceeds limit')
    return int(text)


def decode_json(data):
    require(len(data) <= MAX_JSON, 'JSON object exceeds size limit')
    # Metadata needs only three levels. Bound hostile nesting before the
    # Python decoder, whose recursion limit varies between host environments.
    depth, quoted, escaped = 0, False, False
    for byte in data:
        if quoted:
            if escaped:
                escaped = False
            elif byte == 92:
                escaped = True
            elif byte == 34:
                quoted = False
        elif byte == 34:
            quoted = True
        elif byte in (91, 123):
            depth += 1
            require(depth <= 8, 'JSON nesting exceeds limit')
        elif byte in (93, 125):
            depth -= 1
    try:
        return json.loads(data.decode('utf-8'), object_pairs_hook=unique_object,
                          parse_int=bounded_integer,
                          parse_constant=lambda _: (_ for _ in ()).throw(ProfileError('nonfinite JSON')))
    except (UnicodeError, ValueError, RecursionError) as error:
        raise ProfileError('invalid or truncated JSON') from error


def encode_json(value):
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + '\n').encode()


def profile_id(value):
    require(type(value) is str, 'invalid profile ID')
    try:
        parsed = uuid.UUID(value)
    except ValueError as error:
        raise ProfileError('invalid profile ID') from error
    require(str(parsed) == value and parsed.version == 4, 'profile ID must be canonical UUIDv4')
    return value


def identity(value):
    exact(value, ('id', 'display_name'), 'profile')
    profile_id(value['id'])
    name = value['display_name']
    require(type(name) is str and 1 <= len(name) <= 64 and name == name.strip()
            and all(32 <= ord(c) <= 126 and c not in '/\\' for c in name), 'invalid display name')
    return value


def preferences(value):
    require(type(value) is dict, 'preferences must be an object')
    for key, number in value.items():
        require(key in PREF_RANGES and type(number) is int and number in PREF_RANGES[key],
                'unsupported portable preference key or value')
    return value


def desktop_projection(data):
    """Explicit v7 legacy Settings adapter; never archive the mixed device file."""
    require(len(data) <= 512, 'desktop settings exceed limit')
    try:
        lines = data.decode('ascii').splitlines(keepends=True)
    except UnicodeError as error:
        raise ProfileError('invalid desktop settings') from error
    require(lines and lines[0] == 'srvros-desktop-v7\n', 'only desktop settings v7 supported')
    values = {}
    ranges = dict(PREF_RANGES, **LOCAL_RANGES)
    for line in lines[1:]:
        match = re.fullmatch(r'([a-z_]+)=(-?[0-9]{1,4})\n', line)
        require(match is not None, 'invalid desktop settings line')
        key, raw = match.groups()
        number = int(raw)
        require(key in ranges and key not in values and number in ranges[key], 'invalid desktop setting')
        values[key] = number
    require(set(values) == set(ranges), 'incomplete desktop settings')
    width, height = values['display_width'], values['display_height']
    require((width == height == 0) or (width >= 640 and height >= 480), 'invalid display mode')
    return {key: values[key] for key in PREF_RANGES}


def document_path(value):
    require(type(value) is str and len(value) <= 160, 'invalid document path')
    parts = value.split('/')
    require(len(parts) >= 2 and parts[0] == 'Documents', 'only selected Documents files are portable')
    for part in parts:
        require(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9 _.\-]{0,63}', part) is not None
                and not part.endswith((' ', '.')), 'nonportable document path')
    return parts


def read_regular(path, limit=MAX_JSON):
    """Reject symlinks in every path component, special files and hard links.

    Fixture directories must be offline and controlled by the invoking user;
    this does not defend against a hostile same-user process swapping parents.
    """
    path = Path(os.path.abspath(path))
    for parent in reversed(path.parents):
        require(stat.S_ISDIR(parent.lstat().st_mode), 'symlink or non-directory parent')
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, 'rb') as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_nlink == 1, 'only single-link regular files supported')
        require(before.st_size <= limit, 'file exceeds size limit')
        data = stream.read(limit + 1)
        after = os.fstat(stream.fileno())
        require(len(data) <= limit and len(data) == before.st_size and
                (before.st_size, before.st_mtime_ns, before.st_ctime_ns) ==
                (after.st_size, after.st_mtime_ns, after.st_ctime_ns), 'file changed during read')
        return data


def write_new(path, data):
    with open(path, 'xb') as stream:
        os.chmod(path, 0o600)
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def init_store(root):
    root = Path(os.path.abspath(root))
    require(all(not os.path.lexists(p) or stat.S_ISDIR(p.lstat().st_mode)
                for p in (root, *root.parents)), 'invalid fixture root')
    require(not root.exists() or (root.is_dir() and not any(root.iterdir())), 'fixture root must be new or empty')
    root.mkdir(mode=0o700, parents=True, exist_ok=True)
    # Reject a symlink root or ancestor before creating any store metadata.
    require(all(stat.S_ISDIR(p.lstat().st_mode) for p in (root, *root.parents)), 'invalid fixture root')
    os.chmod(root, 0o700)
    (root / 'profiles').mkdir(mode=0o700)
    (root / 'staging').mkdir(mode=0o700)
    write_new(root / '.lock', b'')
    write_new(root / '.profile-store-v1', MARKER)


@contextmanager
def store_lock(root):
    root = Path(os.path.abspath(root))
    require(read_regular(root / '.profile-store-v1', 64) == MARKER, 'not a profile fixture')
    for child in ('profiles', 'staging'):
        require(stat.S_ISDIR((root / child).lstat().st_mode), 'invalid store directory')
    fd = os.open(root / '.lock', os.O_RDWR | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        info = os.fstat(fd)
        require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1, 'invalid lock file')
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ProfileError('profile store busy; retry after current operation') from error
        yield root
    finally:
        os.close(fd)


def binding(alias, uid, gid):
    require(type(alias) is str and re.fullmatch(r'[a-z][a-z0-9_-]{0,31}', alias) is not None, 'invalid local account name')
    require(type(uid) is int and type(gid) is int and 1 <= uid <= 2**31-1 and 1 <= gid <= 2**31-1, 'invalid local UID/GID')
    return {'alias': alias, 'uid': uid, 'gid': gid}


def local_binding(path):
    value = decode_json(read_regular(path / 'binding.json', 4096))
    exact(value, ('alias', 'uid', 'gid'), 'local binding')
    return binding(**value)


def profile_directory(root, identifier):
    path = root / 'profiles' / profile_id(identifier)
    require(stat.S_ISDIR(path.lstat().st_mode), 'invalid profile directory')
    return path


def install_profile(root, who, portable_preferences, local, populate=None,
                    checkpoint=lambda _: None, verify_source=None):
    who = identity(who)
    portable_preferences = preferences(portable_preferences)
    exact(local, ('alias', 'uid', 'gid'), 'local binding')
    local = binding(**local)
    identifier = who['id']
    with store_lock(root) as root:
        require(not any((root / 'staging').iterdir()), 'interrupted import; run recover before retry')
        destination = root / 'profiles' / identifier
        require(not os.path.lexists(destination), 'profile identity already exists; nothing replaced')
        for path in (root / 'profiles').iterdir():
            profile_id(path.name)
            require(stat.S_ISDIR(path.lstat().st_mode), 'invalid profile store entry')
            old = local_binding(path)
            require(old['alias'] != local['alias'], 'local account name already exists')
            require(old['uid'] != local['uid'], 'local UID already assigned')
        stage = Path(tempfile.mkdtemp(prefix='import-', dir=root / 'staging'))
        checkpoint('stage')
        home = stage / 'home'
        for directory in ('Documents', '.config/srvros', '.local/share', '.cache'):
            (home / directory).mkdir(mode=0o700, parents=True, exist_ok=True)
        write_new(stage / 'identity.json', encode_json(who))
        write_new(stage / 'binding.json', encode_json(local))
        write_new(home / PREF_PATH, encode_json(portable_preferences))
        checkpoint('metadata')
        if populate is not None:
            populate(home)
        checkpoint('before-publish')
        if verify_source is not None:
            verify_source()
        require(not os.path.lexists(destination), 'profile identity already exists')
        os.rename(stage, destination)
        checkpoint('after-publish')
        return {'profile_id': identifier, 'home': str(destination / 'home'), **local}


def create_profile(root, name, alias, uid, gid):
    who = identity({'id': str(uuid.uuid4()), 'display_name': name})
    return install_profile(root, who, {}, binding(alias, uid, gid))


def recover(root):
    with store_lock(root) as root:
        stages = list((root / 'staging').iterdir())
        for path in stages:
            require(re.fullmatch(r'import-[a-z0-9_]{8}', path.name) is not None
                    and stat.S_ISDIR(path.lstat().st_mode), 'unknown staging entry; inspect manually')
        for path in stages:
            shutil.rmtree(path)
        return {'discarded_unpublished_imports': len(stages),
                'published_profiles': sorted(p.name for p in (root / 'profiles').iterdir())}


def environment(root, identifier):
    with store_lock(root) as root:
        directory = profile_directory(root, identifier)
        who = identity(decode_json(read_regular(directory / 'identity.json', 4096)))
        require(who['id'] == identifier, 'stored profile identity mismatch')
        home = str(directory / 'home')
        # JSON output is data, never a shell command to eval.
        return {'SRVROS_PROFILE_ID': identifier, 'HOME': home,
                'SRVROS_DESKTOP_HOME': home, 'XDG_CONFIG_HOME': home + '/.config',
                'XDG_DATA_HOME': home + '/.local/share', 'XDG_CACHE_HOME': home + '/.cache'}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    for name in ('init', 'create', 'env', 'recover'):
        cmd = commands.add_parser(name)
        cmd.add_argument('root', type=Path, help='disposable offline installation fixture')
        if name == 'create':
            cmd.add_argument('name')
            cmd.add_argument('--alias', required=True)
            cmd.add_argument('--uid', required=True, type=int)
            cmd.add_argument('--gid', required=True, type=int)
        if name == 'env':
            cmd.add_argument('profile_id')
    args = parser.parse_args(argv)
    try:
        if args.command == 'init':
            init_store(args.root)
            result = {'initialized': str(args.root)}
        elif args.command == 'create':
            result = create_profile(args.root, args.name, args.alias, args.uid, args.gid)
        elif args.command == 'recover':
            result = recover(args.root)
        else:
            result = environment(args.root, args.profile_id)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (ProfileError, OSError) as error:
        print('profile: ' + str(error), file=sys.stderr)
        if args.command == 'recover':
            print('Inspect published profiles and staging before retrying.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
