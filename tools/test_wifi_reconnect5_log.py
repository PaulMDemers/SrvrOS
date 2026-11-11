"""Strict reconnect mutations using an actual two-generation native HOST trace.

WIFI_RECONNECT5_TRACE must come from --trace-reconnect, not spliced one-shot
runs. The minimal parent bindings below are labeled fixture metadata only;
check_owner never grants RF/hardware acceptance. Full RF is never fabricated.
"""
import os
import tempfile
from pathlib import Path
import re
import unittest
from unittest.mock import patch
import wifi_reconnect5_log as c
import wifi_stack5_log as stack

RELEASE='0123456789abcdef';VOLUME='12345678';SESSION='session-0000000000000001-00'


def emit(prefix,**fields):
    return prefix+': '+' '.join(k+'='+str(v) for k,v in fields.items())


def boot_rows():
    tokens=' '.join(sorted(c.BOOT_FIXED|{'srvros.desktop-release='+RELEASE,'srvros.workspace-volume='+VOLUME}))
    return ['cmdline: '+tokens,c.BOOT,'laptop-capture: boot options='+tokens]


def actual_rows(text):
    return [s for s in text.splitlines() if s.startswith(('wifi-reconnect5','wifi-stream5','wifi-stack5')) and
            s!='wifi-reconnect5-host: modeled_usb=1 acceptance=0']


def modeled_bindings(fragment):
    """Only standalone raw-owner test inputs; these are NOT full RF proof."""
    plan=stack.fields(next(s for s in fragment if s.startswith('wifi-stream5-plan:')),'wifi-stream5-plan')
    rows=[emit('wifi-join5-plan',tx=0x500000,rx=0x600000,old=0x100001,slot=plan['slot'],max_rx=48),
          emit('wifi-handoff5',old_ring=0x100000,new_ring=0x200000),emit('wifi-tx5',ring=0x700000)]
    for d in (0,1):
        value=stack.fields(next(s for s in fragment if s.startswith(f'wifi-stream5-context: p=0 d={d} ')),
                           'wifi-stream5-context')
        rows.append(emit('wifi-join5-context',phase=8,dci=8+d,**{'w'+str(i):value['w'+str(i)] for i in range(8)}))
    return rows


def text(rows):return '\n'.join(rows)+'\n'


def mutate(rows,prefix,key,value,occurrence=0):
    result=rows.copy();matches=[i for i,s in enumerate(result) if s.startswith(prefix)]
    at=matches[occurrence]
    result[at],n=re.subn(r'\b'+key+r'=(?:0x[0-9a-f]+|\d+)',key+'='+str(value),result[at])
    assert n==1
    return result


