#!/usr/bin/env python3
"""Exercise actual SYS_RANDOM bodies and patched WTF against controlled providers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--upstream', type=Path, required=True, help='pinned unmodified RandomDevice.cpp')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--sdk', type=Path, default=ROOT / 'build/sysroot/srvros', help='actual retained SDK native headers')
parser.add_argument('--sanitizers', action='store_true')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


kernel = ROOT / 'kernel/src/arch/x86_64/syscall.c'
text = kernel.read_text()
parts = [function(text, sig) for sig in (
    'static bool rdrand64(', 'static uint64_t splitmix64_next(',
    'static uint64_t random_u64(', 'static bool random_entropy_u64(',
    'static int64_t syscall_random(')]
# Only the physical instruction is replaced. Its real feature gate and ten-try
# bounded retry loop remain in the fixture; host providers are deterministic.
instruction = '__asm__ volatile (\n            "rdrand %0; setc %1"\n            : "=r"(*value), "=qm"(ok));'
if parts[0].count(instruction) != 1:
    raise SystemExit('RDRAND instruction fixture anchor changed')
parts[0] = parts[0].replace(instruction, 'ok = test_rdrand_attempt(value);')
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <srvros/random.h>
#define MAX_SYSCALL_COPY (256 * 1024)
static unsigned pool_words, hw_words, hw_attempts, pool_calls, irq_depth, weak_calls;
static unsigned fail_attempts, copies, copy_fail_on;
static bool has_cpu, valid_buffer, zero_word;
static uint64_t random_state;
static uint64_t irq_save(void) {assert(!irq_depth++);return 9;}
static void irq_restore(uint64_t flags) {assert(flags==9 && irq_depth--==1);}
static bool cpu_has_rdrand(void) {return has_cpu;}
static bool virtio_rng_take_u64(uint64_t *v) {
    assert(irq_depth==1);pool_calls++;if(!pool_words)return false;
    pool_words--;*v=zero_word?0:UINT64_C(0x0807060504030201);return true;
}
static uint8_t test_rdrand_attempt(uint64_t *v) {
    assert(irq_depth==1);hw_attempts++;*v=UINT64_MAX;
    if(fail_attempts) {fail_attempts--;return 0;}
    if(!hw_words)return 0;
    hw_words--;*v=zero_word?0:UINT64_C(0x1817161514131211);return 1;
}
static uint64_t rdtsc(void) {weak_calls++;return 42;}
static uint64_t timer_ticks(void) {weak_calls++;return 2;}
static void *process_current(void) {weak_calls++;return NULL;}
static uint64_t process_pid(void *p) {(void)p;weak_calls++;return 7;}
static bool user_buffer_ok(const void *p,uint64_t n,bool writable) {
    assert(writable);return !n || (p && valid_buffer);
}
static bool copy_to_user(void *p,const void *q,uint64_t n) {
    assert(!irq_depth);copies++;if(copies==copy_fail_on)return false;
    memcpy(p,q,(size_t)n);return true;
}
static void reset(void) {
    pool_words=hw_words=hw_attempts=pool_calls=irq_depth=weak_calls=0;
    fail_attempts=copies=copy_fail_on=0;
    has_cpu=zero_word=false;valid_buffer=true;random_state=0;
}
'''
cases = r'''
int main(void) {
    uint8_t b[256];
    reset();memset(b,0xa5,sizeof b);
    assert(syscall_random(b,16,2)==-1 && !pool_calls && !copies);
    assert(syscall_random(b,16,3)==-1 && !pool_calls);
    assert(syscall_random(b,16,UINT64_MAX)==-1 && !pool_calls);
    assert(syscall_random(NULL,0,SRV_RANDOM_REQUIRE_ENTROPY)==0 && !pool_calls);
    assert(syscall_random(NULL,1,SRV_RANDOM_REQUIRE_ENTROPY)==-1 && !pool_calls);
    valid_buffer=false;
    assert(syscall_random(b,1,SRV_RANDOM_REQUIRE_ENTROPY)==-1 && !pool_calls);
    valid_buffer=true;
    assert(syscall_random(b,MAX_SYSCALL_COPY+1,SRV_RANDOM_REQUIRE_ENTROPY)==-1 && !pool_calls);
    assert(syscall_random(b,16,SRV_RANDOM_REQUIRE_ENTROPY)==SRV_RANDOM_UNAVAILABLE);
    assert(!copies && !hw_attempts && !weak_calls && b[0]==0xa5 && b[15]==0xa5);
    reset();has_cpu=true;
    assert(syscall_random(b,16,SRV_RANDOM_REQUIRE_ENTROPY)==SRV_RANDOM_UNAVAILABLE);
    assert(hw_attempts==10 && !copies && !weak_calls && b[0]==0xa5);
    reset();pool_words=1;has_cpu=true;hw_words=1;
    assert(syscall_random(b,24,SRV_RANDOM_REQUIRE_ENTROPY)==16);
    for(unsigned i=0;i<8;i++)assert(b[i]==i+1 && b[i+8]==i+17);
    assert(b[16]==0xa5 && copies==1 && hw_attempts==11 && !weak_calls);
    reset();pool_words=9;memset(b,0xa5,sizeof b);
    assert(syscall_random(b,96,SRV_RANDOM_REQUIRE_ENTROPY)==72);
    assert(copies==2 && b[71]==8 && b[72]==0xa5 && !weak_calls);
    reset();has_cpu=true;hw_words=1;fail_attempts=9;
    assert(syscall_random(b,7,SRV_RANDOM_REQUIRE_ENTROPY)==7 && hw_attempts==10 && !weak_calls);
    reset();pool_words=1;zero_word=true;memset(b,0xa5,sizeof b);
    assert(syscall_random(b,1,SRV_RANDOM_REQUIRE_ENTROPY)==1 && b[0]==0 && b[1]==0xa5);
    reset();pool_words=1;has_cpu=true;hw_words=1;
    assert(syscall_random(b,8,SRV_RANDOM_REQUIRE_ENTROPY)==8 && !hw_attempts && hw_words==1);
    reset();pool_words=16;copy_fail_on=2;memset(b,0xa5,sizeof b);
    assert(syscall_random(b,96,SRV_RANDOM_REQUIRE_ENTROPY)==-1 && copies==2 && b[63]==8 && b[64]==0xa5);
    reset();pool_words=8;memset(b,0xa5,sizeof b);
    assert(syscall_random(b,1,0)==1 && pool_calls==8 && b[0]==1 && b[1]==0xa5);
    reset();assert(syscall_random(b,1,0)==1 && weak_calls && pool_calls==8);
    assert(!irq_depth);
    puts("random-entropy: native contract PASS");
}
'''
native = args.output / 'native.c'
native.write_text(prefix + '\n'.join(parts) + cases)

upstream_hash = 'dceb6005e5d9762e65c7213cd28465847def2ec5cd8f9cd8323b486ba704390d'
if digest(args.upstream) != upstream_hash:
    raise SystemExit('expected pinned unmodified RandomDevice.cpp')
stage = args.output / 'applied'
source = stage / 'Source/WTF/wtf'
source.mkdir(parents=True, exist_ok=True)
shutil.copyfile(args.upstream, source / 'RandomDevice.cpp')
patches = [ROOT / 'ports/srvros/webkit' / name for name in ('random-entropy.patch', 'random-native-linkage.patch')]
for patch in patches:
    subprocess.run(['patch', '--batch', '--fuzz=0', '-p1', '-d', str(stage.resolve()), '-i', str(patch)], check=True)
from browser_random_contract import verify_random_contract
contract = verify_random_contract(ROOT, stage)
stub = args.output / 'stubs'
(stub / 'wtf').mkdir(parents=True, exist_ok=True)
sdk_headers = args.sdk / 'include/srvros'
shutil.copytree(sdk_headers, stub / 'srvros')
native_headers = {str(p.relative_to(args.sdk)): digest(p) for p in sdk_headers.rglob('*') if p.is_file()}
(stub / 'config.h').write_text('''#pragma once
#define OS_SRVROS 1
#define OS_UNIX 1
#define OS(x) OS_##x
#define WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#define WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
extern void test_assert(bool);
#define RELEASE_ASSERT(x) test_assert(x)
''')
upstream_header = args.upstream.with_name('RandomDevice.h')
shutil.copyfile(upstream_header, source / 'RandomDevice.h')
(stub / 'wtf/FastMalloc.h').write_text('#define WTF_DEPRECATED_MAKE_FAST_ALLOCATED(Type)\n')
(stub / 'wtf/Noncopyable.h').write_text('''#define WTF_MAKE_NONCOPYABLE(Type) \\
    Type(const Type&) = delete; Type& operator=(const Type&) = delete
''')
(stub / 'wtf/StdLibExtras.h').write_text('#include <span>\n#include <cstdint>\n')
wtf = args.output / 'wtf.cpp'
wtf.write_text(r'''
#include "config.h"
#include <wtf/RandomDevice.h>
#include <srvros/random.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
static long replies[8];static unsigned calls,reply_count;static size_t total;
static uint8_t *start;
void test_assert(bool ok) {if(!ok)throw std::runtime_error("closed");}
extern "C" long srv_random(void *p,size_t n,uint64_t flags) {
    assert(flags==SRV_RANDOM_REQUIRE_ENTROPY && n>0 && n<=256);
    assert(p==start+total && calls<reply_count);
    long result=replies[calls++];
    if(result>0 && (size_t)result<=n) {memset(p,0x5a,result);total+=result;}
    return result;
}
static void reset(uint8_t *p,std::initializer_list<long> values) {
    calls=0;total=0;reply_count=values.size();start=p;
    unsigned i=0;for(long v:values)replies[i++]=v;
}
int main(void) {
    WTF::RandomDevice device;uint8_t b[600]={0};
    reset(b,{});device.cryptographicallyRandomValues({b,0});assert(!calls);
    reset(b,{256,1,255,88});device.cryptographicallyRandomValues(b);
    assert(calls==4 && total==sizeof b && b[599]==0x5a);
    for(long failure:{-1L,(long)SRV_RANDOM_UNAVAILABLE,0L,257L}) {
        reset(b,{failure});bool closed=false;
        try {device.cryptographicallyRandomValues(b);}catch(const std::runtime_error&) {closed=true;}
        assert(closed && calls==1 && !total);
    }
    reset(b,{8,SRV_RANDOM_UNAVAILABLE});bool closed=false;
    try {device.cryptographicallyRandomValues(b);}catch(const std::runtime_error&) {closed=true;}
    assert(closed && calls==2 && total==8);
    puts("random-entropy: actual WTF adapter PASS");
}
''')
commands = []
profiles = [('strict', [])]
if args.sanitizers:
    profiles.append(('asan-ubsan', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']))
for profile, flags in profiles:
    for label, compiler, inputs in (
        ('native', 'clang', ['-std=c11', '-I', str(ROOT / 'shared/include'), str(native)]),
        ('wtf', 'clang++', ['-std=c++20', '-I', str(stub), '-I', str(source.parent),
                          '-I', str(ROOT / 'shared/include'), str(source / 'RandomDevice.cpp'), str(wtf)])):
        binary = args.output / (label + '-' + profile)
        argv = [compiler, '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags, *inputs, '-o', str(binary)]
        subprocess.run(argv, check=True, timeout=60)
        result = subprocess.run([str(binary.resolve())], check=True, text=True, capture_output=True, timeout=10)
        print(result.stdout.strip())
        commands.append({'argv': argv, 'status': result.returncode, 'stdout': result.stdout, 'binary_sha256': digest(binary)})
receipt = {'result': 'passed', 'kernel_sha256': digest(kernel),
           'patch_sha256': {p.name: digest(p) for p in patches}, 'actual_sdk_native_headers': native_headers,
           'upstream_sha256': upstream_hash, 'upstream_class_header_sha256': digest(upstream_header),
           'header_contract': contract, 'commands': commands,
           'scope': 'actual kernel bodies, mocked providers and instruction; actual patched WTF with native stub'}
(args.output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
