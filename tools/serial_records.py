"""Match serial diagnostics only after their final line terminator arrives."""
import re


def complete_lines(data):
    return data[:data.rfind(b'\n')+1]


def search_complete(pattern, data):
    # A read can end after the first digit of a counter. Regex \d+ alone would
    # accept that prefix and turn e.g. 207000 us into a spurious 2 us failure.
    # Multi-line records also wait for the terminator of the final line.
    return re.search(pattern, complete_lines(data))
