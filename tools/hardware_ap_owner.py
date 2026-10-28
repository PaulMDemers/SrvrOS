"""Strict saved receipt validation for the isolated AP ownership diagnostic."""
import re

def ap_owner_result(text):
    lines=[s for s in text.splitlines() if s.startswith(('ap-owner:', 'ap-owner-cpu:'))]
    header=r'ap-owner: v=1 admitted=1 mode=(native|vm) cpus=([0-9]+) bsp=([0-9]+) mask=([0-9]+) method=(monitor-c1|pause) rounds=32 limit_us=20000 retained=yes'
    if len(lines)<3 or not (m:=re.fullmatch(header,lines[0])):return None
    mode,n,bsp,mask,method=m.groups();n,bsp,mask=map(int,(n,bsp,mask))
    if not 2<=n<=4 or not 0<=bsp<4 or mask.bit_count()!=n or mask&~15 or not mask&(1<<bsp):return None
    if mode=='native' and (n!=4 or bsp!=0 or mask!=15 or method!='monitor-c1'):return None
    if len(lines)!=n+1 or lines[-1]!='ap-owner: completed=yes retired=yes memory=retained policy_writes=0':return None
    rows=[];seen={bsp}
    for line in lines[1:-1]:
        m=re.fullmatch(r'ap-owner-cpu: id=([0-9]+) seen=([0-9]+) signature=([0-9]+) ready_us=([0-9]+) rounds=([0-9]+) max_us=([0-9]+) retire_us=([0-9]+) state=([0-9]+) fault=([0-9]+) waits=([0-9]+)',line)
        if not m:return None
        row=dict(zip(('id','seen','signature','ready_us','rounds','max_us','retire_us','state','fault','waits'),map(int,m.groups())))
        if row['id'] in seen or row['id']!=row['seen'] or not 0<=row['id']<4 or not mask&(1<<row['id']):return None
        if row['rounds']!=32 or row['state']!=2 or row['fault'] or any(row[k]>20000 for k in ('ready_us','max_us','retire_us')):return None
        if method=='monitor-c1' and not row['waits']:return None
        if mode=='native' and row['signature']!=0x40651:return None
        seen.add(row['id']);rows.append(row)
    if sum(1<<i for i in seen)!=mask:return None
    return {'status':'PASS','mode':mode,'cpus':n,'method':method,'rows':rows,'memory':'retained-until-reset','c3_requested':False}
