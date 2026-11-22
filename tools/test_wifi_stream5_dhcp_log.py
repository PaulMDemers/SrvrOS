"""Explicit retry receipt tests;synthetic parents never establish hardware proof."""
import os,re,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import test_wifi_stream5_log as old
import wifi_stream5_log as base
import wifi_stream5_dhcp_log as check
import wifi_eapol5_log as eapol
RELEASE=old.RELEASE

def child(retry=1):
    rows=old.synthetic_child();rows[0]=base.PROGRESS_DHCP
    if retry:
        changed=[]
        for line in rows:
            if line.startswith('wifi-stream5-td:'):
                p,v=line.split(': ',1);a=v.split();i,d=int(a[0]),int(a[1]);old_i=i
                if d==0:
                    token=i//2+1;new_token=token+int(token>=6);a[2]=f'0x{0x300000+16*((new_token-1)%63):016x}'
                a[0]=str(i+int(i>=9));changed.append(p+': '+' '.join(a))
                if old_i==8:changed.append(f'wifi-stream5-td: 9 0 0x{0x300000+16*5:016x} {1<<24} {(1<<24)|(8<<16)|(32<<10)|1}')
            elif line.startswith(('wifi-stream5-data:','wifi-stream5-rx:')):
                p,v=line.split(': ',1);a=list(map(int,v.split()));old_i=a[0];is_tx=p.endswith('data');a[0]+=int(old_i>=9)
                if is_tx:
                    old_token=a[2];a[1]+=int(old_token>=6);a[2]+=int(old_token>=6);a[3]=(a[2]-1)%4;a[4]=1^(((a[2]-1)//63)&1);a[5]+=int(old_token>=6)
                else:a[1]+=int(a[2]>=6)
                changed.append(p+': '+' '.join(map(str,a)))
                if is_tx and old_i==8:changed.append('wifi-stream5-data: 9 11 6 1 1 2 404')
            elif line.startswith('wifi-stream5-stop: d=0 '):
                a=base.fields(line,'wifi-stream5-stop');a['p']+=16;a['q']+=16;changed.append(old.emit('wifi-stream5-stop',**a))
            elif line.startswith(('wifi-stream5-context: p=5 d=0 ','wifi-stream5-context: p=9 d=0 ')):
                a=base.fields(line,'wifi-stream5-context');a['w2']+=16;changed.append(old.emit('wifi-stream5-context',**a))
            elif line.startswith('wifi-stream5-keys:'):changed.append(line.replace('pn=70','pn=71'))
            elif line.startswith('wifi-stream5-probe:'):changed.append(line.replace('setup_sends=6','setup_sends=7').replace('elapsed_us=5000','elapsed_us=2005000').replace('total_polls=70','total_polls=71'))
            elif line.startswith('wifi-stream5:'):changed.append(line.replace(' tx=75 ',' tx=76 ').replace(' data_tx=70 ',' data_tx=71 ').replace(' tx_after=11 ',' tx_after=12 '))
            else:changed.append(line)
        rows=changed
    meta=[]
    for phase in (0,1):
        classes=[0]*29;classes[28]=1;kinds=[0]*4;kinds[phase]=1
        meta.append('wifi-stream5-dhcp: '+' '.join(map(str,[phase,retry,6+retry,6+retry,0,0,*kinds,*classes])))
    return rows[:-1]+meta+rows[-1:]

def boot_rows():return [s+' srvros.wifi-stream5-dhcp=1' for s in old.boot_rows()]

def minimal_parent(fragment):
    rows=old.minimal_parent(fragment)
    return [s+' srvros.wifi-stream5-dhcp=1' if s.startswith(('cmdline: ','laptop-capture: boot options=')) else s for s in rows]

def compose(reference,fragment):
    text=old.compose(reference,fragment)
    lines=[s+' srvros.wifi-stream5-dhcp=1' if s.startswith(('cmdline: ','laptop-capture: boot options=')) else s for s in text.splitlines()]
    text='\n'.join(lines)+'\n';marker='workspace-log: snapshot=exit.log '
    if marker in text:
        i=text.rindex(marker);text=text[:i]+re.sub(r'total_before=\d+',f'total_before={len(text[:i].encode())}',text[i:])
    return text

class Tests(unittest.TestCase):
    def check(self,rows):
        text='\n'.join(rows)
        with patch.object(base,'parent',return_value={'eapol5_complete':True}) as p:
            r=check.check(text,RELEASE);p.assert_called_once_with(text,stream=True,stream_dhcp=True);return r
    def test_success_both_attempt_counts(self):
        for retry in (0,1):
            r=self.check(minimal_parent(child(retry)));self.assertEqual(r['stream5_discover_retries'],retry);self.assertEqual(r['stream5_data_tx'],70+retry)
            self.assertFalse(r['stream5_ordinary_rx'] or r['stream5_backend_retired'])
    def test_explicit_recipe_only(self):
        rows=boot_rows();self.assertEqual(check.boot_identity('\n'.join(rows)),RELEASE)
        config=old.boot.limine_config(RELEASE,'ram-usb-wifi-fw-v136-laptop-probe-12345678-auto-return')
        actual=next(s.strip()[9:].split() for s in config.splitlines() if s.strip().startswith('cmdline: '))
        self.assertEqual(set(actual),set(rows[0][9:].split()))
        for bad in (old.boot_rows(),rows[:1],rows+rows[:1],[s+' srvros.wifi-stream5-dhcp=1' for s in rows],[s.replace('srvros.wifi-stream5=1','') for s in rows]):
            with self.assertRaises(ValueError):check.boot_identity('\n'.join(bad))
        with self.assertRaises(ValueError):base.boot_identity('\n'.join(rows))
        with patch.object(eapol,'parent') as p,self.assertRaises(ValueError):eapol.check('\n'.join(rows),stream=True)
        p.assert_not_called()
    def test_missing_duplicate_order_and_columns(self):
        rows=minimal_parent(child());ids=[i for i,s in enumerate(rows) if s.startswith('wifi-stream5')]
        for i in ids:
            for bad in (rows[:i]+rows[i+1:],rows[:i]+[rows[i]]+rows[i:]):
                with self.subTest(i=i),self.assertRaises(ValueError):self.check(bad)
            bad=rows.copy();bad[i]+=' 0'
            with self.assertRaises(ValueError):self.check(bad)
        for a,b in zip(ids,ids[1:]):
            bad=rows.copy();bad[a],bad[b]=bad[b],bad[a]
            with self.subTest(a=a,b=b),self.assertRaises(ValueError):self.check(bad)
    def test_retry_binding_and_deadline(self):
        rows=minimal_parent(child())
        for prefix,key,value in [('wifi-stream5-keys:','pn',70),('wifi-stream5-probe:','setup_sends',6),('wifi-stream5-probe:','elapsed_us',1999999),('wifi-stream5:','data_tx',70),('wifi-stream5:','tx_after',11),('wifi-stream5:','tx',75)]:
            i=next(i for i,s in enumerate(rows) if s.startswith(prefix));bad=rows.copy();bad[i]=re.sub(r'\b'+key+r'=\d+',key+'='+str(value),bad[i])
            with self.subTest(key=key),self.assertRaises(ValueError):self.check(bad)
        i=next(i for i,s in enumerate(rows) if s.startswith('wifi-stream5-data: 9 '));bad=rows.copy();bad[i]='wifi-stream5-data: 9 11 6 1 1 1 404'
        with self.assertRaises(ValueError):self.check(bad)
    def test_dhcp_metadata(self):
        rows=minimal_parent(child());ids=[i for i,s in enumerate(rows) if s.startswith('wifi-stream5-dhcp:')]
        for i in ids:
            values=rows[i].split(': ',1)[1].split()
            for column,value in ((0,'2'),(1,'2'),(2,'6'),(3,'6'),(4,'9'),(5,'1'),(38,'0')):
                v=values.copy();v[column]=value;bad=rows.copy();bad[i]='wifi-stream5-dhcp: '+' '.join(v)
                with self.subTest(column=column),self.assertRaises(ValueError):self.check(bad)
            for suffix in (' -1',' extra=1',' '):
                bad=rows.copy();bad[i]+=suffix
                with self.assertRaises(ValueError):self.check(bad)
    def test_generator_reproducibility(self):
        root=Path(__file__).resolve().parents[1]
        subprocess.run([os.sys.executable,'-B',str(root/'tools/make_mt76_stream5_oracle.py'),'--discover-retry','--output',str(root/'tools/fixtures/mt76-stream5-dhcp-synthetic.h'),'--check'],check=True,capture_output=True)
        with tempfile.TemporaryDirectory(prefix='stream5-oracle-') as temporary:
            output=Path(temporary)/'default.h'
            subprocess.run([os.sys.executable,'-B',str(root/'tools/make_mt76_stream5_oracle.py'),'--output',str(output)],check=True,capture_output=True)
            self.assertIn(output.read_text(),(root/'tools/test_xhci_wifi_stream5.c').read_text())
    def test_actual_producer_if_supplied(self):
        t=os.environ.get('WIFI_STREAM5_DHCP_TRACE')
        if not t:self.skipTest('actual native-C trace not supplied')
        self.assertTrue(self.check(minimal_parent(Path(t).read_text().splitlines()))['stream5_complete'])
    def test_entire_parent_if_supplied(self):
        p=os.environ.get('WIFI_STREAM5_PARENT');t=os.environ.get('WIFI_STREAM5_DHCP_TRACE')
        if not p or not t:self.skipTest('matched actual child/full parent not supplied')
        text=compose(Path(p).read_text(),Path(t).read_text());result=check.check(text,RELEASE)
        self.assertTrue(result['stream5_complete'] and result['eapol5_complete'] and result['power5_originals_restored'])
        for prefix in ('wifi-eapol5:','wifi-join5:','wifi-cal5:','wifi-channel-vco5:'):
            row=next(s for s in text.splitlines() if s.startswith(prefix))
            with self.subTest(prefix=prefix),self.assertRaises(ValueError):check.check(text.replace(row+'\n','',1),RELEASE)

if __name__=='__main__':unittest.main()
