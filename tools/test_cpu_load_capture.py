#!/usr/bin/env python3
"""Build/run the bounded load fixtures without shared Makefile changes."""
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='srvros-cpu-load-') as tmp:
    binary = Path(tmp) / 'cpu-load-host'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-DSRVROS_CPU_LOAD_HOST_TEST',
        '-idirafter', str(root/'userspace/lib/include'), '-I', str(root/'shared/include'),
        str(root/'tools/test_cpu_load_capture.c'),
        str(root/'userspace/threadstress/load_capture.c'), '-o', str(binary)], check=True)
    result = subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=20)
    text = result.stdout
    assert text.count('cpu-load: completed=yes\n') == 2
    assert text.count('cpu-load: completed=NO ') == 12
    for reason in ['thermal-unavailable-or-stale', 'thermal-limit', 'temperature-stop',
                   'thermal-range', 'load-start-too-warm', 'fans-unavailable-or-stale',
                   'clock-stalled', 'clock-regressed', 'sleep']:
        assert f'reason={reason}\n' in text, reason
    assert len(re.findall(r'^cpu-load: phase=load completed=yes',text,re.M)) == 2
    print(text, end='')
