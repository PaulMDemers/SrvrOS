import unittest
from wifi_cal5_cleanup_log import check,PLAN
from test_wifi_cal5_log import fixture,RECEIVE
from wifi_cal5_log import check as full_check


def trial(posted=2,completed=1):
    ring=0x100000;pending=posted>completed
    pointer=ring+16*(8+posted-int(pending))
    dequeue=pointer+1
    status=((26<<24)|4096) if pending else 27<<24
    control=(32<<10)|(9<<16)|(5<<24)|1
    if not posted:pointer=status=control=0
    codes=sum(13<<(8*i) for i in range(completed))
    residue=[sum(3652<<(16*j) for j in range(4) if 4*b+j<completed) for b in range(2)]
    row=f'wifi-cal5-cleanup: posted={posted} completed={completed} codes=0x{codes:016x} residues0=0x{residue[0]:016x} residues1=0x{residue[1]:016x} stop_p=0x{pointer:016x} stop_s={status} stop_c={control} stop_q=0x{dequeue if posted else 0:016x} held=1 failed=0'
    base=fixture().replace('srvros.wifi-cal5=1','srvros.wifi-cal5=1 srvros.wifi-cal5-cleanup=1',1)
    base=base.replace(RECEIVE,PLAN+'\n'+RECEIVE,1)
    old=next(s for s in base.splitlines() if s.startswith('wifi-cal5-stop:'))
    stop=dict(ok=1,posts=8,state=3,dequeue=dequeue,ring=ring,retained=1,cleanup=posted)
    new='wifi-cal5-stop: '+' '.join(f'{k}=0x{v:016x}' if k in ('dequeue','ring') else f'{k}={v}' for k,v in stop.items())
    return base.replace(old,new+'\n'+row,1),stop,row


class ProofTests(unittest.TestCase):
    def test_full_and_terminal_cases(self):
        for n,c in ((0,0),(1,0),(2,1),(8,7),(1,1),(8,8)):
            log,stop,row=trial(n,c)
            r=full_check(log)
            self.assertTrue(r['cal5_cleanup_enabled'])
            self.assertEqual(r['cal5_cleanup_completions'],c)
            self.assertEqual(r['cal5_completions'],8)
    def test_rows_and_profile_required(self):
        log,stop,row=trial()
        for bad in (log.replace(PLAN+'\n',''),log.replace(row+'\n',''),log+'\n'+row,log.replace(PLAN,PLAN+'\n'+PLAN),log.replace(' srvros.wifi-cal5-cleanup=1',''),log.replace(row+'\n','')+'\n'+row):
            with self.assertRaises(ValueError):check(bad,stop)
    def test_completion_and_stop_refusals(self):
        log,stop,row=trial()
        for before,after in [('posted=2','posted=9'),('completed=1','completed=0'),('held=1','held=0'),('failed=0','failed=1'),('codes=0x000000000000000d','codes=0x000000000000001a'),('residues0=0x0000000000000e44','residues0=0x0000000000001001'),('stop_p=0x0000000000100090','stop_p=0x0000000000100080'),('stop_s=436211712','stop_s=452984832'),('stop_q=0x0000000000100091','stop_q=0x00000000001000a1'),('stop_c=84508673','stop_c=84508677')]:
            self.assertIn(before,row)
            with self.assertRaises(ValueError):check(log.replace(row,row.replace(before,after)),stop)
        # Agreement between two forged advanced dequeues is insufficient:
        # the actual stopped pending TD must remain the sampled frontier.
        advanced=dict(stop,dequeue=0x1000a1)
        with self.assertRaises(ValueError):check(log.replace('stop_q=0x0000000000100091','stop_q=0x00000000001000a1'),advanced)
        for key,value in [('cleanup',1),('posts',7),('state',1),('ok',0),('retained',0)]:
            with self.assertRaises(ValueError):check(log,dict(stop,**{key:value}))
        with self.assertRaises(ValueError):check(log.replace('held=1','held=1 unknown=0'),stop)
    def test_legacy_unchanged(self):
        r=full_check(fixture());self.assertFalse(r['cal5_cleanup_enabled'])

if __name__=='__main__':unittest.main()
