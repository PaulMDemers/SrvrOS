"""Small deterministic v2 bundle writers for host fixtures and smoke tests."""
import hashlib
import json
from pathlib import Path

import profile_bundle
import profile_stream


def manifest(profile, preferences, documents):
    """Build v2 metadata from ordered ``(path, bytes)`` fixture documents."""
    entries = []
    for name, data in documents:
        if not isinstance(data, bytes):
            raise TypeError('fixture document data must be bytes')
        entries.append({'path': name, 'size': len(data),
                        'sha256': hashlib.sha256(data).hexdigest()})
    value = {'format': profile_bundle.FORMAT, 'schema_version': 2,
             'profile': profile, 'preferences': preferences, 'documents': entries}
    profile_stream.validate_manifest(value)
    return value


def write_stream(path, profile, preferences, documents):
    """Write canonical header/manifest and documents in bounded blocks."""
    documents = list(documents)
    value = manifest(profile, preferences, documents)
    encoded = json.dumps(value, sort_keys=True, separators=(',', ':'),
                         ensure_ascii=True).encode('utf-8')
    if len(encoded) > profile_stream.MAX_MANIFEST:
        raise ValueError('fixture manifest exceeds limit')
    target = Path(path)
    with target.open('xb') as output:
        output.write(profile_stream.HEADER.pack(profile_stream.MAGIC, len(encoded), 0))
        output.write(encoded)
        for _, data in documents:
            view = memoryview(data)
            while view:
                block = view[:profile_stream.BUFFER]
                written = output.write(block)
                if written != len(block):
                    raise OSError('short fixture stream write')
                view = view[written:]
    return value
