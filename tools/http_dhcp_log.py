"""Bounded DHCP observation schema, not transport or native acceptance.

RX stage/refusal counts can include unrelated traffic. last_message/last_options
refer to the last parsed DHCP message, not necessarily the last early refusal.
The existing full HTTP/RF/USB/retirement proof remains independently required.
"""
import re
from wifi_held_log import require

PREFIX='wifi-http5-dhcp'
KEYS=('version started finished success end_reason discovers discover_queued requests request_queued '
 'frames ipv4 udp dhcp offers acks naks accepted_offer accepted_ack rejected first_reject last_reject '
 'reject_mask last_message last_options have_offer request_sent configured polls elapsed_us elapsed_ticks')
REASONS=('none owner ethernet ip_header ip_destination ip_checksum ip_fragment udp_length udp_checksum '
 'udp_ttl bootp xid client source option_bounds option_end option_duplicate option_shape address '
 'offer_state ack_state ack_binding ack_config message').split()
ENDS=('not_run success guard_failed time_limit poll_limit send_failed request_failed busy retry_failed').split()

def check(log,*,require_success=False):
 rows=[s for s in log.splitlines() if s.startswith(PREFIX)]
 require(len(rows)==1,'missing/duplicate DHCP observation')
 return check_row(rows[0],require_success=require_success)

def check_row(row,*,prefix=PREFIX,require_success=False):
 require(prefix in (PREFIX,'wifi-stack5-dhcp'),'unknown DHCP observation prefix')
 pattern=re.escape(prefix)+': '+' '.join(k+r'=(0|[1-9][0-9]*)' for k in KEYS.split())
 m=re.fullmatch(pattern,row);require(m is not None,'wrong DHCP observation schema/order')
 d=dict(zip(KEYS.split(),map(int,m.groups())))
 require(all(v<2**64 for v in d.values()),'DHCP observation integer overflow')
 if d['version']==0:
  require(not any(d.values()) and not require_success,'DHCP was not observed')
  return d
 require(d['version']==1 and all(d[k] in (0,1) for k in ('started','finished','success','have_offer','request_sent','configured')),
  'invalid DHCP observation version/flags')
 require(d['finished']==1 and 0<d['end_reason']<len(ENDS),'unfinished DHCP observation')
 require(0<=d['discover_queued']<=d['discovers']<=2 and 0<=d['request_queued']<=d['requests']<=1 and
  d['requests']==d['accepted_offer']<=1 and d['request_sent']==d['request_queued'] and
  d['have_offer']==d['accepted_offer'] and d['accepted_ack']<=d['acks'] and d['accepted_offer']<=d['offers'],
  'inconsistent DHCP send/admission observation')
 require(0<=d['offers']+d['acks']+d['naks']<=d['dhcp']<=d['udp']<=d['ipv4']<=d['frames']<=4800000 and
  d['accepted_ack']<=1 and d['polls']<=20000 and d['elapsed_us']<=15000000 and d['elapsed_ticks']<=1500,
  'invalid DHCP observation accounting/budget')
 require(0<=d['last_message']<=255 and 0<=d['last_options']<=63 and d['rejected']<=d['frames'] and
  0<=d['first_reject']<len(REASONS) and 0<=d['last_reject']<len(REASONS) and
  d['reject_mask']<1<<len(REASONS) and not d['reject_mask']&1,'invalid DHCP refusal observation')
 if d['rejected']:
  require(d['first_reject']>0 and d['last_reject']>0 and d['reject_mask']&(1<<d['first_reject']) and
   d['reject_mask']&(1<<d['last_reject']) and d['reject_mask'].bit_count()<=d['rejected'],'missing DHCP refusal identity')
 else:require(d['first_reject']==d['last_reject']==d['reject_mask']==0,'phantom DHCP refusal')
 require((d['end_reason']==1)==bool(d['success']) and (not d['success'] or
  (d['started'] and d['configured'] and d['accepted_offer']==d['accepted_ack']==d['requests']==d['request_queued']==1 and
   d['discovers']==d['discover_queued']>=1 and d['polls']>0 and d['elapsed_us']<2000000 and d['elapsed_ticks']<200)),
  'invalid DHCP success observation')
 require(not require_success or d['success']==1,'DHCP observation does not prove successful DORA')
 return d
