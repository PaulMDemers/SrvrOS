#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SRVROS_SYSCALL_POLL_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static void *allowed;
static size_t capacity;
static bool write_ok=true,fail_alloc;
static unsigned get_calls,select_calls;
bool vmm_user_range_mapped(uint64_t p,uint64_t n,bool write){return (!write || write_ok) && p>=(uintptr_t)allowed && p-(uintptr_t)allowed<=capacity && n<=capacity-(p-(uintptr_t)allowed);}
void *kmalloc(uint64_t n){return fail_alloc?NULL:malloc(n);}
void kfree(void *p){free(p);}
int wifi_service_snapshot(uint64_t owner,struct srv_wifi_snapshot *out){get_calls++;*out=(struct srv_wifi_snapshot){.version=1,.size=sizeof *out,.owner=owner,.selected=SRV_WIFI_NONE};return SRV_WIFI_OK;}
int wifi_service_select(const struct srv_wifi_select *r){select_calls++;return r->version==1 && r->size==sizeof *r && !r->reserved?SRV_WIFI_OK:SRV_WIFI_INVALID;}
int main(void){
    struct {struct srv_wifi_snapshot s;unsigned char guard[16];} b;memset(&b,0xa5,sizeof b);allowed=&b;capacity=sizeof b.s;
    b.s.version=1;b.s.size=sizeof b.s;assert(syscall_wifi(0,123,&b.s)==0 && b.s.owner==123 && get_calls==1);
    for(unsigned i=0;i<16;i++)assert(b.guard[i]==0xa5);
    b.s.version=99;assert(syscall_wifi(0,0,&b.s)==SRV_WIFI_INVALID && get_calls==1);
    b.s.version=1;b.s.size=15;assert(syscall_wifi(0,0,&b.s)==SRV_WIFI_INVALID && get_calls==1);
    b.s.size=sizeof b.s;capacity=8;assert(syscall_wifi(0,0,&b.s)==SRV_WIFI_INVALID && get_calls==1);
    capacity=sizeof b.s;write_ok=false;assert(syscall_wifi(0,0,&b.s)==SRV_WIFI_INVALID);write_ok=true;
    fail_alloc=true;assert(syscall_wifi(0,0,&b.s)==SRV_WIFI_UNAVAILABLE);fail_alloc=false;
    assert(syscall_wifi(2,0,&b.s)==SRV_WIFI_INVALID && syscall_wifi(0,0,NULL)==SRV_WIFI_INVALID);
    struct srv_wifi_select r={.version=1,.size=sizeof r,.owner=123,.generation=4};allowed=&r;capacity=sizeof r;
    assert(syscall_wifi(1,0,&r)==0 && select_calls==1);
    assert(syscall_wifi(1,123,&r)==SRV_WIFI_INVALID && select_calls==1);
    capacity--;assert(syscall_wifi(1,0,&r)==SRV_WIFI_INVALID && select_calls==1);
    puts("wifi-syscall: PASS version/size/op/reserved-argument,user-copy boundaries,allocation failure and bounded copied output");
}
