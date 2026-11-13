#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static bool console_fd,busy,user_safe,startup_killed,kill_on_wait,invalidate_on_wait;
static bool exec_test,exec_private,trace_on;
static unsigned trace_fcntls,trace_entries,trace_requests;
static unsigned exec_attempts,exec_spawns,exec_refreshes;
static const char *exec_user_path,*exec_user_arg,*exec_user_env;
bool process_should_exit_current(void) { return startup_killed; }
bool scheduler_io_active(void) {return busy;}
uint64_t latency_clock(void) {return 1;}
static unsigned gate_samples;
void latency_gate(uint64_t at,uint64_t call) {assert(at==1 && (call==SYS_MMAP || call==SYS_THREAD_CREATE || call==SYS_NET_STATUS_INFO));gate_samples++;}
static unsigned turn_requests;
static bool retired;
static bool wait_pointer_ok=true,scope_write_ok=true;static unsigned wait_attempts;
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool write) {(void)address;(void)length;return wait_pointer_ok && (!write || scope_write_ok);}
bool process_try_wait_retired_checked(uint64_t pid,process_wait_copy_fn copy,void *context,int64_t *result) {assert(pid==42);wait_attempts++;if(!retired)return false;*result=copy(context,9)?42:SRV_ERR_FAULT;return true;}
int64_t process_wait_checked(int64_t pid,uint64_t flags,process_wait_copy_fn copy,void *context) {assert(pid==42 && !busy && !user_safe && (flags&SRV_WAIT_NOHANG));return copy(context,0)?0:SRV_ERR_FAULT;}
int64_t process_wait_posix(int64_t pid,uint64_t *status,bool nohang) {(void)pid;(void)status;(void)nohang;assert(0);return -1;}
static bool byte_safe;
bool process_file_pipe_io_safe(struct process *p,uint64_t fd,uint64_t length,bool writing) {
    (void)p;(void)writing;return fd==10 && length<=4096 && byte_safe;
}
void scheduler_io_request_turn(void) { assert(busy);turn_requests++; }
static struct process_file pipe_file;
struct process_file *process_file_at(struct process *p,uint64_t fd) { (void)p; return fd==10?&pipe_file:NULL; }
bool process_file_nonblocking(struct process *p,uint64_t fd) { struct process_file *f=process_file_at(p,fd);return f && (f->flags&SRV_FD_NONBLOCK); }
bool scheduler_io_busy(void) { return busy; }
void scheduler_io_set_user_safe(bool value) { user_safe=value; }
struct process *process_current(void) { return (struct process *)1; }
bool process_fd_is_console(struct process *p,uint64_t fd,bool input) {
    assert(p==(struct process *)1);
    if(fd==10) return false;
    assert(input ? fd==0 : fd==1);
    return console_fd;
}
static bool close_pipe;
static unsigned close_attempts,close_waits,slow_closes;
bool process_file_try_close_pipe(struct process *p,uint64_t fd,int64_t *result) {
    assert(p==(struct process *)1);close_attempts++;
    if(fd!=10 || !close_pipe)return false;
    *result=0;return true;
}
void scheduler_io_wait(void) {
    assert(!user_safe);close_waits++;busy=false;pipe_file.handle=456;
    if(kill_on_wait)startup_killed=true;
    if(invalidate_on_wait)wait_pointer_ok=false;
}
int64_t process_file_close(struct process *p,uint64_t fd) {
    assert(p==(struct process *)1 && fd==10 && !user_safe && !busy && pipe_file.handle==456);
    slow_closes++;return -7;
}
static void test_close_admission(void) {
    struct isr_frame frame={.rax=SYS_CLOSE,.rdi=10};
    assert(syscall_io_safe(&frame)); /* Prelude only, not general close permission. */
    busy=user_safe=close_pipe=true;assert(syscall_close(10)==0);
    assert(busy && user_safe && close_attempts==1 && !close_waits && !slow_closes);
    close_pipe=false;pipe_file.handle=123;
    assert(syscall_close(10)==-7);
    assert(!busy && !user_safe && close_attempts==2 && close_waits==1 && slow_closes==1);
    puts("pipe-close-admission: PASS atomic attempt or gated fresh-lookup fallback");
}
static bool private_unmap;
static unsigned unmap_calls,unmap_slow,unmap_handoffs;
void scheduler_io_handoff(void) { assert(user_safe && !busy);unmap_handoffs++; }
bool process_try_munmap_private(struct process *p,uint64_t address,uint64_t length,int64_t *result) {
    assert(p==(struct process *)1 && address==0x400000 && length==8192);unmap_calls++;
    if(!private_unmap)return false;
    *result=0;return true;
}
int64_t process_munmap(struct process *p,uint64_t address,uint64_t length) {
    assert(p==(struct process *)1 && address==0x400000 && length==8192 && !busy && !user_safe);
    unmap_slow++;return -8;
}
static void test_unmap_admission(void) {
    struct isr_frame f={.rax=SYS_MUNMAP};assert(syscall_io_safe(&f));
    unsigned before=close_waits;busy=user_safe=private_unmap=true;
    assert(!syscall_munmap(0x400000,8192) && busy && user_safe && close_waits==before && unmap_calls==1 && !unmap_handoffs);
    private_unmap=false;assert(syscall_munmap(0x400000,8192)==-8);
    assert(!busy && !user_safe && close_waits==before+1 && unmap_calls==2 && unmap_slow==1 && unmap_handoffs==1);
    puts("private-unmap-admission: PASS atomic attempt or ordinary gated fallback; unsupported memory remains gated");
}

