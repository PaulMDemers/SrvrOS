"""Synthetic guest publication fixture; never native DMA/scanout acceptance."""
import re

def model_result(text):
    lines=[line for line in text.splitlines() if line.startswith('scanout-publish-model:')]
    if not lines:return None
    if len(lines)!=1:raise ValueError('duplicate publication model receipt')
    match=re.fullmatch(r'scanout-publish-model: v=1 ok=([01]) cases=([0-2]) pages=32 writes=([0-9]+) flushes=([0-2]) quarantined=(0|32) hardware_writes=0 dma_authority=0 ownership=model',lines[0])
    if not match:raise ValueError('malformed publication model receipt/authority claim')
    ok,cases,writes,flushes,quarantined=map(int,match.groups())
    if writes>65 or (ok and (cases,writes,flushes,quarantined)!=(2,65,2,32)):
        raise ValueError('incomplete publication model success')
    return dict(complete=bool(ok),cases=cases,pages=32,writes=writes,flushes=flushes,quarantined=quarantined,hardware_writes=0,dma_authority=False,physical_tearfree=False,ownership='model')
