import base64
from dataclasses import replace
import struct
import unittest

from haswell_layout_proposal import (APERTURE, PAGE, ClaimToken, ReaderToken,
                                     SyntheticReservations, layout_proposal, simulate)
from test_hardware_haswell_backing import fixture
from test_hardware_haswell_tilectl import fixture_v2
from test_hardware_haswell_fences import change_v3, fixture_v3


def captured_fixture():
    text = fixture_v3()
    for phase in (0, 1):
        for index, value in ((17, 0x16661056), (50, 0x00fff02b00000001), (89, 0)):
            text = change_v3(text, index, value, phase)
    return text


class LayoutTests(unittest.TestCase):
    def test_exact_layout_protects_full_fence_without_inventing_physical_extent(self):
        p = simulate(captured_fixture())
        self.assertEqual(p['surface_ggtt'], [0, 4325376])
        self.assertEqual(p['protected_ggtt'], [0, 16 << 20])
        self.assertEqual(p['extra_fence_bytes'], 12451840)
        self.assertEqual(len(p['physical_pages']), 1056)
        self.assertEqual(p['physical_surface'][1] - p['physical_surface'][0], 4325376)
        self.assertEqual(p['native_managed_ranges'], [])
        for field in ('hardware_authority', 'ownership', 'pin', 'physical_acceptance',
                      'extra_fence_physical_pages_known'):
            self.assertFalse(p[field])

    def test_incomplete_older_overlapping_changed_and_other_layouts_refuse(self):
        text = captured_fixture()
        duplicate = text
        for phase in (0, 1):
            duplicate = change_v3(duplicate, 81, (0x41f02b << 32) | 1, phase)
        for bad in (fixture(), fixture_v2(), fixture_v3(True), duplicate,
                    text.replace('completed=yes', 'completed=NO'),
                    text.replace('ownership=0', 'ownership=1'),
                    change_v3(text, 89, 1, 1),
                    change_v3(change_v3(text, 89, 1, 0), 89, 1, 1)):
            with self.assertRaises(ValueError):
                layout_proposal(bad)

    def test_matching_aliased_vectors_are_not_backing_ownership(self):
        text = captured_fixture()
        # Preserve a well-formed receipt with matching vectors and fresh hashes.
        # The receipt allows nonconsecutive observations; the proposal must not.
        lines = text.splitlines()
        for phase in (0, 1):
            index = next(i for i, x in enumerate(lines) if x.startswith(f'hsw-backing-ptes: v=3 phase={phase}'))
            line = lines[index]
            raw = bytearray(base64.b64decode(line.split('data=')[1]))
            raw[4:8] = raw[:4]
            lines[index] = line.split('data=')[0] + 'data=' + base64.b64encode(raw).decode()
            digest = 14695981039346656037
            for byte in raw:
                digest = ((digest ^ byte) * 1099511628211) & ((1 << 64) - 1)
            index = next(i for i, x in enumerate(lines) if x.startswith(f'hsw-backing-meta: v=3 phase={phase}'))
            line = lines[index]
            values = list(struct.unpack('<50Q32Q3Q5I', base64.b64decode(line.split('data=')[1])))
            values[84] = digest
            lines[index] = line.split('data=')[0] + 'data=' + base64.b64encode(struct.pack('<50Q32Q3Q5I', *values)).decode()
        from hardware_haswell_backing import backing_result
        text = '\n'.join(lines) + '\n'
        self.assertEqual(backing_result(text)['status'], 'OBSERVED')
        with self.assertRaisesRegex(ValueError, 'aliased'):
            layout_proposal(text)