static bool startup_fast;static unsigned startup_attempts,startup_slow;
bool process_try_mmap_private(struct process *p,uint64_t address,uint64_t length,uint64_t prot,uint64_t flags,int64_t fd,int64_t offset,int64_t *result) {
    (void)p;(void)address;(void)length;(void)prot;(void)flags;(void)fd;(void)offset;startup_attempts++;
    if(!startup_fast)return false;
    *result=0x500000;return true;
}
int64_t process_mmap(struct process *p,uint64_t address,uint64_t length,uint64_t prot,uint64_t flags,int64_t fd,int64_t offset) {
    (void)p;(void)address;(void)length;(void)prot;(void)flags;(void)fd;(void)offset;
    assert(!busy && !user_safe);startup_slow++;return -9;
}
bool process_try_thread_create_private(uint64_t e,uint64_t a,uint64_t s,uint64_t f,int64_t *result) {
    (void)e;(void)a;(void)s;(void)f;startup_attempts++;if(!startup_fast)return false;*result=2;return true;
}
int64_t process_thread_create(uint64_t e,uint64_t a,uint64_t s,uint64_t f) {
    (void)e;(void)a;(void)s;(void)f;assert(!busy && !user_safe);startup_slow++;return -10;
}
static void test_startup_admission(void) {
    struct isr_frame f={.rax=SYS_MMAP};assert(syscall_io_safe(&f));
    f.rax=SYS_THREAD_CREATE;assert(syscall_io_safe(&f));
    unsigned waits=close_waits,handoffs=unmap_handoffs;
    busy=user_safe=startup_fast=true;
    assert(syscall_mmap(0,4096,3,9,-1,0)==0x500000);
    assert(syscall_thread_create(0x400000,123,0x500800,0)==2);
    assert(busy && user_safe && startup_attempts==2 && !startup_slow && close_waits==waits && unmap_handoffs==handoffs);
    startup_fast=false;
    assert(syscall_mmap(0,4096,3,9,-1,0)==-9);
    busy=user_safe=true;assert(syscall_thread_create(0x400000,123,0x500800,0)==-10);
    assert(!busy && !user_safe && startup_slow==2 && close_waits==waits+2 && unmap_handoffs==handoffs+2);
    busy=user_safe=true;wait_pointer_ok=false;
    assert(syscall_thread_create(0x400000,123,0x500800,0)==-1 && !busy && !user_safe && unmap_handoffs==handoffs+3);
    wait_pointer_ok=true;
    busy=user_safe=kill_on_wait=true;unsigned slow=startup_slow;
    assert(syscall_thread_create(0x400000,123,0x500800,0)==-1 && startup_killed && startup_slow==slow);
    startup_killed=false;busy=user_safe=true;
    assert(syscall_mmap(0,4096,3,9,-1,0)==-1 && startup_killed && startup_slow==slow);
    assert(gate_samples==5);startup_killed=kill_on_wait=false;
    busy=user_safe=invalidate_on_wait=true;
    assert(syscall_thread_create(0x400000,123,0x500800,0)==-1 && startup_slow==slow && !wait_pointer_ok);
    assert(gate_samples==6);invalidate_on_wait=false;wait_pointer_ok=true;
    puts("startup-admission: PASS sole-process attempt or ordinary gated fallback, invalid-buffer handoff, no foreign gate mutation");
}
static void test_pipe_retry_admission(void) {
    const struct { enum process_file_type type; bool read,write; } cases[]={
        {PROCESS_FILE_PIPE_READ,true,false},{PROCESS_FILE_PIPE_WRITE,false,true},
        {PROCESS_FILE_PIPE_DUPLEX,true,true},{PROCESS_FILE_PTY_MASTER,true,true},
        {PROCESS_FILE_PTY_SLAVE,true,true},{PROCESS_FILE_VFS,false,false},
        {PROCESS_FILE_VFS_WRITE,false,false},{PROCESS_FILE_NET_CONNECTION,false,false}
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        pipe_file.type=cases[i].type;
        for(unsigned nonblock=0;nonblock<2;nonblock++) {
            pipe_file.flags=nonblock?SRV_FD_NONBLOCK:0;
            for(unsigned write=0;write<2;write++) {
                bool eligible=nonblock && (write?cases[i].write:cases[i].read);
                unsigned before=turn_requests;
                busy=true;user_safe=true;
                /* Each retry must request quiescence without blocking or
                 * entering descriptor cleanup under the other I/O owner. */
                for(unsigned retry=0;retry<64;retry++) {
                    assert((write?io_defer_pipe_write(10,4096):io_defer_pipe_read(10,4096))==eligible);
                    assert(user_safe && turn_requests==before+(eligible?retry+1:0));
                }
                before=turn_requests;busy=false;user_safe=true;
                assert(!(write?io_defer_pipe_write(10,4096):io_defer_pipe_read(10,4096)));
                assert(turn_requests==before && user_safe==!eligible);
            }
        }
    }
    busy=true;user_safe=true;unsigned before=turn_requests;
    assert(!io_defer_pipe_read(99,4096) && !io_defer_pipe_write(99,4096));
    assert(user_safe && turn_requests==before); /* Invalid descriptors are not contenders. */
    busy=false;
    puts("pipe-retry-admission: PASS read/write/duplex/PTY, repeated requests, blocking/invalid/nonpipe exclusion and cleanup safety");
}
/* Exercise the real syscall validation/copy dispatch with a controlled process backend. */
static bool scope_live,scope_revoked,scope_pending;
static uint64_t scope_next=100;
int64_t process_scope_create(uint64_t *out) {scope_live=true;scope_revoked=false;*out=scope_next++;return 0;}
int64_t process_scope_reserve(uint64_t token) {if(!scope_live || token!=scope_next-1)return SRV_ERR_INVAL;scope_pending=true;return 0;}
void process_scope_cancel_reservation(uint64_t token) {(void)token;scope_pending=false;}
int64_t process_scope_control(unsigned op,uint64_t token,struct srv_scope_status *out) {
    if(!scope_live || token!=scope_next-1)return SRV_ERR_INVAL;
    if(op==SRV_SCOPE_REVOKE)scope_revoked=true;
    else if(op==SRV_SCOPE_RELEASE) {if(!scope_revoked)return SRV_ERR_BUSY;scope_live=false;}
    else if(op==SRV_SCOPE_STATUS && out) *out=(struct srv_scope_status){.version=SRV_SCOPE_VERSION,.size=sizeof(*out),.members=2};
    else return SRV_ERR_INVAL;
    return 0;
}
int64_t process_scope_reap(uint64_t token) {(void)token;return 0;}
void process_refresh_mappings(struct process *p) {assert(p==(struct process *)1);if(exec_test)exec_refreshes++;}
static void exec_copied(const char *p,uint64_t a,const char *const *v,uint64_t e,const char *const *ev) {
    assert(p!=exec_user_path && !__builtin_strcmp(p,"/fat/bin/desktools"));
    assert(a==2 && e==1 && v[0]!=exec_user_arg && ev[0]!=exec_user_env);
    assert(!__builtin_strcmp(v[0],"app") && !__builtin_strcmp(v[1],"--test") && !__builtin_strcmp(ev[0],"A=B"));
}
bool process_try_spawn_private(const char *p,uint64_t a,const char *const *v,uint64_t e,const char *const *ev,int64_t *result) {
    assert(exec_test && user_safe && busy);exec_copied(p,a,v,e,ev);exec_attempts++;
    if(!exec_private)return false;
    *result=77;return true;
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
int64_t process_exec_replace(const char *p,uint64_t a,const char *const *v,uint64_t e,const char *const *ev,int64_t in,int64_t out,int64_t err,uint64_t f,uint64_t m,uint64_t d) {return SRV_ERR_INVAL;}
int64_t process_spawn_exec_scoped(uint64_t t,const char *p,uint64_t a,const char *const *v,uint64_t e,const char *const *ev,bool detached,int64_t in,int64_t out,int64_t err,uint64_t g,bool fg,const struct process_spawn_file_action *actions,uint64_t count,uint64_t f,uint64_t m,uint64_t d) {
    if(!exec_test)return SRV_ERR_INVAL;
    assert(!busy && !user_safe && detached);exec_copied(p,a,v,e,ev);exec_spawns++;return 88;
}
#pragma GCC diagnostic pop
static bool helper_private,pipe_flags_private;
static unsigned helper_attempts,helper_slow,flag_attempts,flag_slow;
bool process_try_spawn_private_helper(const char *p,const char *a,int64_t in,int64_t out,int64_t *result) {
    assert(p!=(const char *)exec_user_path && !__builtin_strcmp(p,"/fat/bin/desktools") && !a[0] && in==11 && out==12 && busy && user_safe);
    helper_attempts++;if(!helper_private)return false;*result=91;return true;
}
int64_t process_spawn_background_elf_args_fds(const char *p,const char *a,int64_t in,int64_t out) {
    (void)p;(void)a;assert(in==11 && out==12 && !busy && !user_safe);helper_slow++;return 92;
}
bool process_file_try_set_pipe_flags(struct process *p,uint64_t fd,uint64_t flags,int64_t *result) {
    assert(p==(struct process *)1 && fd==10 && flags==SRV_FD_NONBLOCK && busy && user_safe);
    flag_attempts++;if(!pipe_flags_private)return false;*result=0;return true;
}
int64_t process_file_set_flags(struct process *p,uint64_t fd,uint64_t flags){assert(p==(struct process *)1 && fd==10 && flags==SRV_FD_NONBLOCK && !busy && !user_safe && pipe_file.handle==456);flag_slow++;return -9;}
int64_t process_file_reserve_append_ram(struct process *p,uint64_t fd,uint64_t n){(void)p;(void)fd;(void)n;assert(0);return -1;}
int64_t process_file_dup_min(struct process *p,uint64_t fd,uint64_t n,bool cloexec){(void)p;(void)fd;(void)n;(void)cloexec;assert(0);return -1;}
int64_t process_file_get_fd_flags(struct process *p,uint64_t fd){(void)p;(void)fd;assert(0);return -1;}
int64_t process_file_set_fd_flags(struct process *p,uint64_t fd,uint64_t flags){(void)p;(void)fd;(void)flags;assert(0);return -1;}
int64_t process_file_get_flags(struct process *p,uint64_t fd){(void)p;(void)fd;assert(0);return -1;}
int64_t process_file_get_lock(struct process *p,uint64_t fd,struct srv_flock *l){(void)p;(void)fd;(void)l;assert(0);return -1;}
int64_t process_file_set_lock(struct process *p,uint64_t fd,const struct srv_flock *l,bool wait){(void)p;(void)fd;(void)l;(void)wait;assert(0);return -1;}
static void helper_and_flag_contract(void) {
    const char *path="/fat/bin/desktools";exec_user_path=path;
    busy=user_safe=helper_private=true;unsigned before=close_waits;
    assert(syscall_io_safe(&(struct isr_frame){.rax=SYS_SPAWN_BG_ARGS_FDS}));
    assert(syscall_spawn_bg_args_fds(path,"",11,12)==91 && busy && user_safe && close_waits==before);
    helper_private=false;
    assert(syscall_spawn_bg_args_fds(path,"",11,12)==92 && !busy && !user_safe && close_waits==before+1 && helper_attempts==2 && helper_slow==1);
    assert(syscall_io_safe(&(struct isr_frame){.rax=SYS_FCNTL,.rsi=SRV_F_SETFL}));
    busy=user_safe=pipe_flags_private=true;before=close_waits;
    assert(!syscall_fcntl(10,SRV_F_SETFL,SRV_FD_NONBLOCK) && busy && user_safe && close_waits==before);
    pipe_flags_private=false;pipe_file.handle=123;
    assert(syscall_fcntl(10,SRV_F_SETFL,SRV_FD_NONBLOCK)==-9 && !busy && !user_safe && close_waits==before+1 && flag_attempts==2 && flag_slow==1);
    busy=user_safe=kill_on_wait=true;
    assert(syscall_spawn_bg_args_fds(path,"",11,12)==SRV_ERR_INTR && startup_killed && helper_slow==1);
    startup_killed=false;busy=user_safe=true;
    assert(syscall_fcntl(10,SRV_F_SETFL,SRV_FD_NONBLOCK)==SRV_ERR_INTR && startup_killed && flag_slow==1);
    startup_killed=kill_on_wait=false;
    puts("helper/pipe-flags: PASS copied prelude, strict attempt or ordinary fresh-lookup fallback;foreign ownership unchanged on success");
}
static void scope_syscall_contract(void) {
    uint64_t token=999;scope_write_ok=false;
    assert(syscall_session_scope(SRV_SCOPE_CREATE,0,&token)==SRV_ERR_FAULT && token==999 && !scope_live);
    scope_write_ok=true;assert(!syscall_session_scope(SRV_SCOPE_CREATE,0,&token));
    struct srv_scope_status st={.version=SRV_SCOPE_VERSION,.size=sizeof(st),.members=99};
    struct srv_scope_status before=st;scope_write_ok=false;
    assert(syscall_session_scope(SRV_SCOPE_STATUS,token,&st)==SRV_ERR_FAULT && !__builtin_memcmp(&st,&before,sizeof(st)));
    scope_write_ok=true;st.version=2;before=st;
    assert(syscall_session_scope(SRV_SCOPE_STATUS,token,&st)==SRV_ERR_INVAL && !__builtin_memcmp(&st,&before,sizeof(st)));
    st.version=SRV_SCOPE_VERSION;st.size--;before=st;
    assert(syscall_session_scope(SRV_SCOPE_STATUS,token,&st)==SRV_ERR_INVAL && !__builtin_memcmp(&st,&before,sizeof(st)));
    st.size=sizeof(st);st.reserved=1;before=st;
    assert(syscall_session_scope(SRV_SCOPE_STATUS,token,&st)==SRV_ERR_INVAL && !__builtin_memcmp(&st,&before,sizeof(st)));
    st.reserved=0;before=st;
    assert(syscall_session_scope(SRV_SCOPE_STATUS,token+1,&st)==SRV_ERR_INVAL && !__builtin_memcmp(&st,&before,sizeof(st)));
    struct syscall_exec_request exec={.flags=SRV_EXEC_BACKGROUND,.path=NULL};
    struct srv_scope_exec req={.version=SRV_SCOPE_VERSION,.size=sizeof(req),.request=(const struct srv_exec_request *)&exec};
    assert(syscall_session_scope(SRV_SCOPE_EXEC,token,&req)==SRV_ERR_FAULT && !scope_pending);
    assert(!syscall_session_scope(SRV_SCOPE_REVOKE,token,NULL));
    assert(!syscall_session_scope(SRV_SCOPE_RELEASE,token,NULL));
    puts("session-scope-syscall: PASS output fault rollback, validation, stale handle and failed deep copy cleanup");
}

static void exec_syscall_contract(void) {
    char path[]="/fat/bin/desktools",arg[]="app",env[]="A=B";
    char *argv[]={arg,"--test",NULL},*envp[]={env,NULL};
    exec_user_path=path;exec_user_arg=arg;exec_user_env=env;exec_test=true;
    struct syscall_exec_request request={.path=path,.argv=argv,.envp=envp,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
    unsigned waits_before=close_waits;
    busy=user_safe=exec_private=true;
    assert(syscall_exec(&request,0)==77 && busy && user_safe && exec_attempts==1 && !exec_spawns && !exec_refreshes && close_waits==waits_before);
    exec_private=false;
    assert(syscall_exec(&request,0)==88 && !busy && !user_safe && exec_attempts==2 && exec_spawns==1 && exec_refreshes==1 && close_waits==waits_before+1);
    /* Unsupported request shapes never enter the private attempt. */
    for(unsigned reason=0;reason<6;reason++) {
        request.stdin_fd=-1;request.flags=SRV_EXEC_BACKGROUND;request.process_group=request.foreground=request.file_action_count=0;
        uint64_t scope=0;struct process_spawn_file_action action={.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=0,.new_fd=1};
        switch(reason) {
        case 0:request.stdin_fd=0;break;case 1:request.process_group=5;break;
        case 2:request.foreground=1;break;case 3:request.flags|=SRV_EXEC_SET_SIGNAL_MASK;break;
        case 4:scope=42;break;case 5:request.file_action_count=1;request.file_actions=&action;break;
        }
        busy=user_safe=exec_private=true;
        assert(syscall_exec(&request,scope)==88 && exec_attempts==2 && !busy && !user_safe);
    }
    request.flags=UINT64_MAX;busy=user_safe=true;unsigned before=close_waits;
    assert(syscall_exec(&request,0)==SRV_ERR_INVAL && busy && user_safe && close_waits==before && exec_attempts==2);
    request.flags=SRV_EXEC_BACKGROUND;request.stdin_fd=-1;request.process_group=request.foreground=request.file_action_count=0;
    busy=user_safe=kill_on_wait=true;exec_private=false;unsigned spawns=exec_spawns,refreshes=exec_refreshes;
    assert(syscall_exec(&request,0)==SRV_ERR_INTR && startup_killed && exec_spawns==spawns && exec_refreshes==refreshes);
    startup_killed=false;exec_test=false;busy=false;kill_on_wait=invalidate_on_wait=false;wait_pointer_ok=true;
    puts("exec-prelude: PASS copied kernel snapshots, checked private result, foreign owner retained; unsupported/refused requests regain ordinary admission and refresh");
}

static bool network_empty=true;
static unsigned network_slow,network_allocations;
bool net_status_info_try_without_device(struct srv_net_status_info *out) {
    if(!network_empty)return false;
    *out=(struct srv_net_status_info){.abi_version=SRV_NET_ABI_VERSION,.struct_size=sizeof(*out),.rx_frames=17};return true;
}
int64_t net_status_info(struct srv_net_status_info *out) {
    assert(!busy && !user_safe);network_slow++;
    *out=(struct srv_net_status_info){.abi_version=SRV_NET_ABI_VERSION,.struct_size=sizeof(*out),.initialized=1};return 0;
}
void *kmalloc(size_t n) {assert(!busy && !user_safe && n==sizeof(struct srv_net_status_info));network_allocations++;return malloc(n);}
void kfree(void *p) {assert(!busy && !user_safe);free(p);}
static void test_network_snapshot(void) {
    struct srv_net_status_info out={.abi_version=SRV_NET_ABI_VERSION,.struct_size=sizeof(out)};
    struct isr_frame frame={.rax=SYS_NET_STATUS_INFO};assert(syscall_io_safe(&frame));
    unsigned waits=close_waits,gates=gate_samples;
    busy=user_safe=true;
    assert(syscall_net_status_info(&out)==0 && out.rx_frames==17 && !out.initialized);
    assert(busy && user_safe && close_waits==waits && !network_slow && !network_allocations);
    struct {struct srv_abi_header header;uint64_t canary;} short_out={{SRV_NET_ABI_VERSION,sizeof(struct srv_abi_header)},123};
    assert(syscall_net_status_info((void *)&short_out)==0 && short_out.canary==123 && short_out.header.struct_size==sizeof(out));
    out.abi_version=0;assert(syscall_net_status_info(&out)==-1);out.abi_version=SRV_NET_ABI_VERSION;
    assert(syscall_net_status_info(NULL)==-1);
    scope_write_ok=false;assert(syscall_net_status_info(&out)==-1);scope_write_ok=true;
    wait_pointer_ok=false;assert(syscall_net_status_info(&out)==-1);wait_pointer_ok=true;
    assert(busy && user_safe && close_waits==waits && !network_slow && !network_allocations);
    network_empty=false;
    assert(syscall_net_status_info(&out)==0 && out.initialized && network_slow==1 && network_allocations==1);
    assert(!busy && !user_safe && close_waits==waits+1 && gate_samples==gates+1);
    busy=user_safe=kill_on_wait=true;
    assert(syscall_net_status_info(&out)==SRV_ERR_INTR && startup_killed && network_slow==1 && network_allocations==1);
    assert(close_waits==waits+2 && gate_samples==gates+2);
    startup_killed=kill_on_wait=false;busy=user_safe=true;invalidate_on_wait=true;
    assert(syscall_net_status_info(&out)==-1 && network_slow==2 && network_allocations==2);
    invalidate_on_wait=false;wait_pointer_ok=true;network_empty=true;busy=user_safe=false;
    puts("network-status-admission: PASS no-device snapshot, negotiated copy and intact gated/cancelled fallback");
}

int main(void) {
    test_network_snapshot();
    /* Existing independent fixtures start their own gate counters. */
    close_waits=gate_samples=0;
    scope_syscall_contract();
    struct srv_scope_status source={.version=SRV_SCOPE_VERSION,.size=sizeof(source),.members=17}, destination={.members=99};
    wait_pointer_ok=false;assert(!scope_publish(&destination,&source,sizeof(source)) && destination.members==99);
    wait_pointer_ok=true;assert(scope_publish(&destination,&source,sizeof(source)) && destination.members==17);
    test_close_admission();
    test_unmap_admission();
    struct isr_frame frame={.rax=SYS_READ,.rdi=0};
    assert(!syscall_io_safe(&frame));console_fd=true;assert(syscall_io_safe(&frame));
    frame.rax=SYS_WRITE;frame.rdi=1;assert(syscall_io_safe(&frame));
    console_fd=false;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_DIRECTORY;frame.rdi=SRV_DIRECTORY_READ_INFO;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_POINTER_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_BATTERY_STATUS;assert(syscall_io_safe(&frame));
    frame.rax=SYS_BATTERY_CONTROL;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_WAIT_IO;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_CANCEL_WAIT;assert(syscall_io_safe(&frame));
    frame.rax=SYS_SCHED_TRACE;assert(syscall_io_safe(&frame));
    frame.rax=SYS_GUI_SERVER_WAIT;assert(syscall_io_safe(&frame));
    const uint64_t blocked[]={SYS_SIGNAL_POLL,SYS_SIGNAL_CONSUME,SYS_OPEN,SYS_FS_WRITE,SYS_SYNC,SYS_PREAD,SYS_PWRITE,SYS_VECTOR_IO,SYS_PIPE2,SYS_DUP3,SYS_PIPE_PAIR_FLAGS,SYS_THREAD_SIGNAL,
        SYS_SOCKET_TRY_IO,SYS_PIPE_MESSAGE,SYS_SESSION_SCOPE,SYS_KILL,SYS_SPAWN,SYS_FILE_CHANGE,UINT64_MAX};
    for(unsigned i=0;i<sizeof(blocked)/sizeof(*blocked);i++) {
        frame.rax=blocked[i];assert(!syscall_io_safe(&frame));
    }
    const uint64_t admitted[]={SYS_SIGNAL_CONFIG,SYS_SIGNAL_PENDING,SYS_SIGNAL_MASK,SYS_POLL,SYS_CONSOLE_INFO,SYS_CONSOLE_CLEAR,SYS_CONSOLE_GOTOXY,SYS_GUI_SEND,SYS_GUI_RECV,SYS_MOUSE_SCAN,
        SYS_GUI_SURFACE_BLIT,SYS_GUI_SURFACE_COPY,SYS_GUI_SURFACE_CREATE,SYS_GUI_SURFACE_DESTROY,
        SYS_GFX_BLIT_RECT,SYS_GFX_BLIT_SCALED_RECT,SYS_SCHEDULER_STATUS,SYS_PROC_LIST,SYS_MEMINFO,SYS_SLEEP_TICKS,SYS_REALTIME_US,SYS_MONOTONIC_US};
    for(unsigned i=0;i<sizeof(admitted)/sizeof(*admitted);i++) { frame.rax=admitted[i]; assert(syscall_io_safe(&frame)); }
    frame.rax=SYS_WAIT; frame.rdx=0; assert(syscall_io_safe(&frame));
    frame.rdx=SRV_WAIT_NOHANG; assert(syscall_io_safe(&frame));
    busy=true;user_safe=true;
    assert(io_defer_wait(SRV_WAIT_NOHANG) && turn_requests==1 && user_safe);
    assert(io_defer_wait(SRV_WAIT_NOHANG|SRV_WAIT_POSIX) && turn_requests==2 && user_safe);
    assert(!io_defer_wait(0) && turn_requests==2); /* Blocking path retains its gate. */
    busy=false;assert(!io_defer_wait(SRV_WAIT_NOHANG) && turn_requests==2);
    busy=user_safe=retired=true;uint64_t wait_status=0;wait_pointer_ok=false;
    assert(syscall_wait(42,&wait_status,0)==SRV_ERR_FAULT && !wait_attempts);
    wait_pointer_ok=true;assert(syscall_wait(42,&wait_status,0)==42 && wait_status==9 && busy && user_safe);
    unsigned attempts=wait_attempts;
    assert(syscall_wait(42,NULL,~(SRV_WAIT_NOHANG|SRV_WAIT_POSIX))==SRV_ERR_INVAL && wait_attempts==attempts);
    assert(!syscall_wait(42,NULL,SRV_WAIT_NOHANG|SRV_WAIT_POSIX) && wait_attempts==attempts);
    retired=false;unsigned turns=turn_requests;assert(!syscall_wait(42,NULL,SRV_WAIT_NOHANG) && turn_requests==turns+1 && user_safe);
    busy=false;assert(!syscall_wait(42,NULL,SRV_WAIT_NOHANG) && !user_safe);
    frame.rax=SYS_READ; frame.rdi=10;
    pipe_file.type=PROCESS_FILE_PIPE_READ; assert(!syscall_io_safe(&frame));
    pipe_file.flags=SRV_FD_NONBLOCK; assert(syscall_io_safe(&frame));
    busy=true; user_safe=true; assert(io_defer_pipe_read(10,4096) && user_safe);
    busy=false; assert(!io_defer_pipe_read(10,4096) && !user_safe);
    pipe_file.type=PROCESS_FILE_VFS;frame.rdx=16384;assert(syscall_io_safe(&frame));
    frame.rdx=16385;assert(!syscall_io_safe(&frame));frame.rdx=UINT64_MAX;assert(!syscall_io_safe(&frame));
    frame.rdx=0;assert(syscall_io_safe(&frame));
    assert(!io_snapshot_read(0,16) && !io_snapshot_read(1,16) && !io_snapshot_read(2,16));
    pipe_file.type=PROCESS_FILE_VFS_WRITE;assert(!syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_NET_CONNECTION; assert(!syscall_io_safe(&frame));
    frame.rax=SYS_WRITE;pipe_file.type=PROCESS_FILE_PIPE_WRITE;pipe_file.flags=0;
    assert(!syscall_io_safe(&frame));pipe_file.flags=SRV_FD_NONBLOCK;assert(syscall_io_safe(&frame));
    busy=true;user_safe=true;assert(io_defer_pipe_write(10,4096) && user_safe);
    busy=false;assert(!io_defer_pipe_write(10,4096) && !user_safe);
    pipe_file.type=PROCESS_FILE_PIPE_READ;assert(!syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_PIPE_DUPLEX;assert(syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_VFS_WRITE;assert(!syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_PTY_MASTER;assert(syscall_io_safe(&frame));
    frame.rax=SYS_READ;assert(syscall_io_safe(&frame));
    pipe_file.type=PROCESS_FILE_PTY_SLAVE;assert(syscall_io_safe(&frame));
    pipe_file.flags=0;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_TTY_SETATTR;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_PTY_CREATE;assert(!syscall_io_safe(&frame));
    frame.rax=SYS_FUTEX_WAKE;assert(syscall_io_safe(&frame));
    frame.rax=SYS_FUTEX_WAIT;assert(syscall_io_safe(&frame));
    busy=user_safe=true;unsigned identity_waits=close_waits,identity_turns=turn_requests;
    frame.rax=SYS_THREAD_SELF;assert(syscall_io_safe(&frame));
    assert(busy && user_safe && close_waits==identity_waits && turn_requests==identity_turns);
    const uint64_t thread_gated[]={SYS_THREAD_JOIN,SYS_THREAD_DETACH,SYS_THREAD_STATUS,SYS_SBRK};
    for(unsigned n=0;n<sizeof(thread_gated)/sizeof(thread_gated[0]);n++) {
        frame.rax=thread_gated[n];assert(!syscall_io_safe(&frame));
    }

    /* Lifecycle calls admit only validation; the kernel proof/fallback tests
     * separately reject storage/device resources and ordinary teardown. */
    const uint64_t lifecycle_preludes[]={SYS_EXEC,SYS_EXIT,SYS_THREAD_EXIT};
    for(unsigned n=0;n<sizeof(lifecycle_preludes)/sizeof(*lifecycle_preludes);n++) {
        frame.rax=lifecycle_preludes[n];assert(syscall_io_safe(&frame) && busy && user_safe);
    }
    frame.rax=SYS_KEY_SCAN;assert(syscall_io_safe(&frame));
    frame.rax=SYS_TTY_GETATTR;assert(syscall_io_safe(&frame));
    test_startup_admission();
    test_pipe_retry_admission();
    pipe_file.type=PROCESS_FILE_PIPE_DUPLEX;pipe_file.flags=SRV_FD_NONBLOCK;
    busy=true;user_safe=true;byte_safe=true;unsigned before=turn_requests;
    for(unsigned i=0;i<1024;i++) {
        assert(!io_defer_pipe_write(10,4096) && !io_defer_pipe_read(10,32));
        assert(user_safe && turn_requests==before);
    }
    assert(io_defer_pipe_write(10,4097) && io_defer_pipe_read(10,4097));
    byte_safe=false;assert(io_defer_pipe_write(10,4096) && io_defer_pipe_read(10,32));
    assert(user_safe && turn_requests==before+4);

    struct isr_frame traced={.cs=3,.rax=SYS_FCNTL,.rdi=10,.rsi=SRV_F_GETLK,.rdx=0xdeadbeef};
    pipe_file.type=PROCESS_FILE_PIPE_DUPLEX;pipe_file.flags=SRV_FD_NONBLOCK;busy=true;user_safe=false;trace_on=true;
    syscall_gate_trace(&traced);assert(trace_fcntls==1 && !syscall_io_safe(&traced) && busy && !user_safe);
    traced.cs=0;syscall_gate_trace(&traced);assert(trace_fcntls==1);
    traced.cs=3;trace_on=false;syscall_gate_trace(&traced);assert(trace_fcntls==1);
    trace_on=true;traced.rax=SYS_EXEC;syscall_gate_trace(&traced);assert(trace_entries==1 && busy && !user_safe);
    exec_syscall_contract();assert(trace_requests>0);trace_on=false;
    helper_and_flag_contract();
    puts("launch-trace-entry: PASS disabled/kernel exclusions, early scalar snapshot without pointer dereference or changed admission");
    puts("syscall-io-host: PASS (bounded desktop admission, bounded retained-snapshot reads, nonblocking pipe deferral, redirected I/O/lifecycle/mapping/filesystem/default-deny)");
}

bool scheduler_launch_trace_enabled(void){return trace_on;}
void scheduler_launch_trace_exec(uint64_t r,uint64_t q,uint64_t m,uint64_t a,uint64_t d){if(trace_on){assert(r==LAUNCH_TRACE_REQUEST && q && q<=63 && !m && !a && !d);trace_requests++;}}
void scheduler_launch_trace_fcntl(uint64_t f,uint64_t c,uint64_t a,uint64_t t,uint64_t g){assert(trace_on && !user_safe && busy && f==10 && c==SRV_F_GETLK && a==0xdeadbeef && t==PROCESS_FILE_PIPE_DUPLEX && g==SRV_FD_NONBLOCK);trace_fcntls++;}

void scheduler_launch_trace_entry(void){assert(trace_on && !user_safe && busy);trace_entries++;}
