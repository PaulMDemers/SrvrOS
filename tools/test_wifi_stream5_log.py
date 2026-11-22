"""Strict stream receipt mutations; all generated test parents are modeled.

WIFI_STREAM5_TRACE may point to an independently run actual native-C fixture.
WIFI_STREAM5_PARENT may supply an unchanged complete parent for composition.
Neither optional fixture test grants physical hardware acceptance.
"""
import os
from pathlib import Path
import re
import unittest
from unittest.mock import patch
import hardware_boot as boot
import wifi_eapol5_log as eapol
import wifi_stream5_log as c

RELEASE='0123456789abcdef'


def boot_rows():
    config=boot.limine_config(RELEASE,'ram-usb-wifi-fw-v135-laptop-probe-12345678-auto-return')
    tokens=next(x.strip()[9:] for x in config.splitlines() if x.strip().startswith('cmdline: '))
    return ['cmdline: '+tokens,'laptop-capture: boot options='+tokens]


def emit(prefix,**values):return prefix+': '+' '.join(f'{k}={v}' for k,v in values.items())


def synthetic_child(slot=1,tx=0x300000,rx=0x400000,old=None):
    """Modeled raw receipts, never a substitute for the actual producer fixture."""
    old=old or [3,33554486,0x100001,0,4096,0,0,0]
    rows=[c.PROGRESS,emit('wifi-stream5-plan',tx=tx,rx=rx,epoch=1,slot=slot,pages=4,positions=63,max_tx=80,max_rx=252,max_fences=4)]
    event=lambda d:(slot<<24)|((8+d)<<16)|(32<<10)|1
    i=0
    for n in range(75):
        rows.append(f'wifi-stream5-td: {i} 0 0x{tx+16*(n%63):016x} {1<<24} {event(0)}');i+=1
        if n<74:rows.append(f'wifi-stream5-td: {i} 1 0x{rx+16*(n%63):016x} {(13<<24)|3700} {event(1)}');i+=1
    for n in range(75):
        if 4<=n<74:
            pn=n-3;length=(404,404,104,136,104,360)[pn-1] if pn<=6 else 360
            rows.append(f'wifi-stream5-data: {2*n} {2*n+2} {n+1} {n%4} {1^((n//63)&1)} {pn} {length}')
        if n in (4,62,63,73):rows.append(f'wifi-stream5-rx: {2*n+1} {2*n+1} {n+1} {n%4} {1^((n//63)&1)} 298 9')
    for i in range(4):
        d=i//2;ring=(tx,rx)[d]
        rows.append(emit('wifi-stream5-fence',i=i,p=0x900020+16*i,t=15+i%2,d=8+d,q=ring if i%2 else 0,e=0x900020+16*i,s=1<<24,c=(slot<<24)|(33<<10)|1,sp=0 if i%2 else ring,ss=0 if i%2 else 27<<24,sc=0 if i%2 else event(d),sq=0 if i%2 else ring,used=1))
    for i,(kind,d,offset) in enumerate(((12,8,0),(16,9,1),(15,9,6),(15,8,7),(16,9,8),(12,8,9))):
        q=rx|1 if i==1 else old[2]|old[3]<<32 if i==4 else 0
        rows.append(emit('wifi-stream5-sync',i=i,p=0x900000+16*offset,e=0x900000+16*offset,t=kind,d=d,q=q,slot=slot,cc=1,delta=1))
    configured=[[1,33554454,tx|1,0,1580,0,0,0],old.copy()];configured[1][2]=rx|1;configured[1][3]=0
    stopped=[configured[0].copy(),configured[1].copy()]
    for d,n in enumerate((12,11)):stopped[d][0]=3;stopped[d][2]=(tx,rx)[d]+16*n
    final=stopped[0].copy();final[0]=0
    contexts={0:[[0]*8,old],2:configured,5:stopped,9:[final,old]}
    for phase in (0,2,5,9):
        for d in (0,1):rows.append(emit('wifi-stream5-context',p=phase,d=d,**{'w'+str(i):v for i,v in enumerate(contexts[phase][d])}))
    for d,n in enumerate((12,11)):rows.append(emit('wifi-stream5-stop',d=d,p=(tx,rx)[d]+16*n,s=(27 if d==0 else 26)<<24,c=event(d),q=(tx,rx)[d]+16*n))
    rows.extend([emit('wifi-stream5-state',failed=0,phase=9,first_phase=0,pending_tx=0,pending_rx=0,round=0,command_pending=0,fault_event=0,p=0,s=0,c=0),emit('wifi-stream5-keys',auth=1,assoc=1,m1=1,m2=1,m3=1,m4=1,activated=1,erased=1,leave=1,unsafe=0,installs=1,pn=70),emit('wifi-stream5-probe',complete=1,unsafe=0,setup=1,setup_sends=6,dhcp=1,dns=1,echo=1,sent=64,matched=64,elapsed_us=5000,private_erased=1,timed_out=0,exhausted=0,clock_fault=0,polls=64,ignored=0,total_polls=70),emit('wifi-stream5',complete=1,parent=1,restored=1,retired=1,empty=1,tx=75,rx=75,tx_wraps=1,rx_wraps=1,data_tx=70,data_rx=70,tx_after=11,rx_after=11,borrowed=74,released=74,faults=0,commands=6,fences=2,rx_receipts=4,retained=1,ordinary_rx=0,tcp=0,http=0,backend_retired=0)])
    return rows


