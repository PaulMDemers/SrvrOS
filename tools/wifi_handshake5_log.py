"""Categorical native one-shot WPA response observations, never key/RF acceptance."""
from wifi_http5_log import canonical_decimal

PREFIX='wifi-handshake5'
SUMMARY='version epoch responses observation_only'
RESPONSE='stage result polls present ignored kind reject start_us last_us'
KEYS='auth assoc m1 m2 m3 m4 activated erased leave unsafe installs pn'

def need(ok,why):
    if not ok:raise ValueError(why)

def observe(text):
    lines=text.splitlines();owned=[s for s in lines if s.startswith(PREFIX)]
    if not owned:return dict(present=False,native_accepted=False)
    need(all(s.startswith((PREFIX+': ',PREFIX+'-response: ')) for s in owned),'unknown handshake companion')
    keys=[canonical_decimal(s,'wifi-stream5-keys',KEYS) for s in lines if s.startswith('wifi-stream5-keys: ')]
    at=0;epochs=[]
    while at<len(owned):
        s=canonical_decimal(owned[at],PREFIX,SUMMARY);at+=1;j=len(epochs)
        need(j<2 and s['version']==1 and s['epoch']==j+1 and 0<=s['responses']<=4 and s['observation_only']==1,'handshake summary bounds')
        need(j<len(keys),'missing handshake key companion');k=keys[j];rows=[]
        for stage in range(s['responses']):
            need(at<len(owned),'truncated handshake observation')
            r=canonical_decimal(owned[at],PREFIX+'-response',RESPONSE);at+=1
            need(r['stage']==stage and 1<=r['result']<=9 and 0<=r['ignored']<=r['present']<=r['polls']<=20000 and r['kind']<=3 and r['reject']<=13,'handshake response bounds')
            need(not rows or rows[-1]['result']==1,'response after failed stage')
            need(r['result']==4 or r['polls']>0,'response without poll')
            need(r['start_us']<2**64 and r['last_us']<2**64,'handshake clock overflow')
            if r['result']!=3:need(r['last_us']>=r['start_us'],'handshake clock rollback unreported')
            limit=750000 if stage<2 else 1500000
            if r['result']==2:need(r['last_us']-r['start_us']>=limit,'early handshake timeout')
            if r['result'] in (1,9):need(r['last_us']-r['start_us']<limit,'late handshake response accepted/rejected as protocol')
            if r['result']==8:need(r['polls']==20000,'early handshake attempt exhaustion')
            if stage<2:
                need(r['kind']==r['reject']==0,'management claims EAPOL category')
                if r['result'] in (1,9):need(r['present']==r['ignored']+1,'management response count')
            else:
                need(r['ignored']==0 and r['present']<=1,'one-shot EAPOL response count')
                if r['result']==1:need(r['present']==1 and r['kind']==(1 if stage==2 else 2) and r['reject']==1,'accepted EAPOL category differs')
                elif r['result']==9:need(r['present']==1 and r['kind']>0 and 2<=r['reject']<=13,'missing EAPOL rejection category')
                else:need(r['present']==r['kind']==r['reject']==0,'non-protocol refusal claims EAPOL result')
            rows.append(r)
        accepted=lambda i:len(rows)>i and rows[i]['result']==1
        need(all(k[name]==int(accepted(i)) for i,name in enumerate(('auth','assoc','m1','m3'))),'handshake/key result differs')
        need(all(k[x] in (0,1) for x in KEYS.split()[:10]) and k['installs']<=1 and k['pn']<2**48,'key companion bounds')
        need(not k['m2'] or k['m1'],'M2 without M1');need(not k['m4'] or (k['m3'] and k['installs']==1),'M4 without validated M3')
        need(not k['activated'] or (k['m4'] and k['m2']),'activated without four-way completion')
        epochs.append(dict(summary=s,responses=rows,key_observation=k))
    need(len(epochs)==len(keys),'missing handshake epoch')
    return dict(present=True,epochs=epochs,native_accepted=False,radio_or_AP_delivery_proved=False,scope='Existing one-shot response clocks and categorical refusal only; no packet/nonce/replay/key bytes, retries, acceptance or retirement authority')
