"""Categorical stack DHCP observations; never RF/transport/ownership acceptance."""
import re
import http_dhcp_log as dhcp
from wifi_held_log import require

PREFIX='wifi-stack5-dhcp'
WORKLOAD_KEYS=('version complete phase reason dhcp retries dns requested started matched exact body_bytes '
 'icmp_bytes payload_bytes setup_tx data_tx arp polls services elapsed_us private_erased')

def workload_row(row):
 pattern='wifi-stack5-workload: '+' '.join(k+r'=(0|[1-9][0-9]*)' for k in WORKLOAD_KEYS.split())
 m=re.fullmatch(pattern,row);require(m is not None,'wrong stack DHCP companion workload')
 w=dict(zip(WORKLOAD_KEYS.split(),map(int,m.groups())))
 require(all(v<2**64 for v in w.values()),'stack DHCP workload overflow')
 return w

def bind(d,w):
 require(w['dhcp'] in (0,1) and w['retries'] in (0,1),'invalid stack DHCP workload flags')
 require(d['success']==w['dhcp'] and d['polls']<=w['polls'] and d['elapsed_us']<=w['elapsed_us'],
  'DHCP observation contradicts workload outcome/budget')
 if w['retries']:require(d['discovers']==d['discover_queued']==2,'DHCP retry lacks actual queued discovers')
 if d['success']:require(d['discovers']==d['discover_queued']==1+w['retries'],'DHCP discover/retry accounting differs')
 return d

def optional(rows,raw,w):
 """Consume the exact next row without projecting or rewriting parent logs."""
 pos=raw['position']()
 if pos>=len(rows) or not rows[pos].startswith(PREFIX):return None
 d=dhcp.check_row(rows[pos],prefix=PREFIX,require_success=True)
 require(raw['take'](PREFIX,dhcp.KEYS)==d,'DHCP observation numeric mismatch')
 return bind(d,w)

def observe(log):
 """Retain failed/partial categorical outcomes before success-only parsers."""
 rows=[];generation=None;seen=set();lines=log.splitlines()
 for i,line in enumerate(lines):
  if line.startswith('wifi-reconnect5-begin:'):
   m=re.fullmatch(r'wifi-reconnect5-begin: version=1 generation=([12]) adapter_parent_held=1',line)
   require(m is not None,'invalid DHCP generation bracket');generation=int(m[1])
  if not line.startswith(PREFIX):continue
  require(generation not in seen,'duplicate stack DHCP observation');seen.add(generation)
  require(i>0 and lines[i-1].startswith('wifi-stack5-workload:'),'missing/late DHCP companion workload')
  d=dhcp.check_row(line,prefix=PREFIX);w=workload_row(lines[i-1]);bind(d,w)
  rows.append(dict(generation=generation,fields=d,workload=w))
 return dict(rows=rows,native_accepted=False,full_protocol_acceptance=False,
  scope='Categorical DHCP delivery/validation counters only; complete native RF/key/transport/retirement proof remains required')
