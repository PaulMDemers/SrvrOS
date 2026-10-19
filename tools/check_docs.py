#!/usr/bin/env python3
"""Check repository Markdown file links and current gallery hashes, offline.

This checks inline Markdown destinations outside fenced code, not remote URLs
or heading anchors. Upstream submodules/vendor Markdown are excluded.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from urllib.parse import unquote, urlsplit


def main():
    root = Path(__file__).resolve().parents[1]
    files = subprocess.check_output(['git', 'ls-files', '--cached', '--others',
        '--exclude-standard', '-z', '--', '*.md'], cwd=root).decode().split('\0')
    errors = []
    checked = 0
    for name in sorted(set(files)):
        if not name or name.startswith('ports/upstream/'):
            continue
        path = root / name
        if not path.is_file():
            continue
        fence = None
        for number, line in enumerate(path.read_text().splitlines(), 1):
            marker = re.match(r'^\s*(`{3,}|~{3,})', line)
            if marker:
                if fence is None:
                    fence = marker[1][0]
                elif marker[1][0] == fence:
                    fence = None
                continue
            if fence:
                continue
            for match in re.finditer(r'!?\[[^\]]*\]\((<[^>]+>|[^\s)]+)(?:\s+"[^"]*")?\)', line):
                destination = match[1].strip('<>')
                parsed = urlsplit(destination)
                if parsed.scheme or parsed.netloc or not parsed.path:
                    continue
                target = unquote(parsed.path)
                if target.startswith('/'):
                    errors.append(f'{name}:{number}: machine-local absolute link: {target}')
                    continue
                checked += 1
                if not (path.parent / target).exists():
                    errors.append(f'{name}:{number}: missing link target: {target}')
    gallery = root / 'docs/assets/screenshots/current'
    try:
        manifest = json.loads((gallery / 'manifest.json').read_text())
        if manifest['schema'] != 'srvros-desktop-gallery-v1' or not manifest['images']:
            raise ValueError('invalid gallery schema or empty images')
        for entry in manifest['images']:
            name = entry['file']
            if Path(name).name != name:
                raise ValueError('gallery filename must be a basename')
            data = (gallery / name).read_bytes()
            if hashlib.sha256(data).hexdigest() != entry['sha256']:
                errors.append(f'gallery: hash mismatch: {name}')
            if data[:8] != b'\x89PNG\r\n\x1a\n' or data[12:16] != b'IHDR':
                raise ValueError(f'invalid PNG header: {name}')
            if struct.unpack('>II', data[16:24]) != (entry['width'], entry['height']):
                errors.append(f'gallery: dimensions mismatch: {name}')
    except (OSError, ValueError, KeyError, TypeError, struct.error) as error:
        errors.append(f'gallery: {error}')
    for error in errors:
        print(error)
    print(f'docs: {checked} local inline links checked; {len(errors)} errors; remote URLs and anchors not checked')
    return bool(errors)


if __name__ == '__main__':
    raise SystemExit(main())
