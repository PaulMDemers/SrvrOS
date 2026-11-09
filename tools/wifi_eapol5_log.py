"""Bounded native M1 structural capture plus complete historical owner proof.
No EAPOL reply,authentication,key installation or controlled-port authority.
"""
from wifi_join5_log import check as parent
from wifi_power5_log import fields
from wifi_held_log import require
PROGRESS='wifi-eapol5-progress: bounded post-association M1 receive;no keys or EAPOL TX;leave and full retirement follow'
def check(log,*,stream=False):
 require(("srvros.wifi-stream5=1" in log)==stream,"wrong stream/legacy M1 profile")
 require(stream or not any(x.startswith("wifi-stream5") for x in log.splitlines()),"unexpected stream receipt in legacy M1 proof")
 require('srvros.wifi-eapol5=1' in log,'missing M1 profile');r=parent(log);lines=log.splitlines()
 rows=[x for x in lines if x.startswith('wifi-eapol5')];rx=[x for x in rows if x.startswith('wifi-eapol5-rx:')];summaries=[x for x in rows if x.startswith('wifi-eapol5:')]
 require(rows and rows[0]==PROGRESS and len(summaries)==1 and rows[-1]==summaries[0] and len(rows)==len(rx)+2,'missing/duplicate/unknown M1 receipts')
 s=fields(summaries[0],'wifi-eapol5');require(set(s)=={'complete','polls','frames','matched','clock_valid','elapsed_us','eapol_tx','keys','authenticated'},'wrong M1 summary schema')
 require(s['complete']==s['matched']==s['clock_valid']==1 and 1<=s['frames']<=46 and s['frames']==len(rx) and s['frames']<=s['polls']<=20000 and 0<=s['elapsed_us']<1700000 and s['eapol_tx']==s['keys']==s['authenticated']==0,'incomplete or unauthorized M1 capture')
 old=[fields(x,'wifi-join5-rx') for x in lines if x.startswith('wifi-join5-rx:')];start=len(old);matches=0
 for i,line in enumerate(rx):
  a=fields(line,'wifi-eapol5-rx');require(set(a)=={'index','bytes','view','rxinfo','mpdu','fc','eapol','m1'} and a['index']==start+i and 0<=a['index']<48 and 0<=a['bytes']<=4096 and 0<=a['view']<=3 and 0<=a['rxinfo']<=0xffffffff and a['m1'] in (0,1),'wrong M1 RX schema/index')
  if a['view']:require(a['mpdu']==a['fc']==a['eapol']==a['m1']==0,'invalid M1 view leaked fields')
  else:
   base=a['fc']&~0x2800;require(base in (0x0208,0x0288) and 99<=a['eapol']<=1123 and a['mpdu']==(26 if base==0x0288 else 24)+8+a['eapol'] and a['bytes']>=36+a['mpdu'],'invalid EAPOL dimensions')
  if a['m1']:require(a['view']==0 and a['eapol'] in (99,121),'invalid structural M1');matches+=a['m1']
 require(matches==1 and fields(rx[-1],'wifi-eapol5-rx')['m1']==1,'missing/extra matched M1')
 require(start+len(rx)<=r['join5_posts'],'M1 beyond retained RX posts')
 assoc=next(x for x in lines if x.startswith('wifi-join5-rx:') and fields(x,'wifi-join5-rx')['fc']&0xfc==0x10 and fields(x,'wifi-join5-rx')['parser']==0)
 leave=next(x for x in lines if x.startswith('wifi-join5-send: kind=2 '))
 require(lines.index(assoc)<lines.index(PROGRESS)<lines.index(rx[0])<=lines.index(rx[-1])<lines.index(summaries[0])<lines.index(leave),'M1 outside associated owned lifetime')
 r.update(eapol5_complete=True,eapol5_m1_shape=True,eapol5_frames=s['frames'],eapol5_polls=s['polls'],eapol5_elapsed_us=s['elapsed_us'],scope='matched open management plus private structurally valid M1 capture and full owner/parent restoration;no EAPOL reply,keys or authenticated data')
 return r