class Tests(unittest.TestCase):
    def fixture(self):
        name=os.environ.get('WIFI_RECONNECT5_TRACE')
        if not name:self.skipTest('WIFI_RECONNECT5_TRACE actual two-generation C fixture not supplied')
        fragment=actual_rows(Path(name).read_text())
        self.assertTrue(fragment and fragment[0].startswith('wifi-reconnect5-begin:'))
        return modeled_bindings(fragment)+fragment

    def check(self,rows):return c.check_owner(text(rows))

    def test_exact_boot_identity_and_old_recipe_refusal(self):
        rows=boot_rows();self.assertEqual(c.boot_identity(text(rows),RELEASE,VOLUME),RELEASE)
        for token in c.BOOT_FIXED:
            with self.subTest(token=token),self.assertRaises(ValueError):
                c.boot_identity(text([s.replace(token,'') for s in rows]))
        for changed in (rows+[rows[0]],rows[:1],list(reversed(rows)),
                        [s+' srvros.wifi-http5=1' for s in rows],
                        [s+' srvros.http-child-preflight=1' for s in rows],
                        [s.replace('srvros.wifi-reconnect5=1','srvros.wifi-reconnect5=2') for s in rows],
                        [rows[0],rows[1],rows[2]+' srvros.extra=1']):
            with self.assertRaises(ValueError):c.boot_identity(text(changed))
        with self.assertRaises(ValueError):stack.boot_identity(text(rows))
        with self.assertRaises(ValueError):c.boot_identity(text(rows),'f'*16,VOLUME)
        with self.assertRaises(ValueError):c.boot_identity(text(rows),RELEASE,'f'*8)

    def test_actual_v140_stage_recipe_matches_checker(self):
        import hardware_boot
        config=hardware_boot.limine_config(RELEASE,'ram-usb-wifi-fw-v140-laptop-probe-'+VOLUME+'-auto-return')
        command=next(s.strip()[9:] for s in config.splitlines() if s.strip().startswith('cmdline: '))
        rows=['cmdline: '+command,c.BOOT,'laptop-capture: boot options='+command]
        self.assertEqual(c.boot_identity(text(rows),RELEASE,VOLUME),RELEASE)
        for checker in (stack.boot_identity,):
            with self.assertRaises(ValueError):checker(text(rows),RELEASE)

    def test_actual_v142_journal_recipe_matches_checker(self):
        import hardware_boot
        config=hardware_boot.limine_config(RELEASE,'ram-usb-wifi-fw-v142-laptop-probe-'+VOLUME+'-auto-return')
        command=next(s.strip()[9:] for s in config.splitlines() if s.strip().startswith('cmdline: '))
        rows=['cmdline: '+command,c.BOOT,'laptop-capture: boot options='+command]
        self.assertEqual(c.boot_identity(text(rows),RELEASE,VOLUME),RELEASE)
        for extra in ('srvros.startup-diagnostic=1','srvros.startup-journal=0',
                      'srvros.startup-journal=1','srvros.wifi-http5=1'):
            changed=[s+' '+extra if s.startswith(('cmdline: ','laptop-capture: ')) else s for s in rows]
            with self.subTest(extra=extra),self.assertRaises(ValueError):c.boot_identity(text(changed))

    def test_empty_truncated_owner_refuses(self):
        for value in ('','wifi-reconnect5-begin: version=1 generation=1 adapter_parent_held=1\n'):
            with self.assertRaises(ValueError):c.check_owner(value)

    def test_qualification_tag_is_never_native_admission(self):
        rows=boot_rows()+['workspace-log: directory=/work/srvros-work/Logs/'+SESSION+' periodic_seconds=10',c.QUALIFICATION_TAG]
        payload=text(rows);payload+=f'workspace-log: snapshot=exit.log monotonic_us=2000000 total_before={len(payload.encode())} tail_limit=4194304\n'
        with patch.object(c,'parent',side_effect=AssertionError('must refuse before RF')) as parent:
            with self.assertRaisesRegex(ValueError,'qualification-only'):
                c.check(payload,RELEASE,volume=VOLUME,session=SESSION)
            parent.assert_not_called()
        for changed in (rows[:-1],rows+[c.QUALIFICATION_TAG],rows[:-1]+['qualification-other: synthetic=1']):
            body=text(changed);body+=f'workspace-log: snapshot=exit.log monotonic_us=2000000 total_before={len(body.encode())} tail_limit=4194304\n'
            with self.assertRaisesRegex(ValueError,'qualification-only'):
                c.check_qualification(body,RELEASE,volume=VOLUME,session=SESSION)
        with self.assertRaisesRegex(ValueError,'unknown reconnect collection profile'):
            c._check('',profile=object())

    def test_actual_two_generation_owner_and_scope(self):
        result=self.check(self.fixture())
        self.assertTrue(result['provider_retired'] and result['backend_tombstones_retained'])
        self.assertFalse(result['backend_retired'] or result['native_accepted'] or result['full_rf_parent_checked'])
        self.assertTrue(result['modeled_transport_only'])
        first,second=result['connections']
        self.assertEqual((first['epoch'],second['epoch']),(1,2))
        self.assertEqual((first['pings'],second['pings']),(64,64))
        self.assertGreater(second['next_token'],first['next_token'])
        self.assertEqual(first['commands'][-1]+16,second['commands'][0])
        self.assertFalse(set(first['rings'])&set(second['rings']))

    def test_actual_quiet_and_background_geometry(self):
        import subprocess
        binary=os.environ.get('WIFI_RECONNECT5_BINARY')
        if not binary:self.skipTest('explicit actual C binary required')
        for case in (0,10,11,16):
            with self.subTest(case=case):
                raw=subprocess.check_output([binary,'--trace-reconnect-case',str(case)],text=True)
                fragment=actual_rows(raw)
                result=self.check(modeled_bindings(fragment)+fragment)
                self.assertFalse(result['native_accepted'])
                for group in result['connections']:
                    self.assertEqual(group['pings'],64)
                    self.assertEqual(group['tx_fences'],0)
                    self.assertEqual(group['rx_fences'],int(case!=0))
                self.assertTrue(all('positions=127' in row for row in fragment if row.startswith('wifi-stream5-plan:')))

    def test_missing_duplicate_and_unknown_receipts(self):
        rows=self.fixture()
        families=('wifi-reconnect5-begin:','wifi-stream5-plan:','wifi-stream5-td:',
                  'wifi-stream5-data:','wifi-stream5-rx:','wifi-stream5-fence:',
                  'wifi-stream5-sync:','wifi-stream5-context:','wifi-stream5-stop:',
                  'wifi-stream5-state:','wifi-stream5-keys:','wifi-stack5-auth:',
                  'wifi-stream5-probe:','wifi-stack5:','wifi-stack5-pings:',
                  'wifi-stack5-workload:','wifi-reconnect5-connection:','wifi-reconnect5-final:')
        for family in families:
            indices=[i for i,s in enumerate(rows) if s.startswith(family)]
            for at in sorted({indices[0],indices[-1]}):
                for changed in (rows[:at]+rows[at+1:],rows[:at]+[rows[at]]+rows[at:]):
                    with self.subTest(family=family,at=at),self.assertRaises(ValueError):self.check(changed)
        for prefix in ('wifi-reconnect5-unknown: x=1','wifi-stack5-final: complete=1',
                       'wifi-stream5-unknown: x=1'):
            with self.assertRaises(ValueError):self.check(rows[:-1]+[prefix]+rows[-1:])

    def test_reordered_groups_and_generation_identity(self):
        rows=self.fixture();starts=[i for i,s in enumerate(rows) if s.startswith('wifi-reconnect5-begin:')]
        final=next(i for i,s in enumerate(rows) if s.startswith('wifi-reconnect5-final:'))
        with self.assertRaises(ValueError):self.check(rows[:starts[0]]+rows[starts[1]:final]+rows[starts[0]:starts[1]]+rows[final:])
        for prefix,key,value,at in [('wifi-reconnect5-begin:','generation',1,1),
                ('wifi-stream5-plan:','epoch',1,1),('wifi-reconnect5-connection:','epoch',1,1),
                ('wifi-reconnect5-connection:','generation',1,1),
                ('wifi-reconnect5-connection:','next_token',1,1),
                ('wifi-reconnect5-connection:','stack_deconfigured',0,0),
                ('wifi-reconnect5-connection:','provider_detached',0,1),
                ('wifi-reconnect5-connection:','native_closed',0,0),
                ('wifi-reconnect5-connection:','adapter_parent_restored',1,0),
                ('wifi-reconnect5-final:','parent_restored',0,0),
                ('wifi-reconnect5-final:','provider_retired',0,0),
                ('wifi-reconnect5-final:','backend_retired',1,0)]:
            with self.subTest(prefix=prefix,key=key),self.assertRaises(ValueError):self.check(mutate(rows,prefix,key,value,at))

    def test_old_events_context_and_cumulative_counters(self):
        rows=self.fixture()
        tds=[i for i,s in enumerate(rows) if s.startswith('wifi-stream5-td:')]
        plans=[i for i,s in enumerate(rows) if s.startswith('wifi-stream5-plan:')]
        second_td=next(i for i in tds if i>plans[1])
        changed=rows.copy();changed[second_td]=rows[tds[0]]
        with self.assertRaises(ValueError):self.check(changed)
        summary=stack.fields(next(s for i,s in enumerate(rows) if i>plans[1] and s.startswith('wifi-stack5:')),'wifi-stack5')
        changed=rows
        for key in ('enqueued','submitted','completed'):changed=mutate(changed,'wifi-stack5:',key,summary['data_tx'],1)
        with self.assertRaises(ValueError):self.check(changed)
        for prefix,key,value,at in [('wifi-stream5-stop: d=1 ','q',0,1),
                ('wifi-stream5-fence:','used',0,0),('wifi-stream5-keys:','erased',0,1),
                ('wifi-stream5-context: p=0 d=0 ','w2',1,1),
                ('wifi-stack5:','parent',1,0),('wifi-stack5:','complete',1,0),
                ('wifi-stream5-plan:','max_fences',4,0),
                ('wifi-stream5-plan:','positions',63,0),
                ('wifi-stream5-plan:','positions',255,1),
                ('wifi-stack5:','tx_wraps',1,0),
                ('wifi-stack5:','rx_wraps',0,0),
                ('wifi-reconnect5-connection:','max_fences',4,1),
                ('wifi-stack5-workload:','matched',63,1),
                ('wifi-stack5:','elapsed_us',15000000,1)]:
            with self.subTest(prefix=prefix,key=key),self.assertRaises(ValueError):self.check(mutate(rows,prefix,key,value,at))

    def test_legacy_owner_profiles_remain_strict(self):
        rows=self.fixture();at=next(i for i,s in enumerate(rows) if s.startswith(stack.PROGRESS))
        end=next(i for i,s in enumerate(rows[at:],at) if s.startswith('wifi-reconnect5-connection:'))
        fragment=rows[at:end]
        for kwargs in ({},{'profile':stack._HTTP_TRANSPORT},
                       {'expected_epoch':1},{'profile':stack._HTTP_TRANSPORT,'previous_context':{}}):
            with self.subTest(kwargs=kwargs),self.assertRaises(ValueError):stack._owner(text(rows),rows,fragment,**kwargs)

    def test_full_native_path_rejects_host_and_requires_full_rf(self):
        name=os.environ.get('WIFI_RECONNECT5_TRACE')
        if not name:self.skipTest('actual C trace not supplied')
        with self.assertRaises(ValueError):c.check(Path(name).read_text(),RELEASE,volume=VOLUME,session=SESSION)
        rows=boot_rows()+self.fixture()
        payload=text(rows);payload+=f'workspace-log: snapshot=exit.log monotonic_us=2000000 total_before={len(payload.encode())} tail_limit=4194304\n'
        with self.assertRaises(ValueError):c.check(payload,RELEASE,volume=VOLUME,session=SESSION)
        payload=text(boot_rows()+['workspace-log: directory=/work/srvros-work/Logs/'+SESSION+' period_seconds=120']+self.fixture())
        payload+=f'workspace-log: snapshot=exit.log monotonic_us=2000000 total_before={len(payload.encode())} tail_limit=4194304\n'
        host_payload=payload[:payload.rindex('workspace-log: snapshot=exit.log ')]
        host_payload+='wifi-reconnect5-host: modeled_usb=1 acceptance=0\n'
        host_payload+=f'workspace-log: snapshot=exit.log monotonic_us=2000000 total_before={len(host_payload.encode())} tail_limit=4194304\n'
        with self.assertRaisesRegex(ValueError,'foreign/modeled'):
            c.check(host_payload,RELEASE,volume=VOLUME,session=SESSION)
        with patch.object(c,'parent',side_effect=ValueError('full RF proof deliberately absent')) as parent:
            with self.assertRaisesRegex(ValueError,'full RF proof deliberately absent'):
                c.check(payload,RELEASE,volume=VOLUME,session=SESSION)
            parent.assert_called_once_with(payload,stream=True)
        self.assertIn('wifi-stack5: complete=0 parent=0',payload)
        self.assertNotIn('wifi-stack5: complete=1 parent=1',payload)