def minimal_parent(child):
    lines=list(child)
    plan=c.fields(lines[1],'wifi-stream5-plan')
    old={}
    for d in (0,1):
        a=c.fields(next(s for s in lines if s.startswith(f'wifi-stream5-context: p=0 d={d} ')),'wifi-stream5-context')
        old[d]=[a['w'+str(i)] for i in range(8)]
    parent=[c.LOG_PARKED,emit('wifi-join5-plan',tx=0x500000,rx=0x600000,old=old[1][2]|old[1][3]<<32,slot=plan['slot'],max_rx=48),emit('wifi-handoff5',old_ring=0x100000,new_ring=0x200000),emit('wifi-tx5',ring=0x700000)]
    for d in (0,1):parent.append(emit('wifi-join5-context',phase=8,dci=8+d,**{'w'+str(i):v for i,v in enumerate(old[d])}))
    return boot_rows()+parent+['wifi-join5: modeled=1',lines[0],'wifi-channel-vco5: modeled=1']+lines[1:]+['wifi-firmware-capture: completed=yes; RF and connectivity NOT_TESTED','laptop-capture: completed=yes']


def compose(reference,fragment):
    """Compose actual child with unchanged join/EAPOL/full RF parent; synthetic."""
    foreign=('wifi-nop5','wifi-wpa5','wifi-data5','wifi-retire5','wifi-provider5','wifi-reuse5','wifi-stream5')
    lines=[s for s in reference.splitlines() if not s.startswith(foreign)]
    fresh=iter(boot_rows())
    lines=[next(fresh) if s.startswith(('cmdline: ','laptop-capture: boot options=')) else s for s in lines]
    i=next(i for i,s in enumerate(lines) if s.startswith('laptop-capture: boot options='));lines.insert(i+1,c.LOG_PARKED)
    child=[s for s in fragment.splitlines() if s.startswith('wifi-stream5')]
    i=next(i for i,s in enumerate(lines) if s.startswith('wifi-join5:'));lines.insert(i+1,child[0])
    i=next(i for i,s in enumerate(lines) if s.startswith('wifi-firmware-capture:'));lines[i:i]=child[1:]
    text='\n'.join(lines)+'\n'
    marker='workspace-log: snapshot=exit.log '
    if marker in text:
        i=text.rindex(marker);text=text[:i]+re.sub(r'total_before=\d+',f'total_before={len(text[:i].encode())}',text[i:])
    return text


