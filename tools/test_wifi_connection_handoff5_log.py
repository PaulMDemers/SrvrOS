import unittest
import wifi_connection_handoff5_log as h

ROW = 'wifi-handoff5-refusal: version=1 captured=1 reason=41 frontier=49 first=18 sync_users=0 poll_users=0 pending=0 command_failed=0 dequeue=112 cycle=1 event_valid=1 event_type=32 event_slot=1 event_dci=9 event_cc=1 event_owner=3 observation_only=1'
CLOSE = 'wifi-handoff5-closure: version=1 reason=0 observation_only=1'

class HandoffObservation(unittest.TestCase):
    def test_empty_success_is_not_a_refusal(self):
        self.assertFalse(h.observe('wifi-http-reconnect5-final: complete=1')['present'])
    def test_refusal_never_grants_acceptance(self):
        r = h.observe(ROW + '\n' + CLOSE)
        self.assertEqual(r['guard']['reason'], 41)
        self.assertEqual(r['guard']['frontier'], 49)
        self.assertTrue(r['ring_empty_refusal'])
        self.assertFalse(r['native_accepted'])
        self.assertFalse(r['full_protocol_acceptance'])
    def test_pair_is_unique_complete_and_ordered(self):
        for text in (ROW, CLOSE, CLOSE+'\n'+ROW, ROW+'\n'+ROW+'\n'+CLOSE, ROW+'\n'+CLOSE+'\n'+CLOSE):
            with self.subTest(text=text), self.assertRaises(ValueError): h.observe(text)
    def test_bad_fields_and_false_event_facts_refuse(self):
        for text in (ROW.replace('reason=41','reason=58'), ROW.replace('cycle=1','cycle=2'), ROW.replace('event_valid=1','event_valid=0'), ROW.replace('reason=41','reason=29'), ROW.replace('frontier=49','frontier=049'), ROW+' unknown=1'):
            with self.subTest(text=text), self.assertRaises(ValueError): h.observe(text+'\n'+CLOSE)
    def test_unknown_closure_and_version_refuse(self):
        for text in (CLOSE.replace('reason=0','reason=89'), CLOSE.replace('version=1','version=2')):
            with self.assertRaises(ValueError): h.observe(ROW+'\n'+text)

    def test_post_dispatch_failure_is_not_an_empty_ring_or_acceptance(self):
        row = ROW.replace('reason=41', 'reason=57')
        for key in ('event_valid', 'event_type', 'event_slot', 'event_dci', 'event_cc', 'event_owner'):
            import re
            row = re.sub(r'\b'+key+r'=[0-9]+', key+'=0', row)
        result = h.observe(row+'\n'+CLOSE)
        self.assertFalse(result['ring_empty_refusal'])
        self.assertFalse(result['native_accepted'])
        with self.assertRaises(ValueError):
            h.observe(ROW.replace('reason=41', 'reason=57')+'\n'+CLOSE)
