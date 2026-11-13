"""Categorical native handoff refusals; never connection or retirement proof."""
from wifi_http_reconnect5_log import canonical_decimal

KEYS = 'version captured reason frontier first sync_users poll_users pending command_failed dequeue cycle event_valid event_type event_slot event_dci event_cc event_owner observation_only'
CLOSURE_KEYS = 'version reason observation_only'

def observe(text):
    lines = text.splitlines()
    rows = [(i, line) for i, line in enumerate(lines) if line.startswith('wifi-handoff5-refusal:')]
    closures = [(i, line) for i, line in enumerate(lines) if line.startswith('wifi-handoff5-closure:')]
    if not rows and not closures:
        return dict(present=False, native_accepted=False, full_protocol_acceptance=False)
    if len(rows) != 1 or len(closures) != 1 or rows[0][0] >= closures[0][0]:
        raise ValueError('handoff refusal requires one ordered guard/closure pair')
    guard = canonical_decimal(rows[0][1], 'wifi-handoff5-refusal', KEYS)
    closure = canonical_decimal(closures[0][1], 'wifi-handoff5-closure', CLOSURE_KEYS)
    if guard['version'] != 1 or closure['version'] != 1 or guard['observation_only'] != 1 or closure['observation_only'] != 1:
        raise ValueError('unsupported handoff observation')
    if guard['captured'] not in (0, 1) or not 0 <= guard['reason'] <= 56 or bool(guard['reason']) != bool(guard['captured']) or not 0 <= closure['reason'] <= 88:
        raise ValueError('invalid refusal reason')
    for key in ('pending', 'command_failed', 'cycle', 'event_valid'):
        if guard[key] not in (0, 1):
            raise ValueError('nonboolean handoff flag')
    if not 0 <= guard['event_type'] <= 63 or not 0 <= guard['event_slot'] <= 255 or not 0 <= guard['event_dci'] <= 31 or not 0 <= guard['event_cc'] <= 255 or not 0 <= guard['event_owner'] <= 3:
        raise ValueError('invalid event metadata')
    if guard['event_valid'] and guard['reason'] not in (41, 56):
        raise ValueError('event sampled before ring admission')
    if not guard['event_valid'] and any(guard[k] for k in ('event_type', 'event_slot', 'event_dci', 'event_cc', 'event_owner')):
        raise ValueError('metadata without event')
    return dict(present=True, guard=guard, closure=closure,
                ring_empty_refusal=guard['reason'] in (41, 56),
                native_accepted=False, full_protocol_acceptance=False,
                scope='First retained refusal only; numeric predicate map in docs/wifi-handoff5-refusal.md; no ownership, radio delivery or retirement acceptance')
