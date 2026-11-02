"""Lossless v2 fixed-order CPU capture records; legacy semantics stay elsewhere."""
import functools
import re

POWER_KEYS = 'power_state flags energy_shift package_mw active_mhz nominal_mhz package_uj sample_us interval_us samples rejected energy aperf mperf monotonic_us'.split()
IDLE_KEYS = 'supported valid rejected sample_us interval_us interval_tsc tsc_begin tsc_end'.split()+[f'c{i}_{k}' for i in range(10) for k in ('raw','bp')]
ENTRY_KEYS = 'sample_us epoch enters returns ready'.split()
SCHEMAS = {'p':('cpu-load-power:',POWER_KEYS),'i':('cpu-load-idle:',IDLE_KEYS),'e':('cpu-idle-entry:',ENTRY_KEYS)}


def expand_capture(text):
    lines=text.splitlines(keepends=True)
    tagged=[x for x in lines if (x.startswith('cpu-capture-') or re.match(r'cpu-[a-z]2:',x))]
    if not tagged:return text
    headers=[x.rstrip('\n') for x in lines if x.startswith('cpu-capture-format:')]
    if len(headers)!=1:raise ValueError('capture header missing/duplicate')
    m=re.fullmatch(r'cpu-capture-format: v=2 idle=([01]) entry=([01])',headers[0])
    if not m:raise ValueError('unknown capture schema')
    idle,entry=map(int,m.groups())
    receipts=re.findall(r'^cpu-load-power: retained=(\d+) overflow=(\d+)$',text,re.M)
    if len(receipts)!=1 or receipts[0][1]!='0' or not 0<int(receipts[0][0])<=24:raise ValueError('capture count/overflow')
    count=int(receipts[0][0]);seen={k:0 for k in SCHEMAS};out=[]
    if any(x.startswith(('cpu-load-power: power_state=','cpu-load-idle:','cpu-idle-entry:')) for x in lines):raise ValueError('mixed capture encodings')
    for line in lines:
        if line.startswith('cpu-capture-format:'):continue
        if not line.startswith(('cpu-p2:','cpu-i2:','cpu-e2:')):
            if line.startswith('cpu-capture-') or re.match(r'cpu-[a-z]2:',line):raise ValueError('unknown capture record')
            out.append(line);continue
        m=re.fullmatch(r'cpu-([pie])2: i=(\d+) v=(\d+(?:,\d+)*)\n?',line)
        if not m:raise ValueError('malformed capture row')
        kind,index,values=m.groups();index=int(index);values=list(map(int,values.split(',')))
        prefix,keys=SCHEMAS[kind]
        if index!=seen[kind] or index>=count or len(values)!=len(keys) or any(v>=2**64 for v in values):raise ValueError('capture order/width/count')
        seen[kind]+=1
        out.append(prefix+' '+' '.join(f'{k}={v}' for k,v in zip(keys,values))+'\n')
    if seen!={'p':count,'i':count if idle else 0,'e':count if entry else 0}:raise ValueError('missing/unexpected capture row')
    return ''.join(out)


def capture_input(fn):
    @functools.wraps(fn)
    def parse(text,*args,**kwargs):
        try:text=expand_capture(text)
        except ValueError:return None
        return fn(text,*args,**kwargs)
    return parse


def compact_legacy(text):
    """Regression modeling only: preserve every numeric field; never acceptance."""
    if 'cpu-capture-format:' in text:raise ValueError('already encoded')
    out=[];counts={k:0 for k in SCHEMAS}
    idle='cpu-load-idle:' in text;entry='cpu-idle-entry:' in text
    inserted=False
    for line in text.splitlines(keepends=True):
        kind=next((k for k,(p,_) in SCHEMAS.items() if line.startswith(p) and not (k=='p' and line.startswith('cpu-load-power: retained='))),None)
        if kind is None:out.append(line);continue
        prefix,keys=SCHEMAS[kind];parts=line.split()[1:]
        pairs=[p.split('=') for p in parts]
        if any(len(x)!=2 for x in pairs) or [x[0] for x in pairs]!=keys or any(not x[1].isdigit() for x in pairs):raise ValueError('noncanonical legacy row')
        if not inserted:
            out.append(f'cpu-capture-format: v=2 idle={int(idle)} entry={int(entry)}\n');inserted=True
        out.append(f'cpu-{kind}2: i={counts[kind]} v='+','.join(x[1] for x in pairs)+'\n');counts[kind]+=1
    result=''.join(out)
    # A field-for-field roundtrip is mandatory, including failed legacy values.
    if expand_capture(result)!=text:raise ValueError('lossy legacy model')
    return result
