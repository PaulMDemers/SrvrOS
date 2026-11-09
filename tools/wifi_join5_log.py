"""Fixed161 live management exchange plus complete historical parent restoration.
USB completion is not AP acknowledgement;open authentication is not WPA2.
"""
from wifi_tx5_log import check as parent
from wifi_power5_log import fields
from wifi_held_log import require
PROGRESS='wifi-join5-progress: frozen parent;fresh live RX/data OUT;matched AUTH ASSOC leave;MAC disable;stop/drop/restore;no keys or IP data'
def check(log):
 require('srvros.wifi-join5=1' in log,'missing live management profile');r=parent(log);lines=log.splitlines();aux=[s for s in lines if s.startswith(('wifi-join5-rx:','wifi-join5-exchange:','wifi-join5-response:'))];rows=[s for s in lines if s.startswith('wifi-join5') and s not in aux];i=0
 def take(prefix):
  nonlocal i
  require(i<len(rows),'missing live management receipt');line=rows[i];i+=1;return fields(line,prefix)
 require(rows and rows[i]==PROGRESS,'missing live management progress');i+=1
 p=take('wifi-join5-plan');require(set(p)=={'rx','tx','old','slot','max_rx'} and p['max_rx']==48 and 0<p['slot']<256,'wrong live plan')
 rx,tx,slot=p['rx'],p['tx'],p['slot'];require(slot==fields(next(x for x in lines if x.startswith('wifi-tx5-command:')),'wifi-tx5-command')['slot'],'live slot differs from accepted parent');require(all(0<x<=2**64-4096 and not x%4096 for x in (rx,tx)) and rx!=tx,'invalid live rings')
 h=fields(next(x for x in lines if x.startswith('wifi-handoff5:')),'wifi-handoff5');t=fields(next(x for x in lines if x.startswith('wifi-tx5:')),'wifi-tx5')
 require(all(x not in (h['old_ring'],h['new_ring'],t['ring']) for x in (rx,tx)) and p['old']==h['old_dequeue'],'aliased live ring or wrong old dequeue')
 old=fields(next(x for x in lines if x.startswith('wifi-handoff5-context: op=6 ')),'wifi-handoff5-context');old=[old['w'+str(n)] for n in range(8)]
 def context(phase,dci):
  a=take('wifi-join5-context');require(set(a)=={'phase','dci'}|{'w'+str(n) for n in range(8)} and a['phase']==phase and a['dci']==dci,'wrong live context schema/order');v=[a['w'+str(n)] for n in range(8)];require(all(0<=x<2**32 for x in v),'context word overflow');return v
 def contexts(phase,posts=0):
  a=context(phase,8);b=context(phase,9)
  if phase==0 or phase==8:require(a[0]&7==0,'live OUT not disabled')
  else:
   require(a[0]==(1 if phase<6 else 3) and a[1]==(3<<1)|(2<<3)|(512<<16) and a[4:]==[124,0,0,0],'wrong live OUT context')
   deq=a[2]|a[3]<<32;require(deq&15==1 and tx<=deq&~15<=tx+(0 if phase in (1,2) else 48),'OUT dequeue outside three-frame ring');require(phase<6 or deq==tx+49,'OUT stop dequeue not exact')
  if phase in (0,1,7,8):require(b==old,'old RX context not exact')
  else:
   require(b[0]==old[0] and b[1]==old[1] and b[4:]==old[4:],'RX context not identically stopped')
   deq=b[2]|b[3]<<32;require(deq&15==1 and rx<=deq&~15<=rx+16*posts,'RX dequeue outside retained ring')
 def command(phase,kind,dci):require(take('wifi-join5-command')==dict(phase=phase,type=kind,dci=dci,slot=slot,ok=1),'wrong live command receipt')
 contexts(0);command(0,12,8);contexts(1);command(1,16,9);contexts(2)
 for kind,length in enumerate((60,100,56)):require(take('wifi-join5-send')==dict(kind=kind,bytes=length,completed=1,replay=0),'missing/replayed management publication')
 m=take('wifi-join5-mac');fixed=dict(complete=1,started=1,snapshot=1,disabled=1,tx_drained=1,rx_drained=1,filter_restored=1,txop_restored=1,filter=0x17f97,ibi=0xffffffff)
 require(set(m)==set(fixed)|{'operations','reads','writes','waits','alc','txop','hold_us'} and all(m[k]==v for k,v in fixed.items()),'incomplete MAC lifecycle')
 power=[fields(x,'wifi-power5-usb') for x in lines if x.startswith('wifi-power5-usb:')];alc=next(x['value'] for x in power if x['cleanup']==0 and x['write']==1 and x['address']==0x13b0)
 require(m['alc']==alc and m['operations']==m['reads']+m['writes']+m['waits'] and 0<m['operations']<20000 and m['hold_us']>=10000,'wrong selected MAC power/counters')
 stop=take('wifi-join5-stop-event');require(set(stop)=={'dci','cc','residue','pointer','ok'} and stop['dci']==9 and stop['ok']==1 and stop['cc'] in (26,27,28) and 0<=stop['residue']<=4096 and rx<=stop['pointer']<=rx+48*16 and (stop['pointer']-rx)%16==0 and (stop['cc']!=27 or stop['residue']==0),'wrong RX stop receipt')
 command(4,15,9);contexts(5,48)
 require(take('wifi-join5-stop-event')==dict(dci=8,cc=27,residue=0,pointer=tx+48,ok=1),'wrong empty OUT forced stop')
 command(5,15,8);contexts(6,48);command(6,16,9);contexts(7);command(7,12,8);contexts(8)
 s=take('wifi-join5');fixed=dict(ok=1,exchange=1,auth=1,assoc=1,leave=1,unsafe=0,sends=3,status=0,commands=6,restored=1,phase=9,retained=1,authenticated=0,ip_data=0)
 require(set(s)==set(fixed)|{'polls','ignored','aid','posts','controls'} and all(s[k]==v for k,v in fixed.items()) and 2<=s['polls']<=16384 and 0<=s['ignored']<=s['polls'] and 1<=s['aid']<=2007 and 3<=s['posts']<=48 and s['controls']==m['reads']+m['writes'],'incomplete matched management or retirement')
 require(i==len(rows),'extra live management receipt')
 rxrows=[x for x in aux if x.startswith('wifi-join5-rx:')];exrows=[x for x in aux if x.startswith('wifi-join5-exchange:')]
 require(2<=len(rxrows)<=s['posts'] and len(exrows)==1,'missing/extra private RX/exchange diagnostics')
 ex=fields(exrows[0],'wifi-join5-exchange');require(set(ex)=={'complete','clock_valid','elapsed_us'} and ex['complete']==ex['clock_valid']==1 and 0<=ex['elapsed_us']<=5000000,'incomplete exchange timing')
 sends=[x for x in rows if x.startswith('wifi-join5-send:')];mac=next(x for x in rows if x.startswith('wifi-join5-mac:'))
 require(lines.index(sends[-1])<lines.index(exrows[0])<lines.index(mac),'exchange timing outside lifetime')
 matched=[]
 for n,line in enumerate(rxrows):
  a=fields(line,'wifi-join5-rx');require(set(a)=={'index','bytes','view','mpdu','fc','body','peer','algorithm','transaction','code','parser'} and a['index']==n and 0<=a['bytes']<=4096 and 0<=a['view']<=3 and 0<=a['peer']<=1 and 0<=a['parser']<=3 and all(0<=a[k]<=65535 for k in ('fc','algorithm','transaction','code')),'wrong RX diagnostic schema/range/order')
  require(lines.index(sends[0])<lines.index(line)<lines.index(sends[-1]),'RX diagnostic outside reply interval')
  if a['view']:
   require(all(a[k]==0 for k in ('mpdu','fc','body','peer','algorithm','transaction','code')) and a['parser']==3,'invalid RX view leaked decoded fields')
  else:require(24<=a['mpdu']<=4096 and a['body']==a['mpdu']-24 and a['bytes']>=a['mpdu']+36,'invalid normalized RX dimensions')
  if a['peer'] and a['parser']==0 and a['fc']&0xfc in (0xb0,0x10):matched.append((a,line))
 require(any(a['fc']&0xfc==0xb0 and a['mpdu']>=30 and a['algorithm']==0 and a['transaction']==2 and a['code']==0 and lines.index(line)<lines.index(sends[1]) for a,line in matched),'missing diagnostic proof of matched AUTH')
 require(any(a['fc']&0xfc==0x10 and a['code']==0 and lines.index(sends[1])<lines.index(line) for a,line in matched),'missing diagnostic proof of matched association')
 for phase in (5,6):
  a=fields(next(x for x in rows if x.startswith(f'wifi-join5-context: phase={phase} dci=9 ')),'wifi-join5-context');deq=(a['w2']|a['w3']<<32)&~15;require(rx<=deq<=rx+16*s['posts'] and deq==stop['pointer'],'RX stop beyond publications or different from stop event')
 require(stop['pointer']<=rx+16*s['posts'] and (stop['cc']==27 or stop['pointer']==rx+16*(s['posts']-1)),'RX stop receipt not bound to posted TD')
 cleanup=next(x for x in lines if x.startswith('wifi-power5-usb:') and fields(x,'wifi-power5-usb')['cleanup']==1)
 require(lines.index(next(x for x in lines if x.startswith('wifi-tx5:')))<lines.index(PROGRESS)<lines.index(rows[-1])<lines.index(cleanup),'live owner outside frozen parent lifetime')
 # Optional only for historical source fixtures. New-source capture recipes
 # separately require these two companions; their absence never fabricates one.
 observations=[x for x in aux if x.startswith('wifi-join5-response:')]
 if observations:
  require(len(observations)==2,'missing/duplicate response observation')
  observed=[]
  keys='phase attempted reason polls present ignored peer clocks span_us'.split()
  for phase,line in enumerate(observations):
   a=fields(line,'wifi-join5-response')
   require(set(a)==set(keys) and line=='wifi-join5-response: '+' '.join(k+'='+str(a[k]) for k in keys),'wrong response observation schema/canonical order')
   require(a['phase']==phase and a['attempted']==a['reason']==a['peer']==1 and 1<=a['polls']<=8192 and 1<=a['present']<=a['polls'] and a['ignored']==a['present']-1 and a['clocks']==a['polls'] and 0<=a['span_us']<750000,'incomplete/mismatched response observation')
   require(lines.index(rows[-1])<lines.index(line)<lines.index(cleanup),'response observation outside restored lifetime')
   observed.append(a)
  require(sum(a['polls'] for a in observed)==s['polls'] and sum(a['ignored'] for a in observed)==s['ignored'],'response observations differ from exchange counters')
 r.update(join5_complete=True,join5_management_out=3,join5_auth_matched=True,join5_assoc_matched=True,join5_leave_submitted=True,join5_originals_restored=True,join5_posts=s['posts'],scope='fresh live RX/data OUT;matched open-system AUTH and association replies;leave submitted;MAC disabled,endpoint/input/parent originals restored;no WPA2 or authenticated IP/data')
 return r
