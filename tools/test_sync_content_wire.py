#!/usr/bin/env python3
"""Join C observer fields, optionally through the production logger formatter.

The parent USB sync envelope is the existing synthetic validated fixture; all
content rows come from the executed C producer.
"""
import argparse
from pathlib import Path
import subprocess
from sync_content import sync_content
from test_sync_content import TestSyncContent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--production-formatter', action='store_true',
                        help='Also require the production report_sync_content command rows')
    args = parser.parse_args()
    process = subprocess.run([str(args.binary.resolve())], check=True, capture_output=True)
    if args.production_formatter:
        assert b'sync-command-snapshot: version=1 ' in process.stdout, 'Production logger command companion missing'
        assert b'sync-command-role: version=1 ' in process.stdout, 'Production role formatting missing'
    actual = [line for line in process.stdout.splitlines(keepends=True)
              if line.startswith(b'sync-content-')]
    assert len(actual) == 10, 'Missing/extra actual C content rows'
    fixture = TestSyncContent().fixture()
    fixture = b''.join(line for line in fixture.splitlines(keepends=True)
                       if not line.startswith(b'sync-content-')) + b''.join(actual)
    result = sync_content([fixture])
    assert result['summaries'][0]['calls'] == 1 and result['summaries'][0]['bytes'] == 65536
    assert result['io_us'] == 2 and result['io'][0]['flushes'] == 1
    assert result['worst']['version'] == 2 and result['worst']['size'] == 65536
    assert result['worst']['dirty_after'] == 1
    assert result['prefix']['start_generation'] == 11 and result['prefix']['end_generation'] == 12
    assert result['prefix']['committed_size'] == 65536 and result['prefix']['live_size'] == 65573
    assert result['prefix']['append_calls'] == 1 and result['prefix']['append_bytes'] == 37
    for prefix in (b'sync-content-prefix:', b'sync-content-worst:'):
        incomplete = b''.join(line for line in fixture.splitlines(keepends=True)
                              if not line.startswith(prefix))
        try:
            sync_content([incomplete])
        except ValueError:
            pass
        else:
            raise AssertionError('Incomplete actual C wire was accepted')
    print('sync-content-wire: PASS actual C captured-prefix retention and strict parser binding' +
          (' through production workspace formatter' if args.production_formatter else ''))


if __name__ == '__main__':
    main()
