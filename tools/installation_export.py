"""Export explicitly selected portable data from an admitted offline installation."""
import base64
from contextlib import nullcontext
import hashlib
import json
import os
from pathlib import Path
import selectors
import stat
import struct
import subprocess
import tarfile
import tempfile
import time

import installation_image as image
import installation_inspection as inspection
import profile_bundle as p
import profile_stream as stream

MAGIC = b'srvros-installed-export-v1\n'


class PublicationUncertain(OSError):
    pass


def collect_driver(driver, fd, spec, home, documents, magic, total_limit, sink=None):
    proc = subprocess.Popen([str(driver), '--stream-fd' if sink is not None else '--fd',
                             str(fd), str(spec['offset_bytes']), str(spec['size_bytes']),
                             home, *documents],
                            pass_fds=(fd,), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    sel = selectors.DefaultSelector()
    raw = bytearray()
    errors = bytearray()
    byte_limit = len(magic) + 8 * (len(documents) + 1) + 512 + total_limit
    received = 0
    try:
        sel.register(proc.stdout, selectors.EVENT_READ)
        sel.register(proc.stderr, selectors.EVENT_READ)
        deadline = time.monotonic() + 120
        while sel.get_map():
            inspection.require(time.monotonic() < deadline, 'export driver timed out')
            for key, _ in sel.select(timeout=min(2, max(0, deadline - time.monotonic()))):
                chunk = os.read(key.fd, stream.BUFFER)
                if not chunk:
                    sel.unregister(key.fileobj)
                elif key.fileobj is proc.stderr:
                    errors.extend(chunk)
                    inspection.require(len(errors) <= 16384, 'export driver output exceeds limit')
                else:
                    received += len(chunk)
                    inspection.require(received <= byte_limit, 'export driver output exceeds limit')
                    if sink is None:
                        raw.extend(chunk)
                    elif sink.write(chunk) != len(chunk):
                        raise OSError('short driver spool write')
        inspection.require(proc.wait(timeout=30) == 0,
                           'export driver refused filesystem: ' + errors.decode('utf-8', 'replace')[:512])
        return bytes(raw)
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        sel.close()
        proc.stdout.close()
        proc.stderr.close()


def driver_bytes(driver, fd, spec, home, documents):
    raw = collect_driver(driver, fd, spec, home, documents, MAGIC, p.MAX_TOTAL)
    inspection.require(raw.startswith(MAGIC), 'invalid export driver header')
    position = len(MAGIC)
    frames = []
    for limit in [512] + [p.MAX_FILE] * len(documents):
        inspection.require(position + 8 <= len(raw), 'truncated export frame')
        size = struct.unpack_from('<Q', raw, position)[0]
        position += 8
        inspection.require(size <= limit and position + size <= len(raw), 'invalid export frame size')
        frames.append(raw[position:position + size])
        position += size
    inspection.require(position == len(raw) and sum(map(len, frames[1:])) <= p.MAX_TOTAL,
                       'invalid export frame length')
    return frames


def encode_stream(driver, fd, spec, home, documents, who, bundle_spool):
    magic = b'srvros-installed-export-v2\n'

    def frame_size(source, limit):
        length = source.read(8)
        inspection.require(len(length) == 8, 'truncated export frame')
        size, = struct.unpack('<Q', length)
        inspection.require(size <= limit, 'invalid export frame size')
        return size

    def payload(source, size):
        while size:
            chunk = source.read(min(size, stream.BUFFER))
            inspection.require(chunk, 'truncated export payload')
            size -= len(chunk)
            yield chunk

    with tempfile.TemporaryFile() as frames:
        collect_driver(driver, fd, spec, home, documents, magic, stream.MAX_TOTAL, frames)
        frames.seek(0)
        inspection.require(frames.read(len(magic)) == magic, 'invalid export driver header')
        size = frame_size(frames, 512)
        prefs = inspection.guest_preferences(b''.join(payload(frames, size)))
        entries = []
        offsets = []
        total = 0
        for name in documents:
            size = frame_size(frames, stream.MAX_FILE)
            total += size
            inspection.require(total <= stream.MAX_TOTAL, 'invalid export frame length')
            offsets.append(frames.tell())
            digest = hashlib.sha256()
            for chunk in payload(frames, size):
                digest.update(chunk)
            entries.append({'path': name, 'size': size, 'sha256': digest.hexdigest()})
        inspection.require(not frames.read(1), 'invalid export frame length')
        bundle = {'format': p.FORMAT, 'schema_version': 2, 'profile': who,
                  'preferences': prefs, 'documents': entries}
        stream.validate_manifest(bundle)
        manifest = json.dumps(bundle, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode('utf-8')
        inspection.require(len(manifest) <= stream.MAX_MANIFEST, 'manifest exceeds size limit')
        bundle_spool.write(stream.HEADER.pack(stream.MAGIC, len(manifest), 0))
        bundle_spool.write(manifest)
        for offset, entry in zip(offsets, entries):
            frames.seek(offset)
            for chunk in payload(frames, entry['size']):
                bundle_spool.write(chunk)
        bundle_spool.flush()
    return bundle


def output_path(path, target):
    path = image.checked_path(path)
    inspection.require(path.name not in ('', '.', '..') and not os.path.lexists(path), 'output already exists or invalid')
    inspection.require(path != target and target not in path.parents, 'output inside stage')
    parent = path.parent
    inspection.require(stat.S_ISDIR(parent.stat().st_mode), 'invalid output parent')
    return path


def publish(path, bundle):
    p.validate_bundle(bundle)
    data = p.encode_json(bundle)
    p.require(len(data) <= p.MAX_BUNDLE, 'encoded bundle exceeds size limit')
    directory = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    temporary = None
    linked = False
    error = None
    residual = None
    try:
        fd, temporary = tempfile.mkstemp(prefix='.profile-export-', dir=path.parent)
        with os.fdopen(fd, 'wb') as stream:
            if stream.write(data) != len(data):
                raise OSError('short bundle write')
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temporary, path.name, dst_dir_fd=directory, follow_symlinks=False)
        linked = True
        os.unlink(temporary)
        temporary = None
        os.fsync(directory)
    except Exception as caught:
        error = caught
    finally:
        if temporary is not None:
            try:
                os.unlink(temporary)
            except OSError as caught:
                residual = (temporary, caught)
        try:
            os.close(directory)
        except OSError as caught:
            if error is None:
                error = caught
            else:
                error = OSError(str(error) + '; output directory close failed: ' + str(caught))
    if residual is not None:
        message = 'unpublished temporary artifact may remain: ' + residual[0] + ': ' + str(residual[1])
        error = OSError((str(error) + '; ' if error else '') + message)
    if error is not None:
        if linked:
            raise PublicationUncertain('output is visible but publication durability is uncertain: ' + str(error)) from error
        raise error
    return hashlib.sha256(data).hexdigest()


def export_installed(stage, output, profile_id, home, documents, inspect_driver, export_driver,
                     publication_dir=None, checkpoint=None, *, bundle_version=1):
    try:
        inspection.require(type(bundle_version) is int and bundle_version in (1, 2), 'invalid bundle version')
        inspection.require(bundle_version == 1 or publication_dir is not None, 'v2 export requires a publication directory')
        target = image.checked_path(stage)
        path = output_path(output, target)
        p.profile_id(profile_id)
        inspection.require(inspection.guest_home(home), 'invalid profile home')
        inspection.require(type(documents) in (list, tuple) and len(documents) <= p.MAX_FILES, 'invalid document selection')
        # Apply the complete v1 path, duplicate and collision rules before reading.
        empty_entries = [{'path': x, 'size': 0, 'sha256': hashlib.sha256(b'').hexdigest(), 'data_base64': ''} for x in documents]
        p.validate_bundle({'format': p.FORMAT, 'schema_version': p.VERSION,
                           'profile': {'id': profile_id, 'display_name': 'validation'},
                           'preferences': {}, 'documents': empty_entries})
        driver_record = image.source(export_driver)
        inspection.require(os.access(driver_record['path'], os.X_OK), 'export driver is not executable')
        publication_path = image.checked_path(publication_dir) if publication_dir is not None else None
        inspection.require(checkpoint is None or publication_path is not None, 'checkpoint requires publication directory')
        if publication_path is not None:
            inspection.require(publication_path != target and target not in publication_path.parents and
                               not os.path.lexists(publication_path), 'publication directory exists or overlaps stage')
        spool = tempfile.TemporaryFile() if bundle_version == 2 else nullcontext()
        with spool as bundle_spool, image.plan_lock(target):
            def read_selected(fd, plan, profiles):
                for source in plan['sources'].values():
                    source_path = Path(source['path'])
                    inspection.require(path != source_path and path not in source_path.parents, 'output overlaps source path')
                    if publication_path is not None:
                        inspection.require(publication_path != source_path and publication_path not in source_path.parents,
                                           'publication directory overlaps source path')
                inspection.require(path != Path(driver_record['path']) and path != image.checked_path(inspect_driver), 'output overlaps driver')
                matches = [x for x in profiles if x['profile_id'] == profile_id and x['home'] == home]
                inspection.require(len(matches) == 1, 'selected profile not admitted')
                who = p.identity({'id': profile_id, 'display_name': matches[0]['display_name']})
                if bundle_version == 2:
                    bundle = encode_stream(driver_record['path'], fd, plan['layout']['exfat'], home, documents, who, bundle_spool)
                    inspection.require(image.source(export_driver) == driver_record, 'export driver changed')
                    return bundle
                frames = driver_bytes(driver_record['path'], fd, plan['layout']['exfat'], home, documents)
                inspection.require(image.source(export_driver) == driver_record, 'export driver changed')
                prefs = inspection.guest_preferences(frames[0])
                entries = [{'path': name, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                            'data_base64': base64.b64encode(data).decode()} for name, data in zip(documents, frames[1:])]
                bundle = {'format': p.FORMAT, 'schema_version': p.VERSION, 'profile': who,
                          'preferences': prefs, 'documents': entries}
                p.validate_bundle(bundle)
                return bundle
            result, bundle = inspection._inspect(target, inspect_driver, read_selected)
            # Inspection has completed its final revalidation before publication.
            recovery = None
            if publication_path is None:
                digest = publish(path, bundle)
            else:
                import installation_export_publication as publication
                snapshot = {'stage': str(target), 'plan_sha256': image.digest(target / 'plan.json'),
                            'image_sha256': result['current_image_sha256'], 'home': home}
                if bundle_version == 2:
                    recovery = publication.publish_stream(publication_path, path, bundle_spool, snapshot, checkpoint)
                else:
                    recovery = publication.publish(publication_path, path, bundle, snapshot, checkpoint)
                if recovery['state'] != 'complete':
                    return {'state': 'refused', 'read_only_source': True, 'output': str(path),
                            'export_publication': recovery, 'reason': recovery.get('reason', 'publication incomplete'),
                            'visible_uncertain': recovery.get('visible_uncertain', False)}
                digest = recovery['bundle_sha256']
        return {'state': 'exported', 'read_only_source': True, 'output': str(path),
                'bundle_sha256': digest, 'profile': bundle['profile'],
                'documents': [{k: e[k] for k in ('path', 'size', 'sha256')} for e in bundle['documents']],
                'current_image_sha256': result['current_image_sha256'], 'source_used': result['used'],
                'filesystem': result['filesystem'], 'publication': result['publication'], 'inspect_driver': result['driver'],
                'export_driver': driver_record, **({'export_publication': recovery} if recovery is not None else {}),
                **({'bundle_version': 2} if bundle_version == 2 else {})}
    except PublicationUncertain as error:
        return {'state': 'refused', 'read_only_source': True, 'visible_uncertain': True,
                'output': str(path), 'reason': str(error)[:512]}
    except (OSError, ValueError, KeyError, TypeError, IndexError, RuntimeError, UnicodeError,
            tarfile.TarError, subprocess.SubprocessError, OverflowError) as error:
        return {'state': 'refused', 'read_only_source': True, 'reason': str(error)[:512]}
