"""Strict observation/refusal cases; optional exact actual-C trace integration."""
import os
from pathlib import Path
import unittest

import wifi_mcu_bulk_observation as c


def fields(reason=4, **updates):
    values = dict(version=1, cal_step=4, first_failure=1, sequence=100,
                  reason=reason, dci=11, requested=1024, flags=13, cc=0,
                  residue=0, elapsed_us=500069, polls=1000, dispatched=9,
                  event_start=63, event_start_cycle=1, event_end=8,
                  event_end_cycle=0, trb=0x100010, ring=0x100000,
                  endpoint_valid=1, endpoint_state=1,
                  endpoint_dequeue=0x100011)
    if reason == 0:
        values = {key: 0 for key in c.KEYS}
        values.update(version=1, cal_step=4)
    elif reason == 1:
        values.update(dci=0, requested=0xffffffff, trb=0, ring=0, polls=0,
                      dispatched=0, elapsed_us=0, endpoint_valid=0,
                      endpoint_state=0, endpoint_dequeue=0, cc=13, residue=1008)
    elif reason == 2:
        values.update(first_failure=0, flags=2, cc=13, residue=1008,
                      elapsed_us=22, endpoint_valid=0, endpoint_state=0,
                      endpoint_dequeue=0)
    elif reason == 3:
        values.update(flags=14, cc=0, elapsed_us=22)
    elif reason == 5:
        values.update(elapsed_us=0, polls=20000000)
    elif reason == 6:
        values.update(elapsed_us=(1 << 64) - 17)
    values.update(updates)
    return values


def emit(values):
    return c.PREFIX + ': ' + ' '.join(
        key + '=' + (f'0x{values[key]:016x}' if key in c.HEX_KEYS
                     else str(values[key])) for key in c.KEYS)


