"""Export explicitly selected portable data from an admitted offline installation."""
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

MAGIC = b'srvros-installed-export-v2\n'


def collect_driver(driver, fd, spec, home, documents, magic, total_limit, sink):
    proc = subprocess.Popen([str(driver), '--stream-fd', str(fd), str(spec['offset_bytes']), str(spec['size_bytes']),
                             home, *documents],
                            pass_fds=(fd,), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    sel = selectors.DefaultSelector()
    errors = bytearray()
    byte_limit = len(magic) + 8 * (len(documents) + 1) + 512 + total_limit
    received = 0

    def write_sink(data):
        view = memoryview(data)
        while view:
            count = sink.write(view)
            inspection.require(type(count) is int and 0 < count <= len(view), 'short driver spool write')
            view = view[count:]

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
                    write_sink(chunk)
        inspection.require(proc.wait(timeout=30) == 0,
                           'export driver refused filesystem: ' + errors.decode('utf-8', 'replace')[:512])
        return received
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        sel.close()
        proc.stdout.close()
        proc.stderr.close()


def encode_stream(driver, fd, spec, home, documents, who, bundle_spool):
    def write_spool(data):
        view = memoryview(data)
        while view:
            count = bundle_spool.write(view)
            inspection.require(type(count) is int and 0 < count <= len(view), 'short profile spool write')
            view = view[count:]

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
        collect_driver(driver, fd, spec, home, documents, MAGIC, stream.MAX_TOTAL, frames)
        frames.seek(0)
        inspection.require(frames.read(len(MAGIC)) == MAGIC, 'invalid export driver header')
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
        write_spool(stream.HEADER.pack(stream.MAGIC, len(manifest), 0))
        write_spool(manifest)
        for offset, entry in zip(offsets, entries):
            frames.seek(offset)
            for chunk in payload(frames, entry['size']):
                write_spool(chunk)
        bundle_spool.flush()
    return bundle


def output_path(path, target):
    path = image.checked_path(path)
    inspection.require(path.name not in ('', '.', '..') and not os.path.lexists(path), 'output already exists or invalid')
    inspection.require(path != target and target not in path.parents, 'output inside stage')
    parent = path.parent
    inspection.require(stat.S_ISDIR(parent.stat().st_mode), 'invalid output parent')
    return path


def export_installed(stage, output, profile_id, home, documents, inspect_driver, export_driver,
                     publication_dir, checkpoint=None):
    try:
        target = image.checked_path(stage)
        path = output_path(output, target)
        p.profile_id(profile_id)
        inspection.require(inspection.guest_home(home), 'invalid profile home')
        inspection.require(type(documents) in (list, tuple) and len(documents) <= p.MAX_FILES, 'invalid document selection')
        empty_entries = [{'path': x, 'size': 0, 'sha256': hashlib.sha256(b'').hexdigest()} for x in documents]
        stream.validate_manifest({'format': p.FORMAT, 'schema_version': 2,
                                  'profile': {'id': profile_id, 'display_name': 'validation'},
                                  'preferences': {}, 'documents': empty_entries})
        driver_record = image.source(export_driver)
        inspection.require(os.access(driver_record['path'], os.X_OK), 'export driver is not executable')
        publication_path = image.checked_path(publication_dir)
        inspection.require(publication_path != target and target not in publication_path.parents and
                           not os.path.lexists(publication_path), 'publication directory exists or overlaps stage')
        with tempfile.TemporaryFile() as bundle_spool, image.plan_lock(target):
            def read_selected(fd, plan, profiles):
                for source in plan['sources'].values():
                    source_path = Path(source['path'])
                    inspection.require(path != source_path and path not in source_path.parents, 'output overlaps source path')
                    inspection.require(publication_path != source_path and publication_path not in source_path.parents,
                                       'publication directory overlaps source path')
                inspection.require(path != Path(driver_record['path']) and path != image.checked_path(inspect_driver), 'output overlaps driver')
                matches = [x for x in profiles if x['profile_id'] == profile_id and x['home'] == home]
                inspection.require(len(matches) == 1, 'selected profile not admitted')
                who = p.identity({'id': profile_id, 'display_name': matches[0]['display_name']})
                bundle = encode_stream(driver_record['path'], fd, plan['layout']['exfat'], home, documents, who, bundle_spool)
                inspection.require(image.source(export_driver) == driver_record, 'export driver changed')
                return bundle
            result, bundle = inspection._inspect(target, inspect_driver, read_selected)
            # Inspection has completed its final revalidation before publication.
            import installation_export_publication as publication
            snapshot = {'stage': str(target), 'plan_sha256': image.digest(target / 'plan.json'),
                        'image_sha256': result['current_image_sha256'], 'home': home}
            recovery = publication.publish_stream(publication_path, path, bundle_spool, snapshot, checkpoint)
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
                'export_driver': driver_record, 'export_publication': recovery}
    except (OSError, ValueError, KeyError, TypeError, IndexError, RuntimeError, UnicodeError,
            tarfile.TarError, subprocess.SubprocessError, OverflowError) as error:
        return {'state': 'refused', 'read_only_source': True, 'reason': str(error)[:512]}
