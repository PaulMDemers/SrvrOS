/* Actual copied-user adapter and explicit raw-entry route. Process reservation
 * ownership has its own real-process fixture; this seam controls its outcomes
 * to exercise user faults, late refusal and fresh ordinary fallback precisely. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
static uint64_t entry_flags;
static unsigned enabled_restores;
static uint64_t syscall_copy_test_irq_save(void) {uint64_t f=entry_flags;entry_flags&=~0x200ull;return f;}
static void syscall_copy_test_irq_restore(uint64_t f) {entry_flags=f;if(f&0x200)enabled_restores++;}
#define SRVROS_SYSCALL_COPY_HOST_TEST
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"

static _Alignas(4096) uint8_t source[65536],tail[65536];
static uint64_t mapped_bytes,allowed_count;
static bool eligible,decline,late_fault,busy,user_safe,kill_on_wait,killed,replace_on_wait,revoke_on_wait;
static bool private_progress,private_kill,private_fault;
static unsigned handler_records,wait_retries;
static unsigned validations,tries,candidate_calls,fast_calls,invalidations,gate_calls,gate_records,normal_writes,published,reservations;
static uint64_t last_requested;static int64_t last_result;
static struct process_file file;
static jmp_buf cancelled;
__attribute__((noinline)) bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(!writable && !(entry_flags&0x200));validations++;
    uintptr_t base=(uintptr_t)source;
    if(address<base || address-base>mapped_bytes || length>mapped_bytes-(address-base))return false;
    /* First validation covers the complete request, then page copies repeat
     * their own validation. Revoke only at the second copied page. */
    if(late_fault && validations>=3 && address>=base+4096)return false;
    return true;
}
struct process *process_current(void) {return (struct process *)(uintptr_t)1;}
struct process_file *process_file_at(struct process *p,uint64_t fd) {assert(p==process_current());return fd==4?&file:NULL;}
bool process_file_nonblocking(struct process *p,uint64_t fd) {(void)p;(void)fd;return false;}
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) {(void)p;(void)fd;(void)input;return false;}
bool process_file_pipe_io_safe(struct process *p,uint64_t fd,uint64_t n,bool writing) {(void)p;(void)fd;(void)n;(void)writing;assert(0);return false;}
bool process_file_reserved_append_entry_candidate(struct process *p,uint64_t fd,uint64_t n) {
    assert(p==process_current() && fd==4 && n && n<=65536 && !entry_flags);candidate_calls++;return eligible;
}
bool process_file_try_reserved_append(struct process *p,uint64_t fd,uint64_t n,process_reserved_append_copy_fn copy,void *context,int64_t *result) {
    assert(p==process_current() && fd==4 && n && n<=65536 && !entry_flags && user_safe);tries++;
    if(decline)return false;
    assert(allowed_count && allowed_count<=n);
    if(!copy(context,tail,allowed_count,n)){*result=SRV_ERR_FAULT;return true;}
    published=(unsigned)allowed_count;*result=(int64_t)allowed_count;return true;
}
bool process_file_wait_reserved_append(struct process *p,uint64_t fd,uint64_t n) {
    assert(p==process_current() && fd==4 && n && n<=65536 && !entry_flags && user_safe);
    if(!private_progress)return false;
    assert(decline);private_progress=false;decline=false;wait_retries++;source[0]=0x3c;
    if(private_kill)killed=true;
    if(private_fault)mapped_bytes=0;
    return true;
}
int64_t process_file_reserve_append_ram(struct process *p,uint64_t fd,uint64_t limit) {
    assert(p==process_current() && fd==4 && limit==65536);reservations++;return -7;
}
void process_capture_write_fast_result(uint64_t n,int64_t result) {
    assert(user_safe && !entry_flags && !gate_calls);fast_calls++;last_requested=n;last_result=result;
}
void process_capture_write_invalidate(void) {invalidations++;}
uint64_t process_capture_write_clock(void) {return 10;}
void process_capture_write_record(enum capture_write_site site,uint64_t at) {assert(at==10 && (site==CAP_WRITE_GATE || site==CAP_WRITE_HANDLER));if(site==CAP_WRITE_GATE)gate_records++;else handler_records++;}
void scheduler_io_set_user_safe(bool value) {user_safe=value;}
bool scheduler_io_active(void) {return busy;}
bool scheduler_io_busy(void) {return busy;}
void scheduler_io_request_turn(void) {assert(0);}
void scheduler_io_wait(void) {
    assert(!user_safe && !entry_flags);gate_calls++;busy=false;
    if(kill_on_wait)killed=true;
    if(replace_on_wait){file.handle=2;source[0]=0x7d;}
    if(revoke_on_wait)mapped_bytes=0;
}
void process_capture_write_gate_wait(void) {scheduler_io_wait();}
uint64_t latency_clock(void) {return 1;}
void latency_gate(uint64_t at,uint64_t call) {assert(at==1 && call==SYS_WRITE);}
bool process_should_exit_current(void) {return killed;}
bool process_signal_pending_current(void) {return false;}
uint64_t process_current_kill_status(void) {assert(killed);return 137;}
__attribute__((noreturn)) void process_exit(uint64_t status) {assert(status==137 && !busy && !user_safe && !normal_writes);longjmp(cancelled,1);}
int64_t process_file_write(struct process *p,uint64_t fd,const uint8_t *data,uint64_t n) {
    assert(p==process_current() && fd==4 && !busy && !user_safe && !entry_flags);normal_writes++;
    if(replace_on_wait)assert(file.handle==2 && data[0]==0x7d);
    return (int64_t)n;
}
/* Unselected descriptor branches must never become side effects of this test. */
int64_t process_file_read(struct process *p,uint64_t fd,uint8_t *b,uint64_t n) {(void)p;(void)fd;(void)b;(void)n;assert(0);return -1;}
int64_t process_file_pread(struct process *p,uint64_t fd,uint8_t *b,uint64_t n,int64_t o) {(void)p;(void)fd;(void)b;(void)n;(void)o;assert(0);return -1;}
int64_t process_file_pwrite(struct process *p,uint64_t fd,const uint8_t *b,uint64_t n,int64_t o) {(void)p;(void)fd;(void)b;(void)n;(void)o;assert(0);return -1;}

