"""Strict validation of the desktop trial's auxiliary records and durable payload."""
import binascii
import hashlib
import re

BYTES=256*1024

def record(data,run,kind,numbers):
    if not data or len(data)>2048: raise ValueError('Missing/oversized desktop I/O record')
    lines=data.decode('ascii').splitlines()
    pairs=[line.split('=',1) for line in lines]
    if any(len(pair)!=2 for pair in pairs): raise ValueError('Malformed desktop I/O record')
    fields=dict(pairs)
    if len(fields)!=len(pairs) or set(fields)!={'format','run','crc32',*numbers}:
        raise ValueError('Unexpected desktop I/O fields')
    if fields['format']!=f'srvros-desktop-{kind}-v1' or fields['run']!=run:
        raise ValueError('Desktop I/O identity mismatch')
    for key in (*numbers,'crc32'):
        if not re.fullmatch(r'[0-9]{1,20}',fields[key]): raise ValueError('Invalid desktop I/O number')
    tail=f"crc32={fields['crc32']}\n".encode()
    if not data.endswith(tail) or binascii.crc32(data[:-len(tail)])&0xffffffff!=int(fields['crc32']):
        raise ValueError('Desktop I/O checksum mismatch')
    return {key:int(fields[key]) for key in numbers}

def validate(run,worker_data,ui_data,payload):
    worker=record(worker_data,run,'io',('state','rounds','bytes'))
    ui=record(ui_data,run,'ui',('state','rounds','keys','motions','wheels','focus_changes','cancels'))
    rounds=worker['rounds']
    if worker['state'] not in (2,3) or ui['state']!=worker['state'] or ui['rounds']!=rounds:
        raise ValueError('Desktop I/O worker failed or results disagree')
    if not 1<=rounds<=128 or worker['bytes']!=rounds*BYTES or len(payload)!=BYTES:
        raise ValueError('Incomplete desktop I/O payload')
    if worker['state']==2 and rounds!=128: raise ValueError('Premature desktop I/O completion')
    expected=bytes((i*17+rounds*29+ord(run[i%16]))&255 for i in range(BYTES))
    if payload!=expected: raise ValueError('Durable desktop I/O payload mismatch')
    covered=rounds>=16 and ui['keys']>=8 and ui['motions']>0 and ui['wheels']>0 and ui['focus_changes']>=2 and ui['cancels']>0
    return {'status':'EXERCISED' if covered else 'INCOMPLETE','worker':worker,'input_during_workload':ui,
            'payload_sha256':hashlib.sha256(payload).hexdigest()}


def validate_timing(run,data):
    fields=('clock','active_count','active_p95_us','active_p99_us','active_max_us',
            'render_count','render_p95_us','render_p99_us','render_max_us')
    values=record(data,run,'timing',fields)
    if values['clock']!=1: raise ValueError('No reference clock for desktop timing')
    for kind in ('active','render'):
        if not values[kind+'_count'] or not 0<=values[kind+'_p95_us']<=values[kind+'_p99_us']<=values[kind+'_max_us']:
            raise ValueError('Incomplete/inconsistent desktop timing')
    return values


def timing_gate(t):
    return (t['active_count']>=100 and t['active_p95_us']<=50000 and
            t['active_p99_us']<=100000 and t['active_max_us']<=250000 and
            t['render_p95_us']<=16667)


def validate_text(run,data):
    values=record(data,run,'text',('clock','sent','presented','overflow','p95_us','p99_us','max_us'))
    if values['clock']!=1 or values['presented']>values['sent'] or values['overflow']>values['sent'] or not 0<=values['p95_us']<=values['p99_us']<=values['max_us']:
        raise ValueError('Incomplete/inconsistent desktop text timing')
    if not values['presented'] and any(values[k] for k in ('p95_us','p99_us','max_us')):
        raise ValueError('Text latency without samples')
    return values


def text_gate(t):
    return (t['sent']>=32 and t['presented']==t['sent'] and not t['overflow'] and
            t['p95_us']<=50000 and t['p99_us']<=100000 and t['max_us']<=250000)
