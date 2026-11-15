"""Actual v36 port and original ownership graph; local captured firmware stays private."""
import base64
import json
import os
from pathlib import Path
import re
import resource
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from hardware_boot import limine_config, stage
from intel_vtd_native import (FIELDS, drain_recipe, drain_result, drain_view, verify_drain_checkpoint)

ROOT = Path(__file__).resolve().parents[1]
BIN = Path(os.environ.get('INTEL_VTD_NATIVE_BINARY', ROOT / 'build/tests/intel-vtd-native'))
DMAR = Path(os.environ.get('SRVROS_VTD_DMAR', ROOT / 'build/device-fixtures/macbookair7-2-dmar.bin')).resolve()


def run(mode, binary=BIN):
    from fixture_client import run as fixture_run
    p = fixture_run(binary, mode)
    if p.returncode:
        raise AssertionError((mode, p.returncode, p.stderr, p.stdout[-2200:]))
    return p.stdout


def stats(text):
    line = next(l for l in text.splitlines() if l.startswith('vtd-native-fixture:'))
    return {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)}


def native_chain(text):
    from test_intel_source_audit import native_chain as previous
    pre, final = previous(drain_view(text))
    def restore(s):
        return s.replace('srvros.intel-ring=source-audit-v1', 'srvros.intel-ring=vtd-drain-v1').replace(
            'deferred=source-audit-preinit;', 'deferred=vtd-drain-preinit;').replace(
            'intel-wake-summary: v=35 ', 'intel-wake-summary: v=36 ')
    return restore(pre), restore(final)


def replace_raw(text, change):
    prefix = 'intel-vtd-drain: v=2 b='
    line = next(l for l in text.splitlines() if l.startswith('intel-vtd-drain:'))
    prefix = line.split('b=',1)[0]+'b='
    raw = bytearray(base64.b64decode(line[len(prefix):]))
    change(raw)
    return text.replace(line, prefix + base64.b64encode(raw).decode(), 1)


class Recipe(unittest.TestCase):
    def test_exact_recipe_excludes_old_and_foreign_modes(self):
        config = limine_config('0123456789abcdef', 'ram-usb-v1-intel-vtd-drain-laptop-probe-37d8c5d0-auto-return')
        cmd = next(l.split('cmdline:', 1)[1].strip() for l in config.splitlines() if 'cmdline:' in l)
        self.assertEqual(drain_recipe(cmd), dict(release='0123456789abcdef', volume='37d8c5d0'))
        tokens = cmd.split(); self.assertEqual(len(tokens), 15)
        for n in range(15):
            for bad in (' '.join(tokens[:n]+tokens[n+1:]), cmd+' '+tokens[n]):
                with self.assertRaises(ValueError): drain_recipe(bad)
        for extra in ({'intel_source_audit': True}, {'desktop_audio': True}, {'hold_on_failure': True}, {'repeat': 5}):
            with self.assertRaises(ValueError): stage(laptop_probe_volume='37d8c5d0', intel_vtd_drain=True, **extra)
        from workflow.runner import trial_parsers, profile
        with self.assertRaises(ValueError): trial_parsers('unknown')
        jobs = profile(ROOT, 'intel-vtd-drain-v36')
        self.assertIn('audit-full', [j['name'] for j in jobs['qualification']])
        self.assertIn('vtd-image', [j['name'] for j in jobs['qualification']])
        self.assertIn('vtd-native', [j['name'] for j in jobs['qualification']])


    def test_native_policy_children_are_linked_separately(self):
        plan=subprocess.check_output(['make','-B','-n','build/intel-vtd-drain/srvros.elf'],cwd=ROOT,text=True,timeout=120)
        link=next(l for l in plan.splitlines() if '-o build/intel-vtd-drain/srvros.elf' in l)
        for child in ('reset','source','buffers','transfer','requester','engines'):
            name='build/intel-vtd-drain/'+child+'.o'
            self.assertIn(name,link)
            self.assertNotIn('build/intel-source-audit/'+child+'.o',link)
            compile_line=next(l for l in plan.splitlines() if ' -c ' in l and l.endswith(' -o '+name))
            self.assertIn('-DSRVROS_INTEL_VTD_DRAIN',compile_line)
        self.assertNotIn('-DSRVROS_INTEL_VTD_DRAIN',subprocess.check_output(['make','-n','-B','build/intel-source-audit/reset.o'],cwd=ROOT,text=True,timeout=120))