int64_t process_file_pipe_write_checked(struct process *p,uint64_t fd,uint64_t n,bool nw,process_pipe_copyin c,void *x) {(void)p;(void)fd;(void)n;(void)nw;(void)c;(void)x;assert(0);return -1;}
int64_t process_file_pty_write_checked(struct process *p,uint64_t fd,uint64_t n,process_pipe_copyin c,void *x) {(void)p;(void)fd;(void)n;(void)c;(void)x;assert(0);return -1;}
int64_t process_file_pipe_write(struct process *p,uint64_t fd,const uint8_t *b,uint64_t n) {(void)p;(void)fd;(void)b;(void)n;assert(0);return -1;}
uint64_t process_fd_generation(struct process *p,uint64_t fd) {(void)p;(void)fd;assert(0);return 0;}
void *kmalloc(size_t n) {(void)n;assert(0);return NULL;}
void kfree(void *p) {(void)p;assert(0);}
int64_t net_respond_checked(uint64_t h,const char *b,uint64_t n,bool nonblock,const struct net_transfer *t) {(void)h;(void)b;(void)n;(void)nonblock;(void)t;assert(0);return -1;}
int64_t net_udp_send_checked(uint64_t h,uint32_t ip,uint16_t port,const uint8_t *b,uint64_t n,bool nonblock,const struct net_transfer *t) {(void)h;(void)ip;(void)port;(void)b;(void)n;(void)nonblock;(void)t;assert(0);return -1;}
size_t console_write_n_checked(const char *b,size_t n,bool(*ready)(void *),void *x) {(void)b;(void)n;(void)ready;(void)x;assert(0);return 0;}
int64_t process_file_dup_min(struct process *p,uint64_t fd,uint64_t min,bool ce) {(void)p;(void)fd;(void)min;(void)ce;assert(0);return -1;}
int64_t process_file_get_fd_flags(struct process *p,uint64_t fd) {(void)p;(void)fd;assert(0);return -1;}
int64_t process_file_set_fd_flags(struct process *p,uint64_t fd,uint64_t flags) {(void)p;(void)fd;(void)flags;assert(0);return -1;}
int64_t process_file_get_flags(struct process *p,uint64_t fd) {(void)p;(void)fd;assert(0);return -1;}
int64_t process_file_set_flags(struct process *p,uint64_t fd,uint64_t flags) {(void)p;(void)fd;(void)flags;assert(0);return -1;}
int64_t process_file_get_lock(struct process *p,uint64_t fd,struct srv_flock *l) {(void)p;(void)fd;(void)l;assert(0);return -1;}
int64_t process_file_set_lock(struct process *p,uint64_t fd,const struct srv_flock *l,bool wait) {(void)p;(void)fd;(void)l;(void)wait;assert(0);return -1;}
static void reset_case(void) {
    private_progress=private_kill=private_fault=false;handler_records=wait_retries=0;
    memset(source,0x5a,sizeof(source));memset(tail,0xa5,sizeof(tail));mapped_bytes=sizeof(source);allowed_count=32;
    entry_flags=0;enabled_restores=validations=tries=candidate_calls=fast_calls=invalidations=gate_calls=gate_records=normal_writes=published=reservations=0;
    eligible=busy=user_safe=true;decline=late_fault=kill_on_wait=killed=replace_on_wait=revoke_on_wait=false;
    last_requested=0;last_result=0;file=(struct process_file){.used=true,.type=PROCESS_FILE_VFS_WRITE,.handle=1};
}
int main(void) {
    reset_case();struct isr_frame f={.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=65536};
#if defined(SRVROS_SIGNAL_EXPERIMENTAL_INFO) || defined(SRVROS_SIGNAL_PUBLIC_GUEST_TEST)
    assert(!syscall_reserved_append_candidate(&f));
    assert(syscall_fcntl(4,SRV_F_RESERVE_APPEND_RAM,65536)==SRV_ERR_NOTSUP && !reservations && !tries && !candidate_calls);
    puts("reserved-append-syscall: PASS extension disabled in controlled signal image");
#else
    assert(syscall_reserved_append_candidate(&f) && candidate_calls==1 && !validations);
    assert(!syscall_io_safe(&f)); /* No general writable-file safety widening. */
    struct isr_frame reserve_frame={.cs=3,.rax=SYS_FCNTL,.rdi=4,.rsi=SRV_F_RESERVE_APPEND_RAM,.rdx=65536};
    assert(!syscall_io_safe(&reserve_frame));
    for(unsigned i=0;i<4;i++) {
        struct isr_frame bad=f;
        if(i==0)bad.cs=0;
        if(i==1)bad.rax=SYS_PWRITE;
        if(i==2)bad.rdi=2;
        if(i==3)bad.rdx=65537;
        assert(!syscall_reserved_append_candidate(&bad));
    }
    struct isr_frame zero=f;zero.rdx=0;assert(!syscall_reserved_append_candidate(&zero));
    eligible=false;assert(!syscall_reserved_append_candidate(&f));eligible=true;
    allowed_count=65536;assert(syscall_reserved_append_dispatch(&f) && f.rax==65536 && published==65536 &&
        fast_calls==1 && last_requested==65536 && last_result==65536 && !memcmp(source,tail,65536) && !enabled_restores && !gate_calls);
    reset_case();f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=1024};
    allowed_count=32;mapped_bytes=128;
    assert(syscall_reserved_append_dispatch(&f) && (int64_t)f.rax==SRV_ERR_FAULT && !published &&
        fast_calls==1 && last_requested==1024 && last_result==SRV_ERR_FAULT && validations==1 && tail[0]==0xa5);
    reset_case();f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=1024};
    assert(syscall_reserved_append_dispatch(&f) && f.rax==32 && published==32 && !memcmp(source,tail,32) && tail[32]==0xa5);
    reset_case();late_fault=true;allowed_count=8192;f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=8192};
    assert(syscall_reserved_append_dispatch(&f) && (int64_t)f.rax==SRV_ERR_FAULT && !published &&
        !memcmp(source,tail,4096) && tail[4096]==0xa5 && !enabled_restores && fast_calls==1);
    reset_case();f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=UINT64_MAX-7,.rdx=32};
    assert(syscall_reserved_append_dispatch(&f) && (int64_t)f.rax==SRV_ERR_FAULT && !published && !gate_calls);
    reset_case();f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=32};
    assert(syscall_reserved_append_candidate(&f));decline=replace_on_wait=true;
    assert(!syscall_reserved_append_dispatch(&f) && f.rax==32 && invalidations==0 && gate_calls==1 && gate_records==1 &&
        normal_writes==1 && !fast_calls && !user_safe && !enabled_restores);
    reset_case();decline=revoke_on_wait=true;f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=32};
    assert(!syscall_reserved_append_dispatch(&f) && (int64_t)f.rax==SRV_ERR_FAULT &&
        invalidations==0 && gate_calls==1 && !normal_writes && !fast_calls);
    reset_case();decline=kill_on_wait=true;f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=32};
    if(!setjmp(cancelled)){(void)syscall_reserved_append_dispatch(&f);assert(0);}
    assert(invalidations==0 && !normal_writes && !fast_calls && killed);
    reset_case();decline=private_progress=true;f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=32};
    assert(syscall_reserved_append_dispatch(&f) && f.rax==32 && wait_retries==1 && tries==2 &&
        tail[0]==0x3c && fast_calls==1 && handler_records==1 && !gate_calls && !invalidations);
    reset_case();decline=private_progress=private_fault=true;f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=32};
    assert(syscall_reserved_append_dispatch(&f) && (int64_t)f.rax==SRV_ERR_FAULT && wait_retries==1 &&
        !published && tail[0]==0xa5 && handler_records==1 && !gate_calls);
    reset_case();decline=private_progress=private_kill=true;f=(struct isr_frame){.cs=3,.rax=SYS_WRITE,.rdi=4,.rsi=(uint64_t)source,.rdx=32};
    if(!setjmp(cancelled)){(void)syscall_reserved_append_dispatch(&f);assert(0);}
    assert(wait_retries==1 && tries==1 && !published && !fast_calls && !normal_writes && gate_calls==2);
    reset_case();assert(syscall_fcntl(4,SRV_F_RESERVE_APPEND_RAM,65536)==-7 && reservations==1);
    assert(syscall_fcntl(4,9999,0)==-1 && reservations==1); /* Old/default rejection contract. */
    puts("reserved-append-syscall: PASS exact raw token, full-request/FSIZE-short user proof, partial hidden-tail fault, bounded copy, fresh gated refusal/cancellation and explicit reserve command");
#endif
}
