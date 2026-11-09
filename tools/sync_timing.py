"""Strict opt-in SYS_SYNC wall subphases; USB observation is software only."""
import re
from usb_timing import transfer_records

def sync_timing(blobs,require_final=True):
    groups={};usb={'files':[],'devices':[]}
    for blob in blobs:
        for line in blob.decode(errors='replace').splitlines():
            if line.startswith('sync-usb-timing:'):
                match=re.fullmatch(r'sync-usb-timing: version=2 scope=(files|devices) (.*)',line)
                if not match:raise ValueError('Malformed sync USB row')
                usb[match[1]].append('usb-timing: version=2 scope=logger '+match[2]);continue
            if not line.startswith(('sync-snapshot:','sync-record:','sync-worst:')):continue
            d={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
            if d.get('version')!=1 or 'snapshot_us' not in d or any(v>=2**64 for v in d.values()):raise ValueError('Unsupported sync record')
            g=groups.setdefault(d['snapshot_us'],{'records':{},'worst':{}})
            if line.startswith('sync-snapshot:'):table=g;key='header'
            else:
                if not d.get('pid'):raise ValueError('Missing sync PID')
                table=g['records' if line.startswith('sync-record:') else 'worst'];key=d['pid']
            if key in table and table[key]!=d:raise ValueError('Conflicting sync record')
            table[key]=d
    transfers={k:transfer_records('\n'.join(v),'logger') for k,v in usb.items()}
    for at,g in sorted(groups.items(),reverse=True):
        h=g.get('header')
        if not h or (require_final and h.get('final')!=1):continue
        if any(h.get(k)!=v for k,v in [('enabled',1),('active',0),('invalid',0),('dropped',0)]):raise ValueError('Incomplete/invalid sync scope')
        if not 1<=len(g['records'])<=8 or set(g['records'])!=set(g['worst']):raise ValueError('Missing sync records')
        output=[]
        for pid,r in sorted(g['records'].items()):
            w=g['worst'][pid]
            if not {'calls','total_us','files_us','devices_us','errors','conflicts','invalid','final'}<=r.keys() or any(r.get(k)!=v for k,v in [('errors',0),('conflicts',0),('invalid',0),('final',h['final'])]):raise ValueError('Invalid sync totals')
            if not r['calls'] or r['files_us']+r['devices_us']!=r['total_us']:raise ValueError('Inconsistent sync partition')
            if not {'start_us','files_end_us','end_us','file_failed','device_failed','files_usb_failures','devices_usb_failures','final'}<=w.keys() or any(w.get(k)!=v for k,v in [('file_failed',0),('device_failed',0),('files_usb_failures',0),('devices_usb_failures',0),('final',h['final'])]):raise ValueError('Invalid sync worst call')
            if not 0<w['start_us']<=w['files_end_us']<=w['end_us']<=at or w['end_us']-w['start_us']>r['total_us']:raise ValueError('Invalid sync clocks')
            phases={}
            for name,wall in [('files',w['files_end_us']-w['start_us']),('devices',w['end_us']-w['files_end_us'])]:
                if wall>r[name+'_us']:raise ValueError('Worst phase exceeds total')
                rows=transfers[name]
                if rows is None or any((w['start_us'],p) not in rows for p in range(3)):raise ValueError('Missing/invalid sync USB evidence')
                found=[rows[(w['start_us'],p)] for p in range(3)]
                if sum(p['callback_us'] for p in found)>wall:raise ValueError('USB callbacks exceed sync phase')
                phases[name]=dict(wall_us=wall,usb=found)
            output.append(dict(pid=pid,record=r,worst=w,phases=phases))
        if not any(p['calls'] for r in output for phase in r['phases'].values() for p in phase['usb']):raise ValueError('No observed sync USB work')
        return dict(status='VALID',snapshot_us=at,workers=output,scope='SYS_SYNC file/device wall phases; retained USB callbacks include copies, software event dispatch, resume and service. Not hardware completion or CPU time.')
    raise ValueError('Missing complete final sync snapshot')
