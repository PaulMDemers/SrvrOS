import unittest
from wifi_channel_log import observations
from test_wifi_burst_log import fixture
from wifi_burst_log import visit

def sample():
    visits=[visit(fixture(kind=k),6) for k in ['normal','mismatch','malformed']]
    lines=[]
    for i,v in enumerate(visits):
        for n,item in enumerate(v['items']):
            ch=6 if i==0 else 1 if i==1 else 0
            lines.append(f'wifi-ds-observation: visit={i} index={n} tuned=6 advertised={ch} identifiers_logged=0')
    return '\n'.join(lines)+'\nwifi-ds-summary: attempted=1 complete=1 entries=24 identifiers_logged=0\n',visits
class ChannelProof(unittest.TestCase):
    def test_classification(self):
        text,visits=sample();r=observations(text,visits)
        self.assertEqual((r['matched'],r['mismatched'],r['unresolved_mismatches']),(8,8,0))
        text=text.replace('advertised=1','advertised=0');self.assertEqual(observations(text,visits)['unresolved_mismatches'],8)
    def test_damage(self):
        text,visits=sample()
        for old,new in [('visit=0','visit=1'),('index=0','index=1'),('tuned=6','tuned=1'),('advertised=1','advertised=6'),('advertised=1','advertised=12'),('advertised=0','advertised=1'),('entries=24','entries=23'),('complete=1','complete=0'),('identifiers_logged=0','identifiers_logged=1')]:
            with self.subTest(old=old),self.assertRaises(AssertionError):observations(text.replace(old,new,1),visits)
        line=text.splitlines(keepends=True)[0]
        for changed in [text+line,text[len(line):],text+'wifi-ds-unknown: value=1\n']:
            with self.assertRaises(AssertionError):observations(changed,visits)
if __name__=='__main__':unittest.main()