class ReservationTests(unittest.TestCase):
    def setUp(self):
        self.c = SyntheticReservations()

    def test_partial_ggtt_overlap_and_physical_alias_are_independent(self):
        self.c.propose([PAGE, 3 * PAGE], [10 * PAGE, 11 * PAGE])
        for ggtt, physical in (([0, 2 * PAGE], [20 * PAGE]),
                               ([2 * PAGE, 4 * PAGE], [20 * PAGE]),
                               ([4 * PAGE, 5 * PAGE], [11 * PAGE])):
            with self.assertRaises(ValueError):
                self.c.propose(ggtt, physical)
        self.c.propose([3 * PAGE, 4 * PAGE], [12 * PAGE])
        self.c.propose([0, PAGE], [9 * PAGE])

    def test_full_inherited_fence_is_protected_beyond_surface(self):
        p = layout_proposal(captured_fixture())
        self.c.propose(p['protected_ggtt'], p['physical_pages'])
        with self.assertRaisesRegex(ValueError, 'GGTT collision'):
            self.c.propose([8 << 20, (8 << 20) + PAGE], [0x100000])
        self.c.propose([16 << 20, (16 << 20) + PAGE], [0x100000])

    def test_busy_reader_close_drain_and_stale_slot_reuse(self):
        t = self.c.propose([0, PAGE], [PAGE])
        r = self.c.hold(t)
        self.c.begin(r)
        with self.assertRaises(ValueError):
            self.c.release(r)
        self.c.close(t)
        for action in (lambda: self.c.hold(t), lambda: self.c.begin(r),
                       lambda: self.c.retire(t, certain=True)):
            with self.assertRaises(ValueError):
                action()
        self.c.end(r)
        self.c.release(r)
        self.c.retire(t, certain=True)
        new = self.c.propose([0, PAGE], [PAGE])
        self.assertEqual(t.slot, new.slot)
        self.assertGreater(new.generation, t.generation)
        for action in (lambda: self.c.close(t), lambda: self.c.hold(t),
                       lambda: self.c.end(r), lambda: self.c.release(r)):
            with self.assertRaises(ValueError):
                action()

    def test_foreign_forged_and_duplicate_tokens_refuse(self):
        t = self.c.propose([0, PAGE], [PAGE])
        other = SyntheticReservations().propose([0, PAGE], [PAGE])
        for bad in (other, replace(t), replace(t, generation=t.generation + 1),
                    ClaimToken(t.catalog, t.generation, -1), None):
            with self.assertRaises(ValueError):
                self.c.close(bad)
        r = self.c.hold(t)
        with self.assertRaises(ValueError):
            self.c.release(ReaderToken(t, r.serial))
        self.c.release(r)
        with self.assertRaises(ValueError):
            self.c.release(r)

    def test_uncertain_retirement_keeps_ranges_and_readers_quarantined(self):
        t = self.c.propose([0, PAGE], [PAGE])
        r = self.c.hold(t)
        self.c.begin(r)
        self.c.close(t)
        self.c.retire(t, certain=False)
        for action in (lambda: self.c.retire(t, certain=True), lambda: self.c.end(r),
                       lambda: self.c.propose([0, PAGE], [2 * PAGE]),
                       lambda: self.c.propose([PAGE, 2 * PAGE], [PAGE])):
            with self.assertRaises(ValueError):
                action()

    def test_capacity_bounds_and_invalid_requests_do_not_consume_slots(self):
        for ggtt, pages in (([0, 0], [PAGE]), ([1, PAGE], [PAGE]),
                            ([0, APERTURE + PAGE], [PAGE]), ([0, PAGE], []),
                            ([0, PAGE], [PAGE, PAGE]), ([0, PAGE], [True]),
                            ([0, PAGE], [1 << 39]), ([0, PAGE], [PAGE + 1])):
            with self.assertRaises(ValueError):
                self.c.propose(ggtt, pages)
        tokens = [self.c.propose([i * PAGE, (i + 1) * PAGE], [(i + 10) * PAGE]) for i in range(8)]
        self.assertEqual(tokens[-1].generation, 8)
        with self.assertRaises(ValueError):
            self.c.propose([8 * PAGE, 9 * PAGE], [20 * PAGE])
        readers = [self.c.hold(tokens[0]) for _ in range(8)]
        with self.assertRaises(ValueError):
            self.c.hold(tokens[0])
        for reader in readers:
            self.c.release(reader)
        self.c._generation = (1 << 64) - 1
        self.c.close(tokens[0])
        self.c.retire(tokens[0], certain=True)
        with self.assertRaises(ValueError):
            self.c.propose([0, PAGE], [PAGE])


if __name__ == '__main__':
    unittest.main()
