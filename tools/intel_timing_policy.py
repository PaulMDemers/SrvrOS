"""Scoped receipt policy only; historical decoders default to100ms.

Only the exact version3 VT-d receiver selects150ms. Context-local validation
never changes raw evidence, creates hardware authority or leaks to another
receipt/thread. Admission/source age and every other deadline remain separate.
"""
from contextlib import contextmanager
from contextvars import ContextVar

_prepare = ContextVar('intel_receipt_prepare_us', default=100000)

def prepare_us():
    return _prepare.get()

@contextmanager
def preparation_policy(budget):
    if budget not in (100000,150000):
        raise ValueError('Unsupported preparation policy')
    token=_prepare.set(budget)
    try:
        yield
    finally:
        _prepare.reset(token)
