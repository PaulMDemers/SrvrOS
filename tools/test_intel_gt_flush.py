"""Exercise the Broadwell hardware acknowledgement capture and its receipts."""

import base64
import json
import subprocess
import re
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import hardware_boot as boot
from fixture_compile import compile_fixture, recipe
from hardware_desktop import session_log_limit
from hardware_boot_smoke import shared_receipt
from intel_display_backing import backing_result
from intel_gt_bootstrap import bootstrap_result, historical_view
from intel_gt_primary import verify_primary_backing
from intel_gt_flush import MEDIA_WORDS
from intel_gt_park import TIMING_PREFIXES, verify_timing_file
from intel_gt_preflight import gt_result
from test_intel_gt_primary import run
from test_intel_gt_range import RECIPE, ROOT, TESTS
from workflow.gt_range import FLUSH_RECEIPTS
from workflow.runner import trial_parsers

TARGET = "build/tests/intel-gt-flush-adapter"


def observe(mode, position=0, *, entry=None, binary=ROOT / TARGET):
    return run(mode, position, entry=entry, binary=binary)


class FlushAcknowledgements(unittest.TestCase):
    def test_boot_page_preflight_stays_inside_the_pci_hold(self):
        text = observe('pass')
        preflight = bootstrap_result(text)['flush']['boot_page_preflight']
        self.assertTrue(preflight['capture_complete'])
        self.assertEqual(preflight['summary']['slot'], 12292)
        self.assertEqual(preflight['translation']['low'], 0x8f005001)
        self.assertEqual(preflight['widi_go'], [True, True])
        self.assertEqual(preflight['win_clock_gated'], [True, True])
        self.assertTrue(preflight['widi_tlb_empty'])
        self.assertFalse(preflight['write_authority'])
        for vector in (1, 2):
            with self.subTest(vector=vector):
                live = bootstrap_result(observe('boot-page-valid', vector))['flush']['boot_page_preflight']
                self.assertTrue(live['capture_complete'])
                self.assertFalse(live['widi_tlb_empty'])
                self.assertFalse(live['write_authority'])
        for mode, position, stage in (('boot-page-pte-drift', 1, 3), ('boot-page-plane', 3, 3),
                                       ('boot-page-smi', 4, 4), ('boot-page-map', 3, 2),
                                       ('boot-page-alias', 1, 1), ('boot-page-read', 5, 1),
                                       ('boot-page-unmap', 2, 2)):
            with self.subTest(mode=mode):
                fault = bootstrap_result(observe(mode, position))
                self.assertFalse(fault['complete'])
                self.assertEqual(fault['summary']['commands'], 1)
                self.assertEqual(fault['summary']['pci_samples'], 3)
                self.assertEqual(fault['summary']['pte_writes'], 0)
                self.assertEqual(fault['flush']['boot_page_preflight']['summary']['stage'], stage)
        lines = text.splitlines()
        translation = next(line for line in lines if line.startswith('intel-boot-page-translation:'))
        for altered in ('', translation.replace('low=0x8f005001', 'low=0x8f006001')):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(translation, altered))

    def test_samples_and_timing_on_both_original_paths(self):
        for entry in (None, "disabled"):
            for mode, expected in (("pass", [0, 31, 0, 0]), ("flush-quiet", [0, 0, 0, 0]), ("flush-stale", [31, 31, 31, 31])):
                with self.subTest(entry=entry, mode=mode):
                    result = bootstrap_result(observe(mode, entry=entry))
                    self.assertEqual(result["version"], 12)
                    self.assertEqual(result["range"]["slot"]["returned"], 1)
                    self.assertTrue(result["complete"])
                    self.assertFalse(result["hardware_ownership"])
                    self.assertEqual(result["summary"]["obligations"], 255)
                    self.assertEqual(result["summary"]["pte_writes"], 0)
                    flush = result["flush"]
                    self.assertEqual(flush["engine_ack"], expected)
                    self.assertEqual(flush["engine_ack_rise"], 31 if mode == "pass" else 0)
                    self.assertEqual(flush["l3_flush_ack"], list(map(bool, expected)))
                    self.assertEqual(flush["summary"], {
                        "v": 9, "samples": 4, "reads": 100, "maps": 28, "retired": 28, "live": 0,
                    })
                    self.assertEqual(flush["media_outstanding"], [dict.fromkeys(MEDIA_WORDS, 0)] * 4)
                    power = flush["power_controls"]
                    self.assertEqual([p["requested_clients"] for p in power], [0x5f, 0x5f if mode == "flush-quiet" else 0, 0x5f, 0x5f])
                    self.assertEqual([p["sw_target"] for p in power], [4, 4, 4, 4])
                    self.assertEqual([p["rc6_enabled"] for p in power], [True, True, True, True])
                    self.assertTrue(all(p["threshold_ticks"] == 625 for p in power))
                    idle = mode != "flush-quiet"
                    self.assertEqual(flush["gam"], [
                        dict(allowed=0x5f, idle=False), dict(allowed=0 if idle else 0x5f, idle=idle),
                        dict(allowed=0x5f, idle=False), dict(allowed=0x5f, idle=False),
                    ])
                    self.assertEqual(flush["samples"][1]["gam_go"], 0x20 if idle else 0x7f)
                    self.assertEqual(flush["samples"][1]["gam_eco"], 0x201 if idle else 0x200)
                    self.assertEqual(flush["smi_guard"], {
                        "v": 1, "reads": result["summary"]["native_writes"] + 7 + flush["l3_banks"]["summary"]["writes"],
                        "point": 6, "fault": 0, "first": 13, "last": 13,
                    })

    def test_bank_steering_and_restoration(self):
        text = observe("pass")
        bank = bootstrap_result(text)["flush"]["l3_banks"]
        self.assertEqual(bank["summary"]["original"], bank["summary"]["final"])
        self.assertEqual([b["sqc"] for b in bank["banks"]], [0x40400000 | i for i in range(8)])
        self.assertEqual([b["scratch"] for b in bank["banks"]], [1]*7 + [17])
        for mode, limit in (("bank-float", 8), ("bank-power", 8), ("bank-foreign", 1), ("bank-map", 4), ("bank-post", 8), ("bank-alias", 10)):
            for i in range(1, limit + 1):
                with self.subTest(mode=mode, i=i):
                    failed = bootstrap_result(observe(mode, i))
                    self.assertFalse(failed["complete"])
                    self.assertEqual(failed["reset"]["reset_attempts"], 0)
                    if mode in ("bank-float", "bank-power"):
                        self.assertTrue(failed["flush"]["l3_banks"]["summary"]["restored"])
                    if mode == "bank-foreign":
                        self.assertFalse(failed["flush"]["l3_banks"]["summary"]["restored"])
        row = next(l for l in text.splitlines() if l.startswith("intel-l3-bank: v=1 n=7 "))
        for bad in ("", row.replace("n=7", "n=0"), re.sub(r"selector=\S+", "selector=0", row),
                    re.sub(r"scratch=\S+", "scratch=0xffffffff", row)):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, bad))

    def test_independent_slot_survives_reopening_and_post_reset_drift(self):
        healthy = bootstrap_result(observe("pass"))
        self.assertEqual(healthy["range"]["vectors"][1][-3:], [0x8f003001, 0x8f004001, 0x8f005001])
        self.assertEqual(healthy["range"]["independent_slot"], dict(
            v=1, slot=12291, reserved=1, returned=1, reopen_verified=1, pte_writes=0))
        for mode, position in (("command-unknown", 2), ("range-drift", 785),
                               ("range-drift", 788), ("range-drift", 791)):
            with self.subTest(mode=mode, position=position):
                result = bootstrap_result(observe(mode, position))
                slot = result["range"]["independent_slot"]
                self.assertFalse(result["complete"])
                self.assertEqual((slot["reserved"], slot["returned"], slot["reopen_verified"]), (1, 0, 0))
                self.assertEqual(result["range"]["slot"]["returned"], 0)

    def test_efi_trace_is_bound_to_this_boot_and_records_partial_failures(self):
        import air_efi_trace as trace
        release = '0123456789abcdef'
        files = trace.seed(release)
        env = files[trace.GRUB].replace(b'prepared', b'returned')
        probe = bytearray(files[trace.PROBE])
        for i, (name, value) in enumerate((('entered', 0), ('capture', 2 << 32 | 6),
                                           ('locate', 0x800000000000000e)), start=1):
            row = f'{name} {value:016x}\n'.encode()
            probe[i*64:i*64+len(row)] = row
        receipt = trace.decode(env, probe, release)
        self.assertEqual(receipt['probe']['locate'], 0x800000000000000e)
        self.assertNotIn('allocate', receipt['probe'])
        with self.assertRaisesRegex(ValueError, 'identity'):
            trace.decode(env, probe, release, 'different-boot')
        self.assertNotIn('eg2_clients', receipt)
        for row, state in ((b'eg2-open 0 0 0 0 0', 'observed'), (b'eg2-open 2 1 1 1 0', 'observed'),
                           (b'eg2-absent', 'absent'), (b'eg2-unavailable', 'unavailable')):
            probe[384:448] = (row+b'\n').ljust(64, b'\0')
            self.assertEqual(trace.decode(env, probe, release)['eg2_clients']['state'], state)
        for row in (b'eg2-open 1 1 2 0 0', b'eg2-open 1 1 0 1 0', b'eg2-open 0 0 0 0 1', b'eg2-open 0'):
            probe[384:448] = (row+b'\n').ljust(64, b'\0')
            with self.assertRaisesRegex(ValueError, 'EG2 open'):
                trace.decode(env, probe, release)
        probe[384:448] = b'eg2-open 3 1 0 2 0\n'.ljust(64, b'\0')
        for row, expected in (
                ('panel 0003 0001 deadbeefcafef00d 1 80010100', (3, 1, 'deadbeefcafef00d', 0x80010100)),
                ('panel 0003 0003 0000000000000000 0 00000000', (3, 3, None, None)),
                ('panel 0002 0000 0000000000000000 0 00000000', (2, 0, None, None))):
            probe[448:] = (row + '\n').encode().ljust(64, b'\0')
            self.assertEqual(trace.decode(env, probe, release)['eg2_panel'],
                             dict(state='observed', **dict(zip(('gop_mask', 'active_mask', 'path_hash', 'acpi_adr'), expected))))
        for row in ('panel 0004 0000 0000000000000000 0 00000000', 'panel 0001 0002 0000000000000000 0 00000000',
                    'panel 0003 0003 deadbeefcafef00d 0 00000000', 'panel 0001 0001 0000000000000000 0 80010100'):
            probe[448:] = (row + '\n').encode().ljust(64, b'\0')
            with self.assertRaisesRegex(ValueError, 'panel'):
                trace.decode(env, probe, release)
        probe[448:] = b'panel-unavailable\n'.ljust(64, b'\0')
        self.assertEqual(trace.decode(env, probe, release)['eg2_panel'], {'state': 'unavailable'})
        probe[448:] = bytes(64)
        probe[384:448] = bytes(64)
        probe[128:192] = bytes(64)
        with self.assertRaisesRegex(ValueError, 'sequence'):
            trace.decode(env, probe, release)

    def test_gop_routes_compare_each_console_instance_and_child(self):
        import air_efi_trace as trace
        import struct
        import zlib

        pci = bytes.fromhex('010106000002')
        vendor = bytes.fromhex('030a1400') + bytes(16)
        adr = bytes.fromhex('020308001f000000')
        end = bytes.fromhex('7fff0400')
        instance = bytes.fromhex('7f010400')
        active = pci + vendor + end

        def row(handle, path, state=0):
            return struct.pack('<BBH', handle, state, len(path)) + path

        def packet(rows):
            body = bytes([1, 6]) + b''.join(rows)
            return b'GOP1' + struct.pack('<I', zlib.crc32(body)) + body

        rows = [row(0, pci + adr + instance + active), row(1, b'', 1), row(2, active),
                row(3, pci + end), row(2, active), row(4, pci + adr + end),
                row(5, pci + vendor + adr + end), row(6, b'', 1), row(7, b'', 2)]
        report = trace._decode_routes(packet(rows), 6)
        comparisons = report['comparisons']['gop0']
        self.assertEqual([r['relation'] for r in comparisons['ConOut']], ['diverged', 'exact'])
        self.assertEqual([comparisons[f'child{i}'][0]['relation'] for i in range(4)],
                         ['other-is-ancestor', 'exact', 'diverged', 'gop-is-ancestor'])
        self.assertEqual(comparisons['child2'][0]['common_nodes'], 1)
        self.assertEqual(report['paths']['child1']['handle'], report['paths']['gop0']['handle'])
        self.assertEqual(report['paths']['child4']['state'], 'absent')
        self.assertEqual(report['paths']['child5']['state'], 'unavailable')
        for broken in (packet(rows)[:-1], packet(rows[:-1]), packet(rows + rows[:1]),
                       packet(rows[:4] + [row(2, pci + end)] + rows[5:]),
                       packet([row(0, pci + instance)] + rows[1:])):
            with self.assertRaises(ValueError):
                trace._decode_routes(broken, 6)
        with self.assertRaises(ValueError):
            trace._decode_routes(packet(rows), 5)

    def test_per_engine_prepare_messages(self):
        text = observe("pass")
        steps = bootstrap_result(text)['flush']['prepare_messages']
        self.assertEqual(len(steps), 5)
        self.assertTrue(all(not s['observed_closed'] and not s['fresh_engine_ack'] for s in steps[:4]))
        self.assertTrue(steps[-1]['observed_closed'] and steps[-1]['fresh_l3_under_block'])
        self.assertEqual(len(steps[-1]['fresh_engine_ack']), 5)
        quiet = bootstrap_result(observe("flush-quiet"))['flush']['prepare_messages']
        self.assertTrue(all(not s['observed_closed'] and not s['fresh_l3_under_block'] for s in quiet))
        missing = next(line for line in text.splitlines() if line.startswith('intel-gpm-step: v=2 n=2 '))
        with self.assertRaisesRegex(ValueError, 'Preparation message'):
            bootstrap_result(text.replace(missing, ""))

    def test_idle_policy_spans_reset(self):
        text = observe('pass')
        idle = bootstrap_result(text)['flush']['idle_policy']
        keys = ('wake_rate', 'evaluation', 'hysteresis', 'sleep', 'rcs', 'vcs0', 'vecs', 'vcs1', 'bcs')
        self.assertEqual([[p[k] for k in keys] for p in idle],
                         [list(range(1, 10)), list(range(10, 19))])
        rows = [line for line in text.splitlines() if line.startswith('intel-idle-policy:')]
        for replacement in ('', rows[0], rows[1].replace('bcs=0x12', 'bcs=0xffffffff'),
                            re.sub(r'end=\d+', 'end=1', rows[1])):
            with self.assertRaisesRegex(ValueError, 'Idle policy'):
                bootstrap_result(text.replace(rows[1], replacement))
        legacy = '\n'.join(line for line in text.splitlines() if line not in rows)
        with self.assertRaisesRegex(ValueError, 'MAX_IDLE'):
            bootstrap_result(legacy)
        for mode in ('idle-float', 'idle-read'):
            for pos in range(1, 19):
                result = bootstrap_result(observe(mode, pos))
                self.assertFalse(result['complete'])
                self.assertEqual(len(result['flush'].get('idle_policy', [])), int(pos > 9))

    def test_cold_rc6_windows(self):
        text = observe('pass')
        result = bootstrap_result(text)
        cold = result['flush']['cold_rc6']
        self.assertEqual(cold['delta_ticks_mod32'], [1256, 1000])
        rows = [line for line in text.splitlines() if line.startswith('intel-air-rc6:')]
        self.assertEqual(len(rows), 9)
        self.assertEqual(set(cold['idle_intervals']), {
            'max_idle_10_20ms', 'max_idle_10_100ms', 'restored_audio_released_20ms', 'restored_audio_released_100ms'})
        self.assertGreaterEqual(cold['idle_intervals']['max_idle_10_100ms']['elapsed_us'], 100000)
        changes = ('intel-idle-change:', 'intel-idle-engine:', 'intel-park:', 'intel-park-frame:')
        v1 = '\n'.join(line for line in text.splitlines() if not line.startswith(changes))
        for i, row in enumerate(rows):
            v1 = v1.replace(row, row.replace('v=3', 'v=1') if i < 4 else '')
        self.assertEqual(bootstrap_result(v1)['flush']['cold_rc6']['version'], 1)
        with self.assertRaises(ValueError):
            bootstrap_result(text.replace(rows[7], ''))
        with self.assertRaises(ValueError):
            bootstrap_result(text.replace(rows[6], rows[6].replace('v=3', 'v=1')))
        for replacement in ('', rows[2], rows[3].replace('ack=0', 'ack=0x1'),
                            rows[3].replace('core=0x3', 'core=0xffffffff'), rows[3].replace('live=0', 'live=0x1000'),
                            re.sub(r'begin=\d+', f"begin={cold['samples'][2]['end']}", rows[3])):
            with self.subTest(replacement=replacement), self.assertRaises(ValueError):
                bootstrap_result(text.replace(rows[3], replacement))
        for at in (0, 2):
            point = cold['samples'][at]
            clock = result['binding']['gt'] if at == 0 else result['timing']['gt_end'] - 2
            forged = rows[at].replace(f"begin={point['begin']} end={point['end']}",
                                      f"begin={clock} end={clock+1}")
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(rows[at], forged))
        legacy = '\n'.join(line for line in text.splitlines() if line not in rows)
        legacy = '\n'.join(line for line in legacy.splitlines() if not line.startswith(changes))
        self.assertNotIn('cold_rc6', bootstrap_result(legacy)['flush'])
        v2 = '\n'.join(line for line in text.splitlines() if not line.startswith(changes))
        for i, row in enumerate(rows):
            old = row.replace('v=3', 'v=2').replace(f'n={i} ', f'n={i-1} ') if i > 5 else row.replace('v=3', 'v=2')
            v2 = v2.replace(row, '' if i == 5 else old)
        self.assertEqual(bootstrap_result(v2)['flush']['cold_rc6']['version'], 2)

    def test_software_park_evidence(self):
        positive = observe('park-ack')
        for text, expected in ((positive, True), (observe('pass'), False)):
            park = bootstrap_result(text)['flush']['park']
            self.assertEqual(park, json.loads(json.dumps(park)))
            self.assertTrue(park['complete'])
            self.assertEqual(park['powered_block_observed'], expected)
            self.assertEqual(park['fresh_engine_mask'], 31 if expected else 0)
            self.assertEqual(park['fresh_l3'], expected)
            self.assertFalse(park['hardware_ownership'])
        for mode in ('park-early-l3', 'park-panel-drift'):
            early = bootstrap_result(observe(mode))['flush']['park']
            self.assertFalse(early['complete'])
            self.assertEqual(early['summary']['reopened'], 1)
        header = next(l for l in positive.splitlines() if l.startswith('intel-park:'))
        registered = bootstrap_result(positive)['flush']
        checked = registered['park']
        self.assertLessEqual(checked['frames'][-1]['end'], checked['summary']['returned_us'])
        self.assertLessEqual(checked['summary']['prepare_us'], registered['stop_messages']['begin'])
        for key, stamp in (('returned_us', checked['frames'][-1]['end'] - 1),
                           ('prepare_us', registered['stop_messages']['begin'] + 1)):
            with self.assertRaises(ValueError):
                bootstrap_result(positive.replace(header, re.sub(key + r'=\d+', f'{key}={stamp}', header)))
        row = next(l for l in positive.splitlines() if l.startswith('intel-park-frame: v=1 n=2 '))
        data = bytearray(base64.b64decode(row.split('data=')[1]))
        for offset, value in ((4, 0), (5, 0x10), (6, 0x10)):
            mutated = data.copy()
            mutated[offset*4] = value
            replacement = row.split('data=')[0] + 'data=' + base64.b64encode(mutated).decode()
            with self.assertRaises(ValueError):
                bootstrap_result(positive.replace(row, replacement))
        for before, after in ((row, ''), ('held=2 reopened=1','held=0 reopened=1'),
                              ('final_ctl=0x40000','final_ctl=0x90040000')):
            with self.assertRaises(ValueError):
                bootstrap_result(positive.replace(before, after))

    def test_park_firmware_interference_stops_before_preparation(self):
        for at in range(1, 5):
            result = bootstrap_result(observe('park-smi', at))
            self.assertFalse(result['complete'])
            self.assertFalse(result['flush']['park']['complete'])
            self.assertEqual(result['reset']['reset_writes'], 0)
            self.assertEqual(result['flush']['bcs_stop']['writes'], 0)

    def test_park_spaced_poll_window(self):
        text = observe('park-late-ack')
        park = bootstrap_result(text)['flush']['park']
        origin = park['summary']['poll_origin_us']
        polls = [f for f in park['frames'] if f['op'] == 1]
        self.assertEqual([(f['begin'] - origin) // 1250 for f in polls], list(range(8)))
        self.assertEqual(park['fresh_engine_mask'], 31)
        self.assertTrue(park['fresh_l3'] and park['powered_block_observed'])
        self.assertEqual(park['summary']['held'], 2)
        self.assertLessEqual(park['frames'][-2]['end'] - origin, 10000)
        final_poll = polls[-1]
        stamp = f"op=1 begin={final_poll['begin']} end={final_poll['end']}"
        with self.assertRaises(ValueError):
            bootstrap_result(text.replace(stamp, f"op=1 begin={origin + 8749} end={final_poll['end']}"))
        for mode in ('park-missed-slot', 'park-wait-failed'):
            partial = bootstrap_result(observe(mode))['flush']['park']
            self.assertFalse(partial['complete'] or partial['powered_block_observed'])
            self.assertEqual(partial['summary']['reopened'], 1)
            self.assertEqual(partial['summary']['returns'], 2)

    def test_park_forcewake_pause_and_restore(self):
        text = observe('park-cold')
        park = bootstrap_result(text)['flush']['park']
        self.assertTrue(park['complete'])
        self.assertFalse(park['powered_block_observed'] or park['fresh_l3'])
        self.assertEqual(park['fresh_engine_mask'], 0)
        cold = park['cold']
        self.assertEqual(cold['residency_delta_mod32'], 4000)
        self.assertEqual(cold['wake']['writes'], 2)
        wake = cold['wake']
        self.assertEqual(wake['v'], 2)
        self.assertLessEqual(wake['write_enter_us'], wake['store_us'])
        self.assertLessEqual(wake['store_us'], wake['write_exit_us'])
        self.assertLessEqual(wake['write_exit_us'], wake['ack_clear_us'])
        self.assertLessEqual(wake['ack_clear_us'], wake['guard_exit_us'])
        self.assertLessEqual(cold['samples'][1]['end'], cold['wake']['rewake_us'])
        self.assertEqual([t['failure'] for t in cold['sample_clocks']], [0, 0])
        self.assertEqual(cold['rewake']['close'], wake['rewake_us'])
        self.assertLessEqual(cold['samples'][1]['end'], cold['rewake']['write_enter'])
        for row in (l for l in text.splitlines()
                    if l.startswith(('intel-park-rc6-clock:', 'intel-park-rewake:', 'intel-park-guard:'))):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(row + '\n', ''))
        v6 = re.sub(r'^intel-park-guard: .*\n', '', text, flags=re.M).replace('intel-park: v=7 ', 'intel-park: v=6 ')
        self.assertTrue(bootstrap_result(v6)['flush']['park']['complete'])
        legacy = re.sub(r'^intel-park-(?:rc6-clock|rewake): .*\n', '', v6, flags=re.M)
        old = bootstrap_result(legacy.replace('intel-park: v=6 ', 'intel-park: v=5 '))['flush']['park']
        self.assertTrue(old['complete'])
        origin = park['summary']['poll_origin_us']
        self.assertGreaterEqual(cold['samples'][1]['begin'], origin + 5000)
        polls = [frame for frame in park['frames'] if frame['op'] == 1]
        self.assertEqual([(frame['begin'] - origin) // 1250 for frame in polls], [6, 7])
        with self.assertRaises(ValueError):
            bootstrap_result(text.replace('attempted=2 writes=2 rewake_us=', 'attempted=2 writes=1 rewake_us='))
        fault = bootstrap_result(observe('park-cold-float'))['flush']['park']
        self.assertFalse(fault['complete'])
        self.assertIsNone(fault['cold']['residency_delta_mod32'])
        self.assertEqual(fault['cold']['wake']['writes'], 2)
        self.assertGreater(fault['cold']['wake']['rewake_us'], 0)
        self.assertEqual(fault['summary']['returns'], 2)
        self.assertEqual([t['failure'] for t in fault['cold']['sample_clocks']], [0, 4])
        late_sample = bootstrap_result(observe('park-cold-sample-over'))['flush']['park']
        self.assertFalse(late_sample['complete'])
        self.assertEqual(late_sample['cold']['samples'], [])
        clock, = late_sample['cold']['sample_clocks']
        self.assertEqual(clock['failure'], 6)
        self.assertGreater(clock['end'], late_sample['summary']['poll_origin_us'] + 10000)
        self.assertGreater(late_sample['cold']['rewake']['close'], clock['end'])
        self.assertTrue(late_sample['summary']['returned'])
        retained = bootstrap_result(observe('park-cold-rewake'))
        park = retained['flush']['park']
        self.assertFalse(park['complete'])
        self.assertEqual(park['cold']['wake']['rewake_us'], 0)
        self.assertEqual(retained['reset']['reset_writes'], 0)
        late = bootstrap_result(observe('park-cold-release-late'))
        park = late['flush']['park']
        self.assertEqual(park['cold']['deadline_phase'], 'closing_guard')
        self.assertFalse(park['complete'])
        self.assertEqual(late['reset']['reset_writes'], 0)

    def test_park_guards_fit_measured_census_cost_and_reject_drift(self):
        result = bootstrap_result(observe('park-cold-cost'))
        park = result['flush']['park']
        cold = park['cold']
        origin = park['summary']['poll_origin_us']
        guards = cold['guard']
        self.assertTrue(park['complete'])
        self.assertEqual(len(cold['samples']), 2)
        self.assertGreaterEqual(guards['entry'] - origin, 1361)
        self.assertGreaterEqual(guards['release'] - cold['wake']['ack_clear_us'], 1361)
        self.assertGreaterEqual(guards['rewake'] - cold['rewake']['ack_c0'], 1361)
        self.assertLessEqual(cold['rewake']['close'] - origin, 7500)
        self.assertEqual(guards['fallback'], 0)
        for mode in ('park-cold-window-smi', 'park-cold-exit-pte'):
            with self.subTest(mode=mode):
                failed = bootstrap_result(observe(mode))
                park = failed['flush']['park']
                self.assertFalse(park['complete'] or park['summary']['returned'])
                self.assertEqual(failed['reset']['reset_writes'], 0)
                self.assertEqual(park['cold']['guard']['rewake'], 0)

    def test_air_post_reset_baseline_refuses_software_park(self):
        flush = bootstrap_result(observe('park-reset-reasons'))['flush']
        park = flush['park']
        self.assertFalse(park['complete'])
        self.assertFalse(park['powered_block_observed'])
        self.assertEqual([(f['op'], tuple(f['words'])) for f in park['frames']],
                         [(0, (0x7fff, 0x7f, 0))])
        self.assertEqual(park['summary']['attempts'], 0)
        self.assertEqual(flush['idle_change']['summary']['returned'], 31)

    def test_powered_rc1e_request_distinguishes_fresh_acks_from_readback(self):
        text = observe('park-rc1e')
        result = bootstrap_result(text)['flush']['park']
        self.assertTrue(result['complete'])
        self.assertFalse(result['powered_block_observed'])
        self.assertEqual(result['summary']['set_target'], 0x20000)
        self.assertEqual(result['summary']['restore_target'], 0x40000)
        self.assertEqual(result['rc1e'],
                         dict(v=1, reads=2, cpd_before=0xc5280000, cpd_after=0xc5280000))
        self.assertEqual(len([frame for frame in result['frames'] if frame['op'] == 1]), 8)

        acknowledged = observe('park-rc1e-ack')
        result = bootstrap_result(acknowledged)['flush']['park']
        self.assertTrue(result['complete'] and result['powered_block_observed'])
        self.assertEqual((result['fresh_engine_mask'], result['fresh_l3'], result['summary']['held']),
                         (31, True, 2))
        stuck = bootstrap_result(observe('park-rc1e-stale-closed'))['flush']['park']
        self.assertTrue(stuck['complete'] and stuck['summary']['returned'])
        self.assertEqual((stuck['fresh_engine_mask'], stuck['fresh_l3'], stuck['powered_block_observed']),
                         (31, False, False))
        # The second poll says that GAM and WiDi are closed and L3 pulses.
        # A WiDi reopening just before or after the L3 read invalidates it.
        pulse_row = next(row for row in acknowledged.splitlines()
                         if row.startswith('intel-park-frame: v=1 n=2 op=1 '))
        data = pulse_row.split(' data=')[1]
        for offset in (3, 5):
            raw = bytearray(base64.b64decode(data))
            raw[4 * offset] = 0x10
            forged = acknowledged.replace(pulse_row, pulse_row.replace(data, base64.b64encode(raw).decode()))
            with self.subTest(widi_word=offset), self.assertRaises(ValueError):
                bootstrap_result(forged)
        for fault in ('park-rc1e-stale', 'park-rc1e-cpd-drift'):
            with self.subTest(fault=fault):
                result = bootstrap_result(observe(fault))['flush']['park']
                self.assertFalse(result['complete'] or result['powered_block_observed'])
                self.assertTrue(result['summary']['returned'] and result['summary']['reopened'])
        full = (text + '\nlaptop-capture: boot options=test\nlaptop-capture: completed=yes\n').encode()
        compact = b''.join(row for row in full.splitlines(keepends=True) if row.startswith(TIMING_PREFIXES))
        verify_timing_file(compact, full)
        row = next(line for line in compact.splitlines(keepends=True) if line.startswith(b'intel-park-rc1e: '))
        with self.assertRaises(ValueError):
            verify_timing_file(compact.replace(row, b''), full)
        for altered in ('set_target=0x40000', 'attempted=1', 'cpd_after=0xc7280000'):
            key = altered.split('=')[0]
            with self.subTest(altered=altered), self.assertRaises(ValueError):
                bootstrap_result(re.sub(rf'(?m)(^intel-park[^\n]*\s){key}=[^\s]+',
                                        rf'\g<1>{altered}', text, count=1))

    def test_pre_reset_vcs1_idle_override_and_rollback(self):
        text = observe('park-cold-vcs1')
        park = bootstrap_result(text)['flush']['park']
        self.assertTrue(park['complete'])
        self.assertFalse(park['powered_block_observed'] or park['hardware_ownership'])
        vcs1 = park['vcs1']
        self.assertEqual([vcs1[k] for k in ('saved', 'set', 'at_return', 'returned')], [64, 10, 10, 64])
        origin = park['summary']['poll_origin_us']
        self.assertLessEqual(vcs1['apply'], origin)
        self.assertLessEqual(park['cold']['rewake']['close'] - origin, 7500)
        self.assertLessEqual(vcs1['restore'], park['summary']['prepare_us'])

        full = (text + '\nlaptop-capture: boot options=test\nlaptop-capture: completed=yes\n').encode()
        compact = b''.join(row for row in full.splitlines(keepends=True) if row.startswith(TIMING_PREFIXES))
        verify_timing_file(compact, full)
        row = next(line for line in compact.splitlines(keepends=True) if line.startswith(b'intel-park-vcs1: '))
        with self.assertRaises(ValueError):
            verify_timing_file(compact.replace(row, b''), full)
        for altered in ('', row.decode().replace('returned=64', 'returned=10'),
                        row.decode().replace(f"apply={vcs1['apply']}", f'apply={origin + 1}')):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(row.decode(), altered))

        for mode, attempted, returned in (('baseline', False, False), ('apply', True, True),
                                          ('drift', True, True), ('restore', True, False)):
            with self.subTest(mode=mode):
                result = bootstrap_result(observe('park-cold-vcs1-' + mode))
                failed = result['flush']['park']
                self.assertFalse(failed['complete'])
                self.assertEqual(bool(failed['vcs1']['apply']), attempted)
                self.assertEqual(bool(failed['vcs1']['restore']), returned)
                if mode == 'restore':
                    self.assertEqual(result['reset']['reset_writes'], 0)

    def test_immediate_forcewake_packet_cannot_grant_traffic_ownership(self):
        text = observe('park-cold-vcs1-edge')
        park = bootstrap_result(text)['flush']['park']
        edge = park['edge']
        self.assertTrue(park['complete'] and edge['complete'])
        self.assertEqual(edge['words'], [0, 0, 0, 0, 1, 0, 32, 32, 32, 32, 32, 0, 0])
        self.assertLessEqual(park['cold']['rewake']['ack_c0'], edge['begin'])
        self.assertLessEqual(edge['end'] - park['cold']['rewake']['ack_c0'], 250)
        self.assertLessEqual(edge['end'], park['cold']['guard']['rewake'])
        self.assertLessEqual(park['cold']['rewake']['close'] - park['summary']['poll_origin_us'], 7500)
        self.assertFalse(park['hardware_ownership'] or park['powered_block_observed'] or
                         park['fresh_engine_mask'] or park['fresh_l3'])
        row = next(line for line in text.splitlines(keepends=True) if line.startswith('intel-park-edge: '))
        for altered in ('', row.replace(f"begin={edge['begin']}", 'begin=0'),
                        row.replace(f"end={edge['end']}", f"end={park['cold']['guard']['rewake'] + 1}")):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, altered))
        full = (text + '\nlaptop-capture: boot options=test\nlaptop-capture: completed=yes\n').encode()
        compact = b''.join(line for line in full.splitlines(keepends=True) if line.startswith(TIMING_PREFIXES))
        verify_timing_file(compact, full)
        with self.assertRaises(ValueError):
            verify_timing_file(compact.replace(row.encode(), b''), full)

        for fault in ('float', 'late', 'smi'):
            with self.subTest(fault=fault):
                result = bootstrap_result(observe('park-cold-vcs1-edge-' + fault))
                failed = result['flush']['park']
                self.assertFalse(failed['edge']['complete'] or failed['complete'])
                self.assertTrue(failed['edge']['begin'])
                self.assertEqual(bool(failed['summary']['returned']), fault != 'smi')
                if fault == 'smi':
                    self.assertEqual(result['reset']['reset_writes'], 0)

    def test_max_idle_rollback(self):
        text = observe('pass')
        change = bootstrap_result(text)['flush']['idle_change']
        self.assertEqual([r['saved'] for r in change['engines'].values()], list(range(14, 19)))
        row = next(line for line in text.splitlines() if line.startswith('intel-idle-engine:'))
        for replacement in ('', row.replace('applied=0xa', 'applied=0xb'), row.replace('restored=0xe', 'restored=0xa')):
            with self.assertRaisesRegex(ValueError, 'MAX_IDLE'):
                bootstrap_result(text.replace(row, replacement))
        for mode in ('idle-capture-mismatch', 'idle-apply-mismatch', 'idle-restore-mismatch', 'idle-drift'):
            for pos in range(1, 6):
                with self.subTest(mode=mode, pos=pos):
                    raw = observe(mode, pos)
                    self.assertIn('complete=0 engine_reads=', raw)
                    self.assertIn('intel-idle-change:', raw)

    def test_missing_cold_sample_releases_display(self):
        for mode, position in [('cold-float', n) for n in range(1, 10)] + [('cold-surface', 4), ('cold-surface', 7)]:
            with self.subTest(mode=mode, position=position):
                result = subprocess.run([ROOT / TARGET, mode, str(position)], check=True,
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.stdout.count('intel-air-rc6:'), position - 1)

    def test_stop_blitter_through_prepare_and_reset(self):
        text = observe('pass')
        stop = bootstrap_result(text)['flush']['bcs_stop']
        self.assertEqual(bootstrap_result(text)['flush']['stop_messages']['after_prepare'], 'bcs-stop')
        self.assertEqual((stop['before'], stop['stopped'], stop['at_reset'], stop['after']),
                         (0x200, 0x300, 0x300, 0x200))
        for mode, pos, writes, pulses in (('stop-ring-active',1,0,0), ('stop-parser-busy',1,0,0),
                ('stop-readback',1,1,0), ('stop-readback',4,1,0), ('stop-lost-at-reset',1,1,0),
                ('stop-reset-sticky',1,1,2)):
            with self.subTest(mode=mode, position=pos):
                result = bootstrap_result(observe(mode,pos))
                self.assertFalse(result['complete'])
                self.assertEqual(result['flush']['bcs_stop']['verified'], 0)
                self.assertEqual(result['flush']['bcs_stop']['writes'], writes)
                self.assertEqual(result['reset']['reset_writes'], pulses)
        for field in ('at_reset=0x300','after=0x200','verified=1','ctl=0xf000'):
            with self.subTest(field=field), self.assertRaises(ValueError):
                lines = [line.replace(field,field.split('=')[0]+'=0') if line.startswith('intel-bcs-stop:')
                         else line for line in text.splitlines()]
                bootstrap_result('\n'.join(lines))

    def test_same_boot_efi_handoff(self):
        subprocess.run([ROOT / TARGET, "efi-import"], check=True, timeout=10)
        text = observe("pass")
        proof = bootstrap_result(text)["range"]
        self.assertEqual(proof["efi_handoff"]["summary"]["ram"], proof["command_slots"]["ram"])
        for old, new in (("table=1 state=2", "table=0 state=2"),
                         ("protocols=0 files=0", "protocols=1 files=0"),
                         ("slot=11750 pte=", "slot=11751 pte="),
                         ("backing=1 drivers=", "backing=0 drivers=")):
            with self.subTest(field=old), self.assertRaises(ValueError):
                bootstrap_result(text.replace(old, new))
        legacy = re.sub(r"^intel-(?:efi-|air-rc6:|idle-change:|idle-engine:|park:|park-frame:)[^\n]*\n", "", text, flags=re.M)
        legacy = legacy.replace("intel-command-slots: v=2 ", "intel-command-slots: v=1 ").replace("boot_returned=", "freed=")
        self.assertNotIn("efi_handoff", bootstrap_result(legacy)["range"])

    def test_command_slots_span_reset_and_reopening(self):
        good = bootstrap_result(observe("pass"))["range"]["command_slots"]
        self.assertEqual((good["samples"], good["returned"], good["device_grant"]), (3, 1, 0))
        for phase in range(3):
            for page in (0, 6):
                result = bootstrap_result(observe("command-slot-drift", phase*21+3*page+2))
                slots = result["range"]["command_slots"]
                self.assertEqual((slots["samples"], slots["returned"]), (phase, 0))
                self.assertFalse(result["complete"])

    def test_command_aperture_lifetime_and_drift(self):
        text = observe("pass")
        slots = bootstrap_result(text)["range"]["command_slots"]
        fields = ("mapped", "pinned", "observations", "retired")
        self.assertEqual(tuple(slots[k] for k in fields), (7, 7, 2, 7))
        for mode, index, expected in (("command-aperture-map", 7, (6, 0, 0, 0)),
                                      ("command-aperture-pin", 7, (7, 6, 0, 0)),
                                      ("command-aperture-drift", 7, (7, 7, 1, 0)),
                                      ("command-aperture-unmap", 7, (7, 7, 2, 6))):
            with self.subTest(mode=mode):
                result = bootstrap_result(observe(mode, index))
                slots = result["range"]["command_slots"]
                self.assertFalse(result["complete"])
                self.assertEqual(tuple(slots[k] for k in fields), expected)
        for value in ("observations=2", "retired=7", "pinned=7"):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(value, value[:-1] + "0"))

    def test_widi_clock_samples_and_historical_receipts(self):
        text = observe("pass")
        rows = bootstrap_result(text)["range"]["widi_messages"]
        self.assertEqual([r["clock_ack"] for r in rows], [0x7f]*4)
        self.assertEqual(rows[0]["win"], 0x1f)
        with self.assertRaises(ValueError):
            bootstrap_result(text.replace("clock_before=0x7f", "clock_before=0xffffffff"))
        legacy = re.sub(r" clock_before=\w+ clock_ack=\w+ clock_after=\w+", "", text)
        legacy = legacy.replace("intel-widi-device: v=2", "intel-widi-device: v=1")
        with self.assertRaises(ValueError):
            bootstrap_result(legacy)
        legacy = re.sub(r"^intel-boot-page-.*\n", "", legacy, flags=re.M)
        old = bootstrap_result(legacy.replace("intel-range-summary: v=8", "intel-range-summary: v=7"))
        self.assertTrue(old["complete"])
        self.assertNotIn("clock_ack", old["range"]["widi_messages"][0])

    def test_command_backing_fault_keeps_original_reservation(self):
        for phase in range(3):
            result = bootstrap_result(observe("command-ram-alias", phase+1))
            slots = result["range"]["command_slots"]
            self.assertEqual((slots["samples"], slots["reserved"], slots["returned"], slots["boot_returned"]),
                             (phase, 1, 0, 0))
            self.assertFalse(result["complete"])

    def test_queue_changes_hold_the_slot_before_and_after_requester_reopening(self):
        text = observe("pass")
        self.assertEqual([row["matched"] for row in bootstrap_result(text)["range"]["queue_handoff"]], [1,1])
        for phase, mode in enumerate(("queue-before", "queue-after")):
            for engine in range(5):
                with self.subTest(phase=phase, engine=engine):
                    failed = observe(mode, engine + 1)
                    result = bootstrap_result(failed)
                    self.assertFalse(result["complete"])
                    self.assertEqual(result["summary"]["commands"], phase + 1)
                    self.assertEqual(result["range"]["independent_slot"]["returned"], 0)
                    self.assertEqual(result["range"]["widi_guard"]["closed"], 0)
                    self.assertEqual(result["range"]["queue_handoff"][-1]["matched"], 0)
                    with self.assertRaises(ValueError):
                        bootstrap_result(failed.replace(f"phase={phase} matched=0", f"phase={phase} matched=1"))
        for line in text.splitlines():
            if line.startswith("intel-slot-queues:"):
                with self.assertRaises(ValueError):
                    bootstrap_result(text.replace(line, ""))

    def test_profile_receipt_requirements_match_current_adapter(self):
        observed = bootstrap_result(observe("pass"))["range"]
        self.assertEqual(FLUSH_RECEIPTS, dict(
            independent_slot=observed["independent_slot"]["slot"],
            slot_backing_version=observed["slot_backing"]["v"],
            slot_widi_version=observed["widi_guard"]["v"],
            slot_reopened_version=observed["reopened_pte"]["v"],
            command_slots_version=observed["command_slots"]["v"], range_version=observed["summary"]["v"],
        ))

    def test_pipe_a_writer_remains_exclusive_across_pci_reopen(self):
        text = observe('pipe-writer')
        result = bootstrap_result(text)
        writer = result['flush']['pipe_writer']
        self.assertTrue(writer['complete'])
        self.assertFalse(result['hardware_ownership'])
        self.assertEqual(writer['frame_delta'], 1)
        self.assertEqual([row['frame'] for row in writer['frames']], [0xffffffff, 0])
        self.assertEqual([writer['summary'][key] for key in ('stage', 'writable', 'writes')], [6, 0, 0])
        for before, after in (('stage=6', 'stage=5'), ('writable=0 writes=0', 'writable=0 writes=1'),
                              ('live=0', 'live=0x1000')):
            rows = text.splitlines(keepends=True)
            wrong = ''.join(line.replace(before, after) if line.startswith('intel-pipe-a-writer') else line for line in rows)
            self.assertTrue(wrong != text, before)
            with self.assertRaises(ValueError):
                bootstrap_result(wrong)

        for fault, stage, commands in (('admit', 1, 1), ('drift', 3, 2), ('smi', 3, 2), ('return', 4, 2), ('protect', 5, 2)):
            with self.subTest(fault=fault):
                failed = bootstrap_result(observe('pipe-writer-' + fault, 1))
                writer = failed['flush']['pipe_writer']
                self.assertFalse(writer['complete'])
                self.assertEqual(writer['summary']['stage'], stage)
                self.assertEqual(failed['summary']['commands'], commands)
                self.assertEqual(writer['summary']['returned'], 0)

    def test_inherited_scanout_guard_covers_image_and_tail_before_reset(self):
        text = observe('pipe-writer')
        result = bootstrap_result(text)
        self.assertTrue(result['complete'])
        self.assertEqual(result['range']['scanout_backing_guard'],
                         dict(v=1, physical=0x8c001000, pages=1484, command_pages=7, closed=1, vmm_reason=0))
        row = next(line for line in text.splitlines() if line.startswith('intel-scanout-backing-guard:'))
        for old, new in (('pages=1484', 'pages=1356'), ('command_pages=7', 'command_pages=0'),
                         ('closed=1', 'closed=0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, row.replace(old, new)))
        for fault, pages, reason in (('inherited-tail-gap', 0, 0), ('inherited-physical-alias', 0, 12),
                                     ('firmware-command-alias', 1484, 0)):
            with self.subTest(fault=fault):
                failed = bootstrap_result(observe(fault, 1))
                self.assertFalse(failed['complete'])
                self.assertEqual(failed['summary']['commands'], 0)
                self.assertEqual(failed['reset']['reset_attempts'], 0)
                held = failed['range']['scanout_backing_guard']
                self.assertEqual((held['pages'], held['command_pages'], held['closed']), (pages, 0, 0))
                self.assertEqual(held['vmm_reason'], reason)

    def test_bcs_register_loan_before_requester_reopens(self):
        text = observe("pass")
        receipt = bootstrap_result(text)["range"]["reopened_pte"]
        self.assertEqual((receipt["writer"], receipt["hwsp"], receipt["hwstam"]), (3, 0, 0xffffffff))
        for mode, stage in (("writer-admit", 0), ("writer-lost", 1), ("writer-return", 2)):
            with self.subTest(mode=mode):
                report = bootstrap_result(observe(mode, 1))
                self.assertFalse(report["complete"])
                self.assertEqual(report["summary"]["commands"], 1)
                self.assertEqual(report["range"]["reopened_pte"]["writer"], stage)
        with self.assertRaises(ValueError):
            bootstrap_result(text.replace("writer=3 hwsp=", "writer=2 hwsp="))
        legacy = re.sub(r"(intel-slot-reopened: )v=4 (.*?) writer=3 hwsp=\S+ hwstam=\S+ ", r"\1v=3 \2 ", text)
        self.assertNotIn("writer", bootstrap_result(legacy)["range"]["reopened_pte"])

    def test_readonly_submission_mappings_span_both_queue_checks(self):
        text = observe("pass")
        receipt = bootstrap_result(text)["range"]["reopened_pte"]
        self.assertEqual((receipt["pinned"], receipt["restored"], receipt["ro_mask"]), (5, 5, 0))
        for mode, positions, commands in (("slot-protect", range(1,6), 1), ("slot-foreign", (1,3,5), 1),
                                          ("slot-wp", (1,), 0), ("peer-port", range(1,5), 1),
                                          ("peer-reopened", range(1,5), 2), ("slot-restore", range(1,6), 2)):
            for position in positions:
                with self.subTest(mode=mode, position=position):
                    result = bootstrap_result(observe(mode, position))
                    self.assertFalse(result["complete"])
                    self.assertEqual(result["summary"]["commands"], commands)
                    self.assertEqual(result["range"]["slot_backing"]["held"], 1)
                    self.assertEqual(result["range"]["independent_slot"]["pte_writes"], 0)
        for field in ("pinned=5", "restored=5", "ro_mask=0"):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(field, field[:-1] + "4"))
        old = re.sub(r"^intel-(?:command-slots|air-rc6|idle-change|idle-engine|park|park-frame):.*\n", "", text, flags=re.M).replace("intel-range-summary: v=8 ", "intel-range-summary: v=5 ").replace(
            "intel-slot-reopened: v=4 pinned=5 restored=5 ro_mask=0 writer=3 hwsp=0 hwstam=0xffffffff", "intel-slot-reopened: v=2")
        old = re.sub(r" clock_before=\w+ clock_ack=\w+ clock_after=\w+", "", old).replace(
            "intel-widi-device: v=2", "intel-widi-device: v=1")
        old = re.sub(r"^intel-boot-page-.*\n", "", old, flags=re.M)
        self.assertTrue(bootstrap_result(old)["complete"])

    def test_wireless_programming_guard_spans_requester_reopening(self):
        text = observe("pass")
        base = bootstrap_result(text)["range"]
        self.assertEqual(base["widi_guard"], dict(v=1, acquired=1, pages=3, closed=1, device_grant=0))
        self.assertEqual([r["win"] for r in base["widi_messages"]], [0x1f,0,0x1f,0x1f])
        for altered in ("win=0x1f reset_flow=0","win=0 reset_flow=0x400"):
            modified = text.replace("n=1 win=0 reset_flow=0","n=1 " + altered)
            self.assertNotEqual(modified,text)
            window = bootstrap_result(modified)["flush"]["request_trace"]["windows"][1]
            self.assertFalse(window["sampled_block_and_flush"])
        for mode, position, pages in (("widi-video",1,0), ("widi-audio",1,0), ("widi-codec",5,0),
                                      ("widi-power",6,0), ("widi-begin",1,0), ("widi-alias",3,2),
                                      ("widi-message",3,3), ("widi-reopened-pte",2,3), ("widi-retire",1,3)):
            with self.subTest(mode=mode):
                result = bootstrap_result(observe(mode,position))
                self.assertFalse(result["complete"])
                self.assertEqual(result["range"]["widi_guard"]["pages"],pages)
                self.assertFalse(result["range"]["widi_guard"]["closed"])
                self.assertFalse(result["range"]["independent_slot"]["returned"])
        for original, changed in (("device_grant=0","device_grant=1"), ("acquired=1 pages=3","acquired=1 pages=2"),
                                  ("codec_after=0x80862808","codec_after=0"), ("win=0x1f","win=0xffffffff")):
            self.assertIn(original,text)
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(original,changed))

    def test_backing_checked_at_reset_and_requester_return(self):
        text = observe("pass")
        page = bootstrap_result(text)["range"]["slot_backing"]
        self.assertEqual((page["checks"], page["acquired"], page["held"], page["freed"]), (3, 1, 0, 1))
        for position in (1, 2):
            result = bootstrap_result(observe("backing-content", position))
            page = result["range"]["slot_backing"]
            self.assertFalse(result["complete"])
            self.assertEqual((page["checks"], page["held"], page["freed"]), (position, 1, 0))
        row = next(line for line in text.splitlines() if line.startswith("intel-slot-backing:"))
        for bad in ("", row + "\n" + row, row.replace("checks=3", "checks=2"), row.replace("freed=1", "freed=0"),
                    re.sub(r"planned=\S+", "planned=0x1", row)):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, bad))
        legacy = re.sub(r"^intel-(?:command-slots|scanout-backing-guard|slot-queues|slot-reopened|slot-widi|widi-producers|widi-device|air-rc6|idle-change|idle-engine|park|park-frame):.*\n", "", text, flags=re.M).replace(row, "").replace("intel-range-summary: v=8 ", "intel-range-summary: v=3 ")
        legacy = re.sub(r"^intel-boot-page-.*\n", "", legacy, flags=re.M)
        self.assertTrue(bootstrap_result(legacy)["complete"])

    def test_primary_backing_with_independent_slot(self):
        text = observe("pass")
        parsed = bootstrap_result(text)
        source = backing_result(historical_view(text, practical=True, memory=True, range_check=True, primary=True,
                                                aux=True, flush=True))
        protection = parsed["range"]
        for version in (3, 4, 5, 6, 7, 8):
            with self.subTest(version=version):
                protection["summary"]["v"] = version
                entries = list(protection["vectors"][0])
                protection["vectors"][1] = entries
                verify_primary_backing(source, protection, parsed["primary"])
                entries[128] = source["ptes"][0]
                with self.assertRaisesRegex(ValueError, "aliases primary"):
                    verify_primary_backing(source, protection, parsed["primary"])
                entries.pop()
                with self.assertRaisesRegex(ValueError, "backing coverage"):
                    verify_primary_backing(source, protection, parsed["primary"])

    def test_live_slice_power(self):
        text = observe("pass")
        self.assertEqual(bootstrap_result(text)["flush"]["slice_power"],
                         [dict(available=3, before=m, after=m) for m in (0, 3, 0, 0)])
        row = next(line for line in text.splitlines() if line.startswith("intel-l3-power:"))
        for bad in ("", row + "\n" + row, row.replace("n=0", "n=2"),
                    re.sub(r"after=\S+", "after=0x4", row)):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, bad))
        self.assertFalse(bootstrap_result(observe("slice-read", 1))["complete"])

    def test_smi_change_stops_each_store_and_both_retirement_checkpoints(self):
        healthy = bootstrap_result(observe("pass"))
        expected = [2, *([3] * (healthy["summary"]["native_writes"] + healthy["flush"]["l3_banks"]["summary"]["writes"])), 4, 4, 2, 5, 6]
        for read, point in enumerate(expected, start=2):
            with self.subTest(read=read, point=point):
                result = bootstrap_result(observe("smi-count", read))
                self.assertFalse(result["complete"])
                self.assertFalse(result["hardware_ownership"])
                self.assertEqual(result["summary"]["returned"], point == 6)
                self.assertEqual(result["flush"]["smi_guard"], {
                    "v": 1, "reads": read, "point": point, "fault": 2, "first": 13, "last": 14,
                })
                if read == 2:
                    self.assertEqual(result["summary"]["commands"], 0)
                elif point in (4, 5, 6):
                    self.assertEqual(result["summary"]["commands"], 2 if point in (5, 6) else 1)
        wrapped = bootstrap_result(observe("smi-wrap", 2))
        self.assertFalse(wrapped["complete"])
        self.assertEqual(wrapped["flush"]["smi_guard"]["first"], 0xffffffff)
        self.assertEqual(wrapped["flush"]["smi_guard"]["last"], 0)
        failure = observe("smi-count", 3)
        with self.assertRaisesRegex(ValueError, "GT SMI checkpoint accounting"):
            bootstrap_result(failure.replace("intel-smi-guard: v=1 reads=3 ", "intel-smi-guard: v=1 reads=2 "))

    def test_smi_msr_is_never_accessed_on_an_unsupported_cpu_profile(self):
        for mode in ("smi-no-leaf", "smi-vendor", "smi-model", "smi-hypervisor", "smi-no-msr"):
            with self.subTest(mode=mode):
                result = bootstrap_result(observe(mode, 1))
                self.assertFalse(result["complete"])
                self.assertEqual(result["summary"]["commands"], 0)
                self.assertEqual(result["summary"]["native_writes"], 0)
                self.assertEqual(result["flush"]["smi_guard"], {
                    "v": 1, "reads": 0, "point": 1, "fault": 1, "first": 0, "last": 0,
                })

    def test_access_failures_retain_the_parent_and_report_the_attempt(self):
        for mode, limit in (("flush-read", 100), ("flush-float", 100), ("flush-clock", 100), ("flush-window", 100),
                            ("flush-map", 28), ("flush-map-retained", 28), ("flush-alias", 28), ("flush-unmap", 28)):
            for pos in range(1, limit + 1):
                with self.subTest(mode=mode, position=pos):
                    result = bootstrap_result(observe(mode, pos))
                    self.assertFalse(result["complete"])
                    self.assertEqual(result["range"]["slot"]["reserved"], 1)
                    self.assertEqual(result["range"]["slot"]["returned"], 0)
                    self.assertEqual(result["summary"]["commands"], 1)
                    self.assertFalse(result["summary"]["returned"])
                    summary = result["flush"]["summary"]
                    self.assertEqual(summary["samples"], (pos - 1) // (25 if limit == 100 else 7))
                    self.assertEqual(len(result["flush"]["gam"]), summary["samples"])
                    self.assertEqual(bool(summary["live"]), mode in ("flush-clock", "flush-map-retained", "flush-unmap"))

    def test_each_outstanding_counter_stops_the_current_phase(self):
        for pos in range(16):
            n, index = divmod(pos, 4)
            result = bootstrap_result(observe("flush-busy", pos + 1))
            self.assertFalse(result["complete"])
            self.assertFalse(result["hardware_ownership"])
            queues = result["flush"]["media_outstanding"]
            self.assertEqual(len(queues), n + 1)
            expected = dict.fromkeys(MEDIA_WORDS, 0)
            expected[MEDIA_WORDS[index]] = index + 1
            self.assertEqual(queues[-1], expected)
            self.assertFalse(result["flush"]["summary"]["live"])

    def test_strict_version_authority_and_bracket_validation(self):
        text = observe("pass")
        flush = bootstrap_result(text)["flush"]
        self.assertTrue(gt_result(text)["qualification_complete"])
        self.assertEqual(session_log_limit(text.encode()), 98304)
        for key in MEDIA_WORDS:
            with self.assertRaisesRegex(ValueError, "advanced past outstanding"):
                bootstrap_result(re.sub(rf" {key}=\S+", f" {key}=0x40014321", text, count=1))
        row = next(line for line in text.splitlines(keepends=True) if line.startswith("intel-smi-guard:"))
        for bad in ("", row * 2, row.replace("fault=0", "fault=2"), row.replace("last=13", "last=14"),
                    row.replace("point=6", "point=5"), row.replace("reads=", "other="),
                    row.replace("authority=none", "authority=write")):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                bootstrap_result(text.replace(row, bad))
        old_power = re.sub(r"^intel-l3-(?:power|steer|bank):.*\n|^intel-gam-.*\n|^intel-flush-sample: v=5 n=2 .*\n", "", text, flags=re.M)
        old_power = re.sub(r"^intel-boot-page-.*\n", "", old_power, flags=re.M)
        old_power = re.sub(r"^intel-(?:bcs-stop|air-rc6|idle-policy):.*\n", "", old_power, flags=re.M)
        old_power = re.sub(r"native_writes=(\d+)", lambda m: f"native_writes={int(m[1])-1}",
                           old_power, count=1)
        legacy_smi = row.replace(f"reads={flush['smi_guard']['reads']}",
                                 f"reads={flush['smi_guard']['reads'] - flush['l3_banks']['summary']['writes'] - 1}")
        old_power = old_power.replace(row, legacy_smi)
        old_power = re.sub(r" l3_close=.*", "", old_power)
        old_power = old_power.replace("v=5 n=3 ", "v=5 n=2 ").replace("intel-flush-sample: v=5", "intel-flush-sample: v=4")
        old_power = re.sub(r"^intel-slot-summary:.*\n", "", old_power, flags=re.M)
        old_power = re.sub(r"^intel-independent-slot:.*\n", "", old_power, flags=re.M)
        old_power = re.sub(r"^intel-slot-backing:.*\n", "", old_power, flags=re.M)
        old_power = re.sub(r"^intel-(?:command-slots|scanout-backing-guard|slot-queues|slot-reopened|slot-widi|widi-producers|widi-device|air-rc6|idle-change|idle-engine|park|park-frame):.*\n", "", old_power, flags=re.M)
        old_power = old_power.replace("intel-range-summary: v=8 ", "intel-range-summary: v=1 ")
        old_power = old_power.replace("maps=6 retired=6 live=0 reads=792",
                                      "maps=4 retired=4 live=0 reads=774")
        old_power = old_power.replace("words0=132 words1=132", "words0=129 words1=129")
        old_power = re.sub(r"(intel-range-vector: v=1 sample=[01]) count=132 data=(\S+)",
                           lambda m: m[1] + " count=129 data=" +
                           base64.b64encode(base64.b64decode(m[2])[:129*8]).decode(), old_power)
        old_power = old_power.replace("v=9 samples=4 reads=100 maps=28 retired=28", "v=5 samples=3 reads=48 maps=15 retired=15")
        with self.assertRaisesRegex(ValueError, "compatible baseline"):
            bootstrap_result(old_power)
        old_power = re.sub(r"^intel-gpm-step:.*\n", "", old_power, flags=re.M)
        self.assertNotIn("request_trace", bootstrap_result(old_power)["flush"])
        media = old_power.replace("intel-flush-summary: v=5 samples=3 reads=48 maps=15 retired=15",
                             "intel-flush-summary: v=4 samples=3 reads=36 maps=9 retired=9")
        media = re.sub(r" gam_req=\S+ rc_control=\S+ rc_target=\S+ rc6_threshold=\S+", "", media)
        media = media.replace("intel-flush-sample: v=4 ", "intel-flush-sample: v=3 ")
        self.assertNotIn("power_controls", bootstrap_result(media)["flush"])
        legacy = media.replace("intel-flush-summary: v=4 samples=3 reads=36 maps=9 retired=9",
                              "intel-flush-summary: v=3 samples=3 reads=24 maps=3 retired=3")
        legacy = re.sub(r" vcs0_ref=\S+ vcs0_prefetch=\S+ vcs1_ref=\S+ vcs1_prefetch=\S+", "", legacy)
        legacy = legacy.replace("intel-flush-sample: v=3 ", "intel-flush-sample: v=2 ")
        self.assertNotIn("media_outstanding", bootstrap_result(legacy)["flush"])
        previous = legacy.replace("intel-flush-summary: v=3 samples=3 reads=24 ",
                                "intel-flush-summary: v=2 samples=3 reads=18 ")
        previous = re.sub(r" gam_go=\S+ gam_eco=\S+", "", previous)
        previous = previous.replace("intel-flush-sample: v=2 ", "intel-flush-sample: v=1 ")
        self.assertNotIn("gam", bootstrap_result(previous)["flush"])
        old = previous.replace(legacy_smi, "").replace("intel-flush-summary: v=2 ", "intel-flush-summary: v=1 ")
        self.assertNotIn("smi_guard", bootstrap_result(old)["flush"])
        for n, packet in enumerate(flush["samples"]):
            sample = next(line for line in text.splitlines() if line.startswith(f"intel-flush-sample: v=5 n={n} "))
            for bad in ("", sample + "\n" + sample, sample.replace(f"begin={packet['begin']}", "begin=1"),
                        sample.replace(f"end={packet['end']}", f"end={packet['begin'] + 10001}"),
                        sample.replace(" l3=", " unknown="),
                        sample.replace(" gam_go=", " unknown_go="),
                        sample.rsplit(" ", 1)[0],
                        sample.rsplit("=", 1)[0] + "=0xffffffff",
                        sample.rsplit("=", 1)[0] + "=0x100000000"):
                with self.subTest(bad=bad), self.assertRaises(ValueError):
                    bootstrap_result(text.replace(sample, bad))
        for before, after in (("intel-bootstrap-summary: v=12 ", "intel-bootstrap-summary: v=11 "),
                              ("authority=none\nintel-l3-power", "authority=write\nintel-l3-power"),
                              ("samples=4 reads=100 maps=28 retired=28", "samples=4 reads=100 maps=28 retired=27"),
                              ("samples=4 reads=100 maps=28 retired=28", "samples=4 reads=99 maps=28 retired=28"),
                              ("intel-flush-summary: v=9 ", "intel-flush-summary: v=6 ")):
            self.assertIn(before, text)
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(before, after))

    def test_reason_transition_bounds_and_blocked_flush_evidence(self):
        text = observe("pass")
        trace = bootstrap_result(text)["flush"]["request_trace"]
        self.assertGreater(trace["summary"]["max_gap_us"], 0)
        self.assertEqual([(c["asserted"], c["cleared"]) for c in trace["reason_changes"]], [(127, 0), (0, 127)])
        self.assertEqual(trace["last_observed_reasons"], 0)
        self.assertTrue(all(c["earliest_us"] < c["latest_us"] for c in trace["reason_changes"]))
        self.assertEqual([w["sampled_block_and_flush"] for w in trace["windows"]], [False, True, False, False])
        self.assertEqual(trace["maintenance"]["observed_open_clients"], 0x5f)
        self.assertFalse(trace["maintenance"]["observed_block"])
        sample = next(l for l in text.splitlines() if l.startswith("intel-flush-sample: v=5 n=1 "))
        for old, new in (("req_close=0", "req_close=0x1"), ("go_close=0x20", "go_close=0x21"),
                         ("rcs_close=0x20", "rcs_close=0x10"), ("eco_close=0x201", "eco_close=0x200"),
                         ("l3_close=0x1", "l3_close=0x3")):
            changed = text.replace(sample, sample.replace(old, new))
            self.assertFalse(bootstrap_result(changed)["flush"]["request_trace"]["windows"][1]["sampled_block_and_flush"])
        stale = bootstrap_result(observe("flush-stale"))["flush"]["request_trace"]
        self.assertFalse(any(w["sampled_block_and_flush"] for w in stale["windows"]))
        latched = bootstrap_result(observe("flush-latched"))["flush"]["request_trace"]
        self.assertEqual(latched["last_observed_reasons"], 0x7f)
        self.assertFalse(any(c["cleared"] for c in latched["reason_changes"]))
        chatter = bootstrap_result(observe("flush-chatter"))["flush"]["request_trace"]
        self.assertEqual(chatter["summary"]["spans"], 32)
        self.assertGreater(chatter["summary"]["omitted"], 0)
        self.assertIsNone(chatter["last_observed_reasons"])
        self.assertFalse(any(w["observed_block"] for w in chatter["windows"]))
        span = next(l for l in text.splitlines() if l.startswith("intel-gam-span: v=1 n=1 "))
        for bad in ("", span + "\n" + span, span.replace("pulse=0", "pulse=2"),
                    re.sub(r"first_end=\d+", "first_end=1", span)):
            with self.assertRaises(ValueError):
                bootstrap_result(text.replace(span, bad))
        with self.assertRaises(ValueError):
            bootstrap_result(text.replace("omitted=0 max_gap_us=", "omitted=1 max_gap_us="))

    def test_dedicated_stage_and_boot_recipe(self):
        for build in ("flush", "aux"):
            for token in ("flush", "aux"):
                status = subprocess.run(
                    [TESTS / f"intel-gt-{build}-boot", "gt-bootstrap", RECIPE + token + "-v1"], timeout=5,
                ).returncode
                self.assertEqual(status, 0 if build == token else 1)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / "build"
            for name in ("limine-binary/BOOTX64.EFI", "intel-gt-flush/AIRBOOT.EFI", "intel-gt-flush/srvros.elf", "initramfs.tar"):
                path = build / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            with (patch.object(boot, "ROOT", root), patch.object(boot, "STAGE", build / "hardware-boot"),
                  patch.object(boot.subprocess, "check_output", side_effect=["d" * 40, ""])):
                release = boot.stage(laptop_probe_volume="12345678", intel_gt_flush=True)
            payload = build / "hardware-boot" / release
            self.assertTrue(boot.verify(payload, release)["source_bound"])
            self.assertEqual((payload / "loader/BOOTX64.EFI").read_text(), "limine-binary/BOOTX64.EFI")
            grub = boot.grub_config("1234-5678", release, "e"*16, handoff_releases=(release,))
            self.assertLess(grub.index("AIRBOOT.EFI"), grub.index("BOOTX64.EFI"))
            self.assertEqual(grub.count("AIRBOOT.EFI"), 1)
            helper = payload / boot.AIR_HANDOFF
            helper.write_bytes(b"changed")
            with self.assertRaisesRegex(ValueError, "failed verification"):
                boot.verify(payload, release)
            helper.write_text("intel-gt-flush/AIRBOOT.EFI")
            self.assertEqual((payload / "srvros.elf").read_text(), "intel-gt-flush/srvros.elf")
            cmdline = next(line.split("cmdline:", 1)[1].strip()
                           for line in (payload / "limine.conf").read_text().splitlines() if "cmdline:" in line)
            self.assertEqual(trial_parsers("intel-gt-flush-v59")[0](cmdline), dict(release=release, volume="12345678"))
            options = "laptop-capture: boot options=" + cmdline
            self.assertIn("srvros.intel-ring=entry-v1", shared_receipt(options))
            self.assertNotIn("gt-bootstrap-flush-v1", shared_receipt(options))
            with self.assertRaises(ValueError):
                trial_parsers("intel-gt-aux-v58")[0](cmdline)
        with self.assertRaises(ValueError):
            boot.stage(laptop_probe_volume="12345678", intel_gt_flush=True, intel_gt_aux=True)

    def test_fatal_sanitizers(self):
        cc, flags, sources = recipe(TARGET)
        with tempfile.TemporaryDirectory(prefix="gt-flush-san-") as temp:
            binary = Path(temp) / "flush"
            compile_fixture(TARGET, output=binary, declaration=(
                cc, [*flags, "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"], sources,
            ))
            for mode, pos in (("pass", 0), ("park-cold-vcs1", 0), ("park-cold-vcs1-restore", 0),
                              ("park-cold-vcs1-edge", 0), ("park-cold-vcs1-edge-smi", 0),
                              ("pipe-writer", 0), ("pipe-writer-return", 1),
                              ("flush-clock", 12), ("flush-unmap", 2), ("flush-float", 100),
                              ("smi-count", 2), ("smi-wrap", 2), ("smi-hypervisor", 1)):
                observe(mode, pos, binary=binary)
