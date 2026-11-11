import copy
import unittest
from append_wait import append_wait

class AppendWait(unittest.TestCase):
    def setUp(self):
        self.writes = dict(snapshot_us=1000, workers=[dict(record=dict(pid=7,calls=65,fast_calls=20),sites=dict(call=dict(total_us=500,max_us=250)))])
        self.row = dict(version=1,snapshot_us=1000,pid=7,calls=2,wall_us=300,max_us=200,wakes=3,overlap_ready=1,final=1)
    def blob(self, row=None):
        r=self.row if row is None else row
        return ('capture-append-wait: '+' '.join(f'{k}={v}' for k,v in r.items())).encode()
    def test_bound_final(self):
        self.assertEqual(append_wait([self.blob(),self.blob()],self.writes)['overlap_ready'],1)
    def test_unexercised_explicit(self):
        for k in ('calls','wall_us','max_us','wakes','overlap_ready'): self.row[k]=0
        self.assertEqual(append_wait([self.blob()],self.writes)['overlap_ready'],0)
    def test_invalid(self):
        for key,value in [('version',2),('final',0),('pid',8),('calls',66),('wall_us',501),('max_us',251),('overlap_ready',3),('wakes',0),('wall_us',0),('calls',0),('wakes',2**64)]:
            row=copy.copy(self.row);row[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):append_wait([self.blob(row)],self.writes)
    def test_missing_conflict_and_malformed(self):
        with self.assertRaises(ValueError):append_wait([],self.writes)
        row=copy.copy(self.row);row['wall_us']=301
        with self.assertRaises(ValueError):append_wait([self.blob(),self.blob(row)],self.writes)
        with self.assertRaises(ValueError):append_wait([self.blob()+b' extra=1'],self.writes)

if __name__=='__main__':unittest.main()
