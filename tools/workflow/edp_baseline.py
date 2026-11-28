"""Frozen jobs for the isolated inherited-display experiment."""
import sys


def jobs(dependencies):
    image = "build/intel-edp-baseline/srvros.elf"
    host = ["build/tests/intel-bdw-edp-baseline", "build/tests/intel-bdw-edp", "build/tests/boot-edp-pci-hold"]
    stage = [image, "build/initramfs.tar", "build/limine-binary/BOOTX64.EFI"]
    build = dict(name="edp-image", argv=["make", "-j2", *stage[:2], "build/limine-binary/.ready"], inputs=dependencies,
                 outputs=stage, cache=False, timeout=1800)
    fixtures = dict(name="edp-host", argv=["make", "test-intel-bdw-edp-baseline", "test-intel-bdw-edp", "test-boot-edp-pci-hold"],
                    inputs=dependencies, outputs=host, cache=False, timeout=180)
    cache_binary = "build/tests/intel-cache-probe"
    build_cache = dict(name="edp-cache-build", argv=["make", cache_binary], inputs=dependencies,
                       outputs=[cache_binary], cache=False, timeout=180)
    cache = dict(name="edp-cache", argv=[cache_binary, "edp-uc"], inputs=[cache_binary],
                 outputs=[], cache=False, timeout=30)
    checks = dict(name="edp-receipt-sanitizers",
                  argv=[sys.executable, "-m", "unittest", "-v", "test_intel_edp_baseline"],
                  env={"PYTHONPATH": "tools"}, inputs=dependencies,
                  outputs=[], cache=False, timeout=180)
    return dict(development=[fixtures, build_cache, cache, build],
                qualification=[fixtures, build_cache, cache, checks, build])
