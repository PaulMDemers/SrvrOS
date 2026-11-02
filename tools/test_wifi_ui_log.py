import unittest
from wifi_ui_log import check_ui,check_radio_isolation,check_desktop_input,snapshot,SCAN
RELEASE='0123456789abcdef'

def saved(stream,name,stamp,cap):
    marker=f'workspace-log: snapshot={name} monotonic_us={stamp} total_before={len(stream)} tail_limit={cap}\n'.encode()
    return (stream+marker)[-cap:],stream+marker

def fixture(long=False,empty=False,unselectable=False):
    before='wifi-ui-cache: phase=before status=0 owner=1 generation=1 adapters=1 ready=1 rows=1 selected=4294967295 selectable=1 identifiers_logged=0\n'
    if empty:before=before.replace('rows=1','rows=0').replace('selectable=1','selectable=0')
    elif unselectable:before=before.replace('selectable=1','selectable=0')
    counts='wifi-ui-counts: phase=before published=1 reads=1 selected=0 stale=0 refused=0 removed=0 identifiers_logged=0\n'
    stream=(f'srvros.desktop-release={RELEASE} srvros.wifi-desktop-trial=1\n'+'x'*9000+'\n'+before+counts+'wifi-ui-run: id=123 stage=scan identifiers_logged=0\n'+SCAN+'\n').encode()
    if empty or unselectable:
        prefix=b'srvros.wifi-desktop-empty=1 srvros.trackpad=gestures srvros.wifi-log-isolation=1\nxhci: trackpad slot=6 dci=3 ready=yes request=486 pointer=yes\nworkspace-log: phase=wifi-radio periodic=paused tail_limit=1048576\nwifi-ui-log: isolation_required=1 parked=1\nwifi-firmware-control: captured\n'
        stream=prefix+stream
    radio,stream=saved(stream,'wifi-scan.log',100,1048576)
    stream+=b'workspace-log: phase=wifi-desktop startup_saved=1 tail_limit=65536\n'
    begin,stream=saved(stream,'wifi-ui-begin.log',200,65536)
    stream+=b'wifi-ui-capture: admitted=1 run=123 checklist=wifi-trial timed_exit=0 credentials=0 identifiers_logged=0\n'
    if not (empty or unselectable):stream+=b'wifi-ui-choice: result=0 owner=1 generation=1 index=0 identifiers_logged=0\n'
    if long:stream+=b'y'*80000+b'\n'
    stream+=b'deskd: exited\nwifi-ui-cache: phase=after status=0 owner=0 generation=0 adapters=0 ready=0 rows=0 selected=4294967295 selectable=0 identifiers_logged=0\nwifi-ui-counts: phase=after published=1 reads=9 selected=1 stale=0 refused=0 removed=1 identifiers_logged=0\nwifi-ui-capture: desktop_returned=yes run=123 manual_acceptance=PENDING\nlaptop-capture: completed=yes\n'
    if empty or unselectable:stream=stream.replace(b'published=1 reads=9 selected=1',b'published=1 reads=9 selected=0')
    end,stream=saved(stream,'exit.log',300,65536)
    return radio,begin,end

