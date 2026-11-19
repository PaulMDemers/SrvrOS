"""Frozen jobs for the isolated inherited-display experiment."""
import sys


def jobs(dependencies):
    image = "build/intel-edp-baseline/srvros.elf"
    host = "build/tests/intel-bdw-edp-baseline"
    stage = [image, "build/initramfs.tar", "build/limine-binary/BOOTX64.EFI"]
    build = dict(name="edp-image", argv=["make", "-j2", *stage[:2], "build/limine-binary/.ready"], inputs=dependencies,
                 outputs=stage, cache=False, timeout=1800)
    fixtures = dict(name="edp-host", argv=["make", "test-intel-bdw-edp-baseline"],
                    inputs=dependencies, outputs=[host], cache=False, timeout=180)
    checks = dict(name="edp-receipt-sanitizers",
                  argv=[sys.executable, "-m", "unittest", "-v", "test_intel_edp_baseline"],
                  env={"PYTHONPATH": "tools"}, inputs=dependencies,
                  outputs=[], cache=False, timeout=180)
    return dict(development=[fixtures, build], qualification=[fixtures, checks, build])
