import copy
import unittest
from haswell_backing_audit import pte_fields,ggc_fields,plane_extent,fence_candidates

class HaswellAuditTests(unittest.TestCase):
    def sample(self):
        return dict(state=1,reads=9,bpp=32,color=[1,8,16,8,8,8,0],
                    raw=[0x98004400,5632,0,0,0,(1365<<16)|767,0,0x98004400,0],
                    framebuffer=[1,0xffff8000a0000000,0xa0000000,1366,768,5632,4325376],
                    pci=[0xa000000c,0,0xa000000c,0])
    def fence(self, pitch=5632, size=4325376):
        return ((size-4096)|((pitch//128)-1))<<32|1
    def test_exact_captured_extent_is_not_backing_proof(self):
        r=plane_extent(self.sample())
        self.assertEqual((r['span'],r['pages'],r['first_mmio'],r['end_mmio']),(4325376,1056,0x200000,0x201080))
        self.assertFalse(r['ggtt_proven']);self.assertFalse(r['swizzle_known']);self.assertFalse(r['hardware_authority'])
    def test_tile_padding_is_not_cpu_descriptor_span(self):
        s=self.sample();s['framebuffer'][4]=769;s['framebuffer'][6]=5632*769;s['raw'][5]=(1365<<16)|768
        r=plane_extent(s);self.assertEqual(r['padded_rows'],776);self.assertEqual(r['pages'],1067)
    def test_pte_cache_bit_eleven_does_not_create_address_bit_39(self):
        self.assertEqual(pte_fields(0x12345001)['address_candidate'],0x12345000)
        for cc in range(16):
            raw=0x12345001|((cc&7)<<1)|((cc&8)<<8)
            r=pte_fields(raw);self.assertEqual(r['cache_control'],cc);self.assertEqual(r['address_candidate'],0x12345000)
        self.assertIsNone(pte_fields(0)['address_candidate'])
    def test_all_high_address_bits_and_gen8_width_refuse(self):
        for bit in range(7):self.assertEqual(pte_fields(1|(1<<(4+bit)))['address_candidate'],1<<(32+bit))
        for value in (-1,1<<32,0xffffffff,True):
            with self.assertRaises(ValueError):pte_fields(value)
    def test_haswell_ggc_is_not_broadwell_layout(self):
        # HSW GGMS=2, GMS=4: 2MiB table / 128MiB stolen candidates.
        r=ggc_fields((2<<8)|(4<<3));self.assertEqual((r['ggms'],r['gms']),(2,4))
        self.assertEqual(r['table_bytes_candidate'],2<<20);self.assertEqual(r['stolen_bytes_candidate'],128<<20)
        self.assertFalse(r['range_validated']);self.assertNotEqual(((r['raw']>>6)&3,r['raw']>>8),(2,4))
        self.assertFalse(ggc_fields(0)['range_validated'])
    def test_partial_and_duplicate_fences_cannot_be_unique(self):
        e=plane_extent(self.sample());f=self.fence()
        r=fence_candidates([f]+[0]*15,e,5632);self.assertEqual(r['matches'],[0]);self.assertFalse(r['unique_candidate'])
        r=fence_candidates([f]+[0]*31,e,5632);self.assertTrue(r['unique_candidate']);self.assertFalse(r['ownership'])
        r=fence_candidates([f,f]+[0]*30,e,5632);self.assertFalse(r['unique_candidate'])
        r=fence_candidates([f|2]+[0]*31,e,5632);self.assertFalse(r['matches'])
    def test_pitch_reference_discrepancy_refuses_candidate(self):
        e=plane_extent(self.sample());f=self.fence()|(1<<42)
        self.assertFalse(fence_candidates([f]+[0]*31,e,5632)['unique_candidate'])
        for values in ([0]*8,[0]*33,[(1<<64)-1]+[0]*31):
            with self.assertRaises(ValueError):fence_candidates(values,e,5632)
    def test_layout_refusals_and_capacity(self):
        for field,index,val in [('raw',4,1),('raw',6,4096),('raw',0,0x9800c400),('raw',1,1),('framebuffer',2,0xa0001000),('pci',2,0xb000000c)]:
            s=copy.deepcopy(self.sample());s[field][index]=val
            with self.subTest(field=field,index=index):
                with self.assertRaises(ValueError):plane_extent(s)
        s=self.sample();s['framebuffer'][3:]=[384,4096,1536,1536*4096];s['raw'][1]=1536;s['raw'][5]=(383<<16)|4095
        self.assertEqual(plane_extent(s)['pages'],1536)
        s['framebuffer'][5:]=[2048,2048*4096];s['raw'][1]=2048
        with self.assertRaises(ValueError):plane_extent(s)

if __name__=='__main__':unittest.main()
