"""Check the actual desktop C report against the strict history parser."""
import argparse
import subprocess
from capture_history import capture_history

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--host', required=True)
a = p.parse_args()
blob = subprocess.check_output([a.host, '--capture-history-fixture'])
r = capture_history([blob])
assert r['completed'] == 3 and r['median_us'] == 60 and r['max_us'] == 80
assert [c['pid'] for c in r['captures']] == [42, 43, 44]
for bad in (blob.replace(b'completed=3', b'completed=2'),
            blob.replace(b'index=1', b'index=9'),
            blob.replace(b'retired_us=270', b'retired_us=10')):
    try:
        capture_history([bad])
    except ValueError:
        pass
    else:
        raise AssertionError('Malformed actual C history accepted')
print('capture-history-wire: PASS actual C report, all three clocks and malformed joins')
