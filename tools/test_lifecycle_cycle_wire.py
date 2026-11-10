"""Validate complete lifecycle receipts emitted by the actual desktop C helper."""
import argparse
import subprocess

from app_timing import lifecycle_receipts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    args = parser.parse_args()
    raw = subprocess.check_output([args.host, '--lifecycle-cycle-fixture'])
    expected = ([(0, 42, 20), (1, 43, 40)], [(0, 42, 30), (1, 43, 50)])
    assert lifecycle_receipts([raw], 2) == expected
    originals = (b'desktop-life: ready cycle=0 pid=42 elapsed_us=20\n'
                 b'desktop-life: closed cycle=1 pid=43 elapsed_us=50 errors=0\n')
    assert lifecycle_receipts([raw + originals, raw], 2) == expected
    for bad in (raw + raw.splitlines(keepends=True)[0],
                raw.replace(b'ready_elapsed_us=20', b'ready_elapsed_us=0'),
                raw.replace(b'closed_elapsed_us=50', b'closed_elapsed_us=10000001'),
                raw.replace(b'version=1', b'version=2', 1),
                raw.replace(b'errors=0', b'errors=1', 1),
                raw.replace(b'pid=43', b'pid=42'),
                b'UART'+raw,
                raw + originals.replace(b'elapsed_us=20', b'elapsed_us=21')):
        try:
            lifecycle_receipts([bad], 2)
        except AssertionError:
            pass
        else:
            raise AssertionError('Malformed actual C lifecycle wire accepted')
    # Even maximum-width wire values fit a single existing 384-byte UI log slot.
    wide = raw.splitlines()[0]
    for value in (b'cycle=0', b'pid=42', b'ready_elapsed_us=20', b'closed_elapsed_us=30'):
        wide = wide.replace(value, value.split(b'=')[0] + b'=18446744073709551615')
    assert len(wide) + 1 < 384
    print('lifecycle-cycle-wire: PASS actual C helper, exact originals, bounded whole rows and strict failures')


if __name__ == '__main__':
    main()
