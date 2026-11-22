"""Content/dependency compiler reuse and framed fork isolation tests."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from fixture_compile import compile_fixture, recipes
from fixture_client import Server, close_all
from workflow.store import sha

ROOT = Path(__file__).resolve().parents[1]


class CompilerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.cache = self.root / 'cache'
        (self.root / 'a.h').write_text('#define VALUE 1\n')
        (self.root / 'main.c').write_text('#include "a.h"\nint other(void); int main(void) {return other()!=VALUE;}\n')
        (self.root / 'other.c').write_text('#include "a.h"\nint other(void) {return VALUE;}\n')
        (self.root / 'Makefile').write_text('fixture: main.c other.c a.h\n\tcc -std=c11 -O2 -Wall -Werror main.c other.c -o fixture\n')

    def build(self, **kwargs):
        return compile_fixture('fixture', root=self.root, cache=self.cache, **kwargs)

    def test_support_reused_and_header_membership_and_same_mtime_bytes_invalidate(self):
        cold = self.build()
        self.assertEqual((cold['compiled'], cold['reused']), (2, 0))
        warm = self.build()
        self.assertEqual((warm['compiled'], warm['reused']), (0, 2))
        header = self.root / 'a.h'
        stamp = header.stat().st_mtime_ns
        header.write_text('#define VALUE 2\n')
        os.utime(header, ns=(stamp, stamp))
        self.assertEqual(self.build()['compiled'], 2)
        self.assertEqual(subprocess.run([str(self.root / 'fixture')]).returncode, 0)

    def test_external_mutation_reuses_original_support_and_quote_include_path(self):
        self.build()
        external = self.root / 'mutations'
        external.mkdir()
        mutant = external / 'main.c'
        mutant.write_text('#include "a.h"\nint other(void); int main(void) {return other()!=VALUE;}\n')
        result = self.build(replacements=['main.c=' + str(mutant)], output='mutant')
        self.assertEqual((result['compiled'], result['reused']), (1, 1))
        self.assertEqual(subprocess.run([result['output']]).returncode, 0)

    def test_corrupt_object_is_recompiled(self):
        self.build()
        obj = next(self.cache.glob('*/unit.o'))
        obj.write_bytes(b'corrupt')
        self.assertEqual(self.build()['compiled'], 1)

    def test_flag_change_invalidates_all_units(self):
        self.build()
        makefile = self.root / 'Makefile'
        makefile.write_text(makefile.read_text().replace('-O2', '-O1'))
        self.assertEqual(self.build()['compiled'], 2)

    def test_batch_recipe_preserves_each_targets_flags(self):
        with (self.root / 'Makefile').open('a') as stream:
            stream.write('other: main.c other.c a.h\n\tcc -std=c11 -O1 -Wall -Werror main.c other.c -o other\n')
        declarations = recipes(['fixture', 'other'], self.root)
        self.assertIn('-O2', declarations['fixture'][1])
        self.assertIn('-O1', declarations['other'][1])


class ForkServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        source = cls.root / 'case.c'
        source.write_text('''#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <assert.h>
#include <stdio.h>
static int once;
static int run_case(int argc, char **argv) {
 assert(argc==2); assert(!once++);
 if(!strcmp(argv[1],"--wake-engine-crash")) { assert(0); }
 if(!strcmp(argv[1],"--wake-engine-error")) {fputs("error\\n",stderr); return 7;}
 printf("case %s fresh=%d\\n",argv[1],once); return 0;
}
#include "fixture_server.h"
int main(int argc,char **argv) {
 if(argc==2 && !strcmp(argv[1],"--fixture-server")) return fixture_server(run_case);
 return run_case(argc,argv);
}
''')
        cls.binary = cls.root / 'case'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / 'tools'), str(source), '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        close_all()
        cls.tmp.cleanup()

    def test_every_case_starts_pristine_and_keeps_exact_bytes_and_status(self):
        server = Server(self.binary)
        self.addCleanup(server.close)
        for mode in ('report', 'report', 'error', 'report'):
            ordinary = subprocess.run([str(self.binary), '--wake-engine-' + mode], capture_output=True, text=True)
            batched = server.run(mode)
            self.assertEqual((batched.returncode, batched.stdout, batched.stderr),
                             (ordinary.returncode, ordinary.stdout, ordinary.stderr))
        crashed = server.run('crash')
        self.assertEqual(crashed.returncode, -6)
        self.assertIn('Assertion', crashed.stderr)
        self.assertEqual(server.run('report').returncode, 0)

    def test_invalid_request_refused_without_executing(self):
        server = Server(self.binary)
        self.addCleanup(server.close)
        for mode in ('', '../bad', 'report\ncrash', 'x'*1024):
            with self.assertRaises(ValueError):
                server.run(mode)
        self.assertEqual(server.run('report').returncode, 0)

    def test_closed_transport_is_failure_not_a_retry(self):
        server = Server(self.binary)
        server.close()
        with self.assertRaises((ValueError, OSError, RuntimeError)):
            server.run('report')


if __name__ == '__main__':
    unittest.main()
