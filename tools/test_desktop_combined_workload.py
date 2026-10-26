"""Reject partial disk/audio results and validate concurrent shell execution."""
import unittest
import desktop_combined_workload as w

class WorkloadTest(unittest.TestCase):
    def test_complete_and_corrupt(self):
        run='0123456789abcdef';stem='combined-'+run+'/'
        data=w.files(run,2,1,6,2)|{stem+'payload.bin':w.SEED,stem+'rounds.txt':b'1\n2\n',
             stem+'io.txt':b'1:1\n1:2\n2:1\n2:2\n',stem+'complete.txt':f'COMPLETE {run} 2 1 6 2\n'.encode()}
        for n in (1,2):data[stem+f'audio-{n}.txt']=b'playwav: complete bytes=1152000 underruns=0\n'
        self.assertEqual(w.verify(data.__getitem__,run,2,1,6,2)['audio_completions'],2)
        for name in list(data):
            bad=data.copy();bad[name]=b'incomplete'
            with self.assertRaises(ValueError):w.verify(bad.__getitem__,run,2,1,6,2)
        bad=data.copy();bad[stem+'audio-2.txt']=b'playwav: complete bytes=1152000 underruns=1\n'
        with self.assertRaises(ValueError):w.verify(bad.__getitem__,run,2,1,6,2)
    def test_bounds_and_overlap_order(self):
        for args in [('x; reboot',30,1,60,40),('0'*16,0,1,60,40),('0'*16,31,1,60,40),('0'*16,30,0,60,40),('0'*16,1,1,6,6)]:
            with self.assertRaises(ValueError):w.files(*args)
        s=w.files('0'*16,1,1,6,2)['combined-'+'0'*16+'.sh'].decode()
        self.assertLess(s.index('if test -e'),s.index('mkdir'))
        self.assertLess(s.index('2>&1 &'),s.index('cp ../'))
        self.assertLess(s.rindex('cp ../'),s.index('wait $player'))
        self.assertEqual(s.count('kill -0 "$player"'),2)
        self.assertEqual(s.count('playback ended before disk burst'),1)
        native=w.files('0'*16,1)['combined-'+'0'*16+'.sh'].decode()
        self.assertEqual(native.count('for burst in '),5)
        self.assertIn('echo 40 > expected-count.txt || exit 1',native)
        self.assertIn('cmp io-count.txt expected-count.txt || exit 1',native)
        self.assertNotIn('$(',native)
        self.assertTrue(all(len(line)<256 for line in native.splitlines()))

if __name__=='__main__':unittest.main()
