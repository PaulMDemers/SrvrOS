"""Explicit v132 nonblocking TX handoff;does not accept ordinary provider RX."""
from wifi_wpa5_log import check_data
from wifi_held_log import require
from wifi_power5_log import fields
PROGRESS='wifi-provider5-progress: deferred registry selected;original active CCMP owner;nonblocking USB submit/completion;six strict IP requests;no ring reuse'
BOOT='wifi-provider5-boot: net_init_deferred=1 worker_deferred=1'
def check(log,reuse=False):
 r=check_data(log,provider=True,reuse=reuse);lines=log.splitlines()
 allrows=[x for x in lines if x.startswith('wifi-provider5')]
 boot=[x for x in allrows if x.startswith('wifi-provider5-boot:')]
 require(len(boot)<=1 and all(x==BOOT for x in boot),'invalid deferred network boot receipt')
 rows=[x for x in allrows if x not in boot]
 require(len(rows)==2 and rows[0]==(PROGRESS.replace("no ring reuse","bounded reusable pages;no descriptor wrap") if reuse else PROGRESS) and rows[1].startswith('wifi-provider5:'),'missing native provider lifecycle')
 require(not boot or lines.index(boot[0])<lines.index(rows[0]),'late deferred network boot receipt')
 expected=dict(complete=1,registered=1,attached=1,selected=1,queued=6,submitted=6,completed=6,closed=1,early_release_refused=1,outer_retired=1,detached=1,registry_retired=1,pending=0,faults=0,dropped=0,retained=1,ring_reuse=int(reuse),ordinary_rx=0)
 require(fields(rows[1],'wifi-provider5')==expected,'incomplete/unsafe provider lifecycle')
 m4=next(x for x in lines if x.startswith('wifi-wpa5-send: kind=3 '))
 ip=next(x for x in lines if x.startswith('wifi-data5-progress:'))
 require(lines.index(m4)<lines.index(rows[0])<lines.index(ip),'provider not attached inside original activated owner')
 rf=next(x for x in lines if x.startswith('wifi-channel-vco5:'))
 firmware=next(x for x in lines if x.startswith('wifi-firmware-capture:'))
 require(lines.index(rf)<lines.index(rows[1])<lines.index(firmware),'provider released before full outer retirement')
 r.update(provider5_complete=True,provider5_encrypted_tx=6,provider5_early_release_refused=True,provider5_ordinary_rx=False)
 r['scope']='bounded nonblocking native provider TX handoff,six strict encrypted IP requests and full retirement;private diagnostic RX;ordinary provider RX/persistence/TCP/HTTP unaccepted'
 return r
