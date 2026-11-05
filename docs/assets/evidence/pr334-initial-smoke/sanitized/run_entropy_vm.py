"""Run only the four reviewed PR334 guests in one finite coordinated window."""
from pathlib import Path
import hashlib,json,os,shutil,signal,subprocess,sys,time
P=Path(__file__).resolve().parents[2];V=P.parent/'.local/pr334-build-volume';Q=P.parent/'.local/pr334-entropy-exact-review';O=V/'evidence/entropy-guest-run'
sys.path.insert(0,str(P/'tools'))
from browser_storage import StorageGuard
O.mkdir(exist_ok=False)
grant=json.loads((V/'evidence/entropy-guest-grant.json').read_text());start=time.monotonic();end=start+max(0,min(1230,grant['received_epoch']+1230-time.time()))
record={'result':'failed','stages':[]};guard=StorageGuard(O,backing_path='$BACKING_VOLUME')
def sha(p):
 h=hashlib.sha256()
 with Path(p).open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def check():
 guard.preflight()
 if shutil.disk_usage('$BACKING_VOLUME').free<4*1024**3:raise RuntimeError('backingfloor4GiB')
 if time.monotonic()>=end:raise RuntimeError('aggregate1230s workdeadline')
def table():
 rows={}
 for line in subprocess.check_output(['ps','-axo','pid=,ppid=,pgid=,stat=,command='],text=True).splitlines():
  a=line.strip().split(None,4)
  if len(a)==5:rows[int(a[0])]={'ppid':int(a[1]),'pgid':int(a[2]),'stat':a[3],'command':a[4]}
 return rows
def clean(proc,item):
 proc.poll();remaining={pid:r for pid,r in table().items() if r['pgid']==proc.pid};item['before_cleanup']=remaining;item['status_before_cleanup']=proc.returncode
 if remaining:
  for sig,seconds in [(signal.SIGINT,10),(signal.SIGTERM,5),(signal.SIGKILL,5)]:
   if not any(r['pgid']==proc.pid for r in table().values()):break
   try:os.killpg(proc.pid,sig)
   except ProcessLookupError:pass
   deadline=time.monotonic()+seconds
   while time.monotonic()<deadline:
    proc.poll()
    if not any(r['pgid']==proc.pid for r in table().values()):break
    time.sleep(.05)
 proc.poll();item['status']=proc.returncode;item['remaining']={pid:r for pid,r in table().items() if r['pgid']==proc.pid}
 if item['remaining']:raise RuntimeError('owned harness/QEMU group remains')
 if remaining and item['status_before_cleanup']==0:raise RuntimeError('residuals after nominalsuccess')
try:
 check();assert sha(Q/'packet.json')==grant['packet_sha256'];assert sha(Q/'review-signoff.json')==grant['signoff_sha256']
 packet=json.loads((Q/'packet.json').read_text());signoff=json.loads((Q/'review-signoff.json').read_text());assert signoff['result'].startswith('PASS')
 for path,rel in packet['frozen_paths'].items():check();assert sha(path)==packet['files'][rel],path
 record['fresh_live_identities']=len(packet['frozen_paths']);plan=packet['prepared'];budgets={'normal-absent':90,'normal-rdrand':90,'native-context':180,'jsc-entropy':840}
 for stage in plan:
  check()
  # Inspect executable names, not arbitrary shell text containing those words.
  heavy=[(pid,r) for pid,r in table().items() if Path(r['command'].split()[0]).name.startswith('qemu-system') or (Path(r['command'].split()[0]).name in ('zig','clang','clang++') and (' -c ' in r['command'] or ' cc ' in r['command'] or ' c++ ' in r['command']))]
  assert not heavy,heavy
  name=stage['name'];run=Path(stage['prepared']);item={'name':name,'prepared':str(run),'result':'started','work_budget_seconds':budgets[name]};record['stages'].append(item);begin=time.monotonic()
  argv=['python3','tools/signal_public_guest.py','--run',str(run)];item['argv']=argv
  with (run/'supervised-qualification.log').open('xb') as log:
   proc=subprocess.Popen(argv,cwd=P,env=guard.environment(),stdout=log,stderr=subprocess.STDOUT,start_new_session=True);item['owned_harness_pid']=proc.pid
   try:
    while proc.poll() is None:
     check()
     if time.monotonic()-begin>=budgets[name]:raise RuntimeError('perguest workdeadline '+name)
     time.sleep(.15)
   finally:clean(proc,item);item['seconds']=time.monotonic()-begin
  actual=json.loads((run/'run-result.json').read_text());item['actual']=actual
  assert actual.get('guest_stopped') is True,'owned QEMU not stopped'
  if proc.returncode or actual.get('result')!='passed':raise RuntimeError('guestfailed '+name)
  item['result']='passed';(O/'progress.json').write_text(json.dumps(record,indent=2)+'\n')
 record['result']='passed'
except BaseException as e:record['error']=repr(e)
finally:
 seen={s['name'] for s in record['stages']}
 for stage in json.loads((Q/'packet.json').read_text())['prepared']:
  if stage['name'] not in seen:record['stages'].append({'name':stage['name'],'result':'NOT RUN'})
 for item in record['stages']:
  if item['result']=='started':
   item['result']='failed';r=Path(item['prepared'])/'run-result.json'
   if r.exists():
    actual=json.loads(r.read_text());item['actual']=actual
    prep=json.loads((Path(item['prepared'])/'prepare-receipt.json').read_text());done={x['name'] for x in actual.get('cases',[])};item['later_cases_not_run']=[c['name'] for c in prep.get('suite',{}).get('cases',[]) if c['name'] not in done]
 record['seconds']=time.monotonic()-start;record['storage_minimum']=guard.minimum;(O/'supervisor.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({'result':record['result'],'error':record.get('error'),'seconds':record['seconds'],'stages':[(s['name'],s['result']) for s in record['stages']]},indent=2),flush=True)
sys.exit(record['result']!='passed')
