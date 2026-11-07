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
 require(len(rows)==1+len(sends)+len(receives)+1,'unexpected encrypted data receipt')
 for i,(line,n) in enumerate(zip(sends,(404,404,104,136,104,360))):
  require(fields(line,'wifi-data5-send')==dict(index=i,bytes=n,completed=1,pn=i+1),'ciphertext send/PN gap,replay or uncertainty')
 v=fields(summaries[0],'wifi-data5')
 fixed=dict(complete=1,unsafe=0,phase=5,discover=1,offer=1,request=1,ack=1,arp_dns=1,dns=1,arp_router=1,echo=1,sends=6,tx_pn=6,echo_bytes=256)
 require(set(v)==set(fixed)|{'polls','ignored','received'} and all(v[k]==n for k,n in fixed.items()),'incomplete authenticated IP lifecycle')
 require(6<=v['polls']<=120000 and 0<=v['ignored']<=v['polls'] and 6<=v['received']<=48,'invalid data receive counts')
 require(6<=len(receives)<=48-len(old_rx) and len(old_rx)+len(receives)<=owner['posts'],'missing retained authenticated RX')
 valid=[]
 for i,line in enumerate(receives):
  a=fields(line,'wifi-data5-rx')
  require(set(a)=={'index','bytes','ethernet','present','view'} and a['index']==len(old_rx)+i and 0<=a['bytes']<=4096 and a['present'] in (0,1) and 0<=a['view']<=3,'wrong data RX schema/order')
  require(lines.index(sends[0])<lines.index(line)<lines.index(summaries[0]),'data receive outside window')
  if a['view']==0:
   require(a['present']==1 and 14<=a['ethernet']<=1514 and a['bytes']>=a['ethernet']+70,'invalid protected Ethernet dimensions')
   valid.append(line)
  else:require(a['present']==a['ethernet']==0,'invalid data view leaked plaintext')
 require(len(valid)==v['received'],'authenticated data receive count differs')
 for i,line in enumerate(sends):
  end=sends[i+1] if i<5 else summaries[0]
  require(any(lines.index(line)<lines.index(rx)<lines.index(end) for rx in valid),'missing authenticated response between diagnostic sends')
 require(lines.index(old_rx[-1])<lines.index(sends[0]),'handshake receive after controlled port opened')
 result.update(data5_complete=True,data5_dhcp=True,data5_dns=True,data5_echo=True,data5_echo_bytes=256,data5_encrypted_sends=6,data5_authenticated_received=v['received'],data5_tx_pn=6)
def check(log):
 from wifi_wpa5_log import check_data
 return check_data(log)
