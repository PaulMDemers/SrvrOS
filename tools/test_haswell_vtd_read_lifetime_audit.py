import copy
import unittest
from unittest.mock import patch

from haswell_vtd_read_lifetime_audit import (A, DENIED, READ_LEAF, audit,
    expected_path, exact_path, model_series, sample_words, guard_budget)
from haswell_vtd_read_audit import (SEQUENCE, SAMPLE_COUNT, MAP_CAP, PASS_LIMIT_US,
    GAP_MIN_US, GAP_MAX_US, GAP_ITERATION_CAP, RECEIPT_TAIL_BYTES)
from test_hardware_air61_window import joint, PIN, SESSION


def sample():
    return list(zip(SEQUENCE,(0,0x10,1,0x1234,1,2,0x56,2,0,0x4000,0,0)))


def series():
    passes=[]
    for index in range(2):
        shift=index*77404
        p=dict(owner=0x6169723631,epoch=1000+shift,root=0x2000000,
            borrowed_word=0x2001023,pd=0x2006000,pt=0x2007000,
            begin=1001+shift,published=65000+shift,sample_begin=65001+shift,
            sample_end=65030+shift,retired=78403+shift,words=sample(),receipt_bytes=60000)
        path=expected_path(p['root'],p['borrowed_word'],p['pd'],p['pt'])
        p.update(before=[list(path) for _ in SEQUENCE],after=[list(path) for _ in SEQUENCE]);passes.append(p)
    return passes


