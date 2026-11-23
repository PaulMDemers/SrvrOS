"""Air7,2 acquisition-only diagnostic with PCI bus mastering enabled."""
import base64
import re
import struct

from hardware_boot import limine_config
from intel_engine_prepare import native_prepare_result

PROFILE = 'intel-prepare-gam-v1'


def recipe(cmdline):
    volume = re.findall(r'(?:^| )srvros\.workspace-volume=([0-9a-f]{8})(?= |$)', cmdline)
    release = re.findall(r'(?:^| )srvros\.desktop-release=([0-9a-f]{16})(?= |$)', cmdline)
    if len(volume) != 1 or len(release) != 1:
        raise ValueError('Missing unique prepare volume/release')
    config = limine_config(release[0], f'ram-usb-v1-intel-engine-prepare-laptop-probe-{volume[0]}-auto-return')
    expected, = [line.strip().removeprefix('cmdline: ') for line in config.splitlines()
                 if line.strip().startswith('cmdline: ')]
    if cmdline != expected:
        raise ValueError('Unexpected prepare/cancel recipe')
    return dict(volume=volume[0], release=release[0])


def samples(text):
    rows = [line for line in text.splitlines() if line.startswith('intel-gam-prepare:')]
    if len(rows) != 1:
        raise ValueError('Missing unique prepare GAM receipt')
    match = re.fullmatch(r'intel-gam-prepare: v=1 n=([0-8]) b=(-|[A-Za-z0-9+/]+={0,2})', rows[0])
    if not match:
        raise ValueError('Invalid prepare GAM receipt')
    count = int(match[1])
    data = b'' if match[2] == '-' else base64.b64decode(match[2], validate=True)
    if len(data) != 4 * count or (base64.b64encode(data).decode() or '-') != match[2]:
        raise ValueError('Invalid prepare GAM sample width')
    return list(struct.unpack(f'<{count}I', data))


def result(text):
    native = native_prepare_result(text)
    words = samples(text)
    complete = bool(native and native['native_complete'] and len(words) == 8 and 0xffffffff not in words)
    return dict(complete=complete, native_complete=False, diagnostic_complete=complete,
                hardware_ownership=False, gam=words, prepare=native)


def jobs(inputs):
    image = 'build/intel-engine-prepare/srvros.elf'
    build = dict(name='prepare-image', argv=['make', '-j2', image, 'build/initramfs.tar', 'build/limine-binary/.ready'],
                 inputs=inputs, outputs=[image, 'build/initramfs.tar', 'build/limine-binary/BOOTX64.EFI'],
                 cache=False, timeout=1800)
    fixtures = dict(name='prepare-fixtures', argv=['make', '-j2', 'test-intel-engine-prepare'], inputs=inputs,
                    outputs=['build/tests/intel-engine-prepare-core', 'build/tests/intel-engine-prepare-adapter'],
                    cache=False, timeout=600)
    return dict(development=[fixtures, build], qualification=[fixtures, build])
