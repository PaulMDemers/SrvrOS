"""Companion integrity and real native-C formatter/dispatcher regressions."""
import os
from pathlib import Path
import subprocess
import unittest

import wifi_mcu_bulk_observation as observer
import wifi_mcu_failure_details as details
from test_wifi_mcu_bulk_observation import fields, emit


def fixture():
    bulk = fields(3, flags=30)
    ring = dict(version=1, sequence=bulk['sequence'], valid=1, index=1,
                producer_index=2, producer_cycle=1, expected_parameter=0x200000,
                expected_cycle=1, submitted_parameter=0x200000,
                submitted_status=1024, submitted_control=(1 << 10) | 37,
                link_parameter=bulk['ring'], link_status=0,
                link_control=(6 << 10) | 3)
    events = dict(version=1, sequence=bulk['sequence'], valid=1, slot=1,
                  seen=2, transfers=2, relevant=2, exact=1,
                  samples=2, dropped=0, overflow=0)
    samples = [dict(version=1, sequence=bulk['sequence'], index=0,
                    parameter=bulk['ring'], status=(13 << 24) | 1008,
                    control=(32 << 10) | (11 << 16) | (1 << 24)),
               dict(version=1, sequence=bulk['sequence'], index=1,
                    parameter=bulk['trb'], status=0,
                    control=(32 << 10) | (11 << 16) | (1 << 24))]
    return bulk, ring, events, samples


def line(prefix, row):
    return prefix + ': ' + ' '.join(
        key + '=' + (f'0x{row[key]:016x}' if key in details.HEX else str(row[key]))
        for key in details.SCHEMAS[prefix])


def text(data):
    bulk, ring, events, samples = data
    return '\n'.join([emit(bulk), line(details.RING, ring),
                      line(details.SUMMARY, events)] +
                     [line(details.SAMPLE, s) for s in samples]) + '\n'


def empty(row):
    return {key: value if key in ('version', 'sequence') else 0
            for key, value in row.items()}


