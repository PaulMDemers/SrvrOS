"""Strict repeated-IN visit ledger proof; the caller separately proves MAC/RF
order, parent initialization, raw-prefix checkpoints and whole-run completion.
This parser never edits raw input or infers physical reception from quiet data.
"""
import re

FIELDS = {
 'wifi-burst-begin': 'channel first early_events early_cc early_remaining inspected budget_us pages retained reused data_out',
 'wifi-burst-post': 'index td adopted submitted ring buffer trb retained reused',
 'wifi-burst-event': 'index td cc remaining actual during_window inspected',
 'wifi-burst-poll': 'ok attempts completions polls elapsed_us first deadline_reset',
 'wifi-burst': 'attempted complete failed first posts attempts completions beacons ignored late polls elapsed_us stopped retained reused deadline_reset identifiers_logged',
 'wifi-burst-page': 'index physical retained reused',
 'wifi-burst-item': 'index td submitted completed cc remaining actual inspected view beacon_status consumed mpdu frame_control beacon interval ssid_size privacy rsn wpa ds probe identifiers_logged',
}
HEX = {'ring', 'buffer', 'trb', 'physical', 'frame_control', 'old_ring', 'dequeue', 'filter', 'txop', 'tx_alc', 'ibi9'}


def record(line, tag, fields=None):
    """Exact ordered keys, canonical uint64 decimal/hex values, no suffixes."""
    assert line.startswith(tag + ': '), line
    words = line[len(tag) + 2:].split(' ')
    keys = (FIELDS[tag] if fields is None else fields).split()
    assert len(words) == len(keys), line
    result = {}
    for word, key in zip(words, keys):
        name, value = word.split('=')
        assert name == key, line
        if key in HEX:
            assert re.fullmatch(r'0x[0-9a-f]{16}', value), line
            number = int(value, 16)
        else:
            assert re.fullmatch(r'0|[1-9][0-9]*', value), line
            number = int(value)
            assert number < 1 << 64, line
        result[key] = number
    return result