class ReadLifetimeAuditTests(unittest.TestCase):
    def assert_denied(self, result):
        for key in DENIED:self.assertIs(result[key],False,key)

    def test_fixed_limits_and_encoding_remain_exact(self):
        self.assertEqual((SAMPLE_COUNT,MAP_CAP,PASS_LIMIT_US,GAP_MIN_US,GAP_MAX_US,
            GAP_ITERATION_CAP,RECEIPT_TAIL_BYTES),(2,2,100000,20000,100000,1000000,65536))
        self.assertEqual(READ_LEAF,0x80000000fed90039)
        self.assertEqual(READ_LEAF ^ 0x80000000fed90019,A)

    def test_two_bounded_lifetimes_and_unmapped_repreparation_fit_gap(self):
        rows=series();saved=copy.deepcopy(rows);result=model_series(rows)
        self.assertEqual(result['pass_lifetimes_us'],[77402,77402])
        self.assertEqual(result['simulated_dword_count'],24)
        self.assertEqual(result['gap_us'],77404-29)
        self.assertEqual(rows,saved);self.assert_denied(result)

    def test_exact_path_no_bit_masks_or_borrowed_mutation(self):
        path=expected_path(0x2000000,0x2001023,0x2006000,0x2007000)
        self.assertEqual(path[-1],READ_LEAF);exact_path(path,path)
        for level in range(4):
            for bit in range(64):
                changed=list(path);changed[level]^=1<<bit
                with self.assertRaises(ValueError,msg=str((level,bit))):exact_path(path,changed)
        for root_word in (0x2001003,0x2001027,0x2001063):
            with self.assertRaises(ValueError):expected_path(0x2000000,root_word,0x2006000,0x2007000)
        for pd,pt in ((0x2006000,0x2006000),(0x2000000,0x2007000),
                (0x2001000,0x2007000),(0xfed90000,0x2007000),(0xfed91000,0x2007000),
                (0x1000,0x2007000),(1<<39,0x2007000)):
            with self.assertRaises(ValueError):expected_path(0x2000000,0x2001023,pd,pt)

    def test_exact_sequence_no_fault_command_poll_or_table_access(self):
        self.assertEqual(len(sample_words(sample())),12)
        for index in range(12):
            for offset in (0x18,0x28,0x34,0x1000,0xfed91000):
                rows=sample();rows[index]=(offset,0)
                self.assertRaises(ValueError,sample_words,rows)
            for value in (True,-1,1<<32,0xffffffff):
                rows=sample();rows[index]=(rows[index][0],value)
                self.assertRaises(ValueError,sample_words,rows)
        for rows in (sample()[:-1],sample()+[(0x1c,0)]):self.assertRaises(ValueError,sample_words,rows)
        rows=sample();rows[1]=(0,0);self.assertRaises(ValueError,sample_words,rows)

    def test_torn_changed_brackets_and_inter_sample_changes_refuse(self):
        for index in (0,2,4,5,7,8,10,11):
            rows=sample();rows[index]=(rows[index][0],rows[index][1]^1)
            self.assertRaises(ValueError,sample_words,rows)
        rows=series();rows[1]['words'][3]=(8,0x1235)
        self.assertRaises(ValueError,model_series,rows)

    def test_no_retries_extra_passes_stale_tokens_or_overlapping_lifetimes(self):
        for rows in ([],series()[:1],series()+[series()[1]]):self.assertRaises(ValueError,model_series,rows)
        for key,value in [('owner',7),('root',0x3000000),('epoch',1000),('begin',78402)]:
            rows=series();rows[1][key]=value;self.assertRaises(ValueError,model_series,rows)
        rows=series();rows[0]['native_read_authorized']=True;self.assertRaises(ValueError,model_series,rows)
        for key in ('before','after'):
            rows=series();rows[1][key][5][3]^=64;self.assertRaises(ValueError,model_series,rows)
        rows=series();rows[1]['before']=rows[1]['before'][:-1];self.assertRaises(ValueError,model_series,rows)

    def test_clock_gap_and_receipt_boundaries_are_not_widened(self):
        rows=series();rows[0]['retired']=rows[0]['begin']+PASS_LIMIT_US
        self.assertRaises(ValueError,model_series,rows)
        for gap in (20000,100000):
            rows=series();p=rows[1];p['sample_begin']=rows[0]['sample_end']+gap
            p['published']=p['sample_begin']-1;p['sample_end']=p['sample_begin']+29
            p['retired']=max(p['sample_end']+1,p['begin']+1)
            self.assert_denied(model_series(rows,GAP_ITERATION_CAP))
        for gap in (0,19999,100001):
            rows=series();p=rows[1];p['sample_begin']=rows[0]['sample_end']+gap
            p['published']=p['sample_begin'];p['sample_end']=p['sample_begin']+29;p['retired']=p['sample_end']+1
            self.assertRaises(ValueError,model_series,rows)
        for size in (0,65537):
            rows=series();rows[0]['receipt_bytes']=size;self.assertRaises(ValueError,model_series,rows)
        rows=series();rows[0]['receipt_bytes']=65536;self.assert_denied(model_series(rows))
        self.assertRaises(ValueError,model_series,series(),GAP_ITERATION_CAP+1)
        rows=series();rows[0]['sample_end']=rows[0]['sample_begin'];self.assertRaises(ValueError,model_series,rows)

    def test_firmware_guard_and_mapper_caps_require_explicit_refactor(self):
        self.assertEqual(guard_budget(61621)['firmware_bytes'],4192)
        self.assertFalse(guard_budget(61621)['fits_byte_word_caps'])
        plan=guard_budget(61621,coalesce_one=True)
        self.assertEqual(plan['firmware_bytes'],3996)
        self.assertEqual(plan['mapper_word_upper_bound'],63586)
        self.assertTrue(plan['fits_byte_word_caps']);self.assertFalse(plan['coalescing_implemented'])
        self.assert_denied(plan)
        self.assertFalse(guard_budget(61621,True,True)['fits_byte_word_caps'])
        self.assertFalse(guard_budget(65536,True)['fits_byte_word_caps'])
        for args in [(0,), (65537,), (True,), (61621,2)]:self.assertRaises(ValueError,guard_budget,*args)

    def test_strict_history_replay_is_only_a_host_plan_not_a_live_gate(self):
        text,a,b=joint(mode="root-accessed")
        self.assertRaises(ValueError,audit,text,a,b,SESSION)
        with patch('hardware_haswell_firmware.PIN',PIN):
            result=audit(text,a,b,SESSION);self.assert_denied(result)
            self.assertEqual(result['status'],'HOST_READ_LIFETIME_AUDIT_ONLY')
            self.assertEqual(result['historical_window']['pages'],6)
            self.assertEqual(result['proposed_leaf'],READ_LEAF)
            self.assertEqual(result['raw_A_D_mask'],0)
            for args in [('',a,b,SESSION),(text,'',b,SESSION),(text,a,'',SESSION),
                    (text,a,b,'wrong-session'),(text,a,b.replace('completed=yes','completed=NO'),SESSION)]:
                self.assertRaises(ValueError,audit,*args)
            self.assertFalse(result['runtime_changed']);self.assertFalse(result['boot_test_prepared'])
            no_a,a0,b0=joint();self.assertRaises(ValueError,audit,no_a,a0,b0,SESSION)


if __name__=='__main__':unittest.main()
