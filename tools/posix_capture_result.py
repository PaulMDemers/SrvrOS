"""Strict receipts for the explicit portable-C/native desktop regression."""
import re

CASES = ('threads', 'posix', 'tls', 'sqlite', 'desktop')

def capture_result(text):
    rows = [line for line in text.splitlines() if line.startswith('posix-capture: case=')]
    ends = [line for line in text.splitlines() if line.startswith('posix-capture: complete=')]
    if ends != ['posix-capture: complete=yes tests=5 expected=5'] or len(rows) != len(CASES):
        return None
    records = []
    for name, line in zip(CASES, rows):
        match = re.fullmatch(r'posix-capture: case=(\w+) exited=1 status=0 retired=1 elapsed_us=([0-9]{1,20})', line)
        if not match or match[1] != name or not 0 < int(match[2]) < 2**64:
            return None
        records.append(dict(name=name, elapsed_us=int(match[2]), status=0, retired=True))
    if 'desktop-idle: completed=yes screenshot=yes' not in text.splitlines():
        return None
    if 'laptop-capture: completed=yes' not in text.splitlines():
        return None
    return dict(version=1, cases=records, scope='Bounded RAM POSIX tests and automated desktop/Text screenshot. Physical gestures, battery, sound and networking are not tested.')