def visit(leg, channel):
    assert channel in (1, 6, 11)
    lines = leg.splitlines()
    rows = {tag: [] for tag in FIELDS}
    for pos, line in enumerate(lines):
        if line.startswith('wifi-burst'):
            tag = line.split(':', 1)[0]
            assert tag in FIELDS, line
            rows[tag].append((pos, record(line, tag)))

    def one(tag):
        assert len(rows[tag]) == 1, tag
        return rows[tag][0]

    begin_pos, begin = one('wifi-burst-begin')
    poll_pos, poll = one('wifi-burst-poll')
    summary_pos, summary = one('wifi-burst')
    first = begin['first']
    assert first in (0, 1) and begin['channel'] == channel
    assert (begin['inspected'], begin['budget_us'], begin['pages'], begin['retained'], begin['reused'], begin['data_out']) == (0, 10000000, 9, 1, 0, 0)
    if first:
        assert begin['early_events'] == 1 and begin['early_cc'] in (1, 13) and begin['early_remaining'] <= 4096
    else:
        assert not any(begin[k] for k in ('early_events', 'early_cc', 'early_remaining'))
    assert begin_pos < poll_pos < summary_pos
    assert summary['attempted'] == summary['complete'] == summary['stopped'] == summary['retained'] == 1
    assert not any(summary[k] for k in ('failed', 'ignored', 'reused', 'deadline_reset', 'identifiers_logged'))
    assert summary['first'] == poll['first'] == first and poll['ok'] == 1 and not poll['deadline_reset']
    attempts, completed = poll['attempts'], poll['completions']
    assert 1 <= attempts <= 8 and 0 <= completed <= attempts and attempts-completed <= 1
    for key in ('attempts', 'completions', 'polls', 'elapsed_us'):
        assert poll[key] == summary[key]
    assert completed <= poll['polls'] < 20000000 and poll['polls'] > 0
    assert poll['elapsed_us'] <= 10100000 and (completed == 8 or poll['elapsed_us'] >= 10000000)
    assert summary['posts'] == first + attempts and summary['late'] <= 60000000
    assert len(rows['wifi-burst-page']) == 9
    pages = []
    for i, (pos, page) in enumerate(rows['wifi-burst-page']):
        assert pos > summary_pos and page['index'] == i and page['retained'] == 1 and not page['reused']
        address = page['physical']
        assert address and address % 4096 == 0 and address <= (1 << 64)-4096 and address not in pages
        pages.append(address)
    initial = [record(x, 'wifi-channel-rx-post', 'count requested submitted ring buffer trb old_ring old_pages distinct retained reused') for x in lines if x.startswith('wifi-channel-rx-post:')]
    assert len(initial) == 1
    initial = initial[0]
    ring, old = initial['ring'], initial['old_ring']
    assert ring and old and ring % 4096 == old % 4096 == 0 and ring != old
    assert initial['trb'] == ring and initial['buffer'] == pages[0] and ring not in pages and old not in pages
    assert tuple(initial[k] for k in ('count', 'requested', 'submitted', 'old_pages', 'distinct', 'retained', 'reused')) == (1, 4096, 1, 8, 1, 1, 0)
    posts, events, items = rows['wifi-burst-post'], rows['wifi-burst-event'], rows['wifi-burst-item']
    assert len(posts) == len(items) == attempts and len(events) == completed
    metadata = []
    for i, ((pos, post), (item_pos, item)) in enumerate(zip(posts, items)):
        td = first + i
        assert begin_pos < pos < poll_pos and item_pos > summary_pos
        assert post['index'] == item['index'] == i and post['td'] == item['td'] == td
        assert (post['adopted'], post['submitted'], post['ring'], post['buffer'], post['trb'], post['retained'], post['reused']) == (int(td == 0), 1, ring, pages[td], ring+16*td, 1, 0)
        assert item['submitted'] == 1 and item['completed'] == int(i < completed) and item['identifiers_logged'] == 0
        for key in ('inspected', 'beacon', 'privacy', 'rsn', 'wpa', 'ds', 'probe'):
            assert item[key] in (0, 1)
        if i < completed:
            event_pos, event = events[i]
            assert pos < event_pos < (posts[i+1][0] if i+1 < attempts else poll_pos)
            assert event['index'] == i and event['td'] == td and event['during_window'] == 1 and event['inspected'] == 0
            assert event['cc'] == item['cc'] and event['remaining'] == item['remaining'] and event['actual'] == item['actual']
            assert item['cc'] in (1, 13) and item['remaining'] <= 4096 and item['actual'] == 4096-item['remaining']
            assert item['inspected'] == int(item['actual'] > 0)
        else:
            assert not any(item[k] for k in ('cc', 'remaining', 'actual', 'inspected'))
        if not item['actual']:
            assert (item['view'], item['beacon_status']) == (255, 255)
            assert not any(item[k] for k in ('consumed', 'mpdu', 'frame_control', 'beacon'))
        elif item['view'] == 0:
            assert 24 <= item['mpdu'] and item['mpdu']+36 <= item['consumed'] <= item['actual']
            assert item['frame_control'] <= 65535 and not item['frame_control'] & (3 | 12 | 0xc700)
            assert item['beacon_status'] in (0, 1, 2, 3)
        else:
            assert item['view'] in (1, 2) and item['beacon_status'] == 255
            assert not any(item[k] for k in ('consumed', 'mpdu', 'frame_control'))
        assert item['beacon'] == int(item['actual'] > 0 and item['view'] == item['beacon_status'] == 0)
        if item['beacon']:
            assert 0 < item['interval'] <= 65535 and item['ssid_size'] <= 32
            assert item['frame_control'] & 0xf0 in (0x80, 0x50) and item['probe'] == int(item['frame_control'] & 0xf0 == 0x50)
            assert item['mpdu'] >= 36+2+item['ssid_size']+3+(3 if item['ds'] else 0)+(4 if item['rsn'] else 0)+(6 if item['wpa'] else 0)
            metadata.append(dict(item, channel=channel))
        else:
            assert not any(item[k] for k in ('interval', 'ssid_size', 'privacy', 'rsn', 'wpa', 'ds', 'probe'))
    assert summary['beacons'] == len(metadata)
    stop_rows = [(i, record(x, 'wifi-channel-rx-stop', 'attempted ok state dequeue trb retained')) for i, x in enumerate(lines) if x.startswith('wifi-channel-rx-stop:')]
    assert len(stop_rows) == 1
    stop_pos, stop = stop_rows[0]
    last = ring+16*(first+attempts-1)
    assert poll_pos < stop_pos < summary_pos and stop == dict(attempted=1, ok=1, state=3, dequeue=stop['dequeue'], trb=last, retained=1)
    assert stop['dequeue'] in (last | 1, (last+16) | 1)
    guards = [i for i, x in enumerate(lines) if x.startswith('wifi-channel-rx-guard:')]
    assert len(guards) == 4 and stop_pos < guards[0] < guards[-1] < summary_pos
    for pos, _ in items:
        assert pos > guards[-1]
    return dict(channel=channel, ring=ring, old_ring=old, pages=pages, first=first,
                early=begin, window=summary, items=[x for _, x in items], beacons=metadata,
                pending=attempts-completed, empty=sum(x['completed'] and not x['actual'] for _, x in items),
                unrelated=sum(x['actual'] > 0 and (x['view'] == 2 or x['beacon_status'] == 2) for _, x in items),
                rejected=sum(x['actual'] > 0 and not x['beacon'] and x['view'] != 2 and x['beacon_status'] != 2 for _, x in items))
