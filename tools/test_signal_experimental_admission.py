#!/usr/bin/env python3
"""Test the actual experimental gate and dispatch prefix before any backend."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--sanitizers', action='store_true')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
path = ROOT / 'kernel/src/arch/x86_64/syscall.c'
text = path.read_text()
start = text.index('static bool signal_experimental_raw_allowed(')
end = text.index('\n#endif\nstatic void syscall_dispatch_impl', start)
gate = text[start:end]
start = text.index('static void syscall_dispatch_impl(')
end = text.index('    switch (frame->rax) {', start)
prefix = text[start:end]
# Verify the real normal dispatch routes both IDs to their owned backends.
assert 'case SYS_FILE_STREAM:\n        frame->rax=(uint64_t)syscall_file_stream(' in text
assert 'case SYS_SESSION_SCOPE:' in text
fixture = r'''
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <srvros/syscall_numbers.h>
struct isr_frame {uint64_t rax,rdi,rsi,rdx,rcx,r8,r9;};
static unsigned backends,copies,handles,membership,fd_checks;
static bool process_signal_experimental_fd_allowed(uint64_t fd) {
    fd_checks++;return fd<3;
}
static bool signal_io_raw_backend(uint64_t n,const uint64_t a[6],int64_t *r) {
    (void)n;(void)a;(void)r;backends++;return false;
}
'''
fixture += '\n#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO\n' + gate + '\n#endif\n' + prefix
fixture += r'''
    /* Visible stand-ins only after the actual dispatch prefix; normal API
     * semantics are covered by the existing stream/scope/lifecycle fixtures. */
    if(frame->rax==SYS_FILE_STREAM) {copies++;handles++;frame->rax=91;}
    else if(frame->rax==SYS_SESSION_SCOPE) {membership++;frame->rax=92;}
}
int main(void) {
    const uint64_t calls[]={SYS_SESSION_SCOPE,SYS_FILE_STREAM};
    for(unsigned i=0;i<2;i++) {
        backends=copies=handles=membership=fd_checks=0;
        struct isr_frame f={.rax=calls[i],.rdi=1,.rsi=UINT64_MAX,.rdx=1};
        syscall_dispatch_impl(&f);
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
        assert((int64_t)f.rax==SRV_ERR_NOTSUP);
        assert(!backends && !copies && !handles && !membership && !fd_checks);
#else
        assert(backends==1 && !fd_checks);
        assert(i==0 ? f.rax==92 && membership==1 && !copies && !handles
                    : f.rax==91 && !membership && copies==1 && handles==1);
#endif
    }
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    struct isr_frame f={.rax=SYS_WRITE,.rdi=1};
    assert(signal_experimental_raw_allowed(&f) && fd_checks==1);
    f.rdi=9;assert(!signal_experimental_raw_allowed(&f) && fd_checks==2);
    f.rax=SYS_RANDOM;assert(signal_experimental_raw_allowed(&f));
    puts("signal-admission: experimental scope/stream denied before effects");
#else
    puts("signal-admission: normal scope/stream dispatch retained");
#endif
}
'''
# The normal build does not compile the experimental descriptor helper.
fixture = fixture.replace('static bool process_signal_experimental_fd_allowed(uint64_t fd) {',
                          'static __attribute__((unused)) bool process_signal_experimental_fd_allowed(uint64_t fd) {')
src = args.output / 'admission.c'
src.write_text(fixture)
commands = []
profiles = [('strict', [])]
if args.sanitizers:
    profiles.append(('asan-ubsan', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']))
for mode in ['normal', 'experimental']:
    for name, flags in profiles:
        binary = args.output / (mode + '-' + name)
        argv = ['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                '-I', str(ROOT / 'shared/include'), *flags]
        if mode == 'experimental':
            argv.append('-DSRVROS_SIGNAL_EXPERIMENTAL_INFO=1')
        argv += [str(src), '-o', str(binary)]
        subprocess.run(argv, check=True, timeout=30)
        result = subprocess.run([str(binary.resolve())], check=True, text=True, capture_output=True, timeout=10)
        print(result.stdout.strip())
        commands.append({'argv': argv, 'status': result.returncode, 'stdout': result.stdout})
(args.output / 'receipt.json').write_text(json.dumps({
    'result': 'passed', 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
    'fixture_sha256': hashlib.sha256(src.read_bytes()).hexdigest(), 'commands': commands,
    'scope': 'actual raw gate and dispatch prefix; backend stand-ins after prefix; normal semantics require existing fixtures',
}, indent=2) + '\n')
