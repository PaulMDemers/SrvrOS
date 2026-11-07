"""Pinned HDA non-posted status and display surfaces, without DMA/memory grants."""
from intel_engine_census import _fields
from intel_hda_lease import lease_result,native_lease_result,verify_lease_checkpoint
C=[f'c{i}' for i in range(6)]
W=[f'w{i}' for i in range(13)]
FIELDS='v samples configs planes reads reason primary cursor sprite mismatch stable nonposted complete begin_us end_us maps retired retained cache_checks display_writes posted_drain prefetch_retired reader_retirement managed_pages'.split()
ZERO='display_writes posted_drain prefetch_retired reader_retirement managed_pages'.split()
def summary(line):
 s=_fields(line,'intel-pinned-summary:',FIELDS)
 if any(s[k] for k in ZERO) or any(s[k]>1 for k in ('stable','nonposted','complete','retained')) or s['samples']>2 or s['configs']>2 or s['planes']>6 or s['reads']>90 or s['reason']>8 or s['maps']>6 or s['retired']>s['maps'] or s['cache_checks']>12 or any(s[k]>7 for k in ('primary','cursor','sprite','mismatch')):raise ValueError('pinned counts/unsupported authority')
 if s['complete'] and not (s['samples']==s['configs']==2 and s['planes']==s['maps']==s['retired']==6 and s['reads']==90 and s['cache_checks']==12 and s['stable']==s['nonposted']==1 and s['retained']==s['reason']==0 and 0<s['begin_us']<s['end_us']<=s['begin_us']+150000):raise ValueError('pinned completion')
 return s
def reader_result(text):
 configs=[];planes=[];sums=[];order=[]
 for l in text.splitlines():
  if l.startswith('intel-pinned-config:'):
   c=_fields(l,'intel-pinned-config:',['v','sample']+C);configs.append(c);order.append(('c',c['sample']))
  elif l.startswith('intel-pinned-plane:'):
   w=_fields(l,'intel-pinned-plane:',['v','sample','pipe']+W);planes.append(w);order.append(('p',w['sample'],w['pipe']))
  elif l.startswith('intel-pinned-summary:'):sums.append(summary(l));order.append(('s',))
  elif l.startswith('intel-pinned-'):raise ValueError('unknown pinned receipt')
 if not sums:
  if configs or planes:raise ValueError('missing pinned summary')
  return None
 if len(sums)!=1:raise ValueError('duplicate pinned summary')
 s=sums[0]
 if [c['sample'] for c in configs]!=list(range(s['configs'])) or [(w['sample'],w['pipe']) for w in planes]!=[(i//3,i%3) for i in range(s['planes'])]:raise ValueError('pinned coverage')
 expected=[]
 for n in range(s['configs']):expected += [('c',n)]+[('p',n,p) for p in range(3) if n*3+p<s['planes']]
 if order!=expected+[('s',)]:raise ValueError('pinned order')
 if any(c[k]>0xffffffff for c in configs for k in C) or any(w[k]>0xffffffff for w in planes for k in W):raise ValueError('pinned width')
 lease=lease_result(text)
 complete=bool(s['complete'] and lease and lease['complete'])
 if s['complete']:
  for c in configs:
   if not(c['c0']&255==0x50 and c['c1']&65535==0x6001 and c['c2']&65535==0x7005 and c['c3']==0x00910010 and c['c4']==0x10000fc0 and c['c5']&~0x0010ffff==0 and c['c5']&0x8000==0):raise ValueError('pinned PCIe chain/TXP')
  if any(configs[0][k]!=configs[1][k] for k in C) or any(planes[p][k]!=planes[p+3][k] or planes[p][k]==0xffffffff for p in range(3) for k in W):raise ValueError('pinned instability')
  masks=[sum(1<<p for p in range(3) if planes[p][k]&mask) for k,mask in [('w0',1<<31),('w5',63),('w8',1<<31)]]
  mismatch=sum(1<<p for p in range(3) if any((planes[p][f'w{i}']^planes[p][f'w{j}'])&0xfffff000 for i,j in [(1,2),(3,4),(6,7),(9,10),(11,12)]))
  if masks+[mismatch]!=[s[k] for k in ('primary','cursor','sprite','mismatch')]:raise ValueError('pinned derived readers')
  if lease and lease['complete']:
   ph=lease['phases']
   if not(ph[1]['end_us']<s['begin_us']<ph[2]['begin_us']<ph[2]['end_us']<s['end_us']<ph[3]['begin_us']):raise ValueError('pinned HDA lifetime ordering')
 return dict(complete=complete,native_trial_complete=False,nonposted_clear=bool(s['nonposted']),posted_drain=False,prefetch_retired=False,reader_retirement=False,managed_range=False,physical_tearfree=False,summary=s,configs=configs,planes=planes,lease=lease)
def native_reader_result(text):
 r=reader_result(text)
 if not r or not r['complete']:return r
 lease=native_lease_result(text)
 if not lease or not lease['native_trial_complete']:raise ValueError('pinned native lease')
 from intel_display_capture import capture_result
 capture=capture_result(text)
 if not capture or capture['rows']['identity'][1]['bar0']!=0xc0000004:raise ValueError('pinned display identity')
 for p in range(3):
  original=capture['rows']['pipe'][3+p];row=r['planes'][p]
  if any(row[f'w{i}']!=original[k] for i,k in [(0,'control'),(1,'surface'),(2,'live'),(5,'cursor_control'),(6,'cursor_base')]):raise ValueError('pinned inherited display changed')
 from intel_cache import native_cache_result,_snapshot,_page
 cache=native_cache_result(text)
 for n,s in enumerate(cache['rows']['sample']):
  regions=_snapshot(s,[x for x in cache['rows']['variable'] if x['sample']==n],[x for x in cache['rows']['fixed'] if x['sample']==n])
  for off in (0x70000,0x71000,0x72000):
   d=_page(s,regions,0xc0000000+off,2)
   if d is None or d[0]!=0 or d[2]!=0:raise ValueError('pinned independent plane UC')
 r['native_trial_complete']=True;return r
def verify_reader_checkpoint(pre,text):
 if any(l.startswith('intel-pinned-') for l in pre.splitlines()):raise ValueError('late pinned checkpoint')
 return verify_lease_checkpoint(pre,text)
