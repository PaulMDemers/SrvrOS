"""Explicit bounded encrypted-data profile;never accepts a handshake-only run."""
from wifi_power5_log import fields
from wifi_held_log import require
PROGRESS='wifi-data5-progress: authenticated encrypted DHCP,ARP,DNS example.com and256-byte gateway echo;bounded waits;then erase and leave'
def validate_data(lines,handshake_sends,old_rx,owner,result):
 rows=[x for x in lines if x.startswith('wifi-data5')]
 require(len(rows)>=14 and rows[0]==PROGRESS,'missing encrypted-data progress')
 require(lines.index(handshake_sends[3])<lines.index(rows[0])<lines.index(rows[-1])<lines.index(handshake_sends[4]),'data outside activated M4/leave window')
 sends=[x for x in rows if x.startswith('wifi-data5-send:')]
 receives=[x for x in rows if x.startswith('wifi-data5-rx:')]
 summaries=[x for x in rows if x.startswith('wifi-data5:')]
 require(len(sends)==6 and len(summaries)==1 and rows[-1]==summaries[0],'missing encrypted data sends/summary')
 arp=[x for x in rows if x.startswith('wifi-data5-arp:')]
 require(len(arp)==1 and rows[-2]==arp[0],'missing ARP classification')
 a=fields(arp[0],'wifi-data5-arp');require(set(a)=={'short','other','format','opcode','destination','sender','target','matched','request_us'} and all(0<=x<=48 for x in a.values()) and a['matched']==2 and a['request_us']<=a['opcode'] and sum(v for k,v in a.items() if k!='request_us')<=48,'invalid ARP classification')
 require(len(rows)==1+len(sends)+len(receives)+2,'unexpected encrypted data receipt')
 for i,(line,n) in enumerate(zip(sends,(404,404,104,136,104,360))):
  require(fields(line,'wifi-data5-send')==dict(index=i,bytes=n,completed=1,pn=i+1),'ciphertext send/PN gap,replay or uncertainty')
 v=fields(summaries[0],'wifi-data5')
 fixed=dict(complete=1,unsafe=0,phase=5,exhausted=0,discover=1,offer=1,request=1,ack=1,arp_dns=1,dns=1,arp_router=1,echo=1,sends=6,tx_pn=6,echo_bytes=256)
 require(set(v)==set(fixed)|{'polls','ignored','received'} and all(v[k]==n for k,n in fixed.items()),'incomplete authenticated IP lifecycle')
 require(6<=v['polls']<=120000 and 0<=v['ignored']<=v['polls'] and 6<=v['received']<=48,'invalid data receive counts')
 require(6<=len(receives)<=48-len(old_rx) and len(old_rx)+len(receives)<=owner['posts'],'missing retained authenticated RX')
 valid=[]
 for i,line in enumerate(receives):
  a=fields(line,'wifi-data5-rx')
  require(set(a)=={'index','bytes','ethernet','present','view','reason'} and a['index']==len(old_rx)+i and 0<=a['bytes']<=4096 and a['present'] in (0,1) and 0<=a['view']<=3 and 1<=a['reason']<=9,'wrong data RX schema/order')
  require(lines.index(sends[0])<lines.index(line)<lines.index(summaries[0]),'data receive outside window')
  if a['view']==0:
   require(a['reason']==9 and a['present']==1 and 14<=a['ethernet']<=1514 and a['bytes']>=a['ethernet']+70,'invalid protected Ethernet dimensions')
   valid.append(line)
  else:require(a['reason']!=9 and a['present']==a['ethernet']==0,'invalid data view leaked plaintext')
 require(len(valid)==v['received'],'authenticated data receive count differs')
 for i,line in enumerate(sends):
  end=sends[i+1] if i<5 else summaries[0]
  require(any(lines.index(line)<lines.index(rx)<lines.index(end) for rx in valid),'missing authenticated response between diagnostic sends')
 require(lines.index(old_rx[-1])<lines.index(sends[0]),'handshake receive after controlled port opened')
 retire=[x for x in lines if x.startswith('wifi-retire5')]
 require(retire and retire[-1].startswith('wifi-retire5:') and len(retire)<=49,'missing bounded receive retirement')
 b=fields(retire[-1],'wifi-retire5')
 require(set(b)=={'limit','capacity','forward','drained','posts'} and b['limit']==40 and b['capacity']==48 and b['forward']==len(old_rx)+len(receives) and b['forward']<=40 and 0<=b['drained']<=48-b['forward'] and b['posts']==owner['posts'] and b['posts']-b['forward']-b['drained'] in (0,1),'invalid forward/cleanup budget')
 require(len(retire)==1+b['drained'],'missing cleanup completion')
 mac=next(x for x in lines if x.startswith('wifi-wpa5-mac:'));exchange=next(x for x in lines if x.startswith('wifi-wpa5-exchange:'))
 require(lines.index(exchange)<lines.index(retire[-1])<lines.index(mac),'retirement outside disabled MAC cleanup')
 for i,line in enumerate(retire[:-1]):
  a=fields(line,'wifi-retire5-rx');require(set(a)=={'index','bytes'} and a['index']==b['forward']+i and 0<=a['bytes']<=4096 and lines.index(exchange)<lines.index(line)<lines.index(retire[-1]),'wrong retained cleanup completion')
 result.update(data5_complete=True,data5_dhcp=True,data5_dns=True,data5_echo=True,data5_echo_bytes=256,data5_encrypted_sends=6,data5_authenticated_received=v['received'],data5_tx_pn=6)
def check(log):
 from wifi_wpa5_log import check_data
 return check_data(log)