class ObservationTests(unittest.TestCase):
    def decode(self, values):
        result = c.observe('other log\n' + emit(values) + '\n')
        self.assertEqual(len(result['rows']), 1)
        self.assertEqual(result['rows'][0]['fields'], values)
        for key in ('native_accepted', 'full_protocol_acceptance',
                    'ownership_proven', 'dma_retirement_proven',
                    'recovery_authorized'):
            self.assertIs(result[key], False)
        return result['rows'][0]

    def refuse(self, values):
        with self.assertRaises(ValueError):
            c.observe(emit(values))

    def test_legacy_log_has_no_observation_or_acceptance(self):
        for text in ('', 'wifi-cal5-command: ok=0\n',
                     'wifi-reconnect5-final: complete=0 connections=0\n'):
            result = c.observe(text)
            self.assertEqual(result['rows'], [])
            self.assertFalse(result['native_accepted'])
            self.assertFalse(result['full_protocol_acceptance'])

    def test_all_recorded_branches_roundtrip(self):
        for reason, name in enumerate(c.REASONS):
            with self.subTest(reason=reason):
                row = self.decode(fields(reason))
                self.assertEqual(row['reason_name'], name)
                self.assertEqual(row['record_valid'], reason != 0)

    def test_last_success_can_be_out_or_in_before_calibration_refusal(self):
        for dci, requested, cc, residue in ((16, 16, 1, 0),
                                            (11, 1024, 13, 1008)):
            with self.subTest(dci=dci):
                row = self.decode(fields(2, dci=dci, requested=requested,
                                         cc=cc, residue=residue))
                self.assertIn('higher-level calibration', row['interpretation'])
                self.assertFalse(row['fields']['first_failure'])

    def test_elapsed_branch_does_not_imply_no_late_completion(self):
        for flags, cc, residue in ((13, 0, 0), (14, 13, 1008), (255, 255, 0xffffff)):
            with self.subTest(flags=flags):
                row = self.decode(fields(4, flags=flags, cc=cc, residue=residue))
                self.assertIn('absence of a completion is not proved',
                              row['interpretation'])
                self.assertEqual(row['snapshot_flags']['done'], bool(flags & 2))

    def test_no_elapsed_or_pointer_ownership_inference(self):
        row = self.decode(fields(6, elapsed_us=(1 << 64) - 1,
                                 trb=(1 << 64) - 1, ring=3,
                                 endpoint_dequeue=(1 << 64) - 1))
        self.assertIn('does not establish a physical wait duration',
                      row['interpretation'])
        self.decode(fields(4, elapsed_us=0, trb=0, ring=0))

    def test_refused_admission_allows_stale_completion_and_invalid_size(self):
        for dci, requested in ((0, 0), (11, 1025), (16, 201), (31, 0xffffffff)):
            with self.subTest(dci=dci, requested=requested):
                self.decode(fields(1, dci=dci, requested=requested, cc=255,
                                   residue=0xffffff, flags=255))

    def test_unavailable_endpoint_snapshot(self):
        for reason in (3, 4, 5, 6):
            self.decode(fields(reason, endpoint_valid=0, endpoint_state=0,
                               endpoint_dequeue=0))

    def test_uint32_and_uint64_boundaries(self):
        self.decode(fields(4, sequence=(1 << 64) - 1,
                           elapsed_us=(1 << 64) - 1,
                           polls=(1 << 32) - 1,
                           dispatched=(1 << 32) - 1))
        for key in c.KEYS:
            with self.subTest(key=key):
                bits = 64 if key in c.U64_KEYS else 32
                self.refuse(fields(**{key: 1 << bits}))

    def test_boolean_and_field_ranges(self):
        for key, value in [('version', 0), ('version', 2), ('cal_step', 5),
                           ('reason', 7), ('dci', 32), ('flags', 256),
                           ('cc', 256), ('residue', 1 << 24),
                           ('event_start', 64), ('event_end', 64),
                           ('endpoint_state', 8),
                           *((key, 2) for key in c.BOOL_KEYS)]:
            with self.subTest(key=key, value=value):
                self.refuse(fields(**{key: value}))

    def test_sequence_and_first_failure_selection(self):
        self.refuse(fields(sequence=0))
        self.refuse(fields(first_failure=0))
        self.refuse(fields(2, first_failure=1))
        self.refuse(fields(0, first_failure=1))
        self.refuse(fields(0, sequence=1))

    def test_admitted_direction_and_size(self):
        for dci, requested in ((0, 16), (10, 16), (31, 16),
                               (11, 0), (11, 1025), (16, 201)):
            with self.subTest(dci=dci, requested=requested):
                self.refuse(fields(dci=dci, requested=requested))
        self.decode(fields(dci=16, requested=200))
        self.decode(fields(dci=11, requested=1024))

    def test_refused_does_not_claim_published_or_sampled_work(self):
        for key, value in (('trb', 1), ('ring', 1), ('polls', 1),
                           ('dispatched', 1), ('elapsed_us', 1),
                           ('endpoint_valid', 1)):
            with self.subTest(key=key):
                self.refuse(fields(1, **{key: value}))

    def test_counter_and_unrecorded_snapshot_consistency(self):
        self.refuse(fields(polls=1, dispatched=2))
        self.refuse(fields(endpoint_valid=0, endpoint_state=1))
        self.refuse(fields(endpoint_valid=0, endpoint_dequeue=1,
                           endpoint_state=0))
        self.refuse(fields(2, endpoint_valid=1))
        self.refuse(fields(0, cc=1))

    def test_missing_unknown_duplicate_or_reordered_fields(self):
        good = emit(fields())
        variants = [good + ' extra=0', good + ' reason=4',
                    good.replace(' sequence=100', ''),
                    good.replace('cal_step=4 first_failure=1',
                                 'first_failure=1 cal_step=4'),
                    good.replace('requested=', 'unknown=')]
        for text in variants:
            with self.subTest(text=text), self.assertRaises(ValueError):
                c.observe(text)

    def test_duplicate_rows_and_unknown_family_refuse(self):
        good = emit(fields())
        for text in (good + '\n' + good, c.PREFIX + '-future: version=1',
                     c.PREFIX + ':', good + '\n' + c.PREFIX + '-unknown: x=0'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                c.observe(text)

    def test_noncanonical_decimal_hex_and_whitespace_refuse(self):
        good = emit(fields())
        variants = [good.replace('sequence=100', 'sequence=0100'),
                    good.replace('sequence=100', 'sequence=+100'),
                    good.replace('sequence=100', 'sequence=-1'),
                    good.replace('sequence=100', 'sequence=0x64'),
                    good.replace('sequence=100', 'sequence=1.0'),
                    good.replace('sequence=100', 'sequence=١٠٠'),
                    good.replace('trb=0x0000000000100010', 'trb=0x100010'),
                    good.replace('trb=0x0000000000100010', 'trb=0X0000000000100010'),
                    good.replace('trb=0x0000000000100010', 'trb=0x00000000001000AF'),
                    good.replace('trb=0x0000000000100010', 'trb=1048592'),
                    good + ' ', ' ' + good, '\t' + good,
                    good.replace(': ', ':  ', 1), good.replace(' flags=', '\tflags=')]
        for text in variants:
            with self.subTest(text=text), self.assertRaises(ValueError):
                c.observe(text)

    def test_wrong_type_and_oversized_row_refuse(self):
        for text in (None, b'', 1, c.PREFIX + ': ' + 'x' * 1025):
            with self.subTest(text=type(text)), self.assertRaises(ValueError):
                c.observe(text)

    @unittest.skipUnless(os.environ.get('WIFI_MCU_BULK_TRACE'),
                         'WIFI_MCU_BULK_TRACE actual C observation not supplied')
    def test_actual_c_trace(self):
        trace = Path(os.environ['WIFI_MCU_BULK_TRACE']).read_text()
        result = c.observe(trace)
        self.assertEqual(len(result['rows']), 1)
        self.assertTrue(result['rows'][0]['record_valid'])
        self.assertFalse(result['native_accepted'])
        self.assertFalse(result['ownership_proven'])


if __name__ == '__main__':
    unittest.main()
