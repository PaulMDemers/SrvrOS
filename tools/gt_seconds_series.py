#!/usr/bin/env python3
"""Explicit opt-in, bounded physical diagnostic series. No general boot loop."""
import argparse,datetime,hashlib,json,os,re,shutil,subprocess,sys
from pathlib import Path
from workflow.store import atomic,sha,lock
ROOT=Path(__file__).resolve().parents[1]
STORE=Path.home()/'.local/share/srvros/device-workflow'
UNIT='srvros-gt-seconds-series.service'
PROFILE='intel-gt-seconds-v42'

def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()

def next_action(plan,state,report,boot):
    if plan['status']!='RUNNING':return 'STOP'
    if plan['index']==len(plan['runs']):return 'DONE'
    if not 0<=plan['index']<len(plan['runs'])<=3:raise ValueError('Series bound')
    if state is None:return 'CREATE'
    if state.get('intent') or str(state.get('status','')).startswith('FAILED'):return 'STOP'
    phase=state['phase']
    if phase=='NEW':return 'QUALIFY'
    if phase=='QUALIFIED':return 'BOOT'
    if phase=='BOOT_REQUESTED':return 'COLLECT' if state['host_boot_id']!=boot else 'WAIT_BOOT'
    if phase=='COLLECTED':return 'DIAGNOSE' if report and report.get('diagnostic_complete') and report.get('offline_integrity')=='PASS' and not report.get('native_acceptance') else 'STOP'
    if phase=='DIAGNOSED':return 'BANK'
    return 'STOP'


def save_plan(path,value):
    if path.exists() and json.loads(path.read_text())['status']!='RUNNING' and value['status']=='RUNNING':
        raise ValueError('Series stopped during action')
    atomic(path,value)


def advance(plan,ops,save):
    """One serialized worker. All destructive/privileged effects use Runner CLI."""
    try:
        while plan['status']=='RUNNING':
            ops.integrity(plan)
            if plan['index']==len(plan['runs']):
                plan.update(status='COMPLETE',finished=now(),intent=None);save(plan);ops.disable();return
            name=plan['runs'][plan['index']];state,report=ops.read(name)
            action=next_action(plan,state,report,ops.boot_id())
            if action=='WAIT_BOOT':return
            if action=='STOP':
                plan.update(status='STOPPED',error='Incomplete/failed or interrupted run: '+name,intent=None);save(plan);ops.disable();return
            plan.update(intent=action,updated=now());save(plan)
            if action=='CREATE':ops.create(name)
            elif action=='QUALIFY':ops.qualify(name)
            elif action=='BOOT':
                ops.boot(name);plan.update(intent=None,updated=now());save(plan);return
            elif action=='COLLECT':ops.collect(name)
            elif action=='DIAGNOSE':ops.diagnose(name)
            elif action=='BANK':
                result=ops.bank(name);plan['results'].append(result);plan['index']+=1
            else:raise ValueError('Unknown series action')
            plan.update(intent=None,updated=now());save(plan)
    except BaseException as error:
        plan.update(status='STOPPED',error=str(error),updated=now());save(plan);ops.disable();raise


class Host:
    def __init__(self,path):self.path=path;self.store=path.parent
    def command(self,*args):
        print('series action:',*args,flush=True)
        with (self.store/'seconds-series-actions.log').open('a') as log:
            subprocess.run([sys.executable,str(ROOT/'tools/device_workflow.py'),'--store',str(self.store),*args],cwd=ROOT,check=True,stdout=log,stderr=subprocess.STDOUT)
    def integrity(self,plan):
        if plan['schema']!=1 or plan['profile']!=PROFILE or len(plan['runs'])!=3 or len(set(plan['runs']))!=3:raise ValueError('Invalid series plan')
        if len(plan['results'])!=plan['index'] or [r['run'] for r in plan['results']]!=plan['runs'][:plan['index']]:raise ValueError('Series progress mismatch')
        if any(r.get('native_acceptance') or r.get('original_timing_acceptance') or r.get('source')!=plan['source'] or r.get('return_integrity')!='PASS' for r in plan['results']):raise ValueError('Series result provenance')
        if any(not re.fullmatch('[a-z0-9][a-z0-9-]{0,63}',n) for n in plan['runs']):raise ValueError('Invalid run name')
        if subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()!=plan['source'] or subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True):raise ValueError('Frozen source changed; stop series')
        if sha(self.store/'unit.json')!=plan['unit_sha256']:raise ValueError('Unit/recovery/workspace configuration changed')
        unit=json.loads((self.store/'unit.json').read_text())
        if Path('/sys/class/dmi/id/product_name').read_text().strip()!=unit['model'] or unit['model']!='MacBookAir7,2':raise ValueError('Local model changed')
    def read(self,name):
        d=self.store/'runs'/name
        if not d.exists():return None,None
        m=json.loads((d/'manifest.json').read_text())
        if m['profile']!=PROFILE:raise ValueError('Foreign run in series')
        s=json.loads((d/'state.json').read_text());r=json.loads((d/'report.json').read_text()) if (d/'report.json').exists() else None
        return s,r
    def boot_id(self):return Path('/proc/sys/kernel/random/boot_id').read_text().strip()
    def create(self,name):self.command('new',name,'--profile',PROFILE,'--question','Does this wide-policy original GT lifetime completely retire with preserved state?','--expected','Complete typed seconds diagnostic, all original identities/preservation/retirement, clean media/recovery; original timing acceptance withheld')
    def qualify(self,name):self.command('advance','--run',name)
    def boot(self,name):self.command('advance','--run',name,'--through','boot_requested')
    def collect(self,name):self.command('collect','--run',name)
    def diagnose(self,name):self.command('diagnose','--run',name)
    def disable(self):subprocess.run(['systemctl','--user','disable',UNIT],check=False,capture_output=True)
    def bank(self,name):
        d=self.store/'runs'/name;diag=json.loads((d/'diagnostic.json').read_text());report=json.loads((d/'report.json').read_text())
        if json.loads((d/'state.json').read_text()).get('diagnostic_sha256')!=sha(d/'diagnostic.json'):raise ValueError('Diagnostic close record changed')
        if diag['native_acceptance'] or diag['original_timing_acceptance'] or diag['return_integrity']!='PASS' or diag['status']!='DIAGNOSTIC_COMPLETE':raise ValueError('Not a reviewed diagnostic')
        target=Path.home()/'.local/share/srvros/evidence'/diag['release']/name;target.mkdir(parents=True,exist_ok=True)
        raw=d/'returned'/('raw-'+diag['session'])
        for n,item in report['raw_manifest'].items():
            p=raw/n
            if p.stat().st_size!=item['bytes'] or sha(p)!=item['sha256']:raise ValueError('Raw changed before bank')
        selected=[p for p in d.iterdir() if p.is_file() and p.suffix in ('.json','.log')]
        for folder in ('before','returned','logs'):
            selected.extend(p for p in (d/folder).glob('*') if p.is_file() and p.suffix in ('.json','.log'))
        selected.extend(raw.glob('*'));hashes={}
        for p in selected:
            q=target/p.relative_to(d);q.parent.mkdir(parents=True,exist_ok=True)
            if q.exists():
                if sha(p)!=sha(q):raise ValueError('Bank collision')
            else:shutil.copy2(p,q)
            hashes[str(q.relative_to(target))]=sha(q)
        atomic(target/'bank.json',dict(files=hashes,run=name,native_acceptance=False,diagnostic=diag))
        os.sync()
        if any(sha(target/n)!=h for n,h in hashes.items()):raise ValueError('Bank verification failed')
        return dict(run=name,bank=str(target),diagnostic_sha256=sha(d/'diagnostic.json'),**diag)