class Proof(unittest.TestCase):
    def test_repeated_periodic_snapshots(self):
        stream=b'x'*70000+b'\n'
        for stamp in range(100,106):
            raw,stream=saved(stream,'session-a.log',stamp,65536)
            stream+=b'y'*800+b'\n'
        start,end,stamp=snapshot(raw,'session-a.log',65536,rolling=True)
        self.assertEqual((end,stamp),(len(stream)-801,105))
        self.assertGreater(start,0)
        with self.assertRaises(AssertionError):snapshot(raw,'session-a.log',65536)
        for old,new in [(b'monotonic_us=100',b'monotonic_us=105'),
                        (b'total_before=70001',b'total_before=70002')]:
            damaged=raw.replace(old,new,1);self.assertNotEqual(damaged,raw)
            with self.assertRaises(AssertionError):snapshot(damaged,'session-a.log',65536,rolling=True)
        for damaged in [raw[:-1],raw+b'x',raw[1:]]:
            with self.assertRaises(AssertionError):snapshot(damaged,'session-a.log',65536,rolling=True)
        for name in ['wifi-scan.log','wifi-ui-begin.log','exit.log','boot.log']:
            with self.assertRaises(AssertionError):snapshot(raw.replace(b'session-a.log',name.encode()),name,65536,rolling=True)

    def test_empty_and_unselectable_ui(self):
        for long in [False,True]:
            for empty in [False,True]:
                raw=fixture(long,empty=empty,unselectable=not empty)
                r=check_ui(*raw,RELEASE)
                self.assertFalse(r['selectable_observed'] or r['selection_observed'])
                self.assertEqual(r['discovery_observed'],not empty)
                self.assertTrue(r['removal_observed'])
        # A rolled tail cannot claim successful selection from an empty or
        # entirely unselectable initial cache, even after adapter removal.
        radio,begin,end=fixture(True,empty=True)
        changed=end.replace(b'published=1 reads=9 selected=0',b'published=1 reads=9 selected=1')
        prefix=changed[:changed.rindex(b'workspace-log: snapshot=exit.log ')]
        # Preserve the original absolute start when recomputing the marker.
        marker=end[end.rindex(b'workspace-log: snapshot=exit.log '):]
        changed=prefix+marker
        with self.assertRaises(AssertionError):check_ui(radio,begin,changed,RELEASE)
        # Keep byte bindings valid while removing the opt-in: legacy profiles
        # must still refuse an empty/unselectable cache.
        for empty in [False,True]:
            radio,begin,end=fixture(empty=empty,unselectable=not empty)
            old=b'srvros.wifi-desktop-empty=1';new=b'srvros.wifi-desktop-empty=0'
            with self.assertRaises(AssertionError):check_ui(radio.replace(old,new),begin.replace(old,new),end.replace(old,new),RELEASE)

    def test_desktop_input(self):
        raw=b'srvros.trackpad=gestures\nxhci: trackpad slot=6 dci=5 ready=yes request=512 pointer=yes\n'
        self.assertTrue(check_desktop_input(raw)['pointer_enabled'])
        for old,new in [(b'gestures',b'capture'),(b'ready=yes',b'ready=no'),(b'pointer=yes',b'pointer=no'),(b'dci=5',b'dci=32'),(b'request=512',b'request=4097')]:
            with self.assertRaises(AssertionError):check_desktop_input(raw.replace(old,new))
        with self.assertRaises(AssertionError):check_desktop_input(raw+raw.split(b'\n',1)[1])

    def test_radio_isolation(self):
        start='workspace-log: phase=wifi-radio periodic=paused tail_limit=1048576\n'
        parked='wifi-ui-log: isolation_required=1 parked=1\n'
        prefix='srvros.wifi-log-isolation=1\n'
        raw=(prefix+start+parked+'wifi-firmware-control: captured\n'+SCAN+'\n').encode()
        self.assertTrue(check_radio_isolation(raw)['periodic_paused'])
        for old,new in [(start,''),(start,start*2),(parked,parked.replace('=1\n','=0\n')),
                        (prefix,''),('wifi-firmware-control:', 'firmware-control:')]:
            with self.assertRaises(AssertionError):check_radio_isolation(raw.replace(old.encode(),new.encode(),1))
        for io in ['workspace-log: snapshot=session-a.log','workspace: flushed','workspace-cost:','usb-timing:']:
            with self.assertRaises(AssertionError):check_radio_isolation(raw.replace(parked.encode(),(parked+io+'\n').encode()))

    def test_complete_and_rolled_tail(self):
        for long in [False,True]:
            r=check_ui(*fixture(long),RELEASE)
            self.assertTrue(r['selection_observed'] and r['removal_observed']);self.assertEqual(r['manual_acceptance'],'PENDING')
            self.assertEqual(len(r['visible_choices']),0 if long else 1)
    def test_snapshot_bytes_and_bindings(self):
        for long in [False,True]:
            original=fixture(long)
            for field in range(3):
                for pos in [0,len(original[field])//2,len(original[field])-1]:
                    v=list(original);v[field]=v[field][:pos]+v[field][pos+1:]
                    with self.subTest(long=long,field=field,pos=pos),self.assertRaises(AssertionError):check_ui(*v,RELEASE)
            v=list(original);v[1]=v[1].replace(b'id=123 stage',b'id=124 stage')
            with self.assertRaises(AssertionError):check_ui(*v,RELEASE)
    def test_anonymous_state_and_completion_refusals(self):
        radio,begin,end=fixture()
        mutations=[(b'run=123 manual',b'run=124 manual'),(b'desktop_returned=yes',b'desktop_returned=NO'),(b'completed=yes\n',b'completed=NO\n'),(b'phase=after status=0',b'phase=after status=1'),(b'published=1 reads=9',b'published=2 reads=9'),(b'selected=1 stale=0',b'selected=0 stale=0'),(b'removed=1',b'removed=0'),(b'removed=1',b'removed=2'),(b'credentials=0',b'credentials=1'),(b'owner=1 generation=1 index=0',b'owner=2 generation=1 index=0'),(b'owner=1 generation=1 index=0',b'owner=1 generation=2 index=0'),(b'index=0 identifiers',b'index=1 identifiers'),(b'timed_exit=0',b'timed_exit=1'),(b'identifiers_logged=0',b'identifiers_logged=1')]
        # Preserve snapshot length/offset after content mutation so failures
        # reach phase/accounting gates rather than only a stale byte count.
        for old,new in mutations:
            changed=end.replace(old,new);self.assertNotEqual(changed,end)
            prefix=changed[:changed.rindex(b'workspace-log: snapshot=exit.log ')]
            fixed,_=saved(prefix,'exit.log',300,65536)
            with self.subTest(old=old,new=new),self.assertRaises(AssertionError):check_ui(radio,begin,fixed,RELEASE)
        with self.assertRaises(AssertionError):check_ui(radio,begin,end,'fedcba9876543210')
if __name__=='__main__':unittest.main()
