"""Match serial diagnostics only after their final line terminator arrives."""
import re


def complete_lines(data):
    return data[:data.rfind(b'\n')+1]


def search_complete(pattern, data):
    # A read can end after the first digit of a counter. Regex \d+ alone would
    # accept that prefix and turn e.g. 207000 us into a spurious 2 us failure.
    # Multi-line records also wait for the terminator of the final line.
    return re.search(pattern, complete_lines(data))


COMPOSITOR_TRACE=rb'(?:deskd: |desktop-launch: )[^\n]*\n'


def desktop_progress_end(data, marker, start=0):
    """Wait-position helper, not a completion-proof parser.

    Preserve exact application bytes while allowing only complete compositor
    trace lines between them. Keep raw evidence and validate saved files/records
    separately. Compositor markers themselves must be contiguous.
    """
    if not marker:
        raise ValueError('empty progress marker')
    at=data.find(marker,start)
    if at>=0:
        return at+len(marker)
    if marker.startswith((b'deskd:',b'desktop-launch:')):
        return None
    pattern=(b'(?:'+COMPOSITOR_TRACE+b')*').join(re.escape(bytes([value])) for value in marker)
    found=re.search(pattern,data[start:])
    return start+found.end() if found else None