class FailureDetailsTests(unittest.TestCase):
    def test_legacy_shape_unchanged(self):
        self.assertNotIn('diagnostics', observer.observe('legacy log\n'))
        self.assertNotIn('diagnostics', observer.observe(emit(fields())))

    def test_complete_snapshot_carries_no_authority(self):
        result = observer.observe(text(fixture()))
        d = result['diagnostics']
        self.assertEqual(d['ring'], fixture()[1])
        self.assertEqual(d['samples'], fixture()[3])
        for key in ('native_accepted', 'ownership_proven',
                    'dma_retirement_proven', 'hardware_event_absence_proven'):
            self.assertFalse(d[key])
        self.assertFalse(result['recovery_authorized'])

    def test_corrupt_descriptor_data_is_preserved(self):
        data = fixture()
        for key in ('submitted_parameter', 'link_parameter'):
            data[1][key] = (1 << 64) - 1
        for key in ('submitted_status', 'submitted_control', 'link_status',
                    'link_control'):
            data[1][key] = (1 << 32) - 1
        self.assertEqual(observer.observe(text(data))['diagnostics']['ring'],
                         data[1])

    def test_success_bad_reply_and_occupied_observer(self):
        bulk, ring, events, samples = fixture()
        bulk.update(fields(2))
        self.assertTrue(observer.observe(text((bulk, empty(ring), events,
                                              samples)))['diagnostics']['events']['valid'])
        bulk, ring, events, samples = fixture()
        self.assertEqual(observer.observe(text((bulk, ring, empty(events),
                                               [])))['diagnostics']['samples'], [])

    def test_unsubmitted_refusal_has_zero_companions(self):
        _, ring, events, _ = fixture()
        result = observer.observe(text((fields(1), empty(ring), empty(events), [])))
        self.assertFalse(result['diagnostics']['ring']['valid'])

    def test_wrong_slot_dci_and_event_data_are_observations(self):
        for slot, dci, ed in ((2, 11, 0), (1, 9, 0), (1, 11, 4)):
            with self.subTest(slot=slot, dci=dci, ed=ed):
                data = fixture()
                data[3][1]['control'] = (32 << 10) | (dci << 16) | (slot << 24) | ed
                data[2]['exact'] = int(slot == 1 and dci == 11)
                self.assertEqual(observer.observe(text(data))['diagnostics']['samples'],
                                 data[3])

    def test_missing_duplicate_reordered_and_unanchored_rows_refuse(self):
        rows = text(fixture()).splitlines()
        variants = [rows[1:], rows[:1] + rows[2:], rows[:2] + rows[3:],
                    rows + [rows[1]], rows[:1] + [rows[2], rows[1]] + rows[3:],
                    rows[1:2] + rows[:1] + rows[2:], rows[:-1],
                    rows[:-1] + rows[-2:-1], rows[:3] + rows[3:][::-1]]
        for candidate in variants:
            with self.subTest(rows=candidate):
                with self.assertRaises(ValueError):
                    observer.observe('\n'.join(candidate) + '\n')

    def test_all_companion_sequences_bound_to_bulk(self):
        for position in (1, 2, 3):
            data = fixture()
            row = data[position] if position != 3 else data[3][0]
            row['sequence'] += 1
            with self.assertRaises(ValueError):
                observer.observe(text(data))

    def test_invalid_ring_snapshot_claims_refuse(self):
        changes = [dict(index=63), dict(producer_index=63), dict(index=0),
                   dict(valid=0), dict(valid=2), dict(expected_cycle=2),
                   dict(submitted_control=1 << 32), dict(link_status=1 << 32)]
        for update in changes:
            data = fixture(); data[1].update(update)
            with self.subTest(update=update), self.assertRaises(ValueError):
                observer.observe(text(data))
        for reason in (0, 1, 2):
            data = fixture(); data[0].update(fields(reason))
            with self.subTest(reason=reason), self.assertRaises(ValueError):
                observer.observe(text(data))
        data = fixture()
        data[0]['ring'] += 1
        data[0]['trb'] += 1
        with self.assertRaises(ValueError):
            observer.observe(text(data))

    def test_inconsistent_event_counts_refuse(self):
        changes = [dict(slot=0), dict(slot=256), dict(valid=2), dict(valid=0),
                   dict(seen=1), dict(transfers=1), dict(exact=3),
                   dict(samples=5), dict(samples=1, dropped=1),
                   dict(dropped=1), dict(overflow=2), dict(overflow=1),
                   dict(seen=1 << 32)]
        for update in changes:
            data = fixture(); data[2].update(update)
            with self.subTest(update=update), self.assertRaises(ValueError):
                observer.observe(text(data))

    def test_bounded_dropped_and_saturated_counts(self):
        bulk, ring, events, samples = fixture()
        samples += [dict(samples[0], index=n) for n in (2, 3)]
        events.update(seen=8, transfers=8, relevant=8, samples=4, dropped=4)
        observer.observe(text((bulk, ring, events, samples)))
        maximum = (1 << 32) - 1
        events.update(seen=maximum, transfers=maximum, relevant=maximum,
                      dropped=maximum, overflow=1)
        self.assertTrue(observer.observe(text((bulk, ring, events,
                                              samples)))['diagnostics']['events']['overflow'])
        data = fixture()
        data[2].update(seen=maximum, overflow=1)
        observer.observe(text(data))
        data[2]['exact'] += 1
        with self.assertRaises(ValueError):
            observer.observe(text(data))

    def test_bad_samples_and_raw_widths_refuse(self):
        for update in (dict(index=9), dict(status=1 << 32),
                       dict(control=39 << 10), dict(parameter=1 << 64),
                       dict(parameter=0x300000, control=(32 << 10) | (9 << 16) | (2 << 24))):
            data = fixture(); data[3][0].update(update)
            with self.subTest(update=update), self.assertRaises(ValueError):
                observer.observe(text(data))

    def test_unknown_schema_canonical_numbers_and_indentation_refuse(self):
        original = text(fixture())
        changes = [original.replace(details.RING + ':', details.RING + '-bad:'),
                   original.replace(details.SUMMARY + ': version=1', details.SUMMARY + ': version=2'),
                   original.replace('producer_index=2', 'producer_index=02'),
                   original.replace('producer_index=2', 'producer_index=-1'),
                   original.replace('expected_parameter=0x0000000000200000', 'expected_parameter=0x200000'),
                   original.replace(details.RING + ':', ' ' + details.RING + ':'),
                   original + 'wifi-mcu-event-unknown: version=1\n',
                   original.replace('link_status=0x0000000000000000', 'link_status=0x0000000000000000 extra=0')]
        for candidate in changes:
            with self.subTest(log=candidate), self.assertRaises(ValueError):
                observer.observe(candidate)

    @unittest.skipUnless(os.environ.get('WIFI_MCU_DETAILS_BINARY'),
                         'actual native-C fixture not supplied')
    def test_actual_c_fragments(self):
        binary = Path(os.environ['WIFI_MCU_DETAILS_BINARY']).resolve()
        for fault in range(16):
            target = 0 if not fault else 9 if fault == 12 else 10
            with self.subTest(fault=fault):
                run = subprocess.run([str(binary), '--trace-cal5-mcu', str(fault), str(target)],
                                     capture_output=True, text=True, check=True, timeout=10)
                result = observer.observe(run.stdout)
                self.assertFalse(result['native_accepted'])
                if fault:
                    self.assertIn('diagnostics', result)
                    self.assertEqual(len(result['rows']), 1)
                    snapshot = result['diagnostics']
                    if fault == 13:
                        self.assertEqual(snapshot['events']['samples'], 4)
                        self.assertEqual(snapshot['events']['dropped'], 2)
                    if fault == 14:
                        self.assertTrue(snapshot['events']['overflow'])
                        self.assertEqual(snapshot['events']['samples'], 4)
                    if fault == 15:
                        self.assertFalse(snapshot['ring']['valid'])
                        self.assertTrue(snapshot['events']['valid'])
                else:
                    self.assertEqual(result['rows'], [])
                    self.assertNotIn('diagnostics', result)


if __name__ == '__main__':
    unittest.main()
