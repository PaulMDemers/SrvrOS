"""FAT metadata must have room for every addressable cluster."""
import subprocess
import tempfile
import sys
import unittest
from pathlib import Path

from mk_usb_image import Fat32Builder, SECTOR


class GeometryTests(unittest.TestCase):
    def test_default_layout(self):
        fat = Fat32Builder(384 * 1024 * 1024)
        self.assertEqual(fat.fat_sectors, 767)
        self.assertLessEqual(fat.cluster_count + 2, fat.fat_sectors * SECTOR // 4)

    def test_basename_output_in_current_directory(self):
        script = Path(__file__).with_name("mk_usb_image.py").resolve()
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ("kernel", "initramfs", "config", "bootx64"):
                (root / name).write_bytes((name + " fixture").encode())
            result = subprocess.run([sys.executable, str(script), "--output", "image.img",
                "--kernel", "kernel", "--initramfs", "initramfs", "--config", "config",
                "--bootx64", "bootx64", "--esp-size-mib", "1"], cwd=root,
                capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            image = (root / "image.img").read_bytes()
            self.assertEqual(image[510:512], b"\x55\xaa")
            self.assertEqual(image[512:520], b"EFI PART")

    def test_rounding_boundaries_terminate(self):
        # Use a subprocess so a regression cannot hang the test runner.
        code = """from mk_usb_image import Fat32Builder, SECTOR
for size in (507, 1020):
    fat = Fat32Builder(size * 1024 * 1024)
    assert fat.fat_sectors * SECTOR // 4 >= fat.cluster_count + 2
    smaller = fat.fat_sectors - 1
    clusters = (fat.total_sectors - fat.reserved_sectors - fat.fat_count * smaller) // fat.sectors_per_cluster
    assert smaller * SECTOR // 4 < clusters + 2
"""
        subprocess.run([sys.executable, "-c", code], cwd=Path(__file__).parent,
                       check=True, timeout=5)


if __name__ == "__main__":
    unittest.main()
