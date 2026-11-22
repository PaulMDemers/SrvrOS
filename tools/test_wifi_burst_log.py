import unittest
from wifi_burst_log import FIELDS, HEX, visit


def row(tag, **values):
    return tag+': '+' '.join(k+'='+('0x%016x'%v if k in HEX else str(v)) for k,v in values.items())+'\n'


def fixture(first=0, completed=8, kind='normal', current_stop=False):
    attempts=completed+(completed<8)
    ring,old=0x80000,0x40000
    pages=[0x100000+4096*i for i in range(9)]
    lines=[row('wifi-channel-rx-post',count=1,requested=4096,submitted=1,ring=ring,buffer=pages[0],trb=ring,old_ring=old,old_pages=8,distinct=1,retained=1,reused=0),
           row('wifi-burst-begin',channel=6,first=first,early_events=first,early_cc=13 if first else 0,early_remaining=4012 if first else 0,inspected=0,budget_us=10000000,pages=9,retained=1,reused=0,data_out=0)]
    items=[]
    for i in range(attempts):
        td=first+i
        lines.append(row('wifi-burst-post',index=i,td=td,adopted=int(td==0),submitted=1,ring=ring,buffer=pages[td],trb=ring+16*td,retained=1,reused=0))
        item=dict(index=i,td=td,submitted=1,completed=int(i<completed),cc=0,remaining=0,actual=0,inspected=0,view=255,beacon_status=255,consumed=0,mpdu=0,frame_control=0,beacon=0,interval=0,ssid_size=0,privacy=0,rsn=0,wpa=0,ds=0,probe=0,identifiers_logged=0)
        if i<completed:
            actual=0 if kind=='empty' else 4096 if kind=='full' else 84
            item.update(cc=1 if kind=='full' else 13,remaining=4096-actual,actual=actual,inspected=int(actual>0))
            if actual:
                if kind in ('malformed','unrelated'):item['view']=1 if kind=='malformed' else 2
                else:
                    item.update(view=0,beacon_status=3 if kind=='mismatch' else 0,consumed=84,mpdu=47,frame_control=0x50 if kind=='probe' else 0x80)
                    if kind!='mismatch':item.update(beacon=1,interval=100,ssid_size=3,privacy=1,ds=1,probe=int(kind=='probe'))
            lines.append(row('wifi-burst-event',index=i,td=td,cc=item['cc'],remaining=item['remaining'],actual=actual,during_window=1,inspected=0))
        items.append(item)
    elapsed=10000000 if completed<8 else 1000
    lines.append(row('wifi-burst-poll',ok=1,attempts=attempts,completions=completed,polls=100,elapsed_us=elapsed,first=first,deadline_reset=0))
    last=ring+16*(first+attempts-1)
    lines.append(row('wifi-channel-rx-stop',attempted=1,ok=1,state=3,dequeue=(last+(0 if current_stop else 16))|1,trb=last,retained=1))
    for i in range(4):lines.append(f'wifi-channel-rx-guard: index={i} address=0x0000000000000000 value=0x0000000000000000 ok=1\n')
    lines.append(row('wifi-burst',attempted=1,complete=1,failed=0,first=first,posts=first+attempts,attempts=attempts,completions=completed,beacons=sum(x['beacon'] for x in items),ignored=0,late=0,polls=100,elapsed_us=elapsed,stopped=1,retained=1,reused=0,deadline_reset=0,identifiers_logged=0))
    for i,p in enumerate(pages):lines.append(row('wifi-burst-page',index=i,physical=p,retained=1,reused=0))
    lines.extend(row('wifi-burst-item',**item) for item in items)
    return ''.join(lines)