class Tests(unittest.TestCase):
    def rows(self):return minimal_parent(synthetic_child())
    def check(self,rows):
        text='\n'.join(rows)
        with patch.object(c,'parent',return_value={'eapol5_complete':True}) as p:
            result=c.check(text,RELEASE);p.assert_called_once_with(text,stream=True);return result
    def test_boot(self):
        rows=boot_rows();self.assertEqual(c.boot_identity('\n'.join(rows)),RELEASE)
        for token in c.BOOT_FIXED:
            with self.subTest(token=token),self.assertRaises(ValueError):c.boot_identity('\n'.join(s.replace(token,'') for s in rows))
        for bad in (rows[:1],rows+rows[:1],[s+' srvros.wifi-stream5=1' for s in rows],[s+' srvros.wifi-reuse5=1' for s in rows],rows[:1]+[rows[1]+' x=1']):
            with self.assertRaises(ValueError):c.boot_identity('\n'.join(bad))
        with self.assertRaises(ValueError):c.boot_identity('\n'.join(rows),'f'*16)
    def test_legacy_refuses(self):
        for text in ('srvros.wifi-stream5=1','wifi-stream5: complete=1'):
            with patch.object(eapol,'parent') as p,self.assertRaises(ValueError):eapol.check(text)
            p.assert_not_called()
        with patch.object(eapol,'parent') as p,self.assertRaises(ValueError):eapol.check('srvros.wifi-eapol5=1',stream=True)
        p.assert_not_called()
    def test_success_scope(self):
        result=self.check(self.rows());self.assertTrue(result['stream5_complete']);self.assertFalse(result['stream5_ordinary_rx']);self.assertFalse(result['stream5_backend_retired'])
    def test_schema_missing_duplicate_order(self):
        rows=self.rows();ids=[i for i,s in enumerate(rows) if s.startswith('wifi-stream5')]
        for i in ids:
            for bad in (rows[:i]+rows[i+1:],rows[:i]+[rows[i]]+rows[i:]):
                with self.subTest(i=i),self.assertRaises(ValueError):self.check(bad)
            if rows[i] not in (c.PROGRESS,c.LOG_PARKED):
                bad=rows.copy();bad[i]+=' extra=1'
                with self.assertRaises(ValueError):self.check(bad)
        for a,b in zip(ids,ids[1:]):
            bad=rows.copy();bad[a],bad[b]=bad[b],bad[a]
            with self.subTest(a=a,b=b),self.assertRaises(ValueError):self.check(bad)
    def test_bound_numbers(self):
        rows=self.rows()
        for i,line in enumerate(rows):
            if not line.startswith('wifi-stream5') or line in (c.PROGRESS,c.LOG_PARKED):continue
            for m in re.finditer(r'\b([a-z][a-z_0-9]*)=(\d+)',line):
                key=m[1]
                if key in ('elapsed_us','rx_after','total_polls'):continue
                if line.startswith('wifi-stream5-context: p=9 d=0') and key not in ('p','d','w0'):continue
                value=int(m[2])^(2 if key in ('c','sc') else 1)
                bad=rows.copy();bad[i]=line[:m.start(2)]+str(value)+line[m.end(2):]
                # Status/residue and event-cycle ranges intentionally admit
                # multiple real hardware observations; test targeted faults.
                if key in ('s','ss') and not line.startswith(('wifi-stream5-sync:','wifi-stream5-state:')):continue
                with self.subTest(i=i,key=key),self.assertRaises(ValueError):self.check(bad)
    def test_compact_raw_and_typed_mutations(self):
        rows=self.rows()
        for i,line in enumerate(rows):
            if not line.startswith(('wifi-stream5-td:','wifi-stream5-data:','wifi-stream5-rx:')):continue
            prefix,text=line.split(': ',1);values=text.split(' ')
            for n,value in enumerate(values):
                changed=values.copy()
                if prefix=='wifi-stream5-td':
                    changed[n]=('0x'+format(int(value,16)+1,'016x')) if n==2 else str(int(value)^(2 if n==4 else 1))
                    if n==3:changed[n]=str(255<<24)
                else:
                    changed[n]=str(int(value)+1) if n in (0,2,5,6) else '0' if n==1 else '4' if n==3 else '2'
                    if prefix=='wifi-stream5-rx' and n==5:changed[n]='0'
                bad=rows.copy();bad[i]=prefix+': '+' '.join(changed)
                with self.subTest(i=i,column=n),self.assertRaises(ValueError):self.check(bad)
            for tail in (' ', ' extra', ' -1'):
                bad=rows.copy();bad[i]+=tail
                with self.assertRaises(ValueError):self.check(bad)
        # A real xHCI stopped boundary may expose the Link rather than base.
        for i,line in enumerate(rows):
            if not line.startswith('wifi-stream5-fence:') or ' t=15 ' not in line:continue
            a=c.fields(line,'wifi-stream5-fence');ring=0x300000 if a['d']==8 else 0x400000
            a['sp']=ring+1008;a['sq']=(ring+1008)|1
            good=rows.copy();good[i]=emit('wifi-stream5-fence',**a)
            self.assertTrue(self.check(good)['stream5_complete'])
    def test_unsafe_or_pending(self):
        rows=self.rows()
        for prefix,key,v in [('wifi-stream5-probe:','elapsed_us',15000000),('wifi-stream5-probe:','unsafe',1),('wifi-stream5-state:','pending_rx',1),('wifi-stream5-state:','command_pending',1),('wifi-stream5-keys:','pn',69),('wifi-stream5:','released',73),('wifi-stream5:','rx_after',0),('wifi-stream5:','ordinary_rx',1),('wifi-stream5:','backend_retired',1)]:
            bad=rows.copy();i=next(i for i,s in enumerate(bad) if s.startswith(prefix));bad[i]=re.sub(r'\b'+key+r'=\d+',f'{key}={v}',bad[i])
            with self.subTest(key=key),self.assertRaises(ValueError):self.check(bad)
        for extra in ('wifi-wpa5: ok=1','wifi-stream5-unknown: complete=1'):
            with self.assertRaises(ValueError):self.check(rows+[extra])
    def test_actual_producer_if_supplied(self):
        p=os.environ.get('WIFI_STREAM5_TRACE')
        if not p:self.skipTest('actual native-C trace not supplied')
        self.assertTrue(self.check(minimal_parent(Path(p).read_text().splitlines()))['stream5_complete'])
    def test_entire_parent_if_supplied(self):
        p=os.environ.get('WIFI_STREAM5_PARENT');t=os.environ.get('WIFI_STREAM5_TRACE')
        if not p or not t:self.skipTest('matched actual child/full parent not supplied')
        text=compose(Path(p).read_text(),Path(t).read_text())
        result=c.check(text,RELEASE);self.assertTrue(result['stream5_complete'] and result['eapol5_complete'] and result['power5_originals_restored'])
        self.assertEqual(result['operations'],15719)
        with self.assertRaises(ValueError):eapol.check(text)
        for prefix in ('wifi-eapol5:','wifi-join5:','wifi-tx5:','wifi-power5:','wifi-cal5:','wifi-channel-vco5:'):
            row=next(s for s in text.splitlines() if s.startswith(prefix))
            with self.subTest(prefix=prefix),self.assertRaises(ValueError):c.check(text.replace(row+'\n','',1),RELEASE)


if __name__=='__main__':unittest.main()
