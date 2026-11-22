#!/usr/bin/env python3
"""Validate bounded Intel inherited-display observations, never scanout ownership."""
import argparse
import json
from pathlib import Path
import re

SCHEMAS = {
    'observe': 'v status maps retained capture_writes ownership',
    'identity': 'v sample id class_rev subsystem pch command pm power_valid firmware safe bar0 bar1 bar2 bar3',
    'framebuffer': 'v physical width height pitch bpp model red_size red_shift green_size green_shift blue_size blue_shift',
    'sample': 'v sample begin_us end_us reads before0 before1 before2 before3 after0 after1 after2 after3',
    'pipe': 'v sample pipe source line frames control stride surface offset live cursor_control cursor_base',
    'transcoder': 'v sample transcoder htotal vtotal vblank ddi config',
    'relation': 'v active_planes linear_relation ggtt_backing',
}
STATUSES = {'observed','invalid','identity-refused','access-refused','framebuffer-refused',
            'map-failed','well-off','register-invalid','unstable','clock-invalid','mapping-retained'}
TEXT = {'status', 'ownership', 'ggtt_backing'}


def capture_result(text):
    rows = {key: [] for key in SCHEMAS}
    for line in text.splitlines():
        match = re.fullmatch(r'intel-display-([a-z]+): (v=.*)', line)
        if not match:
            continue
        kind, body = match.groups()
        if kind not in SCHEMAS:
            raise ValueError('unknown Intel capture row')
        fields = {}
        for token in body.split():
            key, sep, raw = token.partition('=')
            if not sep or key in fields:
                raise ValueError('malformed/duplicate Intel capture field')
            if key in TEXT:
                if not re.fullmatch('[a-z-]{1,32}', raw):
                    raise ValueError('invalid Intel capture label')
                fields[key] = raw
            else:
                if not re.fullmatch(r'(?:0x[0-9a-fA-F]{1,16}|[0-9]{1,20})', raw):
                    raise ValueError('invalid Intel capture number')
                fields[key] = int(raw, 16 if raw.startswith('0x') else 10)
                if fields[key] >= 1 << 64:
                    raise ValueError('Intel capture number overflow')
        if set(fields) != set(SCHEMAS[kind].split()) or fields['v'] != 1:
            raise ValueError('unsupported/incomplete Intel capture schema')
        rows[kind].append(fields)
    if not any(rows.values()):
        return None
    for key, count in {'observe':1,'identity':2,'framebuffer':1,'sample':2,'pipe':6,'transcoder':8,'relation':1}.items():
        if len(rows[key]) != count:
            raise ValueError('incomplete/duplicate Intel capture rows: '+key)
    summary = rows['observe'][0]
    if summary['status'] not in STATUSES or summary['capture_writes'] or summary['ownership'] != 'none':
        raise ValueError('invalid capture outcome or claimed hardware ownership')
    if summary['maps'] > 9 or summary['retained'] >= 1 << 9:
        raise ValueError('invalid Intel mapping receipt')
    for kind, axes in [('identity',('sample',)),('sample',('sample',)),('pipe',('sample','pipe')),('transcoder',('sample','transcoder'))]:
        expected = {(n,) for n in range(2)} if len(axes)==1 else {(n,i) for n in range(2) for i in range(3 if kind=='pipe' else 4)}
        actual = [tuple(row[axis] for axis in axes) for row in rows[kind]]
        if len(set(actual)) != len(actual) or set(actual) != expected:
            raise ValueError('invalid Intel row identity')
        rows[kind].sort(key=lambda row:tuple(row[axis] for axis in axes))
    relation = rows['relation'][0]
    if relation['ggtt_backing'] != 'unproven' or relation['active_planes'] > 7 or relation['linear_relation'] & ~relation['active_planes']:
        raise ValueError('invalid Intel surface relation')
    observed = summary['status'] == 'observed'
    if observed:
        if summary['maps'] != 9 or summary['retained']:
            raise ValueError('incomplete Intel mapping retirement')
        ids = rows['identity']
        if {k:v for k,v in ids[0].items() if k!='sample'} != {k:v for k,v in ids[1].items() if k!='sample'}:
            raise ValueError('changed Intel identity/access endpoints')
        for key, value in {'id':0x16268086,'class_rev':0x03000009,'subsystem':0x011b106b,'pch':0x9cc38086,'firmware':1,'safe':1,'power_valid':1}.items():
            if ids[0][key] != value:
                raise ValueError('unmatched Intel profile/access')
        if ids[0]['pm'] & 3 or not ids[0]['command'] & 2:
            raise ValueError('inaccessible Intel device')
        fb = rows['framebuffer'][0]
        if not (0 < fb['width'] <= 4096 and 0 < fb['height'] <= 4096 and fb['width']*4 <= fb['pitch'] <= 65536):
            raise ValueError('invalid Intel framebuffer dimensions')
        for key,value in {'bpp':32,'model':1,'red_size':8,'red_shift':16,'green_size':8,'green_shift':8,'blue_size':8,'blue_shift':0}.items():
            if fb[key] != value:
                raise ValueError('unsupported Intel framebuffer layout')
        samples = rows['sample']
        guards = None
        for sample in samples:
            if sample['reads'] != 58 or not 0 <= sample['end_us']-sample['begin_us'] <= 100000:
                raise ValueError('incomplete/unbounded Intel snapshot')
            before = [sample['before'+str(i)] for i in range(4)]
            after = [sample['after'+str(i)] for i in range(4)]
            if before != after or any(v >= 0xffffffff or not v & (1 << 30) for v in before) or (guards is not None and before != guards):
                raise ValueError('unstable/off Intel power well')
            guards = before
        if not 20000 <= samples[1]['begin_us']-samples[0]['end_us'] <= 100000:
            raise ValueError('invalid Intel sample separation')
        for kind, half, changing in [('pipe',3,{'line','frames'}),('transcoder',4,set())]:
            values = rows[kind]
            for first, second in zip(values[:half],values[half:]):
                if {k:v for k,v in first.items() if k not in changing|{'sample'}} != {k:v for k,v in second.items() if k not in changing|{'sample'}}:
                    raise ValueError('unstable Intel display controls')
                if any(v >= 0xffffffff for k,v in first.items() if k not in {'v','sample','pipe','transcoder'}):
                    raise ValueError('invalid Intel register value')
    return dict(status=summary['status'],observation_complete=observed,scanout_ownership=False,
                physical_tearfree=False,rows=rows)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--log',type=Path,required=True)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=capture_result(args.log.read_text(errors='replace'))
    if result is None:parser.error('no Intel capture receipt')
    data=json.dumps(result,indent=2)+'\n'
    if args.output:args.output.write_text(data)
    else:print(data,end='')


if __name__=='__main__':main()
