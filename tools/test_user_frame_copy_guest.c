/* Private INT3 test-kernel hook only: no public signal/copy ABI is implied. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <srvros/sys.h>

#define PAGE 4096u
#define MAX_TRACKED 65536u
#define COPY_MAGIC UINT64_C(0x534947434f505931)
#define SENTINEL 0xa5u
static __thread volatile uint64_t cookie;
static int worker_result;

static long copy_hook(void *destination,size_t length,unsigned mode,uint64_t expected_cookie)
{
    uint64_t result=COPY_MAGIC,fs_before,fs_after,sp_before,sp_after,gpr;
    __asm__ volatile(
        "movq %%fs:0, %%r10\n\tmovq %%r10,%1\n\tmovq %%rsp,%3\n\t"
        "movabsq $0x123456789abcdef1,%%r12\n\tint3\n\t"
        "movq %%fs:0,%%r10\n\tmovq %%r10,%2\n\tmovq %%rsp,%4\n\tmovq %%r12,%5"
        : "+a"(result),"=m"(fs_before),"=m"(fs_after),"=m"(sp_before),"=m"(sp_after),"=m"(gpr)
        : "D"(destination),"S"(length),"d"((uint64_t)mode)
        : "r10","r11","r12","cc","memory");
    if(fs_before!=fs_after || sp_before!=sp_after || gpr!=UINT64_C(0x123456789abcdef1) || cookie!=expected_cookie)
        return -10000;
    return (long)result;
}
static unsigned char expected_byte(size_t i) {return (unsigned char)(0x5au^(unsigned)(i*13u));}
static int verify_bytes(const unsigned char *p,size_t total,size_t start,size_t length)
{
    for(size_t i=0;i<total;i++) {
        unsigned char expected=(i>=start && i-start<length)?expected_byte(i-start):SENTINEL;
        if(p[i]!=expected)return 0;
    }
    return 1;
}
static int success_case(unsigned char *memory,size_t start,size_t length,unsigned mode,uint64_t expected_cookie)
{
    memset(memory,SENTINEL,PAGE*2);
    if(copy_hook(memory+start,length,mode,expected_cookie)!=0) return 0;
    return verify_bytes(memory,PAGE*2,start,length);
}
static int negative_case(unsigned char *memory,void *destination,size_t length,uint64_t expected_cookie)
{
    memset(memory,SENTINEL,PAGE*2);
    if(copy_hook(destination,length,0,expected_cookie)!=SRV_ERR_FAULT) return 0;
    return verify_bytes(memory,PAGE*2,PAGE*2,0);
}
static int cases(unsigned char *memory,uint64_t expected_cookie)
{
    if(!success_case(memory,0,1,1,expected_cookie) ||
       !success_case(memory,PAGE-512,1024,1,expected_cookie) ||
       !success_case(memory,PAGE-32,64,1,expected_cookie)) return 1;
    if(!negative_case(memory,NULL,1,expected_cookie) ||
       !negative_case(memory,memory,0,expected_cookie) ||
       !negative_case(memory,memory,1025,expected_cookie) ||
       !negative_case(memory,(void *)(uintptr_t)UINT64_C(0x800000000000),1,expected_cookie) ||
       !negative_case(memory,memory+2*PAGE-32,64,expected_cookie)) return 2;
    if(mprotect(memory+PAGE,PAGE,PROT_READ)) return 3;
    /* Reset the writable page only, then validate both readable pages. */
    memset(memory,SENTINEL,PAGE);
    /* Second page retained SENTINEL from the preceding refusal. */
    if(copy_hook(memory+PAGE-32,64,0,expected_cookie)!=SRV_ERR_FAULT ||
       !verify_bytes(memory,PAGE*2,PAGE*2,0)) return 4;
    if(mprotect(memory+PAGE,PAGE,PROT_READ|PROT_WRITE)) return 5;
    return 0;
}
static void *worker(void *unused)
{
    (void)unused;cookie=UINT64_C(0x1122334455667788);
    unsigned char *memory=mmap(NULL,PAGE*3,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(memory==MAP_FAILED) {worker_result=10;return NULL;}
    if(mprotect(memory+PAGE*2,PAGE,PROT_NONE)) worker_result=11;
    else worker_result=cases(memory,cookie);
    if(munmap(memory,PAGE*3) && !worker_result)worker_result=12;
    return NULL;
}
static int mapped_pages(uint64_t *out)
{
    struct srv_process_info info={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(info)};
    long r=srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)getpid(),&info);
    if(r<=0 || info.abi_version!=SRV_ABI_VERSION || info.struct_size!=sizeof(info) ||
       info.pid!=(uint64_t)getpid() || info.user_mapped_bytes%PAGE) return 0;
    *out=info.user_mapped_bytes/PAGE;return 1;
}
int main(void)
{
    cookie=UINT64_C(0x8877665544332211);
    unsigned char *memory=mmap(NULL,PAGE*3,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(memory==MAP_FAILED || mprotect(memory+PAGE*2,PAGE,PROT_NONE)) return 10;
    int main_result=cases(memory,cookie);if(main_result) {printf("frame-copy: main=%d\n",main_result);return 11;}
    pthread_t thread;if(pthread_create(&thread,NULL,worker,NULL)) return 12;
    if(pthread_join(thread,NULL) || worker_result) {printf("frame-copy: worker=%d\n",worker_result);return 13;}
    uint64_t before=0,full=0,after=0;
    if(!mapped_pages(&before) || before>=MAX_TRACKED) {printf("frame-copy: bad baseline=%llu\n",(unsigned long long)before);return 14;}
    size_t bytes=(size_t)(MAX_TRACKED-before)*PAGE;
    void *filler=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(filler==MAP_FAILED) {printf("frame-copy: cannot fill from %llu\n",(unsigned long long)before);return 15;}
    if(!mapped_pages(&full) || full!=MAX_TRACKED) {printf("frame-copy: count=%llu\n",(unsigned long long)full);return 16;}
    if(!success_case(memory,PAGE-512,1024,2,cookie)) return 17;
    if(!mapped_pages(&after) || after!=MAX_TRACKED) {printf("frame-copy: count after=%llu\n",(unsigned long long)after);return 18;}
    if(munmap(filler,bytes) || munmap(memory,PAGE*3)) return 19;
    puts("frame-copy: main worker and maximum mapping count ok");
    return 0;
}
