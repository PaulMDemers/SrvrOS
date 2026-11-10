"""Check the actual fixed C counters and production formatter, not string replicas."""
import argparse
from pathlib import Path
import subprocess
from capture_write_timing import capture_write_timing, capture_gate_timing


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary',type=Path,required=True)
    args=parser.parse_args()
    for mode,ordinary,fast in [('mixed',1,1),('all-fast',0,2)]:
        raw=subprocess.check_output([str(args.binary.resolve()),mode])
        observed=capture_write_timing([raw],1,71000)
        gates=capture_gate_timing([raw],observed)
        worker=observed['workers'][0]
        assert observed['version']==2 and worker['ordinary_calls']==ordinary
        assert worker['record']['calls']==2 and worker['record']['fast_calls']==fast
        assert gates['workers'][0]['total']['calls']==ordinary
        assert gates['workers'][0]['worst']['calls']==bool(ordinary)
        assert not worker['record']['fast_failures']
        # Exact source identity, required fields and ordinary/private coverage
        # remain mandatory even when there were no ordinary waits at all.
        for bad in [raw.replace(b'fast_max_request=',b'unknown_max='),
                    raw.replace(b'pid=42',b'pid=43',1)]:
            try:capture_write_timing([bad],1,71000)
            except ValueError:pass
            else:raise AssertionError('Corrupt production wire was accepted')
    print('capture-write-wire: PASS real fixed C helpers and production formatter; mixed/private-only coverage, explicit zero ordinary gates, bounded copies')


if __name__=='__main__':
    main()
