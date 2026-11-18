"""Frozen checks for the isolated full-GT display translation diagnostic."""
import sys


def jobs(dependencies):
    outputs = ['build/tests/' + name for name in (
        'intel-gt-range-adapter', 'intel-gt-range-boot',
        'intel-gt-memory-adapter', 'intel-gt-memory-boot',
        'intel-vtd-drain', 'intel-vtd-drain-policy',
        'intel-ggtt-invalidate', 'intel-ggtt-invalidate-policy',
        'exfat-driver', 'laptop-firmware-digest',
    )]
    fixtures = dict(name='fixtures',
                    argv=[sys.executable, 'tools/fixture_compile.py', '--under-workflow-lock', *outputs],
                    inputs=dependencies, outputs=outputs, cache=False, timeout=1200)

    def checks(name, *modules, extra=(), env=None, timeout=1200):
        return dict(name=name, argv=[sys.executable, '-m', 'unittest', '-v', *modules],
                    env={'PYTHONPATH': 'tools', 'SRVROS_FIXTURE_BATCH': '1', **(env or {})},
                    inputs=dependencies + outputs + list(extra), outputs=[], timeout=timeout)

    firmware = checks('firmware-identity', 'test_laptop_firmware_digest',
                      extra=('build/device-fixtures/macbookair7-2-dsdt.bin',),
                      env={'SRVROS_REQUIRE_NATIVE_FIRMWARE': '1'}, timeout=300)
    range_checks = checks('range-transport', 'test_intel_gt_range', 'test_hardware_boot',
                          'test_hardware_desktop', 'test_device_workflow', timeout=300)
    images = ('build/intel-gt-range/srvros.elf', 'build/initramfs.tar')
    image = dict(name='range-image', argv=['make', *images], inputs=dependencies,
                 outputs=list(images), cache=False, timeout=1800)
    inherited = checks('original-memory', 'test_intel_gt_memory',
                       'test_intel_vtd_drain', 'test_intel_ggtt_invalidate')
    return dict(development=[fixtures, firmware, range_checks, image],
                qualification=[fixtures, firmware, range_checks, inherited, image])
