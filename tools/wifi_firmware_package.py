#!/usr/bin/env python3
"""Create an explicit firmware/license package; does not install or upload it."""
import argparse
import hashlib
import json
import tempfile
from pathlib import Path
from wifi_firmware_audit import inspect, MAX_BYTES

LICENSE_SHA256 = 'a90d3f66704d85889945fec5525ea77622549da83aced1aac99828383f8f1805'


def package(image, license_path, output):
    with image.open('rb') as stream:
        data = stream.read(MAX_BYTES+1)
    report = inspect(image.name, data)
    with license_path.open('rb') as stream:
        license_data = stream.read(65537)
    if hashlib.sha256(license_data).hexdigest() != LICENSE_SHA256:
        raise ValueError('license does not match pinned MediaTek license')
    if output.exists():
        raise ValueError('output already exists; refusing replacement')
    report.update(license='LICENCE.mediatek', license_sha256=LICENSE_SHA256,
                  source='linux-firmware 431e7f03d2c77c3d027d02c9160b15f0f245d882',
                  installed=False, native_upload_tested=False)
    with tempfile.TemporaryDirectory(prefix='.wifi-package-', dir=output.parent) as temp:
        staged = Path(temp)/'package'
        staged.mkdir()
        (staged/image.name).write_bytes(data)
        (staged/'LICENCE.mediatek').write_bytes(license_data)
        (staged/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
        staged.rename(output)
    return report


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image', type=Path)
    p.add_argument('--license', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    try:
        print(json.dumps(package(a.image, a.license, a.output), indent=2))
    except (OSError, ValueError) as error:
        p.exit(1, f'firmware package refused: {error}\n')
