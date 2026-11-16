"""Actual original native adapter; synthetic evidence never accepts hardware."""
from pathlib import Path
import json,os,re,resource,subprocess,tempfile,unittest
from hardware_boot import limine_config
from intel_gt_bootstrap import bootstrap_result,bootstrap_recipe,historical_view,verify_bootstrap_checkpoint

ROOT=Path(__file__).resolve().parents[1]
EXE=Path(os.environ.get('INTEL_GT_BOOTSTRAP_EXE',ROOT/'build/tests/intel-gt-bootstrap-adapter'))
resource.setrlimit(resource.RLIMIT_CORE,(0,0))


def run(mode='pass',n=0,disabled=False,exe=EXE,env=None):
    p=subprocess.run([str(exe),mode,str(n),*(['disabled'] if disabled else [])],cwd=ROOT,
                     capture_output=True,text=True,timeout=20,env=env)
    if p.returncode:raise AssertionError((mode,n,disabled,p.stdout[-2200:],p.stderr))
    return p.stdout


def edit(text,prefix,key,value):
    changed,n=re.subn(r'(^'+re.escape(prefix)+r':.*?\b'+key+r'=)\S+',lambda m:m[1]+str(value),text,count=1,flags=re.M)
    assert n==1
    return changed


class Native(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text={(d,p):run('preserved' if p else 'pass',disabled=d) for d in (False,True) for p in (False,True)}
        cls.stats={k:{a:int(b) for a,b in re.findall(r'(\w+)=(\d+)',t.split('native-bootstrap-fixture:')[1].splitlines()[0])} for k,t in cls.text.items()}
        cls.phases=[int(v) for _,v in re.findall(r'(\d+)=(\d+)',cls.text[False,False].split('native-bootstrap-phases:')[1])]

    def test_complete_original_owners_and_new_epoch(self):
        for (disabled,preserved),text in self.text.items():
            with self.subTest(disabled=disabled,preserved=preserved):
                p=bootstrap_result(text)
                self.assertTrue(p['complete']);self.assertFalse(p['native_complete'] or p['hardware_ownership'] or p['managed_pages'] or p['physical_tearfree'])
                s,r=p['summary'],p['reset']
                self.assertEqual((s['epoch_before'],s['epoch_after']),(0,1) if disabled else (1,2))
                self.assertEqual((r['changed'],r['restored'],r['writes']),(0,0,7) if preserved else (1,1,10))
                self.assertEqual((s['native_writes'],s['pci_samples'],s['commands'],s['prepared']),(12 if preserved else 15,4,2,5))
                self.assertEqual(p['binding']['post_epoch'],s['epoch_after'])
                self.assertEqual([q['before'] for q in p['fences']],[0x021ff02f00000001]+[0]*31)
                self.assertEqual(p['entry']['entry_kind'],'already-disabled' if disabled else 'enabled')

    def test_every_native_admission_boundary_retains_independent_parent(self):
        points=self.stats[False,False]['boundaries'];count=0
        for mode in ('zero','back','over','root','epoch','borrow','request','hda','alias'):
            for n in range(1,points+1):run(mode,n);count+=1
        # Binding and child registration do not exist during preparation.
        for mode in ('source','registration'):
            for stage in range(1,9):run(mode,self.phases[stage]);count+=1
        run('source',self.phases[9]);count+=1
        print('Native bootstrap admission faults PASS',count,flush=True)

    def test_each_native_register_and_store_failure(self):
        stats=self.stats[False,False];count=0
        for mode in ('read','float','read-over'):
            for n in range(1,stats['reads']+1):run(mode,n);count+=1
        for mode in ('write-none','write-unknown','write-over'):
            for n in range(1,stats['writes']+1):run(mode,n);count+=1
        for mode in ('command-none','command-unknown'):
            for n in (1,2):run(mode,n);count+=1
        for mode in ('display-read','display-float'):
            for n in range(1,79):run(mode,n);count+=1
        for n in range(40,79):run('display-change',n);count+=1
        print('Native bootstrap I/O faults PASS',count,flush=True)

    def test_every_native_pci_read_float_stops_before_more_io(self):
        stats=self.stats[False,False]
        for n in range(stats['first_pci'],stats['pci']+1):run('pci-float',n)
        print('Native bootstrap PCI faults PASS',stats['pci']-stats['first_pci']+1,flush=True)

    def test_cpu_cache_and_every_alias_attribute(self):
        count=0
        for mode in ('cache','cache-change'):
            # First is fence-map admission, second is the initial context.
            for n in range(2,self.stats[False,False]['cache']+1):run(mode,n);count+=1
        # Every slot at every protocol phase, with a real fresh alias walk.
        bases=sorted({0,*[15*(n-1) for n in self.phases if n>0]})
        for mode in ('walk','user-alias','exec-alias','wc-alias','read-only'):
            for base in bases:
                for slot in range(1,16):run(mode,base+slot);count+=1
        print('Native bootstrap CPU faults PASS',count,flush=True)

    def native_fixture(self,disabled=False):
        from test_intel_shutdown import ShutdownTests
        version='ram-usb-v1-intel-gt-bootstrap-laptop-probe-37d8c5d0-auto-return'
        cmd=next(l.split('cmdline:',1)[1].strip() for l in limine_config('0123456789abcdef',version).splitlines() if 'cmdline:' in l)
        prefix='init: deferred=gt-bootstrap-preinit; terminal automatic capture, no user services\nlaptop-capture: boot options='+cmd+'\n'
        native=ShutdownTests.native(self.text[disabled,False]);before,after=native.split('intel-bootstrap-summary:',1)
        return prefix+before+'workspace-log: snapshot=session-a.log\n',prefix+before+'intel-wake: preflight checkpoint saved\nintel-bootstrap-summary:'+after+'laptop-capture: completed=yes\n'

    def test_receipt_and_cost_clock_edges(self):
        for mode in ('ticket-empty','ticket-samples','ticket-clock','ticket-proof','ticket-attempt','ticket-state','ticket-zero','ticket-back','ticket-over'):
            for n in (1,self.stats[False,False]['tickets']):
                proof=bootstrap_result(run(mode,n));self.assertFalse(proof['complete']);self.assertEqual(proof['admission']['rejected'],1)
        for mode in ('cost-zero','cost-back','cost-over'):
            for point in sorted({1,*[n for n in self.phases if n]}):
                for part in range(5):run(mode,5*(point-1)+part+1)

    def test_single_use_receipts_reject_stale_mutated_and_expired_reads(self):
        count=0
        for mode in ('ticket-empty','ticket-samples','ticket-clock','ticket-proof','ticket-attempt','ticket-state','ticket-zero','ticket-back','ticket-over'):
            for n in range(1,self.stats[False,False]['tickets']+1):run(mode,n);count+=1
        for i in range(self.stats[False,False]['seal_bytes']):run('ticket-byte',i);count+=1
        print('Native bootstrap sealed receipt faults PASS',count,flush=True)

    def test_receipt_cost_and_historical_v1_are_separate(self):
        for text in self.text.values():
            p=bootstrap_result(text);a=p['admission']
            self.assertEqual(p['version'],2)
            self.assertEqual((a['consumed'],a['issued'],a['active'],a['rejected']),
                (p['reset']['reads'],p['reset']['proofs'],0,0))
            self.assertLess(p['summary']['guards'],900)
            for q in p['cost']:self.assertEqual(q['calls'],p['summary']['guards'])
            old='\n'.join(l.replace('intel-bootstrap-summary: v=2 ','intel-bootstrap-summary: v=1 ') for l in text.splitlines() if not l.startswith(('intel-bootstrap-admission:','intel-bootstrap-cost:')))
            historical=bootstrap_result(old)
            self.assertTrue(historical['complete']);self.assertEqual(historical['version'],1)
            self.assertIsNone(historical['admission'])

    def test_native_receiver_checkpoint_and_raw_preservation(self):
        from hardware_desktop import collect
        for disabled in (False,True):
            pre,text=self.native_fixture(disabled)
            self.assertTrue(bootstrap_result(text,native=True)['native_complete'])
            self.assertTrue(verify_bootstrap_checkpoint(pre,text,native=True))
            with tempfile.TemporaryDirectory(prefix='native-bootstrap-collector-') as directory:
                root=Path(directory);work=root/'srvros-work';logs=work/'Logs/session-0000000000000001-00';logs.mkdir(parents=True)
                (work/'.workspace').write_text('SRVROS-WORKSPACE-V1 37d8c5d0\n')
                for name,data in [('boot.log',pre),('session-a.log',pre),('exit.log',text)]:(logs/name).write_text(data)
                destination,report=collect(root,'37d8c5d0',root/'positive');row,=report['sessions']
                self.assertEqual(row['attention'],[]);self.assertTrue(row['intel_gt_bootstrap']['native_complete'])
                self.assertEqual((destination/'session-0000000000000001-00/exit.log').read_bytes(),text.encode())

    def test_receiver_mutations_cannot_promote_a_partial_or_changed_source(self):
        pre,text=self.native_fixture();count=0
        changes={
            'intel-bootstrap-summary':dict(complete=0,epoch_before=9,epoch_after=1,epoch_calls=0,active=1,returned=0,commands=1,prepared=4,pci_reads=35,native_writes=14,alias_checks=0,cache_checks=0,full_after=0,post_bound=0,obligations=0,managed_pages=1,hardware_ownership=1,prepare_budget=150001),
            'intel-bootstrap-reset':dict(complete=0,registered=1,retained=1,epoch=0,reset_attempts=1,reset_writes=1,reset_completed=1,cancelled=4,changed='0x0',restored='0x0',reads=309,writes=9,proofs=0,preserved=0,settle_end=1),
            'intel-bootstrap-binding':dict(owner=0,provider=0,lifetime=0,root='0x2000',mmio='0xd0000000',epoch=9,sequence=2,prepare=1,gt=1,hda=1,outer=1,policy='0x1',post_epoch=9,post_parent=0,post_object=0),
            'intel-bootstrap-retire':dict(stage=0,guard_end=1,return_end=1,final_end=1),
            'intel-bootstrap-admission':dict(consumed=0,issued=0,rejected=1,active=1,stage=0,partial_begin=0),
            'intel-bootstrap-cost':dict(calls=0,total=150001,max=150001),
            'intel-bootstrap-fence':dict(before='0x1',after='0x1',final='0x0'),
            'intel-bootstrap-control':dict(before='0x0',after='0x3',cancel_final='0x1',prepare_polls=0,cancel_polls=0),
            'intel-bootstrap-pulse':dict(first='0x2',final='0x1',polls=0),
            'intel-bootstrap-pci':dict(raw='0x0'),
            'intel-bootstrap-display':dict(raw='0x0'),
        }
        for prefix,values in changes.items():
            for key,value in values.items():
                with self.subTest(prefix=prefix,key=key):
                    with self.assertRaises(ValueError):bootstrap_result(edit(text,prefix,key,value),native=True)
                    count+=1
        lines=text.splitlines()
        for i,line in enumerate(lines):
            if not line.startswith('intel-bootstrap-'):continue
            for mutated in ('\n'.join(lines[:i]+lines[i+1:]),'\n'.join(lines[:i]+[line]+lines[i:])):
                with self.assertRaises(ValueError):bootstrap_result(mutated,native=True)
                count+=1
        for modified in (pre+lines[next(i for i,l in enumerate(lines) if l.startswith('intel-bootstrap-summary:'))]+'\n',pre.replace('37d8c5d0','37d8c5d1')):
            with self.assertRaises(ValueError):verify_bootstrap_checkpoint(modified,text,native=True)
            count+=1
        print('Native bootstrap receiver mutations PASS',count,flush=True)

    def test_recipe_preinit_and_profile_isolation(self):
        from hardware_boot import stage
        cmd=next(l.split('cmdline:',1)[1].strip() for l in limine_config('0123456789abcdef','ram-usb-v1-intel-gt-bootstrap-laptop-probe-37d8c5d0-auto-return').splitlines() if 'cmdline:' in l)
        self.assertEqual(bootstrap_recipe(cmd),dict(volume='37d8c5d0',release='0123456789abcdef'))
        self.assertEqual(subprocess.run(['build/tests/intel-gt-boot','gt-bootstrap',cmd],cwd=ROOT).returncode,0)
        subprocess.run(['build/tests/boot-gt-bootstrap'],cwd=ROOT,check=True)
        for i,token in enumerate(cmd.split()):
            for invalid in (' '.join(cmd.split()[:i]+cmd.split()[i+1:]),cmd+' '+token):
                with self.assertRaises(ValueError):bootstrap_recipe(invalid)
                self.assertNotEqual(subprocess.run(['build/tests/intel-gt-boot','gt-bootstrap',invalid],cwd=ROOT).returncode,0)
        for flags in ({'intel_ring_entry':True},{'intel_nonzero_reset':True},{'intel_vtd_drain':True},{'desktop_battery':True},{'hold_on_failure':True}):
            with self.assertRaises(ValueError):stage(laptop_probe_volume='37d8c5d0',intel_gt_bootstrap=True,**flags)
        with self.assertRaises(ValueError):historical_view(self.text[False,False].replace('intel-wake-summary: v=37','intel-wake-summary: v=25'))

    def test_fatal_sanitizers(self):
        from fixture_compile import recipe
        cc,flags,sources=recipe('build/tests/intel-gt-bootstrap-adapter')
        with tempfile.TemporaryDirectory(prefix='native-bootstrap-san-') as directory:
            exe=Path(directory)/'adapter'
            subprocess.run([cc,*flags,'-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer',*sources,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
            for disabled in (False,True):
                for mode in ('pass','preserved'):run(mode,disabled=disabled,exe=exe,env=env)
                for mode,n in [('root',1),('cache',3),('walk',1),('source',self.phases[6]),('registration',self.phases[6]),('write-unknown',6),('write-none',7),('write-over',8),('command-unknown',2),('ticket-empty',1),('ticket-samples',2),('ticket-clock',1),('ticket-state',1),('ticket-zero',1),('ticket-back',1),('ticket-over',1),('ticket-byte',0)]:run(mode,n,disabled=disabled,exe=exe,env=env)

    def test_completed_child_and_late_logical_return_cannot_release_physical_parent(self):
        outer=bootstrap_result(run('outer-pci',1))
        self.assertTrue(outer['owner_complete'])
        self.assertFalse(outer['complete'] or outer['native_complete'])
        run('final-fence',self.phases[9])
        for mode in ('return-zero','return-back','return-over','final-zero','final-back','final-over','final-epoch'):
            text=run(mode,1);proof=bootstrap_result(text)
            self.assertFalse(proof['complete']);self.assertTrue(proof['summary']['failed'])
            self.assertEqual((proof['summary']['returned'],proof['summary']['active']),(1,0))
            self.assertTrue(proof['reset']['complete'])

    def test_compiled_native_weakening_controls(self):
        from fixture_compile import recipe
        source=(ROOT/'kernel/src/drivers/intel_gt_bootstrap_trial.inc').read_text()
        controls={
            'receipt-single-use':('if(!active || bootstrap_admission.samples!=1 ||','if(((void)active,false) || bootstrap_admission.samples!=1 ||','ticket-empty',1),
            'receipt-clock-callback':('bootstrap_admission.samples!=1','false','ticket-samples',1),
            'receipt-byte-seal':('if(bytes[i]!=bootstrap_admission.bytes[i])goto refuse;','if((void)bytes,false)goto refuse;','ticket-byte',0),
            'cache-context':('!x86_cache_equal(&cache,&gt_bootstrap_native.cache)','false','cache-change',3),
            'alias-user':('alias.user ||','false ||','user-alias',1),
            'alias-nx':('!alias.no_execute','false','exec-alias',1),
            'alias-pat':('alias.pat_index!=2','false','wc-alias',1),
            'alias-write':('alias.writable!=(unsigned)writable','(writable && false)','read-only',1),
            'display-preservation':('n && value!=gt_bootstrap_native.display[0][p][i]','false','display-change',43),
            'final-guard-clock':('if(!bootstrap_after_return() || !bootstrap_tick())goto fail;','if(!bootstrap_after_return())goto fail;','final-over',1),
            'completed-child-fence':('r->fence_final[i]!=r->binding.fences[i]','false','final-fence',self.phases[9]),
        }
        cc,flags,sources=recipe('build/tests/intel-gt-bootstrap-adapter')
        with tempfile.TemporaryDirectory(prefix='native-bootstrap-controls-') as directory:
            for name,(old,new,mode,n) in controls.items():
                with self.subTest(control=name):
                    self.assertEqual(source.count(old),1)
                    unit=Path(directory)/(name+'.inc');unit.write_text(source.replace(old,new))
                    exe=Path(directory)/name
                    subprocess.run([cc,*flags,'-DSRVROS_INTEL_GT_BOOTSTRAP_ADAPTER_FILE="'+str(unit)+'"',*sources,'-o',str(exe)],cwd=ROOT,check=True,timeout=180)
                    run(exe=exe)  # Valid control still works; compile failure is not detection.
                    p=subprocess.run([str(exe),mode,str(n)],cwd=ROOT,capture_output=True,text=True,timeout=20)
                    self.assertNotEqual(p.returncode,0,'Undetected native weakening '+name)
        print('Native bootstrap compiled controls PASS',len(controls),flush=True)

    def test_selected_logger_capacity_and_native_image_isolation(self):
        from hardware_desktop import session_log_limit
        for target in ('bootlog-bootstrap','workspace-log-bootstrap'):
            subprocess.run(['build/tests/'+target],cwd=ROOT,check=True)
        text=self.text[False,False]
        self.assertEqual(session_log_limit(text.encode()),98304)
        self.assertEqual(session_log_limit(text.replace('intel-wake-summary: v=37','intel-wake-summary: v=25').encode()),65536)
        self.assertEqual(session_log_limit(b'intel-wake-summary: v=37\n'),65536)
        plans={target:subprocess.check_output(['make','-Bn',target],cwd=ROOT,text=True,timeout=180) for target in
            ('build/intel-gt-bootstrap/srvros.elf','build/intel-ring-entry/srvros.elf','build/kernel/srvros.elf','build/intel-command-ownership/srvros.elf')}
        new=plans.pop('build/intel-gt-bootstrap/srvros.elf')
        self.assertIn('-DSRVROS_INTEL_GT_BOOTSTRAP_NATIVE',new)
        self.assertIn('-Wframe-larger-than=2048 -c kernel/diagnostics/intel_bdw_gt_bootstrap.c',new)
        for old in plans.values():self.assertNotIn('-c kernel/diagnostics/intel_bdw_gt_bootstrap.c',old)
        self.assertNotIn('-DSRVROS_INTEL_NONZERO_RESET',new)
        self.assertNotIn('-DSRVROS_INTEL_VTD_DRAIN',new)


if __name__=='__main__':unittest.main()