class BurstProof(unittest.TestCase):
    def refuse(self,text):
        with self.assertRaises((AssertionError,ValueError,KeyError,IndexError)):visit(text,6)

    def test_complete_and_quiet_matrix(self):
        for first in (0,1):
            for complete in range(9):
                for kind in ('normal','empty','full','malformed','unrelated','mismatch','probe'):
                    for current in (False,True):
                        with self.subTest(first=first,complete=complete,kind=kind,current=current):
                            result=visit(fixture(first,complete,kind,current),6)
                            self.assertEqual(result['window']['completions'],complete)
                            self.assertEqual(result['pending'],int(complete<8))

    def test_every_record_identity_order_and_grammar(self):
        for first in (0,1):
            lines=fixture(first).splitlines(keepends=True)
            for i,line in enumerate(lines):
                if not line.startswith('wifi-burst'):continue
                self.refuse(''.join(lines[:i]+lines[i+1:]))
                self.refuse(''.join(lines[:i]+[line]+lines[i:]))
                self.refuse(''.join(lines[:i]+[line.rstrip()+' extra=1\n']+lines[i+1:]))
            for i,line in enumerate(lines):
                if line.startswith('wifi-burst-event:'):
                    altered=lines[:];altered[i-1],altered[i]=altered[i],altered[i-1];self.refuse(''.join(altered))
            self.refuse(''.join(lines)+'wifi-burst-unrecognized: field=1\n')
            self.refuse(''.join(lines).replace('attempts=8','attempts=9'))
            self.refuse(''.join(lines).replace('deadline_reset=0','deadline_reset=1'))
            self.refuse(''.join(lines).replace('retained=1','retained=0'))
            self.refuse(''.join(lines).replace('inspected=0','inspected=1',1))
            self.refuse(''.join(lines).replace('budget_us=10000000','budget_us=10000001'))
            self.refuse(''.join(lines).replace('channel=6','channel=11'))
            self.refuse(''.join(lines).replace('cc=13','cc=27'))
            self.refuse(''.join(lines).replace('index=0','index=00'))
            self.refuse(''.join(lines).replace('physical=0x0000000000108000','physical=0x0000000000100000'))
            self.refuse(''.join(lines).replace('physical=0x0000000000108000','physical=0x0000000000080000'))
            self.refuse(''.join(lines).replace('physical=0x0000000000108000','physical=0x0000000000040000'))

    def test_early_and_deadline_gates(self):
        for cc,remaining in ((13,0),(13,4096),(1,0)):
            visit(fixture(1).replace('early_cc=13','early_cc='+str(cc)).replace('early_remaining=4012','early_remaining='+str(remaining)),6)
        self.refuse(fixture(1).replace('early_cc=13','early_cc=27'))
        self.refuse(fixture(1).replace('early_remaining=4012','early_remaining=4097'))
        self.refuse(fixture(0).replace('early_events=0','early_events=1'))
        self.refuse(fixture(1,7).replace('elapsed_us=10000000','elapsed_us=9999999'))
        self.refuse(fixture(1).replace('elapsed_us=1000','elapsed_us=10100001'))
        self.refuse(fixture(1).replace('remaining=4012','remaining=4097'))
        self.refuse(fixture(1).replace('actual=84','actual=85'))
        self.refuse(fixture(1).replace('trb=0x0000000000080080','trb=0x0000000000080070'))
        self.refuse(fixture(1).replace('dequeue=0x0000000000080091','dequeue=0x00000000000800a1'))

    def test_pending_and_parser_metadata(self):
        for first in (0,1):
            text=fixture(first,0)
            for key,value in [('cc',13),('remaining',4096),('actual',1),('inspected',1),('beacon',1),('interval',1),('privacy',1)]:
                at=text.index('wifi-burst-item:');item=text[at:].replace(key+'=0',key+'='+str(value),1);self.refuse(text[:at]+item)
            text=fixture(first)
            at=text.index('wifi-burst-item:')
            for old,new in [('frame_control=0x0000000000000080','frame_control=0x0000000000000081'),('interval=100','interval=0'),('ssid_size=3','ssid_size=33'),('privacy=1','privacy=2'),('mpdu=47','mpdu=100'),('consumed=84','consumed=85'),('beacon_status=0','beacon_status=1')]:
                self.refuse(text[:at]+text[at:].replace(old,new,1))


if __name__=='__main__':unittest.main()
