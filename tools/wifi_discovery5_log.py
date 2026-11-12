"""Strict fixed161 private Demers5 candidate selection;not authentication."""
from wifi_cal5_log import check as parent,fields
from wifi_channel_vco5_log import legacy_receipt,HOOK
from wifi_trim5_log import plan as trim_plan
from wifi_held_log import require

def check(log):
 require('srvros.wifi-discovery5=1' in log,'missing fixed161 discovery profile')
 if 'srvros.wifi-cal5-prime=1' in log:
  from wifi_prime5_log import check as primed_parent
  r=primed_parent(log)
 else:r=parent(log)
 if 'srvros.wifi-survey5=1' in log:
  from wifi_survey5_log import check as survey_check
  survey=survey_check(log);last=survey['windows'][-1]
  r.update(survey5=survey,operations=r['operations']+last['end']-r['cal5_operations'],
           cal5_completions=last['completions'],cal5_beacons=last['beacons'],
           cal5_cleanup_posts=last['stop'].get('cleanup',0))
  if last.get('slot'):r['cal5_cleanup_slot']=last['slot']
 lines=log.splitlines();rows=[x for x in lines if x.startswith('wifi-discovery5')]
 require(len(rows)==1,'missing/duplicate discovery summary')
 d=fields(rows[0],'wifi-discovery5')
 require(set(d)==set('complete inputs parsed rows matches status selected channel rsn_bytes generation retained network_out authenticated'.split()),'wrong discovery fields')
 require(d['complete']==d['matches']==d['selected']==d['generation']==d['retained']==1 and d['status']==0 and d['channel']==161 and d['network_out']==d['authenticated']==0,'target not uniquely selected;not authenticated')
 require(20<=d['rsn_bytes']<=257 and 1<=d['rows']<=d['parsed']<=d['inputs']<=8 and d['inputs']==r['cal5_completions'] and d['parsed']==r['cal5_beacons'],'inconsistent discovery/receive evidence')
 cal=[x for x in lines if x.startswith('wifi-cal5:')];require(len(cal)==1,'missing calibration completion')
 after=legacy_receipt(HOOK,trim_plan(r['gain5_lna'])[HOOK]);require(lines.index(cal[0])<lines.index(rows[0])<lines.index(after),'selection outside stopped selected checkpoint')
 r.update(discovery5_complete=True,demers5_selected=True,discovery5_rows=d['rows'],discovery5_rsn_bytes=d['rsn_bytes'],authenticated=False,network_out=0,scope='fixed161 private explicit Demers5/PSK-CCMP candidate selection after stopped retained receive;no authentication,key installation,IP or data authority')
 return r
