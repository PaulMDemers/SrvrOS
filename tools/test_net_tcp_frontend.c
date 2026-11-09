/* Actual syscall helpers plus unrenamed production TCP stack. Reuse only the
 * independent wire checks/host platform stubs from the finite backend fixture.
 * Process pointers, IRQ state and FD tables here are explicitly HOST MODELS;
 * this does not execute the ISR, user process, scheduler or native hardware. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static unsigned allocations;
static void *counted_malloc(size_t size) { allocations++;return malloc(size); }
#define malloc counted_malloc
#define main finite_backend_fixture_main
#include "test_net_tcp_try.c"
#undef main
#undef malloc

static bool irq_enabled = true;
static unsigned irq_exclusions, irq_restores;
static uint64_t syscall_copy_test_irq_save(void) {
    bool previous=irq_enabled;irq_enabled=false;irq_exclusions++;return previous;
}
static void syscall_copy_test_irq_restore(uint64_t flags) {
    irq_enabled=flags!=0;irq_restores++;
}
#define SRVROS_SYSCALL_IO_HOST_TEST
#define SRVROS_SYSCALL_COPY_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"

static struct process_file files[8];
static uint64_t generations[8];
static unsigned maps, file_queries, fd_allocations;
static unsigned fail_map, replace_map, replacement_kind;
static bool no_fd, require_exclusion;
static unsigned max_mapped;
static unsigned char *user_source;
static unsigned groups;

struct process_file *process_file_at(struct process *p,uint64_t fd) {
    file_queries++;
    if(p!=caller || fd>=8 || !files[fd].used)return NULL;
    return &files[fd];
}
uint64_t process_fd_generation(struct process *p,uint64_t fd) {
    assert(p==caller && fd<8);return generations[fd];
}
int64_t process_handle_alloc(struct process *p,enum process_file_type type,uint64_t value) {
    assert(p==caller && type==PROCESS_FILE_NET_CONNECTION);
    if(require_exclusion)assert(!irq_enabled);
    fd_allocations++;
    if(no_fd)return -1;
    for(unsigned i=0;i<8;i++)if(!files[i].used) {
        files[i]=(struct process_file){.used=true,.type=type,.handle=value};
        return i;
    }
    return -1;
}
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(length && !writable);maps++;
    if(length>max_mapped)max_mapped=(unsigned)length;
    if(maps==replace_map) {
        assert(!irq_enabled);
        switch(replacement_kind) {
        case 0:generations[7]++;break;
        case 1:files[7].handle++;break;
        case 2:files[7].type=PROCESS_FILE_NET_UDP;break;
        case 3:files[7].used=false;break;
        default:assert(false);
        }
    }
    return address && maps!=fail_map;
}
int64_t process_file_pipe_read_checked(struct process *p,uint64_t fd,uint64_t n,bool nb,
    process_pipe_copyout fn,void *context) {
    (void)p;(void)fd;(void)n;(void)nb;(void)fn;(void)context;assert(false);return -1;
}
int64_t process_file_pipe_write_checked(struct process *p,uint64_t fd,uint64_t n,bool nb,
    process_pipe_copyin fn,void *context) {
    (void)p;(void)fd;(void)n;(void)nb;(void)fn;(void)context;assert(false);return -1;
}

static void frontend_reset(void) {
    reset();memset(files,0,sizeof files);
    for(unsigned i=0;i<8;i++)generations[i]=1;
    maps=file_queries=fd_allocations=fail_map=replace_map=replacement_kind=0;
    allocations=max_mapped=0;irq_enabled=true;irq_exclusions=irq_restores=0;
    no_fd=false;require_exclusion=true;user_source=NULL;
}
static struct tcp_connection *frontend_connected(void) {
    frontend_reset();struct tcp_connection *c=&tcp_connections[0];
    *c=(struct tcp_connection){.state=TCP_STATE_ESTABLISHED,.accepted=true,.outbound=true,
        .owner=caller,.id=42,.refs=1,.local_seq=1000,.remote_next=2000,.local_port=50000,
        .remote_port=80,.remote_ip=0x0a000202,.peer_window=65535,.peer_mss=1400};
    memcpy(c->remote_mac,router_mac,6);
    files[7]=(struct process_file){.used=true,.type=PROCESS_FILE_NET_CONNECTION,
        .handle=handle(),.flags=SRV_FD_APPEND};
    return c;
}
static void balanced(void) {
    assert(irq_enabled && irq_exclusions==irq_restores && !allocations);
}
static void capability_and_flags(void) {
    frontend_reset();
    assert(syscall_socket_try_io(UINT64_MAX,SRV_SOCKET_TCP_SEND_TRY,NULL,0,NULL)==1);
    assert(!maps && !file_queries && !process_checks && !attempts && !fd_allocations && !irq_exclusions);
    struct srv_socket_peer peer={0};
    assert(syscall_socket_try_io(UINT64_MAX,SRV_SOCKET_TCP_SEND_TRY,(void *)1,0,NULL)==SRV_ERR_INVAL);
    assert(syscall_socket_try_io(UINT64_MAX,SRV_SOCKET_TCP_SEND_TRY,NULL,1,NULL)==SRV_ERR_INVAL);
    assert(syscall_socket_try_io(UINT64_MAX,SRV_SOCKET_TCP_SEND_TRY,NULL,0,&peer)==SRV_ERR_INVAL);
    assert(!maps && !file_queries && !process_checks && !attempts && !fd_allocations && !irq_exclusions);
    const uint64_t invalid[]={SRV_NET_CONNECT_CACHE_ONLY,
        SRV_NET_CONNECT_CACHE_ONLY|SRV_FD_NONBLOCK|SRV_FD_CLOEXEC,
        SRV_NET_CONNECT_CACHE_ONLY|SRV_FD_NONBLOCK|SRV_FD_APPEND,
        SRV_NET_CONNECT_CACHE_ONLY|SRV_FD_NONBLOCK|(1ull<<63)};
    for(unsigned i=0;i<sizeof invalid/sizeof invalid[0];i++)
        assert(syscall_net_connect(0x0a000202,80,invalid[i])==SRV_ERR_INVAL);
    assert(!process_checks && !attempts && !fd_allocations && !irq_exclusions);
    balanced();groups++;
}
static void connect_cases(void) {
    frontend_reset();uint64_t flags=SRV_FD_NONBLOCK|SRV_NET_CONNECT_CACHE_ONLY;
    assert(syscall_net_connect(0x0a000202,80,flags)==SRV_ERR_AGAIN);
    assert(!attempts && !fd_allocations && next_client_port==TCP_CLIENT_PORT_FIRST && next_connection_id==1);
    balanced();
    frontend_reset();arp_cache_update(0x0a000202,router_mac);
    assert(syscall_net_connect(0x0a000202,80,flags)==0 && fd_allocations==1);
    assert(files[0].used && !files[0].flags && files[0].type==PROCESS_FILE_NET_CONNECTION);
    struct tcp_connection *c=find_referenced_connection_by_id(handle_value(files[0].handle));
    assert(c && c->state==TCP_STATE_SYN_SENT && c->owner==caller && c->refs==1);
    wire(c->remote_ip,80,0x63720000u+TCP_CLIENT_PORT_FIRST,TCP_SYN,NULL,0);balanced();
    frontend_reset();arp_cache_update(0x0a000202,router_mac);no_fd=true;
    assert(syscall_net_connect(0x0a000202,80,flags)<0 && attempts==1 && accepted==1 && fd_allocations==1);
    assert(tcp_connections[0].state==TCP_STATE_EMPTY && !tcp_connections[0].refs);
    balanced();
    frontend_reset();arp_cache_update(0x0a000202,router_mac);refuse_output=true;
    assert(syscall_net_connect(0x0a000202,80,flags)<0 && attempts==1 && !accepted && !fd_allocations);
    assert(tcp_connections[0].state==TCP_STATE_EMPTY);balanced();
    frontend_reset();caller=NULL;
    assert(syscall_net_connect(0x0a000202,80,flags)<0 && !attempts && !irq_exclusions);
    frontend_reset();require_exclusion=false;arp_cache_update(0x0a000202,router_mac);
    /* The original nonblocking connect route does not acquire new exclusion. */
    assert(syscall_net_connect(0x0a000202,80,SRV_FD_NONBLOCK)==0 && !irq_exclusions);
    balanced();groups++;
}
static void mutate_user(void) { assert(!irq_enabled);memset(user_source,'z',SRV_SOCKET_TCP_TRY_COPY_MAX); }
static void replace_admitted_fd(void) { assert(!irq_enabled);files[7].used=false;generations[7]++; }
static void send_cases(void) {
    static unsigned char data[SRV_SOCKET_TCP_TRY_COPY_MAX];
    memset(data,'a',sizeof data);struct tcp_connection *c=frontend_connected();user_source=data;
    output_hook=mutate_user;
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,SIZE_MAX,NULL)==1400);
    assert(max_mapped<=sizeof data && c->local_seq==2400 && c->tx_history[0].payload_length==1400);
    for(unsigned i=0;i<1400;i++)assert(c->tx_history[0].payload[i]=='a' && last_frame[54+i]=='a');
    assert(data[0]=='z' && files[7].flags==SRV_FD_APPEND);balanced();
    c=frontend_connected();c->peer_window=5;memcpy(data,"hello",5);
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,50,NULL)==5);
    wire(c->remote_ip,80,1000,TCP_ACK|TCP_PSH,"hello",5);balanced();
    for(unsigned kind=0;kind<3;kind++) {
        c=frontend_connected();
        if(kind==0)c->peer_window=0;
        if(kind==1)for(unsigned i=0;i<TCP_TX_HISTORY;i++)c->tx_history[i]=(struct tcp_tx_record){.used=true,.payload_length=1};
        if(kind==2)for(unsigned i=0;i<4;i++)c->tx_history[i]=(struct tcp_tx_record){.used=true,.payload_length=1400};
        assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,1,NULL)==SRV_ERR_AGAIN && !attempts);
        balanced();
    }
    c=frontend_connected();refuse_output=true;uint32_t seq=c->local_seq;
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,1,NULL)<0 && attempts==1 && !accepted);
    assert(c->local_seq==seq && !c->tx_history[0].used);balanced();
    c=frontend_connected();memcpy(data,"admitted",8);output_hook=replace_admitted_fd;
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,8,NULL)==8);
    assert(!files[7].used && c->local_seq==1008 && c->tx_history[0].used &&
        !memcmp(c->tx_history[0].payload,"admitted",8));
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,8,NULL)==SRV_ERR_BADF && attempts==1);
    balanced();
    frontend_connected();irq_enabled=false;
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,1,NULL)==1);
    assert(!irq_enabled && irq_exclusions==irq_restores && !allocations);
    irq_enabled=true;balanced();groups++;
}
static void mapping_identity_and_types(void) {
    static unsigned char data[64];
    for(unsigned mapping=1;mapping<=2;mapping++) {
        frontend_connected();fail_map=mapping;
        assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,sizeof data,NULL)==SRV_ERR_FAULT && !attempts);
        balanced();
    }
    for(unsigned kind=0;kind<4;kind++) {
        frontend_connected();replace_map=2;replacement_kind=kind;
        assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,sizeof data,NULL)==SRV_ERR_BADF && !attempts);
        balanced();
    }
    frontend_connected();generations[7]=UINT64_MAX;
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,1,NULL)==SRV_ERR_BADF && !attempts);balanced();
    const enum process_file_type unsupported[]={PROCESS_FILE_NET_UDP,PROCESS_FILE_NET_LISTENER,
        PROCESS_FILE_PIPE_READ,PROCESS_FILE_PIPE_WRITE,PROCESS_FILE_PIPE_DUPLEX,PROCESS_FILE_VFS};
    for(unsigned i=0;i<sizeof unsupported/sizeof unsupported[0];i++) {
        frontend_connected();files[7].type=unsupported[i];
        assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,1,NULL)==SRV_ERR_BADF && !attempts);balanced();
    }
    frontend_connected();struct srv_socket_peer peer={0};
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,data,1,&peer)==SRV_ERR_INVAL);
    assert(!maps && !file_queries && !attempts);
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,NULL,1,NULL)==SRV_ERR_FAULT);
    assert(syscall_socket_try_io(7,SRV_SOCKET_TCP_SEND_TRY,NULL,0,NULL)==0 && !attempts);
    balanced();groups++;
}
int main(void) {
    capability_and_flags();connect_cases();send_cases();mapping_identity_and_types();
    frontend_reset();
    printf("net-tcp-frontend: %u groups PASS actual syscall+TCP; modeled host process/IRQ/FD only; no ISR or native acceptance\n",groups);
    return 0;
}
