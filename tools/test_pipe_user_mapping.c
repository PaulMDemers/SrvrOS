/* Inject mapping and descriptor changes at the pipe's blocking boundary. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SYSCALL_IO_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"

static struct process_file file;
static const void *bad;
static char *mutate;
static struct srv_iovec *mutate_iov;
static char replacement[8];
static unsigned committed;
static char captured[8];
static int64_t write_result;
bool vmm_user_range_mapped(uint64_t address, uint64_t length, bool write) {
    (void)length; (void)write; return address != (uint64_t)bad;
}
struct process *process_current(void) { return (struct process *)1; }
struct process_file *process_file_at(struct process *p, uint64_t fd) {
    assert(p == (struct process *)1 && fd == 7); return &file;
}
bool process_file_nonblocking(struct process *p, uint64_t fd) { (void)p; (void)fd; return false; }
bool process_file_pipe_io_safe(struct process *p,uint64_t fd,uint64_t n,bool w) { (void)p;(void)fd;(void)n;(void)w;return true; }
bool scheduler_io_busy(void) { return false; }
void scheduler_io_request_turn(void) {}
void scheduler_io_set_user_safe(bool safe) { (void)safe; }
void *kmalloc(size_t n) { return malloc(n); }
void kfree(void *p) { free(p); }
int64_t process_file_pipe_read_checked(struct process *p,uint64_t fd,uint64_t n,bool nw,process_pipe_copyout copy,void *ctx) {
    assert(p==(struct process *)1 && fd==7 && n>=4 && !nw);
    if (mutate_iov) mutate_iov[1].base = replacement;
    if (mutate) bad = mutate;
    if (!copy(ctx,(const uint8_t *)"ab",2,(const uint8_t *)"cd",2)) return SRV_ERR_FAULT;
    committed++; return 4;
}
int64_t process_file_pipe_write(struct process *p,uint64_t fd,const uint8_t *b,uint64_t n) {
    assert(p==(struct process *)1 && fd==7 && n==4);
    if (mutate) { memset(mutate,'z',4); bad=mutate; }
    memcpy(captured,b,4); return write_result;
}
int64_t process_file_pipe_write_checked(struct process *p,uint64_t fd,uint64_t n,bool nw,process_pipe_copyin copy,void *ctx) {
    assert(p==(struct process *)1 && fd==7 && n==4 && !nw);
    if (mutate) bad=mutate;
    if (!copy(ctx,0,(uint8_t *)captured,2,(uint8_t *)captured+2,2)) return SRV_ERR_FAULT;
    return 4;
}
/* Other descriptor branches are unreachable in this fixture. */
int64_t process_file_pipe_read(struct process *p,uint64_t f,uint8_t *b,uint64_t n) {
    assert(p==(struct process *)1 && f==7 && n>=4);
    if (mutate_iov) mutate_iov[1].base=replacement;
    if (mutate) bad=mutate;
    memcpy(b,"abcd",4); committed++; return 4;
}
int64_t process_file_read(struct process *p,uint64_t f,uint8_t *b,uint64_t n) {(void)p;(void)f;(void)b;(void)n;assert(0);return 0;}
int64_t process_file_write(struct process *p,uint64_t f,const uint8_t *b,uint64_t n) {(void)p;(void)f;(void)b;(void)n;assert(0);return 0;}
int64_t process_file_pread(struct process *p,uint64_t f,uint8_t *b,uint64_t n,int64_t o) {(void)p;(void)f;(void)b;(void)n;(void)o;assert(0);return 0;}
int64_t process_file_pwrite(struct process *p,uint64_t f,const uint8_t *b,uint64_t n,int64_t o) {(void)p;(void)f;(void)b;(void)n;(void)o;assert(0);return 0;}
int64_t net_read(uint64_t h,char *b,uint64_t n,bool x) {(void)h;(void)b;(void)n;(void)x;assert(0);return 0;}
int64_t net_respond(uint64_t h,const char *b,uint64_t n,bool x) {(void)h;(void)b;(void)n;(void)x;assert(0);return 0;}
bool process_should_exit_current(void) { return false; }
bool process_signal_pending_current(void) { return false; }
void console_write_n(const char *s,size_t n) {(void)s;(void)n;assert(0);}
void console_putc(char c) {(void)c;assert(0);}
bool keyboard_scan_char(char *c) {(void)c;assert(0);return false;}
char keyboard_read_char(void) {assert(0);return 0;}
int main(int argc, char **argv) {
    if (argc == 2) {
        char a[4]={0}, b[4]={0};
        struct srv_iovec iov[2]={{a,2},{b,2}};
        int64_t result;
        if (!strcmp(argv[1], "read")) {
            file.type=PROCESS_FILE_PIPE_READ; mutate=a;
            result=syscall_read(7,a,4);
            printf("read: result=%lld committed=%u data=%.4s\n",(long long)result,committed,a);
        } else if (!strcmp(argv[1], "readv")) {
            file.type=PROCESS_FILE_PIPE_READ; mutate=b;
            result=syscall_vector_io(7,iov,2,SRV_VECTOR_READ,0);
            printf("readv: result=%lld committed=%u first=%.2s second=%.2s\n",(long long)result,committed,a,b);
        } else if (!strcmp(argv[1], "write")) {
            char input[4]={'a','b','c','d'};
            file.type=PROCESS_FILE_PIPE_WRITE; mutate=input; write_result=4;
            result=syscall_write(7,input,4);
            printf("write: result=%lld captured=%.4s\n",(long long)result,captured);
        } else return 2;
        return 0;
    }
    assert(argc==1);
    char a[4]={0},b[4]={0};
    file.type=PROCESS_FILE_PIPE_READ; mutate=a;
    assert(syscall_read(7,a,4)==SRV_ERR_FAULT && !committed && !a[0]);
    bad=mutate=NULL;
    assert(syscall_read(7,a,4)==4 && committed==1 && !memcmp(a,"abcd",4));
    struct srv_iovec iov[2]={{a,2},{b,2}};
    memset(a,0,4);mutate=b;
    assert(syscall_vector_io(7,iov,2,SRV_VECTOR_READ,0)==SRV_ERR_FAULT && committed==1 && !a[0] && !b[0]);
    bad=mutate=NULL;mutate_iov=iov;
    assert(syscall_vector_io(7,iov,2,SRV_VECTOR_READ,0)==4 && committed==2);
    assert(!memcmp(a,"ab",2) && !memcmp(b,"cd",2) && !replacement[0]);
    file.type=PROCESS_FILE_PIPE_WRITE;char input[4]={'a','b','c','d'};mutate=input;write_result=2;
    assert(syscall_write(7,input,4)==SRV_ERR_FAULT);
    bad=mutate=NULL; assert(syscall_write(7,input,4)==4 && !memcmp(captured,"abcd",4));
    struct srv_iovec source[1]={{input,4}}; mutate=input; write_result=2;
    assert(syscall_vector_io(7,source,1,SRV_VECTOR_WRITE,0)==2 && !memcmp(captured,"abcd",4));
    puts("pipe-user-mapping: PASS scalar fault, scatter fault/commit, iovec snapshot, write fault, vector write snapshot/partial progress");
}
