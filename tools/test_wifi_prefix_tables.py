from pathlib import Path
import unittest

from wifi_prefix_tables import check

SOURCE = (Path(__file__).resolve().parents[1] / 'kernel/src/drivers/mt76_mcu.c').read_text()
SWITCH = [(8976,535666761),(8984,69),(8992,372526832),(9008,84224121),
          (9012,889520132),(9016,823078460),(9064,8134661),(9068,225),
          (9072,395270),(9084,3619),(9088,12824),(9092,12864),(9100,286332438),
          (9116,707407926),(9132,656880696),(9164,387390492),(9172,640035375),
          (9180,1077952576),(9192,4112),(10240,1028653280)]
DCOC = [(11452,4336),(11456,32896),(11460,3847),(11464,64),(11468,1028),
        (11472,526339),(11476,1796),(11480,10280),(11484,20560)]
PROOF = 'switch_table=' + repr(SWITCH) + '\ndcoc_table=' + repr(DCOC) + '\n'


class PrefixTablesTests(unittest.TestCase):
    def test_current_native_source(self):
        result = check(PROOF, SOURCE)
        self.assertEqual(result['tables'], {'switch_table':20, 'dcoc_table':9})
        self.assertFalse(result['native_accepted'] or result['connectivity_tested'])

    def test_workspace_count_replacement_cannot_change_registers(self):
        for old, new in (('460','464'), ('464','466')):
            changed = PROOF.replace(old,new)
            self.assertNotEqual(changed,PROOF)
            with self.assertRaises(ValueError):check(changed,SOURCE)

    def test_each_observed_constant_corruption_refuses_independently(self):
        for old,new in (('823078460','823078464'), ('11460','11464')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check(PROOF.replace(old,new),SOURCE)

    def test_missing_duplicate_and_nonliteral_proof(self):
        for bad in (PROOF.split('\n',1)[1], PROOF+'switch_table=[]\n',
                    PROOF.replace(repr(SWITCH),'unknown()')):
            with self.assertRaises(ValueError):check(bad,SOURCE)

    def test_native_source_change_requires_new_proof(self):
        with self.assertRaises(ValueError):
            check(PROOF,SOURCE.replace('{0x2338,0x310f2e3c}','{0x2338,0x310f2e40}'))

    def test_native_table_count_duplicate_and_expression_refusals(self):
        for bad in (SOURCE.replace('bbp_switch[20]','bbp_switch[19]'),
                    SOURCE.replace('{0x2cc4,0xf07}','{0x2cc8,0xf07}'),
                    SOURCE.replace('{0x2338,0x310f2e3c}','{0x2338,0x310f2e3c+4}')):
            with self.assertRaises(ValueError):check(PROOF,bad)


if __name__ == '__main__':
    unittest.main()
