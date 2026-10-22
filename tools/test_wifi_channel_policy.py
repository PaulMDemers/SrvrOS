import copy
import unittest
from wifi_channel_policy import candidates

REG = 'global\ncountry US: DFS-FCC\n (5150 - 5250 @ 80), (N/A, 23), (N/A), AUTO-BW\n'
PHY = ' * 5180.0 MHz [36] (11.0 dBm)\n'
AUDIT = {'structural_check': 'PASS', 'transmit_calibration': {
    'channels': [{'channel': 36, 'calibration_units': 17}]}}


class PolicyTests(unittest.TestCase):
    def row(self, audit=None, phy=PHY, reg=REG, country='US', width=20):
        report = candidates(audit or AUDIT, phy, reg, country, width)
        self.assertFalse(report['radio_authorized'])
        self.assertFalse(report['native_policy_installed'])
        self.assertTrue(all(not r['transmit_authorized'] for r in report['channels']))
        return report['channels'][0]

    def test_intersection_not_power_conversion(self):
        row = self.row()
        self.assertTrue(row['candidate'])
        self.assertEqual(row['snapshot_ceiling_dbm'], 11)
        self.assertEqual(row['calibration_units'], 17)
        self.assertEqual(self.row(reg=REG.replace('N/A, 23', 'N/A, 7'))['snapshot_ceiling_dbm'], 7)

    def test_country_and_width_refusals(self):
        for country in ('', 'CA', '00', 'us'):
            self.assertFalse(self.row(country=country)['candidate'])
        for reg in ('', REG.replace('US', 'CA'), REG + 'country US: DFS-FCC\n'):
            self.assertFalse(self.row(reg=reg)['candidate'])
        for width in (0, 10, 40, 80):
            self.assertFalse(self.row(width=width)['candidate'])

    def test_phy_flags_and_missing(self):
        for phy in ('', PHY.replace('5180', '5190'), PHY.replace('(11.0 dBm)', '(disabled)'),
                    *(PHY.rstrip() + ' (' + flag + ')\n' for flag in
                      ('radar detection', 'no IR', 'unknown', 'no 20MHz'))):
            self.assertFalse(self.row(phy=phy)['candidate'])
        with self.assertRaises(ValueError):
            self.row(phy=PHY + PHY)

    def test_regulatory_flags_and_band_edges(self):
        for reg in (REG.replace('AUTO-BW', 'DFS'), REG.replace('AUTO-BW', 'PASSIVE-SCAN'),
                    REG.replace('AUTO-BW', 'unknown'), REG.replace('5150', '5171'),
                    REG.replace('5250', '5189'), REG.replace('@ 80', '@ 10')):
            self.assertFalse(self.row(reg=reg)['candidate'])

    def test_calibration_and_development_set(self):
        for value in (None, -1, 64, True, '17'):
            audit = copy.deepcopy(AUDIT)
            audit['transmit_calibration']['channels'][0]['calibration_units'] = value
            self.assertFalse(self.row(audit=audit)['candidate'])
        audit = copy.deepcopy(AUDIT)
        audit['structural_check'] = 'FAIL'
        self.assertFalse(self.row(audit=audit)['candidate'])
        for ch in (12, 14, 52, 100, 144, 169, 173, 177):
            audit = copy.deepcopy(AUDIT)
            audit['transmit_calibration']['channels'][0]['channel'] = ch
            self.assertFalse(self.row(audit=audit)['candidate'])
        audit = copy.deepcopy(AUDIT)
        audit['transmit_calibration']['channels'] *= 2
        with self.assertRaises(ValueError):
            self.row(audit=audit)


if __name__ == '__main__':
    unittest.main()
