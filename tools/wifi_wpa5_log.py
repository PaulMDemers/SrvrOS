"""Bounded software-key four-way child;all historical M1/owner proof required.
Local M4 completion is not AP acknowledgement or encrypted data acceptance.
"""
from wifi_eapol5_log import check as parent
from wifi_power5_log import fields
from wifi_held_log import require
PROGRESS='wifi-wpa5-progress: frozen parent;independent fresh RX/OUT;strict fresh nonce and bounded four-way;software key erase/leave;MAC disable and full restore;no IP data'
def _check(log,data=False,provider=False,reuse=False):
 require(not reuse or (data and provider),"reusable pages need data/provider parents")
 require(("srvros.wifi-reuse5=1" in log)==reuse,"wrong reusable-page profile")
 require(reuse or not any(x.startswith("wifi-reuse5") for x in log.splitlines()),"reuse receipts in historical profile")
 require(("srvros.wifi-provider5=1" in log)==provider,"wrong provider handoff profile")
 require(provider or not any(x.startswith("wifi-provider5") for x in log.splitlines()),"provider receipts in historical profile")
 require(("srvros.wifi-data5=1" in log)==data,"wrong encrypted data profile")
 require(data or not any(x.startswith(('wifi-data5','wifi-retire5')) for x in log.splitlines()),'data receipts in handshake-only run')
 progress=PROGRESS.replace('bounded four-way;','bounded four-way and authenticated IP window;').replace(';no IP data','') if data else PROGRESS
 publications=11 if data else 5;packet_limit=1580 if data else 184
 require('srvros.wifi-wpa5=1' in log,'missing four-way profile');r=parent(log);lines=log.splitlines()
 aux=[x for x in lines if x.startswith(('wifi-wpa5-rx:','wifi-wpa5-exchange:'))]
 rows=[x for x in lines if x.startswith('wifi-wpa5') and x not in aux];i=0
 def take(prefix):
  nonlocal i
  require(i<len(rows),'missing four-way receipt');line=rows[i];i+=1;return fields(line,prefix)
 require(rows and rows[0]==progress,'missing four-way progress');i+=1
 p=take('wifi-wpa5-plan');require(set(p)=={'rx','tx','old','slot','max_rx'} and p['max_rx']==48 and 0<p['slot']<256,'wrong four-way plan')
 rx,tx,slot=p['rx'],p['tx'],p['slot']
 require(all(0<x<=2**64-4096 and x%4096==0 for x in (rx,tx)) and rx!=tx,'invalid four-way rings')
 oldplan=fields(next(x for x in lines if x.startswith('wifi-join5-plan:')),'wifi-join5-plan')
 require(slot==oldplan['slot'] and p['old']==oldplan['old'],'four-way parent mismatch')
 excluded=[oldplan['rx'],oldplan['tx']]
 for prefix,keys in [('wifi-handoff5:',('old_ring','new_ring')),('wifi-tx5:',('ring',))]:
  v=fields(next(x for x in lines if x.startswith(prefix)),prefix[:-1]);excluded.extend(v[k] for k in keys)
 require(rx not in excluded and tx not in excluded,'four-way aliases prior rings')
 def old_context(dci):
  v=fields(next(x for x in lines if x.startswith(f'wifi-join5-context: phase=8 dci={dci} ')),'wifi-join5-context');return [v['w'+str(j)] for j in range(8)]
 oldrx,oldtx=old_context(9),old_context(8)
 def context(phase,dci):
  a=take('wifi-wpa5-context');require(set(a)=={'phase','dci'}|{'w'+str(j) for j in range(8)} and a['phase']==phase and a['dci']==dci,'wrong four-way context order/schema')
  v=[a['w'+str(j)] for j in range(8)];require(all(0<=x<2**32 for x in v),'four-way context overflow');return v
 def contexts(phase):
  a=context(phase,8);b=context(phase,9)
  if phase==0:require(a==oldtx,'four-way initial disabled OUT differs')
  elif phase==8:require(a[0]&7==0,'four-way OUT not disabled')
  else:
   require(a[0]==(1 if phase<6 else 3) and a[1]==(3<<1)|(2<<3)|(512<<16) and a[4:]==[packet_limit,0,0,0],'wrong four-way OUT context')
   dq=a[2]|a[3]<<32;require(dq&15==1 and tx<=dq&~15<=tx+(0 if phase in (1,2) else 16*publications),'four-way OUT dequeue outside five publications')
   require(phase<6 or dq==tx+16*publications+1,'four-way stopped OUT dequeue differs')
  if phase in (0,1,7,8):require(b==oldrx,'four-way old RX not exact')
  else:
   require(b[0]==oldrx[0] and b[1]==oldrx[1] and b[4:]==oldrx[4:],'four-way RX not identically stopped')
   dq=b[2]|b[3]<<32;require(dq&15==1 and rx<=dq&~15<=rx+16*48,'four-way RX dequeue outside retained pages')
 def command(phase,kind,dci):require(take('wifi-wpa5-command')==dict(phase=phase,type=kind,dci=dci,slot=slot,ok=1),'wrong four-way command')
 contexts(0);require(take('wifi-wpa5-toggle')==dict(type=2,request=1,value=0,index=4,length=0,actual=0,ok=1),'missing/invalid device-side toggle reset');command(0,12,8);contexts(1);command(1,16,9);contexts(2)
 for kind,length in enumerate((60,100,184,160,56)):require(take('wifi-wpa5-send')==dict(kind=kind,bytes=length,completed=1,replay=0),'missing/replayed four-way publication')
 m=take('wifi-wpa5-mac');fixed=dict(complete=1,started=1,snapshot=1,disabled=1,tx_drained=1,rx_drained=1,filter_restored=1,txop_restored=1,filter=0x17f97,ibi=0xffffffff)
 require(set(m)==set(fixed)|{'operations','reads','writes','waits','alc','txop','hold_us'} and all(m[k]==v for k,v in fixed.items()),'incomplete four-way MAC lifecycle')
 oldmac=fields(next(x for x in lines if x.startswith('wifi-join5-mac:')),'wifi-join5-mac')
 require(m['alc']==oldmac['alc'] and m['txop']==oldmac['txop'] and m['operations']==m['reads']+m['writes']+m['waits'] and 0<m['operations']<20000 and m['hold_us']>=10000,'wrong four-way selected power/MAC counters')
 stop=take('wifi-wpa5-stop-event')
 require(set(stop)=={'dci','cc','residue','pointer','ok'} and stop['dci']==9 and stop['ok']==1 and stop['cc'] in (26,27,28) and 0<=stop['residue']<=4096 and rx<=stop['pointer']<=rx+48*16 and (stop['pointer']-rx)%16==0 and (stop['cc']!=27 or stop['residue']==0),'wrong four-way RX stop')
 command(4,15,9);contexts(5)
 require(take('wifi-wpa5-stop-event')==dict(dci=8,cc=27,residue=0,pointer=tx+16*publications,ok=1),'wrong four-way OUT stop')
 command(5,15,8);contexts(6);command(6,16,9);contexts(7);command(7,12,8);contexts(8)
 s=take('wifi-wpa5');fixed=dict(ok=1,exchange=1,auth=1,assoc=1,m1=1,m2=1,m3_mic=1,installed=1,m4=1,activated=1,erased=1,leave=1,unsafe=0,sends=publications,status=0,commands=6,restored=1,phase=9,retained=1,hardware_keys=0,ip_data=int(data),toggle=1)
 require(set(s)==set(fixed)|{'polls','ignored','aid','posts','controls'} and all(s[k]==v for k,v in fixed.items()) and 4<=s['polls']<=80000 and 0<=s['ignored']<=s['polls'] and 1<=s['aid']<=2007 and 5<=s['posts']<=48 and s['controls']==m['reads']+m['writes'],'incomplete four-way/key retirement')
 require(i==len(rows),'extra four-way receipt')
 rxrows=[x for x in aux if x.startswith('wifi-wpa5-rx:')];exrows=[x for x in aux if x.startswith('wifi-wpa5-exchange:')]
 require(4<=len(rxrows)<=s['posts'] and len(exrows)==1,'missing four-way RX/exchange proof')
 ex=fields(exrows[0],'wifi-wpa5-exchange');require(set(ex)=={'complete','clock_valid','elapsed_us'} and ex['complete']==ex['clock_valid']==1 and 0<=ex['elapsed_us']<(24000000 if data else 12000000),'invalid four-way timing')
 sends=[x for x in rows if x.startswith('wifi-wpa5-send:')];valid=[]
 for n,line in enumerate(rxrows):
  a=fields(line,'wifi-wpa5-rx');require(set(a)=={'index','bytes','type','view','mpdu','fc','eapol','present'} and a['index']==n and 0<=a['bytes']<=4096 and a['type'] in (0,1) and 0<=a['view']<=3 and a['present'] in (0,1) and 0<=a['fc']<=65535,'wrong four-way RX shape')
  require(lines.index(sends[0])<lines.index(line)<lines.index(sends[4]),'RX outside four-way window')
  if a['view']:require(a['mpdu']==a['fc']==a['eapol']==a['present']==0,'invalid four-way view leaked fields')
  else:
   require(a['present']==1 and 24<=a['mpdu']<=4096 and a['bytes']>=36+a['mpdu'],'invalid four-way RX dimensions')
   if a['type']==1:
    base=a['fc']&~0x2800;require(base in (0x0208,0x0288) and 99<=a['eapol']<=1123 and a['mpdu']==(26 if base==0x0288 else 24)+8+a['eapol'],'wrong private EAPOL dimensions')
   else:require(a['eapol']==0,'management claims EAPOL')
   valid.append((a,line))
 def between(a,b,line):return lines.index(a)<lines.index(line)<lines.index(b)
 require(any(a['type']==0 and a['fc']&0xfc==0xb0 and between(sends[0],sends[1],line) for a,line in valid),'missing new AUTH reply')
 require(any(a['type']==0 and a['fc']&0xfc==0x10 and between(sends[1],sends[2],line) for a,line in valid),'missing new association reply')
 require(any(a['type']==1 and a['eapol'] in (99,121) and between(sends[1],sends[2],line) for a,line in valid),'missing new M1')
 require(any(a['type']==1 and a['eapol']>=155 and between(sends[2],sends[3],line) for a,line in valid),'missing new M3')
 require(lines.index(sends[4])<lines.index(exrows[0])<lines.index(next(x for x in rows if x.startswith('wifi-wpa5-mac:'))),'four-way exchange outside MAC lifetime')
 for phase in (5,6):
  a=fields(next(x for x in rows if x.startswith(f'wifi-wpa5-context: phase={phase} dci=9 ')),'wifi-wpa5-context');dq=(a['w2']|a['w3']<<32)&~15
  require(dq==stop['pointer'] and rx<=dq<=rx+16*s['posts'],'four-way RX stop differs from retained publications')
 require(stop['cc']==27 or stop['pointer']==rx+16*(s['posts']-1),'four-way pending RX stop not final TD')
 previous=next(x for x in lines if x.startswith('wifi-join5:'))
 cleanup=next(x for x in lines if x.startswith('wifi-power5-usb:') and fields(x,'wifi-power5-usb')['cleanup']==1)
 require(lines.index(previous)<lines.index(progress)<lines.index(rows[-1])<lines.index(cleanup),'four-way outside restored parent lifetime')
 if data:
  from wifi_data5_log import validate_data
  validate_data(lines,sends,rxrows,s,r)
 r.update(wpa5_complete=True,wpa5_m3_mic=True,wpa5_m4_submitted=True,wpa5_software_keys_erased=True,wpa5_originals_restored=True,wpa5_posts=s['posts'],wpa5_elapsed_us=ex['elapsed_us'],scope='matched four-way M3 MIC and local M4 submission;software key activation/erasure;leave and full owner/parent restoration;AP acknowledgement and encrypted IP/data not tested')
 return r

def check(log):return _check(log,False)
def check_data(log,provider=False,reuse=False):
 r=_check(log,True,provider,reuse)
 r["scope"]="bounded authenticated encrypted DHCP DORA,DNS and256-byte gateway echo;software key erasure/leave and full owner/parent restoration;ordinary provider/UI and reconnect not tested"
 return r
