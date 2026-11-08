"""Controller-only proof mutations;fixture/full-parent composition is synthetic."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import unittest
from unittest.mock import patch
import hardware_boot as boot
import wifi_nop5_log as c

RELEASE='0123456789abcdef'


def boot_rows():
    config=boot.limine_config(RELEASE,'ram-usb-wifi-fw-v134-laptop-probe-12345678-auto-return')
    tokens=next(x.strip()[9:] for x in config.splitlines() if x.strip().startswith('cmdline: '))
    return ['cmdline: '+tokens,'laptop-capture: boot options='+tokens]


class BootTests(unittest.TestCase):
    def test_exact_recipe(self):
        rows=boot_rows()
        self.assertEqual(c.boot_identity('\n'.join(rows),RELEASE),RELEASE)
        for key in c.BOOT_FIXED:
            bad=[x.replace(key,'') for x in rows]
            with self.subTest(token=key),self.assertRaises(ValueError):c.boot_identity('\n'.join(bad))

    def test_missing_duplicate_divergent_identity(self):
        rows=boot_rows()
        for bad in (rows[:1],rows[1:],rows+rows[:1],rows+rows[1:],rows[:1]+[rows[1]+' srvros.wifi-wpa5=1'],
                    [x+' srvros.wifi-nop5=1' for x in rows],
                    [x+' srvros.wifi-join5=1' for x in rows],
                    [x.replace('srvros.desktop-release='+RELEASE,'srvros.desktop-release=invalid') for x in rows],
                    [x.replace('srvros.workspace-volume=12345678','srvros.workspace-volume=xyz') for x in rows]):
            with self.assertRaises(ValueError):c.boot_identity('\n'.join(bad))
        with self.assertRaises(ValueError):c.boot_identity('\n'.join(rows),'f'*16)


class SummaryTests(unittest.TestCase):
    def test_complete_and_every_bound_field(self):
        line='wifi-nop5: '+' '.join(f'{k}={v}' for k,v in c.SUMMARY.items())
        c.summary(line)
        for k,v in c.SUMMARY.items():
            bad=re.sub(r'\b'+k+r'=\d+',k+'='+str(v^1),line)
            with self.subTest(key=k),self.assertRaises(ValueError):c.summary(bad)
        for bad in (line+' unknown=1',line+' complete=1',line.replace(' complete=1','',1)):
            with self.assertRaises(ValueError):c.summary(bad)


def compose(reference,fragment):
    """Retain entire pre-WPA v126 parent,replace only boot identity/add child."""
    lines=reference.splitlines()
    fresh=iter(boot_rows())
    lines=[next(fresh) if line.startswith(('cmdline: ','laptop-capture: boot options=')) else line for line in lines]
    # Main's real quiesce receipt is outside the child C fixture. This added
    # line models main behavior for synthetic composition,not native proof.
    boot_end=next(i for i,line in enumerate(lines) if line.startswith('laptop-capture: boot options='))
    lines.insert(boot_end+1,c.LOG_PARKED)
    final=[line for line in fragment.splitlines() if line.startswith('wifi-nop5:')]
    child=[line for line in fragment.splitlines() if line.startswith('wifi-nop5-progress:')]
    late=[line for line in fragment.splitlines() if line.startswith('wifi-nop5') and not line.startswith(('wifi-nop5:','wifi-nop5-progress:','wifi-nop5-log:'))]
    if len(final)!=1:
        raise ValueError('fixture lacks unique final child summary')
    tx=next(i for i,line in enumerate(lines) if line.startswith('wifi-tx5:'))
    lines[tx+1:tx+1]=child
    finish=next(i for i,line in enumerate(lines) if line.startswith('wifi-firmware-capture:'))
    lines[finish:finish]=late+final
    text='\n'.join(lines)+'\n'
    marker='workspace-log: snapshot=exit.log '
    if marker in text:
        i=text.rindex(marker)
        text=text[:i]+re.sub(r'total_before=\d+',f'total_before={len(text[:i].encode())}',text[i:])
    return text


def synthetic_child(slot=7,ring=0x300000):
    rows=[c.PROGRESS]
    def emit(prefix,**values):rows.append(prefix+': '+' '.join(f'{k}={v}' for k,v in values.items()))
    emit('wifi-nop5-plan',ring=ring,epoch=1,slot=slot,dci=8,positions=63,first=0,total=64,tx_pages=0,rx_pages=0)
    event=(slot<<24)|(8<<16)|(32<<10)|1
    for i in range(64):emit('wifi-nop5-td',index=i,pointer=ring+16*i if i<63 else ring,cycle=int(i<63),cc=1,residue=0,event_status=1<<24,event_control=event,bytes=0,page=0)
    config=[1,(3<<1)|(2<<3)|(512<<16),ring|1,0,124,0,0,0]
    contexts=[config,config.copy(),config.copy(),config.copy(),[0]*8]
    contexts[1][0]=3;contexts[1][2]=(ring+63*16)|1
    contexts[2][0]=3;contexts[2][2]=ring
    contexts[3][0]=3;contexts[3][2]=ring+16
    for i,w in enumerate(contexts):emit('wifi-nop5-context',phase=i,**{'w'+str(j):v for j,v in enumerate(w)})
    for i in range(2):
        ptr=0x900000+16*(i+1)
        emit('wifi-nop5-fence-command',index=i,pointer=ptr,epoch=1,token=i+1,slot=slot,dci=8,type=15+i,dequeue=ring if i else 0,control=(slot<<24)|(8<<16)|((15+i)<<10)|1,status=0,event_pointer=ptr,event_status=1<<24,event_control=(slot<<24)|(33<<10)|1,stop_pointer=0 if i else ring+63*16,stop_status=0 if i else 27<<24,stop_control=0 if i else event,stop_dequeue=0 if i else (ring+63*16)|1,consumed=1)
    for i,offset in enumerate((0,3,4)):
        ptr=0x900000+16*offset;kind=15 if i==1 else 12
        emit('wifi-nop5-sync-command',index=i,pointer=ptr,event_pointer=ptr,cc=1,slot=slot,delta=1,parameter=0 if i==1 else 0xa00000,status=0,control=(slot<<24)|(kind<<10)|((8<<16) if i==1 else 0)|1)
    emit('wifi-nop5-final-stop',pointer=ring+16,status=27<<24,control=event)
    emit('wifi-nop5-frontier',sequence=2,engine_sequence=2,pending=0,round=0,failed=0,empty=1,drain_events=0)
    emit('wifi-nop5',**c.SUMMARY)
    return rows


class ControllerProofTests(unittest.TestCase):
    def rows(self):
        child=synthetic_child()
        return boot_rows()+[c.LOG_PARKED,'wifi-handoff5: old_ring=65536 new_ring=131072','wifi-tx5-command: slot=7','wifi-tx5: ring=196608',child[0],'wifi-channel-vco5: status=0']+child[1:]+['wifi-firmware-capture: completed=yes','laptop-capture: completed=yes']

    def check(self,rows):
        with patch.object(c,'parent',return_value={'tx5_complete':True}) as parent:
            result=c.check('\n'.join(rows),RELEASE)
            parent.assert_called_once_with('\n'.join(rows),nop=True)
            return result

    def test_complete_scope(self):
        result=self.check(self.rows())
        self.assertTrue(result['nop5_complete'] and result['nop5_ledger_retired'])
        self.assertFalse(result['nop5_backend_retired'])
        self.assertEqual(result['nop5_network_out'],0)

    def test_missing_duplicate_order_schema(self):
        rows=self.rows()
        indices=[i for i,s in enumerate(rows) if s.startswith('wifi-nop5')]
        for i in indices:
            for bad in (rows[:i]+rows[i+1:],rows[:i]+[rows[i]]+rows[i:]):
                with self.subTest(index=i),self.assertRaises(ValueError):self.check(bad)
            if rows[i]!=c.PROGRESS:
                bad=rows.copy();bad[i]+=' unexpected=1'
                with self.assertRaises(ValueError):self.check(bad)
        for a,b in zip(indices[1:-1],indices[2:]):
            bad=rows.copy();bad[a],bad[b]=bad[b],bad[a]
            with self.assertRaises(ValueError):self.check(bad)

    def test_numeric_bindings(self):
        rows=self.rows()
        for i,line in enumerate(rows):
            if not line.startswith('wifi-nop5') or line==c.PROGRESS:continue
            for m in re.finditer(r'\b([a-z_][a-z_0-9]*)=(\d+)',line):
                # Reserved disabled words/event-ring cycle are not proof of
                # descriptor-cycle identity. Do not invent equality for them.
                if line.startswith('wifi-nop5-context: phase=4') and m[1] not in ('phase','w0'):continue
                if m[1] in ('event_control','stop_control','control'):
                    value=int(m[2])^2
                elif m[1] in ('sequence','engine_sequence','drain_events'):
                    value=99
                else:value=int(m[2])^1
                bad=rows.copy();bad[i]=line[:m.start(2)]+str(value)+line[m.end(2):]
                with self.subTest(index=i,key=m[1]),self.assertRaises(ValueError):self.check(bad)

    def test_live_receipt_and_lifetime_refusals(self):
        rows=self.rows()
        for extra in ('wifi-wpa5: ok=1','wifi-join5-progress: anything','wifi-nop5-unknown: ok=1'):
            with self.assertRaises(ValueError):self.check(rows+[extra])
        progress=rows.index(c.PROGRESS);rf=next(i for i,s in enumerate(rows) if s.startswith('wifi-channel-vco5:'))
        bad=rows.copy();bad[progress],bad[rf]=bad[rf],bad[progress]
        with self.assertRaises(ValueError):self.check(bad)

    def test_entire_reference_parent_composition(self):
        root=Path(__file__).resolve().parents[1]
        w=root/'build/bringup/wifi-nop5-native'
        if not (w/'reference-v126.log').is_file():self.skipTest('unit-local fullparent reference not supplied')
        metadata=json.loads((w/'reference-v126.json').read_text());raw=(w/'reference-v126.log').read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(),metadata['sha256'])
        self.assertEqual(len(raw),metadata['bytes'])
        reference=raw.decode()
        slot=c.fields(next(x for x in reference.splitlines() if x.startswith('wifi-tx5-command:')),'wifi-tx5-command')['slot']
        text=compose(reference,'\n'.join(synthetic_child(slot=slot)))
        result=c.check(text,RELEASE)
        self.assertTrue(result['nop5_complete'] and result['tx5_complete'] and result['power5_originals_restored'])
        self.assertEqual(result['operations'],15719)
        with self.assertRaises(ValueError):c.parent(text)
        for marker in ('wifi-tx5:','wifi-power5:','wifi-cal5:','wifi-channel-vco5:'):
            line=next(x for x in text.splitlines() if x.startswith(marker))
            with self.subTest(marker=marker),self.assertRaises(ValueError):c.check(text.replace(line+'\n','',1),RELEASE)

    def test_actual_native_fixture_if_supplied(self):
        trace=os.environ.get('WIFI_NOP5_TRACE')
        if not trace:self.skipTest('actual native fixture trace not supplied')
        raw=Path(trace).read_text()
        root=Path(__file__).resolve().parents[1]
        reference=(root/'build/bringup/wifi-nop5-native/reference-v126.log').read_text()
        self.assertTrue(c.check(compose(reference,raw),RELEASE)['nop5_complete'])


if __name__=='__main__':unittest.main()
