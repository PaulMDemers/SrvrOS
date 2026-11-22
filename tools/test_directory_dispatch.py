#!/usr/bin/env python3
"""Compile the actual directory helpers and public namespace dispatcher together."""
import argparse, hashlib, json, os
from pathlib import Path
import shlex, subprocess
if not __debug__:
    raise RuntimeError('assertions are required')
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--dispatcher-source',type=Path)
p.add_argument('--expect-failure',action='store_true')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=(root/'kernel/src/arch/x86_64/syscall.c').read_text()
helpers=source[source.index('static bool syscall_directory_try_read_info('):source.index('static int64_t syscall_path_context(')]
outer_source=a.dispatcher_source.read_text() if a.dispatcher_source else source
outer=outer_source[outer_source.index('static bool namespace_syscall('):]
prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <srvros/syscall_numbers.h>
#include <srvros/path_limits.h>
#define MAX_PATH_LENGTH SRV_PATH_MAX
struct isr_frame {uint64_t rax,rdi,rsi,rdx,rcx,r8,r9;};
static bool irq_enabled=true,foreign,user_safe,eligible,mapped,killed;
static bool kill_on_wait,unmap_on_wait,replace_on_wait,fail_begin;
static unsigned held,waits,begins,ends,attempts,reads,copies,cursor,fd_generation;
static uint64_t irq_save(void) {uint64_t f=irq_enabled?512:0;irq_enabled=false;return f;}
static void irq_restore(uint64_t f) {if(f&512)irq_enabled=true;}
static bool user_buffer_ok(void *out,uint64_t n,bool write) {(void)write;assert(!irq_enabled);return out && n && mapped;}
static bool copy_to_user(void *out,const void *in,uint64_t n) {assert(!irq_enabled && mapped);memcpy(out,in,n);copies++;return true;}
static bool process_should_exit_current(void) {return killed;}
static bool process_directory_try_read_info(uint64_t fd,void *out,uint64_t cap,int64_t *result) {
    assert(!irq_enabled && cap<=SRV_DIRECTORY_BATCH_MAX*sizeof(struct srv_dirent_info));attempts++;
    if(!eligible || !foreign || held || !user_safe || fd!=3)return false;
    *(struct srv_dirent_info *)out=(struct srv_dirent_info){.size=77};*result=1;cursor++;return true;
}
static int64_t process_directory(uint64_t op,uint64_t fd,void *out,uint64_t cap) {
    assert(!irq_enabled && held==1 && !foreign && !user_safe && !killed);reads++;
    if(fd!=3)return SRV_ERR_BADF;
    if(op==SRV_DIRECTORY_CHDIR)return 0;
    if(op==SRV_DIRECTORY_READ_INFO) {assert(cap==sizeof(struct srv_dirent_info));*(struct srv_dirent_info *)out=(struct srv_dirent_info){.size=fd_generation==2?222:88};cursor++;return 1;}
    if(op==SRV_DIRECTORY_PATH) {assert(cap>=6);memcpy(out,"/home",6);return 0;}
    assert(op==SRV_DIRECTORY_READ && cap==sizeof(struct srv_dirent));*(struct srv_dirent *)out=(struct srv_dirent){.inode=91};cursor++;return 1;
}
static void scheduler_io_set_user_safe(bool safe) {assert(!safe);user_safe=safe;}
static void scheduler_io_wait(void) {assert(!irq_enabled && !user_safe);waits++;foreign=false;if(kill_on_wait)killed=true;if(unmap_on_wait)mapped=false;if(replace_on_wait)fd_generation=2;}
static bool scheduler_io_begin(void) {assert(!irq_enabled && !foreign && !user_safe && !held);begins++;if(fail_begin)return false;held=1;return true;}
static void scheduler_io_end(void) {assert(!irq_enabled && held==1);held=0;ends++;}
'''
bridge=r'''
static void syscall_dispatch_impl(struct isr_frame *f) {
    if(f->rax==SYS_DIRECTORY)f->rax=(uint64_t)syscall_directory(f->rdi,f->rsi,(void *)f->rdx,f->rcx);
    else {assert(f->rax==SYS_OPEN && held==1 && !foreign && !user_safe);f->rax=92;}
}
'''
main=r'''
static void reset(void) {
    assert(!held);irq_enabled=foreign=user_safe=eligible=mapped=true;killed=false;
    kill_on_wait=unmap_on_wait=replace_on_wait=fail_begin=false;
    waits=begins=ends=attempts=reads=copies=cursor=0;fd_generation=1;
}
static struct isr_frame request(uint64_t op,void *out,uint64_t cap) {return (struct isr_frame){.rax=SYS_DIRECTORY,.rdi=op,.rsi=3,.rdx=(uintptr_t)out,.rcx=cap};}
int main(void) {
    (void)syscall_directory_try_read_info; /* Retain candidate helper in the old-wrapper control. */
    struct srv_dirent_info out={0};struct isr_frame f;
    reset();f=request(SRV_DIRECTORY_READ_INFO,&out,sizeof(out));syscall_dispatch(&f);
    assert(!waits && !begins && !ends && "metadata prelude must run before namespace ownership");
    assert((int64_t)f.rax==1 && out.size==77 && foreign && user_safe && irq_enabled && attempts==1 && !reads && copies==1 && cursor==1);
    reset();eligible=false;out.size=123;replace_on_wait=true;f=request(SRV_DIRECTORY_READ_INFO,&out,sizeof(out));syscall_dispatch(&f);
    assert((int64_t)f.rax==1 && out.size==222 && attempts==1 && reads==1 && waits==1 && begins==1 && ends==1 && cursor==1 && irq_enabled && !held);
    reset();eligible=false;kill_on_wait=true;f=request(SRV_DIRECTORY_READ_INFO,&out,sizeof(out));syscall_dispatch(&f);
    assert((int64_t)f.rax==SRV_ERR_INTR && !reads && !copies && !cursor && begins==1 && ends==1 && irq_enabled);
    reset();eligible=false;unmap_on_wait=true;f=request(SRV_DIRECTORY_READ_INFO,&out,sizeof(out));syscall_dispatch(&f);
    assert((int64_t)f.rax==SRV_ERR_FAULT && !reads && !copies && !cursor && begins==1 && ends==1);
    reset();eligible=false;fail_begin=true;f=request(SRV_DIRECTORY_READ_INFO,&out,sizeof(out));syscall_dispatch(&f);
    assert((int64_t)f.rax==SRV_ERR_IO && !reads && !cursor && begins==1 && !ends);
    reset();f=request(SRV_DIRECTORY_READ_INFO,&out,sizeof(out));f.rsi=99;syscall_dispatch(&f);
    assert((int64_t)f.rax==SRV_ERR_BADF && waits==1 && begins==1 && ends==1 && reads==1 && !copies && !cursor);
    reset();f=request(SRV_DIRECTORY_READ_INFO,&out,sizeof(out)-1);syscall_dispatch(&f);
    assert((int64_t)f.rax==SRV_ERR_INVAL && !attempts && !reads && !waits && !begins);
    reset();mapped=false;f=request(SRV_DIRECTORY_READ_INFO,&out,sizeof(out));syscall_dispatch(&f);
    assert((int64_t)f.rax==SRV_ERR_FAULT && !attempts && !reads && !waits && !begins);
    const uint64_t operations[]={SRV_DIRECTORY_READ,SRV_DIRECTORY_PATH,SRV_DIRECTORY_CHDIR};
    for(unsigned i=0;i<3;i++) {
        union {char path[MAX_PATH_LENGTH];struct srv_dirent entry;} buffer={0};uint64_t cap=i==0?sizeof(buffer.entry):sizeof(buffer.path);
        reset();f=request(operations[i],&buffer,cap);syscall_dispatch(&f);
        assert(!attempts && waits==1 && begins==1 && ends==1 && reads==1 && irq_enabled);
    }
    reset();f=(struct isr_frame){.rax=SYS_OPEN};syscall_dispatch(&f);assert(f.rax==92 && waits==1 && begins==1 && ends==1 && !attempts);
    puts("directory-dispatch: PASS actual public wrapper/prelude; foreign owner retained, complete owned fallback, fresh FD/mapping, cancellation, acquire failure and unchanged other operations");
}
'''
fixture=prefix+helpers+bridge+outer+main
src=a.output/'fixture.c';src.write_text(fixture);binary=a.output/'fixture'
command=shlex.split(os.environ.get('CC','cc'))+['-std=c11','-O2','-Wall','-Wextra','-Werror','-I',str(root/'shared/include'),str(src),'-o',str(binary)]
subprocess.run(command,check=True)
result=subprocess.run([str(binary)],capture_output=True,text=True)
if a.expect_failure:
    assert result.returncode<0 and 'metadata prelude must run before namespace ownership' in result.stderr,(result.returncode,result.stdout,result.stderr)
    status='BASELINE_FAILS_PUBLIC_DISPATCH_REGRESSION'
else:
    assert result.returncode==0,(result.stdout,result.stderr)
    status='PASS_PUBLIC_DISPATCH_REGRESSION'
receipt=dict(status=status,source_sha256=hashlib.sha256(source.encode()).hexdigest(),dispatcher_sha256=hashlib.sha256(outer_source.encode()).hexdigest(),fixture_sha256=hashlib.sha256(fixture.encode()).hexdigest(),returncode=result.returncode,stdout=result.stdout,stderr=result.stderr,scope='Actual production helper and public namespace dispatcher text; backend stand-ins preserve ownership effects. Original process/VFS/scheduler fixtures are separate.')
(a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(status);print(result.stdout.strip())
