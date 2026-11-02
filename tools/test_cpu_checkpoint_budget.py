import unittest
from pathlib import Path
from check_cpu_checkpoint_budget import SOURCES, checkpoint_addition, render
from check_cpu_capture_tail import assess, final_layout
import test_cpu_receipt_tail as tail_tests


class CheckpointBudgetTests(unittest.TestCase):
    def sources(self):
        current={x:Path(x).read_text() for x in SOURCES}
        old=dict(current)
        old[SOURCES[1]]=old[SOURCES[1]].replace('    exfat_print_costs();\n','')
        # The formatter contract before PR261. No Git history is required by
        # this synthetic unit fixture or by a shallow build checkout.
        old[SOURCES[0]]='void scheduler_print_io_owners(void) {\n'+''.join(
            line+'\n' for line in current[SOURCES[0]].splitlines()
            if 'console_printf("io-owner' in line)+'}\n'
        return current,old

    def test_full_capacity_and_explicit_width(self):
        current,old=self.sources();extra,caps=checkpoint_addition(current,old)
        self.assertEqual(caps,dict(gate_rows=9,cost_rows=4,cost_phase_rows=16))
        self.assertEqual(extra.count('io-gate: '),9)
        self.assertEqual(extra.count('exfat-cost: '),4)
        self.assertEqual(extra.count('exfat-cost-phase: '),16)
        self.assertIn(str(2**64-1),extra)
        self.assertEqual(render('%u %s\n'),str(2**64-1)+' '+'x'*16+'\n')
        with self.assertRaises(ValueError):render('%x\n')

    def test_unknown_source_or_capacity_fails_closed(self):
        current,old=self.sources()
        for name,a,b in [(SOURCES[0],'#define IO_OBSERVE_ROWS 8','#define IO_OBSERVE_ROWS 9'),
                         (SOURCES[2],'#define EXFAT_COST_ROWS 4','#define EXFAT_COST_ROWS 5'),
                         (SOURCES[1],'    exfat_print_costs();','    another_report();'),
                         (SOURCES[0],'io-gate: version=1','new-report: version=1')]:
            changed=dict(current);changed[name]=changed[name].replace(a,b)
            with self.subTest(change=a),self.assertRaises(ValueError):checkpoint_addition(changed,old)

    def test_budget_cannot_promote_missing_or_evicted_proofs(self):
        current,old=self.sources();extra,_=checkpoint_addition(current,old)
        fixture=final_layout(tail_tests.FinalReceiptTests().fixture(resources=True))
        result=assess(fixture+extra)
        self.assertFalse(result['physical_acceptance']);self.assertTrue(result['synthetic'])
        # More post-final output must cause loss, never an expanded tail/pass.
        failed=assess(fixture+extra+'post-final-chatter:'+'x'*65536+'\n')
        self.assertEqual(failed['status'],'COVERAGE_FAIL')
        with self.assertRaises(ValueError):assess(fixture.replace('laptop-capture: completed=yes','laptop-capture: completed=NO')+extra)


if __name__=='__main__':unittest.main()