class FullRFQualificationTests(unittest.TestCase):
    """Actual C child plus unchanged captured RF parent: synthetic only."""
    cache=None

    def composition(self):
        parent=os.environ.get('WIFI_RECONNECT5_PARENT')
        binary=os.environ.get('WIFI_RECONNECT5_BINARY')
        if not parent or not binary:
            self.skipTest('explicit WIFI_RECONNECT5_PARENT and built WIFI_RECONNECT5_BINARY required')
        key=(str(Path(parent).resolve()),str(Path(binary).resolve()))
        cls=type(self)
        if cls.cache is None or cls.cache[0]!=key:
            import wifi_reconnect5_qualification as q
            directory=Path(tempfile.mkdtemp(prefix='srvros-reconnect5-full-rf-'))/'qualification'
            log,proof=q.generate(parent,binary,directory,release=RELEASE,volume=VOLUME)
            cls.cache=(key,log,proof,directory)
        return cls.cache[1:]

    def verify(self,log):
        import wifi_reconnect5_qualification as q
        return c.check_qualification(q.retotal(log),RELEASE,volume=VOLUME)

    def test_journal_profile_preserves_complete_rf_parent(self):
        import wifi_reconnect5_qualification as q
        _,proof,directory=self.composition()
        parent=Path(proof['parent_path']).read_text()
        trace=(directory/'host-trace.log').read_text()
        log,journal_proof=q.compose(parent,trace,RELEASE,VOLUME,startup_journal=True)
        result=self.verify(log)
        self.assertTrue(result['reconnect5_complete'])
        self.assertFalse(result['native_accepted'])
        self.assertEqual(journal_proof['unchanged_outer_sha256'],proof['unchanged_outer_sha256'])
        self.assertIn('srvros.startup-journal=1',log)
        with self.assertRaisesRegex(ValueError,'qualification-only'):
            c.check(log,RELEASE,volume=VOLUME)

    def test_actual_full_rf_composition_preserves_parent_and_scope(self):
        import wifi_reconnect5_qualification as q
        log,proof,directory=self.composition()
        self.assertTrue(proof['passed'] and proof['synthetic'])
        self.assertFalse(proof['native_acceptance'])
        result=proof['result']
        self.assertTrue(result['reconnect5_complete'] and result['synthetic_qualification'])
        self.assertFalse(result['native_acceptance_pending_independent_review'])
        with self.assertRaisesRegex(ValueError,'qualification-only'):
            c.check(log,RELEASE,volume=VOLUME)
        self.assertFalse(result.get('native_accepted',False) or result['reconnect5_backend_retired'] or
                         result['reconnect5_tcp'] or result['reconnect5_http'])
        parent=Path(proof['parent_path']).read_text()
        self.assertEqual(q._preserved(parent.splitlines()),q._preserved(log.splitlines()))
        self.assertEqual(proof['parent_sha256'],q.sha(parent.encode()))
        self.assertEqual(result['reconnect5_exact_pings'],128)
        self.assertEqual(result['reconnect5_exact_body_bytes'],4096)
        self.assertEqual(log.count('wifi-stack5: complete=0 parent=0 '),2)
        self.assertNotIn('wifi-stack5: complete=1 parent=1 ',log)
        self.assertNotIn(q.HOST_LABEL,log)
        self.assertTrue((directory/'proof.json').is_file())

    def test_full_rf_failure_final_parent_release_and_session_refuse(self):
        log,proof,_=self.composition();rows=log.splitlines()
        for prefix,key,value in [('wifi-reconnect5-final:','parent_restored',0),
                                 ('wifi-reconnect5-final:','provider_retired',0),
                                 ('wifi-peer5:','ok',0),
                                 ('wifi-stream5-context: p=0 d=1 ','w2',1)]:
            occurrence=1 if prefix=='wifi-stream5-context: p=0 d=1 ' else 0
            with self.subTest(prefix=prefix,key=key),self.assertRaises(ValueError):
                self.verify(text(mutate(rows,prefix,key,value,occurrence)))
        with self.assertRaises(ValueError):c.check_qualification(log,'f'*16,volume=VOLUME)
        with self.assertRaises(ValueError):c.check_qualification(log,RELEASE,volume='f'*8)
        with self.assertRaises(ValueError):c.check_qualification(log,RELEASE,volume=VOLUME,session=SESSION)
        missing=[s for s in rows if not s.startswith('wifi-peer5:')]
        with self.assertRaises(ValueError):self.verify(text(missing))

    def test_full_rf_source_order_cannot_move_connection_after_parent(self):
        log,_,_=self.composition();rows=log.splitlines()
        start=next(i for i,s in enumerate(rows) if s.startswith('wifi-reconnect5-begin: version=1 generation=2 '))
        end=next(i for i,s in enumerate(rows) if s.startswith('wifi-reconnect5-connection: version=1 generation=2 '))+1
        child=rows[start:end];changed=rows[:start]+rows[end:]
        final=next(i for i,s in enumerate(changed) if s.startswith('wifi-reconnect5-final:'))
        changed[final:final]=child
        with self.assertRaises(ValueError):self.verify(text(changed))
        rf=next(i for i,s in enumerate(rows) if s.startswith('wifi-channel-vco5:'))
        changed=rows.copy();changed.insert(start,changed.pop(rf))
        with self.assertRaises(ValueError):self.verify(text(changed))


if __name__=='__main__':unittest.main()
