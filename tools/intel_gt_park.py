"""Replay the powered Gen8 software park observation, including rollback."""
import base64
import re
import struct

from intel_gt_bootstrap import fields

AUTO = 0x90040000
PARK = 4 << 16
GAM = 0x5f


def require(value):
    if not value:
        raise ValueError('Invalid software park evidence')


def arbiter(words, opened):
    request, response, reset = words
    return (request & GAM == response & GAM == (GAM if opened else 0)
            and not request & (0xff00 if opened else 0x8000) and not reset & 0x400)


def open_frame(w):
    return (arbiter(w[:3], True) and arbiter(w[-3:], True)
            and all(v & 0x30 == 0x10 for v in w[3:8])
            and all(v & 0x10 for v in (w[8], w[9], w[11])) and not w[10] & 3)


def held_frame(w):
    return (arbiter(w[:3], False) and arbiter(w[-3:], False)
            and all(not v & 0xff0000 for v in w[3:7]) and all(v & 0x31 == 0x20 for v in w[7:12])
            and not (w[12] | w[14] | w[16]) & 0x11 and w[13] & 1 and not w[15] & 2)


def park_result(text, cold, change, timing, wake_complete):
    lines = text.splitlines()
    headers = [l for l in lines if l.startswith('intel-park:')]
    require(len(headers) == 1)
    regs = ('ctl', 'target', 'set_ctl', 'set_target', 'final_ctl', 'final_target', 'restore_ctl', 'restore_target')
    counts = ('attempts', 'returns', 'opened', 'unmapped', 'sampled', 'held', 'reopened', 'returned')
    h = fields(headers[0], 'intel-park:', ('v', *counts, *regs), regs)
    require(h['returns'] <= h['attempts'] <= 2 and h['unmapped'] <= h['opened'] <= 1 and
            max(h['sampled'], h['reopened'], h['returned']) <= 1 and h['held'] <= 2)
    frames = []
    previous = 0
    for line in lines:
        if not line.startswith('intel-park-frame:'):
            continue
        m = re.fullmatch(r'intel-park-frame: v=1 n=(\d+) op=([0-3]) begin=(\d+) end=(\d+) words=(\d+) data=(\S*)', line)
        require(m)
        n, op, begin, end, size = map(int, m.groups()[:5])
        raw = base64.b64decode(m[6], validate=True)
        require(n == len(frames) < 12 and size <= (15, 16, 20, 15)[op] and len(raw) == size * 4
                and begin >= previous and (not end or end > begin))
        frames.append(dict(op=op, begin=begin, end=end, words=struct.unpack(f'<{size}I', raw)))
        previous = max(end, begin)
    result = dict(summary=h, frames=frames, complete=False, powered_block_observed=False,
                  fresh_engine_mask=0, fresh_l3=False, hardware_ownership=False)
    if not (h['sampled'] and h['returned'] and h['reopened'] and len(cold.get('samples', [])) == 9):
        require(not wake_complete)
        return result
    require(h['attempts'] == h['returns'] == 2 and h['opened'] == h['unmapped'] == 1 and
            tuple(h[k] for k in regs) == (AUTO, PARK, PARK, PARK, PARK, PARK, AUTO, PARK))
    polls = [f for f in frames if f['op'] == 1]
    require(1 <= len(polls) <= 8 and h['held'] in (0, 2) and (h['held'] or len(polls) == 8))
    require([f['op'] for f in frames] == [0] + [1]*len(polls) + [2]*h['held'] + [3])
    for f in frames:
        require(len(f['words']) == (15, 16, 20, 15)[f['op']] and 0xffffffff not in f['words']
                and 0 < f['end'] - f['begin'] <= 10000)
    require(open_frame(frames[0]['words']) and open_frame(frames[-1]['words']) and
            change['summary']['apply_end'] <= frames[0]['begin'] and frames[-2]['end'] <= timing['gt_end']
            and frames[-2]['end'] - polls[0]['begin'] <= 10000 and
            cold['samples'][5]['end'] <= frames[-1]['begin'] < frames[-1]['end'] <= change['summary']['restore_begin'])
    low = high = fresh = drained = False
    engine_ack = go_closed = request_closed = response_closed = 0
    for f in polls:
        require(not drained)
        w = f['words']
        for start in (0, 13):
            req, resp, reset = w[start:start+3]
            require(not (req & (request_closed | 0x8000) or resp & response_closed or
                         (req & ~resp & GAM) or reset & 0x400))
            request_closed |= ~req & GAM
            response_closed |= ~resp & GAM
            if start == 0:
                for i, bit in enumerate((0, 1, 2, 4, 3, 6)):
                    v = w[6+i]
                    ack = (i < 5 and v & 0x20)
                    permit = v & 0x10
                    require(v & 0x11 != 1 and (request_closed & (1 << bit) or (permit and not ack))
                            and (not ack or (not permit and response_closed & (1 << bit)))
                            and (not engine_ack & (1 << i) or ack) and (not go_closed & (1 << i) or not permit))
                    if ack:
                        engine_ack |= 1 << i
                    if not permit:
                        go_closed |= 1 << i
        closed = arbiter(w[:3], False) and arbiter(w[-3:], False)
        pulse = bool(w[4] & 1)
        require(not w[4] & 2 and (not pulse or (not (w[3] | w[5]) & 0x11 and request_closed)))
        if closed and not (w[3] | w[5] | w[11]) & 0x11:
            if not pulse:
                low = True
            elif low and not high:
                fresh = True
        high = pulse
        drained = bool(closed and fresh and engine_ack == 31 and go_closed == 63 and w[12] & 1)
    require(drained == bool(h['held']) and all(held_frame(f['words']) for f in frames if f['op'] == 2))
    result.update(complete=True, powered_block_observed=drained, fresh_engine_mask=engine_ack, fresh_l3=fresh)
    return result
