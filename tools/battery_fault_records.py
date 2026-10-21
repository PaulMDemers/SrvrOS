"""Strict fatal-containment records; a blocked reset is never clean retirement."""
import re
from serial_records import complete_lines


def check_fault(data, workspace=False, allow_io_halt=False):
    data=complete_lines(data)
    assert b'; retired=yes' not in data and b'battery-service: retired ' not in data
    rows=[r for r in data.splitlines() if r.startswith(b'battery-reset:')]
    if rows:
        assert len(rows)==1
        match=re.fullmatch(rb'battery-reset: BLOCKED workspace=([01]) io=([01]); live storage retained',rows[0]);assert match
        mounted,io=map(int,match.groups())
        assert mounted==int(workspace) and (mounted or (allow_io_halt and io))
        assert b'battery: HALTED operation deadline' in data
        assert b'battery: FAILED' not in data and b'contained fault; returning' not in data
        return False
    assert not workspace
    assert b'battery: FAILED operation deadline' in data and b'battery: HALTED' not in data
    assert b'contained fault; returning to Ubuntu without live-runtime cleanup; final report unavailable' in data
    return True
