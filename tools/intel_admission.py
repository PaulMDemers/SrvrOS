"""First failed excluded backing/window check; observation only, never authority."""
from intel_engine_census import _fields
FIELDS='v queries code detail pages phase reason level checked value'.split()
CODES={0,1,2,3,4,5,6,7,100,101,102,103,104,200,201,202,203,204,300,301,302,303,304}

def admission_result(text):
    lines=[l for l in text.splitlines() if l.startswith('intel-admission')]
    if not lines:return None
    if len(lines)!=1:raise ValueError('admission duplicate')
    s=_fields(lines[0],'intel-admission:',FIELDS)
    if any(word.partition('=')[2]!=str(s[word.partition('=')[0]]) for word in lines[0].removeprefix('intel-admission:').split()):raise ValueError('admission noncanonical number')
    if s['v']!=1 or s['queries']>2000000 or s['code'] not in CODES or any(v>=1<<64 for v in s.values()):raise ValueError('admission schema/range')
    q=('pages','phase','reason','level','checked','value')
    if s['code']!=7:
        if any(s[k] for k in q):raise ValueError('admission foreign query witness')
        maximum={0:0,1:0,2:0,3:0,4:0,5:4095,6:4096,100:0,101:7,102:0,103:1535,104:31,200:0,201:0,202:0,203:0,204:0,300:0,301:15,302:7,303:0,304:31}[s['code']]
        if s['detail']>maximum:raise ValueError('admission detail')
    else:
        if not s['queries'] or s['detail'] or s['pages']>1536 or s['phase']>3 or not 1<=s['reason']<=17:raise ValueError('admission query witness')
        phase,reason,level,checked,value=(s[k] for k in ('phase','reason','level','checked','value'))
        if phase==0:
            if reason!=9 or level or checked or not 1<=value<=7:raise ValueError('admission query input')
        elif phase in (1,3):
            if not 1<=reason<=8 or level or (checked!=0 if phase==1 else not s['pages'] or checked!=s['pages']):raise ValueError('admission root phase')
            if reason==1:
                if value!=512:raise ValueError('admission IF witness')
            elif reason==7:
                if not 1<=value<=64:raise ValueError('admission root count')
            elif reason==8:
                if not value:raise ValueError('admission occupancy')
            elif value:raise ValueError('admission root value')
        elif phase==2:
            levels={10:{1},11:{1},12:{2,3,4},13:{2},14:{3},15:{4},16:{4},17:{0}}
            if reason not in levels or level not in levels[reason] or checked>=s['pages'] or value:raise ValueError('admission walk prefix')
    return dict(summary=s,hardware_ownership=False,managed_range=False,physical_tearfree=False)
