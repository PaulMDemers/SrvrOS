"""Verify that early exit, failed audio and changed bytes cannot pass."""
import unittest
import desktop_combined_workload as w

class WorkloadTest(unittest.TestCase):
    def test_complete_and_corrupt(self):
        run='0123456789abcdef';stem='combined-'+run+'/'
        data=w.files(run,2,1)|{stem+'payload.bin':w.SEED,stem+'rounds.txt':b'1\n2\n',
             stem+'complete.txt':f'COMPLETE {run} 2 1\n'.encode()}
        for n in (1,2):data[stem+f'audio-{n}.txt']=b'playwav: complete bytes=1152000 underruns=0\n'
        self.assertEqual(w.verify(data.__getitem__,run,2,1)['audio_completions'],2)
        for name in list(data):
            bad=data.copy();bad[name]=b'incomplete'
            with self.assertRaises(ValueError):w.verify(bad.__getitem__,run,2,1)
        bad=data.copy();bad[stem+'audio-2.txt']=b'playwav: complete bytes=1152000 underruns=1\n'
        with self.assertRaises(ValueError):w.verify(bad.__getitem__,run,2,1)
    def test_bounds(self):
        for run,rounds,pause in [('x; reboot',30,60),('0'*16,0,60),('0'*16,31,60),('0'*16,30,0)]:
            with self.assertRaises(ValueError):w.files(run,rounds,pause)
        s=w.files('0'*16)['combined-'+'0'*16+'.sh'].decode()
        self.assertEqual(s.count('sleep 60 || exit 1'),30)
        self.assertLess(s.index('if test -e'),s.index('mkdir'))

if __name__=='__main__':unittest.main()