@unittest.skipUnless(DMAR.is_file(), 'Air7,2 captured local DMAR fixture required; not shipped in Git')
class Native(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ['SRVROS_VTD_DMAR'] = str(DMAR)
        cls.texts = [run('report'), run('entry-off-report')]

    def retained(self, text):
        self.assertIn('retained=1 success=0', text)
        self.assertRegex(text, r'source-requester-fixture: .*complete=0 .*command=3')
        self.assertFalse(stats(text)['complete'])
        try: r = drain_result(text)
        except ValueError: return
        self.assertFalse(r['complete'])

    def test_timing_uses_original_samples_and_keeps_actual_failed_clock(self):
        for off, text in zip(('', 'entry-off-'), self.texts):
            result = drain_result(text)
            cost = result['timing']
            self.assertEqual(result['version'], 3)
            self.assertEqual(result['prepare_budget_us'], 150000)
            self.assertGreater(cost['calls'], 0)
            self.assertGreater(cost['parents'], 0)
            self.assertLess(cost['parents'], cost['calls'])
            self.assertEqual(sum(cost['phases']), cost['last']-cost['first'])
            failed = drain_result(run(off+'vtd-time:2'))
            self.assertFalse(failed['complete'])
            self.assertEqual(failed['timing']['stage'], 2)
            self.assertEqual(failed['timing']['failed'], failed['summary']['prepare']+150002)  # Fixture clock advances once after injection.
            self.assertEqual(failed['timing']['failed'], failed['audit']['engines']['summary']['clock'])
            self.assertEqual(failed['summary']['clock'], 0)  # Parent refuses; raw timestamp survives separately.
            pre, final = native_chain(text)
            for index, value in ((4, 3), (8, 7), (7, 2**63), (9, 2**63), (5, 0)):
                with self.subTest(route=off, field=index):
                    with self.assertRaises(ValueError):
                        drain_result(replace_raw(final, lambda raw, i=index, v=value: struct.pack_into('<Q', raw, 1552+8*i, v)), native=True)

    def test_explicit_policy_and_historical_receipt_isolation(self):
        from intel_timing_policy import prepare_us
        for off in ('', 'entry-off-'):
            text=run(off+'vtd-policy-window')
            result=drain_result(text)
            self.assertTrue(result['complete'])
            self.assertGreater(result['summary']['native_begin']-result['summary']['prepare'],100000)
            self.assertLess(result['summary']['native_end']-result['summary']['prepare'],150000)
            self.assertEqual(prepare_us(),100000)
            from intel_timing_policy import preparation_policy
            with preparation_policy(150000):
                pre,final=native_chain(text)
            self.assertTrue(drain_result(final,native=True)['native_complete'])
            self.assertTrue(verify_drain_checkpoint(pre,final,native=True))
            self.assertEqual(prepare_us(),100000)
            line=next(l for l in text.splitlines() if l.startswith('intel-vtd-drain:'))
            raw=base64.b64decode(line.split('b=',1)[1])
            for bad in (0,100000,150001,200000):
                with self.assertRaises(ValueError):
                    drain_result(replace_raw(text,lambda b,v=bad:struct.pack_into('<Q',b,1680,v)))
                self.assertEqual(prepare_us(),100000)
            old='intel-vtd-drain: v=2 b='+base64.b64encode(raw[:1680]).decode()
            with self.assertRaises(ValueError):drain_result(text.replace(line,old))
            self.assertEqual(prepare_us(),100000)
        # Ordinary historical-size receipts still use100ms, never the new policy.
        text=self.texts[0];line=next(l for l in text.splitlines() if l.startswith('intel-vtd-drain:'))
        raw=base64.b64decode(line.split('b=',1)[1])
        for version,size in ((1,1552),(2,1680)):
            old=f'intel-vtd-drain: v={version} b='+base64.b64encode(raw[:size]).decode()
            result=drain_result(text.replace(line,old))
            self.assertTrue(result['complete'])
            self.assertEqual(result['prepare_budget_us'],100000)

    def test_resource_paths_and_last_clock_endpoint_lifetime_boundaries(self):
        for off in ('', 'entry-off-'):
            reserved = drain_result(run(off+'vtd-reserved'))
            self.assertTrue(reserved['complete'])
            self.assertEqual(reserved['summary']['resource'], 1)
            self.retained(run(off+'vtd-reserved-table'))
            for mode in ('vtd-hda-pci', 'vtd-hda-alias', 'vtd-hda-borrow'):
                for boundary in (1, 210, 211):
                    with self.subTest(route=off, mode=mode, boundary=boundary):
                        self.retained(run(f'{off}{mode}:{boundary}'))

    def test_retained_following_child_is_reported_without_accepting_return(self):
        from intel_source_audit import audit_result
        for off in ('', 'entry-off-'):
            for mode in ('vtd-read:50', 'vtd-write-unknown:1', 'vtd-unmap:1', 'vtd-late-time:1'):
                with self.subTest(route=off, mode=mode):
                    text = run(off+mode)
                    result = drain_result(text)
                    self.assertFalse(result['complete'])
                    self.assertFalse(result['native_complete'])
                    self.assertFalse(result['hardware_ownership'])
                    self.assertEqual(result['summary']['returned'], 0)
                    self.assertEqual(result['summary']['retained'], int(mode!='vtd-late-time:1'))
                    self.assertTrue(result['audit']['scope_complete'])
                    self.assertEqual(result['audit']['engines']['summary']['child_end'], 0)
                    # The older profile keeps its strict returned-child rule.
                    with self.assertRaisesRegex(ValueError, 'child return order'):
                        audit_result(drain_view(text))
                    with self.assertRaises(ValueError):
                        drain_result(replace_raw(text, lambda b: struct.pack_into('<Q', b, 30*8, 1)))

    def test_fatal_sanitizers_on_both_routes_and_retention(self):
        from fixture_compile import compile_fixture, recipe
        target = 'build/tests/intel-vtd-native'
        compiler, flags, sources = recipe(target)
        with tempfile.TemporaryDirectory(prefix='vtd-native-san-') as folder:
            binary = Path(folder) / 'fixture'
            compile_fixture(target, output=binary,
                declaration=(compiler, [*flags, '-O1', '-g', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-fno-omit-frame-pointer'], sources))
            leaks = '0' if sys.platform == 'darwin' else '1'
            with patch.dict(os.environ, SRVROS_FIXTURE_BATCH='0',
                    ASAN_OPTIONS=f'detect_leaks={leaks}:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'):
                for off in ('', 'entry-off-'):
                    for mode in ('report', 'vtd-poll', 'vtd-policy-window', 'vtd-reserved', 'vtd-reserved-table', 'vtd-write-unknown:1', 'vtd-unmap:1', 'vtd-final:4'):
                        text = run(off+mode, binary)
                        if mode in ('report', 'vtd-poll', 'vtd-policy-window', 'vtd-reserved'): self.assertTrue(drain_result(text)['complete'])
                        else: self.retained(text)

    def test_compiled_adapter_weakening_controls(self):
        from fixture_compile import compile_fixture, recipe
        target = 'build/tests/intel-vtd-native'
        declaration = recipe(target); compiler, flags, _ = declaration
        flattened = subprocess.check_output([compiler, *flags, '-E', '-P', 'tools/test_intel_vtd_native.c'],
                                          cwd=ROOT, text=True, timeout=120)
        mutations = {
            'omit-hardware-census': ('return vtd_parent_identity() && source_engines_guard(&nonzero_native);', 'return vtd_parent_identity();', 'vtd-hda-pci:2'),
            'omit-final-load-hardware-admission': ('!vtd_registration() || !vtd_parent())return 0;', '!vtd_registration())return 0;', 'vtd-hda-pci:6'),
            'omit-final-store-hardware-admission': ('!vtd_registration() || !vtd_parent())return GT_WRITE_NONE;', '!vtd_registration())return GT_WRITE_NONE;', 'vtd-hda-pci:'+str(stats(self.texts[0])['write_boundary'])),
            'omit-map-hardware-admission': ('!vtd_policy() || !vtd_parent() ||', '!vtd_policy() ||', 'vtd-hda-pci:1'),
            'omit-unmap-hardware-admission': ('!vtd_registration() || !vtd_parent() ||', '!vtd_registration() ||', 'vtd-hda-pci:210'),
            'omit-deregister-hardware-admission': (' || !vtd_parent())return vtd_fail(64)', ')return vtd_fail(64)', 'vtd-hda-pci:211'),
            'omit-reserved-table-check': ('(!dma_resource_declared && !dma_table_current())', '0', 'vtd-reserved-table'),
            'omit-operation': ('if(!vtd_run())return', 'if(0 && !vtd_run())return', 'report'),
            'write-low-dword': ('return intel_display_test_vtd_write(off,value);', 'return intel_display_test_vtd_write(0x508,value);', 'report'),
            'omit-write-drain': ('return intel_display_test_vtd_write(off,value);', 'return intel_display_test_vtd_write(off,value&~0x10000u);', 'report'),
            'remove-parent-return-gate': ('|| (n && !vtd_returned())', '|| 0', 'vtd-final:4'),
            'remove-context-return-gate': ('if(v->context[i][j]!=expected[j])return 0', 'if((v->context[i][j]!=expected[j]) && 0)return 0', 'vtd-final:11'),
            'remove-poll-return-gate': ('if(i>=v->polls?w!=0:i+1==v->polls?w!=0x12030000:(w&~0x06000000u)!=0x90030000)return 0', 'if((i>=v->polls?w!=0:i+1==v->polls?w!=0x12030000:(w&~0x06000000u)!=0x90030000) && 0)return 0', 'vtd-final:9'),
            'remove-source-epoch-gate': ('x->epoch!=p->epoch', '0', 'vtd-final:14'),
            'remove-retired-map-gate': ('vtd_native.retired==1', '1', 'vtd-final:21'),
            'remove-final-cache-count': ('vtd_native.cache_checks==v->guards+v->reads+4', '1', 'vtd-final:29'),
            'remove-final-alias-policy': ('vtd_native.alias.no_execute==1', '1', 'vtd-final:27'),
        }
        with tempfile.TemporaryDirectory(prefix='vtd-native-controls-') as folder:
            for name, (old, new, mode) in mutations.items():
                with self.subTest(name=name):
                    self.assertIn(old, flattened)
                    source = Path(folder) / (name+'.c'); source.write_text(flattened.replace(old, new))
                    binary = Path(folder) / name
                    compile_fixture(target, output=binary, declaration=declaration,
                        replacements=[f'tools/test_intel_vtd_native.c={source}'])
                    def limit(): resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
                    p = subprocess.run([str(binary), '--wake-engine-'+mode], capture_output=True, text=True,
                                       preexec_fn=limit, timeout=90)
                    self.assertNotEqual(p.returncode, 0, name+' survived\n'+p.stdout[-1200:])

    def test_raw_first_collector_and_actual_version(self):
        from hardware_desktop import collect
        pre, final = native_chain(self.texts[0])
        with tempfile.TemporaryDirectory(prefix='vtd-collector-') as folder:
            root = Path(folder); home = root/'srvros-work'; logs = home/'Logs/session-0000000000000001-00'
            logs.mkdir(parents=True); (home/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            for name, text in (('boot.log', pre), ('session-a.log', pre), ('exit.log', final)):
                (logs/name).write_text(text)
            saved, report = collect(root, '37d8c5d0', root/'collected')
            row, = report['sessions']
            self.assertTrue(row['intel_vtd_drain']['native_complete'])
            self.assertEqual(row['intel_gt_wake']['summary']['v'], 36)
            self.assertEqual((saved/logs.name/'exit.log').read_bytes(), (logs/'exit.log').read_bytes())
            line = next(l for l in final.splitlines() if l.startswith('intel-vtd-drain:'))
            (logs/'exit.log').write_text(final.replace(line, ''))
            _, report = collect(root, '37d8c5d0', root/'rejected')
            row, = report['sessions']; self.assertTrue(row['attention'])
            self.assertFalse((row['intel_vtd_drain'] or {}).get('native_complete'))
            (logs/'exit.log').write_text(final)
            (logs/'session-a.log').write_text(pre.replace('vtd-drain-v1', 'source-audit-v1'))
            _, report = collect(root, '37d8c5d0', root/'spliced')
            row, = report['sessions']; self.assertTrue(row['attention'])
            self.assertIsNone(row['intel_vtd_drain'])

    def test_collector_policy_bound_shared_projections(self):
        from hardware_desktop import collect
        from intel_timing_policy import prepare_us
        with tempfile.TemporaryDirectory(prefix='vtd-collector-policy-') as folder:
            root=Path(folder); home=root/'srvros-work'; logs=home/'Logs'
            logs.mkdir(parents=True); (home/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
            first=logs/'session-0000000000000001-00';first.mkdir()
            second=logs/'session-0000000000000002-00';second.mkdir()
            def write(path, pre, final):
                for name,text in (('boot.log',pre),('session-a.log',pre),('exit.log',final)):
                    (path/name).write_text(text)
            # A historical receipt in the next session must keep its own policy.
            pre,final=native_chain(self.texts[0])
            line=next(l for l in final.splitlines() if l.startswith('intel-vtd-drain:'))
            raw=base64.b64decode(line.split('b=',1)[1])
            old='intel-vtd-drain: v=2 b='+base64.b64encode(raw[:1680]).decode()
            write(second,pre,final.replace(line,old))
            for off in ('','entry-off-'):
                pre,final=native_chain(run(off+'vtd-policy-window'))
                write(first,pre,final)
                saved,report=collect(root,'37d8c5d0',root/'accepted')
                current,historical=report['sessions']
                for row in (current,historical):
                    self.assertTrue(row['intel_vtd_drain']['native_complete'])
                    self.assertTrue(row['intel_gt_wake']['native_wake_complete'])
                    self.assertFalse(any(x.startswith('Invalid Intel ') for x in row['attention']),row['attention'])
                self.assertEqual(current['intel_vtd_drain']['prepare_budget_us'],150000)
                self.assertEqual(historical['intel_vtd_drain']['prepare_budget_us'],100000)
                self.assertEqual((saved/first.name/'exit.log').read_bytes(),(first/'exit.log').read_bytes())
                self.assertEqual(prepare_us(),100000)
            # A downstream shared-reader exception must not leak150ms into
            # the next historical session or out of collect().
            from intel_gt_wake import native_wake_result
            def fail_shared(text):
                if prepare_us()==150000:
                    raise ValueError('injected shared-reader failure')
                return native_wake_result(text)
            with patch('intel_gt_wake.native_wake_result',side_effect=fail_shared):
                _,report=collect(root,'37d8c5d0',root/'shared-failure')
            current,historical=report['sessions']
            self.assertIsNone(current['intel_gt_wake'])
            self.assertTrue(historical['intel_gt_wake']['native_wake_complete'])
            self.assertEqual(prepare_us(),100000)
            line=next(l for l in final.splitlines() if l.startswith('intel-vtd-drain:'))
            raw=base64.b64decode(line.split('b=',1)[1])
            downgraded='intel-vtd-drain: v=2 b='+base64.b64encode(raw[:1680]).decode()
            bad_policy=replace_raw(final,lambda b:struct.pack_into('<Q',b,1680,150001))
            for bad_pre,bad_final in ((pre,final.replace(line,'')),(pre,final+'\n'+line),
                    (pre,bad_policy),(pre,final.replace(line,downgraded)),
                    (pre.replace('vtd-drain-v1','source-audit-v1'),final)):
                write(first,bad_pre,bad_final)
                _,report=collect(root,'37d8c5d0',root/'refused')
                current,historical=report['sessions']
                self.assertIsNone(current['intel_vtd_drain'])
                self.assertIsNone(current['intel_gt_wake'])
                self.assertTrue(any(x.startswith('Invalid Intel disabled-state VT-d drain:') for x in current['attention']))
                self.assertTrue(historical['intel_vtd_drain']['native_complete'])
                self.assertEqual(historical['intel_vtd_drain']['prepare_budget_us'],100000)
                self.assertEqual(prepare_us(),100000)

    def test_original_routes_and_pending_operation(self):
        for off in ('', 'entry-off-'):
            for mode in ('report', 'vtd-poll'):
                text = run(off+mode); r = drain_result(text)
                self.assertTrue(r['complete']); self.assertFalse(r['native_complete'])
                self.assertEqual((r['summary']['maps'], r['summary']['retired'], r['summary']['writes']), (1, 1, 1))
                self.assertEqual(r['audit']['summary']['pci_reads'], 18)
                self.assertEqual(r['audit']['summary']['plane_reads'], 54)
                self.assertFalse(r['hardware_ownership']); self.assertEqual(r['managed_pages'], 0)

    def test_native_receiver_and_checkpoint_original_binding(self):
        for text in self.texts:
            pre, final = native_chain(text)
            r = drain_result(final, native=True)
            self.assertTrue(r['native_complete'])
            self.assertTrue(verify_drain_checkpoint(pre, final, native=True))
            with self.assertRaises(ValueError): drain_result(text, native=True)

    def test_each_target_failure_and_mapping_cache_permissions(self):
        for off, text in zip(('', 'entry-off-'), self.texts):
            s = stats(text)
            for mode, count in (('vtd-read', s['reads']), ('vtd-float', s['reads']), ('vtd-value', s['reads']),
                                ('vtd-cache', s['caches']), ('vtd-cache-drift', s['caches']),
                                ('vtd-alias', s['aliases']), ('vtd-alias-readonly', s['aliases']),
                                ('vtd-alias-user', s['aliases']), ('vtd-alias-pat', s['aliases']),
                                ('vtd-alias-nx', s['aliases']), ('vtd-alias-large', s['aliases'])):
                for n in range(1, count+1):
                    with self.subTest(off=off, mode=mode, n=n): self.retained(run(f'{off}{mode}:{n}'))
            for mode in ('vtd-table', 'vtd-resource', 'vtd-map', 'vtd-unmap', 'vtd-write-none', 'vtd-write-unknown'):
                with self.subTest(off=off, mode=mode): self.retained(run(f'{off}{mode}:1'))

    def test_every_original_boundary_and_final_record_gate(self):
        for off, text in zip(('', 'entry-off-'), self.texts):
            for mode in ('vtd-epoch', 'vtd-parent', 'vtd-registration', 'vtd-root', 'vtd-source', 'vtd-time', 'vtd-map-drift', 'vtd-inherited-map', 'vtd-inherited-root', 'vtd-hda-pci', 'vtd-hda-alias', 'vtd-hda-borrow'):
                for n in range(1, stats(text)['boundaries']+1):
                    if mode in ('vtd-epoch', 'vtd-registration', 'vtd-source') and n == 1: continue # not constructed yet
                    if mode == 'vtd-map-drift' and n in (1, stats(text)['boundaries']): continue # no alias
                    with self.subTest(off=off, mode=mode, n=n): self.retained(run(f'{off}{mode}:{n}'))
            for n in range(1, 41):
                with self.subTest(off=off, final=n): self.retained(run(f'{off}vtd-final:{n}'))

    def test_receiver_rejects_missing_duplicate_wrong_policy_and_claims(self):
        for text in self.texts:
            pre, final = native_chain(text)
            lines = [l for l in final.splitlines() if l.startswith(('intel-vtd-drain:', 'intel-vtd-table:'))]
            for line in lines:
                for bad in (final.replace(line, '', 1), final+'\n'+line):
                    with self.assertRaises(ValueError): drain_result(bad, native=True)
            for n, key in enumerate(FIELDS):
                def change(raw, n=n, key=key):
                    old = struct.unpack_from('<Q', raw, n*8)[0]
                    value = 0 if key in ('begin','end','last','native_begin','native_end','hda','outer') else old+1
                    if key == 'resource': value = 0
                    struct.pack_into('<Q', raw, n*8, value)
                with self.subTest(key=key):
                    with self.assertRaises(ValueError): drain_result(replace_raw(final, change), native=True)
            for offset in [400+8*n for n in range(16)] + [528, 528+4*255]:
                with self.subTest(offset=offset):
                    with self.assertRaises(ValueError): drain_result(replace_raw(final, lambda b: b.__setitem__(offset, b[offset]^1)), native=True)
            with self.assertRaises(ValueError): verify_drain_checkpoint(pre+'\n'+lines[0], final, native=True)


if __name__ == '__main__':
    unittest.main()
