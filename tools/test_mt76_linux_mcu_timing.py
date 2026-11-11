#!/usr/bin/env python3
import struct
import unittest
from mt76_linux_mcu_timing import Correlator, HEADER, event


def packet(tag, kind, ep, timestamp, payload=b'', status=0, length=None, bus=1, address=2, flag=b'\0'):
    header = HEADER.pack(tag, ord(kind), 3, ep, address, bus, b'-', flag,
                         timestamp//1000000, timestamp%1000000, status,
                         len(payload) if length is None else length, len(payload), b'\0'*8, 0, 0, 0, 0)
    return event(header, payload, 1, 2)


def command(seq=1):
    return struct.pack('<IIII', 0x51f00008 | seq << 16, 1, 0, 0)


def reply(seq=1):
    return struct.pack('<IIII', 0x4a000008 | seq << 16, 1, 0, 0x4a000008 | seq << 16)


class TimingTests(unittest.TestCase):
    def test_complete_and_unrelated_reply(self):
        c = Correlator()
        for r in [packet(1,'S',8,100,command()), packet(1,'C',8,130,length=16),
                  packet(2,'S',0x85,140,length=1024), packet(2,'C',0x85,180,reply(2)),
                  packet(2,'S',0x85,190,length=1024), packet(2,'C',0x85,1100,reply())]:
            c.consume(r)
        r, = c.calibrations()
        self.assertEqual((r['command_to_reply_us'],r['in_wait_us'],len(r['receive_attempts'])), (1000,910,2))
        self.assertEqual(c.pending,{})

    def test_timeout_not_matching(self):
        c = Correlator()
        for r in [packet(1,'S',8,100,command()),packet(1,'C',8,120,length=16),
                  packet(2,'S',0x85,125,length=1024),packet(2,'C',0x85,300125,status=-110)]:
            c.consume(r)
        r, = c.calibrations()
        self.assertEqual(r['matching_replies'],0)
        self.assertNotIn('in_wait_us',r)

    def test_tag_reuse_and_endpoint_mismatch(self):
        c=Correlator()
        c.consume(packet(1,'S',8,10,command()))
        c.consume(packet(1,'S',0x85,20,length=1024))
        c.consume(packet(1,'C',8,30,length=16))
        self.assertEqual((c.reused,c.orphans), (1,1))
        self.assertIn(1,c.pending)
        self.assertEqual(c.records[0]['incomplete'],'tag reused')

    def test_sequence_reuse_cannot_cross_command(self):
        c=Correlator()
        for r in [packet(1,'S',8,10,command()), packet(1,'C',8,11,length=16),
                  packet(2,'S',0x85,12,length=1024),packet(3,'S',8,20,command()),
                  packet(3,'C',8,21,length=16),packet(2,'C',0x85,22,reply())]:
            c.consume(r)
        a,b=c.calibrations()
        self.assertEqual(a['matching_replies'],0)
        self.assertEqual(b['matching_replies'],0)

    def test_no_payload_and_no_arbitrary_persistence(self):
        for ep in [8,0x85]:
            r=packet(1,'S' if ep==8 else 'C',ep,10,b'private arbitrary bytes')
            self.assertNotIn('payload',r)
            self.assertNotIn('calibration_id',r)
        r=packet(1,'S',8,10,command(),flag=b'>')
        self.assertNotIn('command',r)
        self.assertNotIn('calibration_id',packet(1,'S',8,10,command()[:8],length=16))

    def test_identity_endpoint_and_header_refusals(self):
        self.assertIsNone(packet(1,'S',8,10,command(),address=3))
        self.assertIsNone(packet(1,'S',8,10,command(),bus=2))
        self.assertIsNone(packet(1,'S',1,10,command()))
        with self.assertRaises(ValueError):event(b'',b'',1,2)

    def test_clock_reversal_and_duplicate(self):
        c=Correlator()
        c.consume(packet(1,'S',8,20,command()))
        c.consume(packet(1,'C',8,10,length=16))
        c.consume(packet(1,'C',8,30,length=16))
        self.assertEqual(c.records[0]['incomplete'],'clock reversed')
        self.assertEqual(c.orphans,1)


if __name__ == '__main__':unittest.main()
