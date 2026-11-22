import subprocess
import unittest
from scanout_publish_model import model_result

class PublicationModelTests(unittest.TestCase):
    def test_actual_producer(self):
        self.text=subprocess.check_output(['build/tests/intel-scanout-trial','--fixture-report'],text=True)
        result=model_result(self.text)
        self.assertTrue(result['complete']);self.assertEqual(result['writes'],65)
        self.assertFalse(result['dma_authority']);self.assertFalse(result['physical_tearfree'])
    def test_invalid_receipts(self):
        text='scanout-publish-model: v=1 ok=1 cases=2 pages=32 writes=65 flushes=2 quarantined=32 hardware_writes=0 dma_authority=0 ownership=model'
        for before,after in [('v=1','v=2'),('cases=2','cases=1'),('pages=32','pages=31'),('writes=65','writes=64'),('flushes=2','flushes=1'),('quarantined=32','quarantined=0'),('hardware_writes=0','hardware_writes=1'),('dma_authority=0','dma_authority=1'),('ownership=model','ownership=native')]:
            with self.subTest(before=before):
                with self.assertRaises(ValueError):model_result(text.replace(before,after))
        with self.assertRaises(ValueError):model_result(text+'\n'+text)
        self.assertFalse(model_result(text.replace('ok=1','ok=0'))['complete'])
        self.assertIsNone(model_result('ordinary native desktop'))
if __name__=='__main__':unittest.main()