def start(first,store):
    path=store/'seconds-series.json'
    if path.exists():raise ValueError('Existing series record must be archived/reviewed before a new opt-in series')
    run=store/'runs'/first;m=json.loads((run/'manifest.json').read_text());s=json.loads((run/'state.json').read_text())
    if m['profile']!=PROFILE or s['phase']!='QUALIFIED' or s.get('intent'):raise ValueError('First run must be frozen and fully qualified seconds diagnostic')
    stem=first[:-3] if first.endswith('-r1') else first
    names=[first,stem+'-r2',stem+'-r3']
    plan=dict(schema=1,profile=PROFILE,status='RUNNING',source=m['source'],unit_sha256=sha(store/'unit.json'),
              runs=names,index=0,results=[],intent=None,created=now(),authorization='User requested three multi-second diagnostic runs; existing unattended1% reboot authorization; stop on failure')
    host=Host(path);host.integrity(plan)
    if any((store/'runs'/n).exists() for n in names[1:]):raise ValueError('Series run name collision')
    atomic(path,plan)
    def quote(v):return '"'+str(v).replace('%','%%').replace('\\','\\\\').replace('"','\\"')+'"'
    unit=Path.home()/'.config/systemd/user'/UNIT
    content='[Unit]\nDescription=Explicit three-run SrvrOS seconds diagnostic\nWants=srvros-return-collect.service\nAfter=srvros-return-collect.service\n[Service]\nType=oneshot\nWorkingDirectory='+str(ROOT)+'\nExecStart='+' '.join(quote(v) for v in (sys.executable,ROOT/'tools/gt_seconds_series.py','--store',store,'run'))+'\nTimeoutStartSec=7200\nUMask=0077\n[Install]\nWantedBy=default.target\n'
    atomic(unit,content.encode());subprocess.run(['systemctl','--user','daemon-reload'],check=True)
    try:
        subprocess.run(['systemctl','--user','enable',UNIT],check=True)
        subprocess.run(['systemctl','--user','start','--no-block',UNIT],check=True)
    except BaseException as error:
        plan.update(status='STOPPED',error=str(error));atomic(path,plan);host.disable();raise
    return path


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--store',type=Path,default=STORE)
    sub=p.add_subparsers(dest='action',required=True);st=sub.add_parser('start');st.add_argument('--first-run',required=True)
    sub.add_parser('run');sub.add_parser('status');sub.add_parser('stop');a=p.parse_args();path=a.store/'seconds-series.json'
    if a.action=='start':print(start(a.first_run,a.store));return
    plan=json.loads(path.read_text())
    if a.action=='status':print(json.dumps(plan,indent=2));return
    host=Host(path)
    if a.action=='stop':
        plan.update(status='STOPPED',error='Explicit stop requested');atomic(path,plan);host.disable()
        subprocess.run(['systemctl','--user','stop',UNIT],check=False)
        # An already requested reboot may finish; the return collector still
        # saves it. A stopped plan never launches a subsequent trial.
        print('Future trials stopped; an already requested boot may finish.');return
    with lock(a.store/'seconds-series.lock'):
        plan=json.loads(path.read_text())
        advance(plan,host,lambda value:save_plan(path,value))
    print(json.dumps(plan,indent=2))

if __name__=='__main__':main()
