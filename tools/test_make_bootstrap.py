"""Exercise real Make rules with a delayed, disposable compiler bootstrap."""
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest


class BootstrapOrderTest(unittest.TestCase):
    def test_parallel_acpi_compile_waits_for_toolchain(self):
        root = Path(__file__).resolve().parents[1]
        for target in (
            'build/kernel/ports/upstream/uacpi/source/default_handlers.c.o',
            'build/acpi-runtime-check/ports/upstream/uacpi/source/default_handlers.c.o',
            'build/acpi-runtime/ports/upstream/uacpi/source/default_handlers.c.o',
        ):
            with self.subTest(target=target), tempfile.TemporaryDirectory(prefix='srvros-make-order-') as directory:
                work = Path(directory)
                (work / 'Makefile').write_bytes((root / 'Makefile').read_bytes())
                for name in ('kernel', 'userspace', 'ports', 'shared', 'initramfs'):
                    (work / name).symlink_to(root / name, target_is_directory=True)
                (work / 'tools').mkdir()
                (work / 'build/limine-protocol').mkdir(parents=True)
                (work / 'build/limine-protocol/.ready').touch()
                (work / 'tools/bootstrap_zig.py').write_text('''
from pathlib import Path
import time
time.sleep(.25)
p = Path('build/tooling/zig/zig')
p.parent.mkdir(parents=True, exist_ok=True)
p.write_text('ready')
''')
                (work / 'compile.py').write_text('''
from pathlib import Path
import sys
if not Path('build/tooling/zig/zig').is_file():
    sys.exit('compiler invoked before bootstrap completed')
Path(sys.argv[sys.argv.index('-o') + 1]).touch()
''')
                # The parent kernel target has both compiler and objects as
                # prerequisites. Sibling prerequisites are not ordered by make.
                result = subprocess.run(['make', '-j2', target, 'build/tooling/zig/zig',
                    'PYTHON=' + shlex.quote(sys.executable),
                    'CC=' + shlex.join([sys.executable, 'compile.py'])], cwd=work,
                    text=True, capture_output=True, timeout=20)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue((work / target).is_file())


if __name__ == '__main__':
    unittest.main()
