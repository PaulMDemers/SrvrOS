"""Validate the actual immutable C transfer companions, not a copied wire schema."""
import argparse
import re
import subprocess
from capture_transfer import capture_transfer
from capture_history import capture_history

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--host', required=True)
a = p.parse_args()
blob = subprocess.check_output([a.host, '--capture-transfer-fixture'])
result = capture_transfer([blob], expected=3, expected_pixels=4096)
assert result['status'] == 'VALID'
assert capture_history([blob])['completed'] == 3
rows = [line for line in blob.splitlines(keepends=True)
        if line.startswith(b'desktop-capture-transfer')]
assert len(rows) == 9
bounds = {}
for line in rows:
    longest = re.sub(rb'=[0-9]+', b'=18446744073709551615', line)
    assert len(longest) < 1024
    bounds[line.split(b':', 1)[0].decode()] = len(longest)
for bad in (blob.replace(b'pixels_done_us=111', b'pixels_done_us=1'),
            blob.replace(b'worker_allocated_us=108', b'worker_allocated_us=1'),
            blob.replace(b'write_bytes=4272', b'write_bytes=4271'),
            b'\n'.join(line for line in blob.splitlines()
                       if not (line.startswith(b'desktop-capture-transfer-read:') and b'index=1 ' in line))):
    try:
        capture_transfer([bad], expected=3, expected_pixels=4096)
    except ValueError:
        pass
    else:
        raise AssertionError('Corrupted actual C companion accepted')
print('capture-transfer-wire: PASS actual C immutable three-capture companions, malformed joins and atomic row bounds', bounds)
