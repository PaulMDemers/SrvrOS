#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static int fail_allocation,freed_wiped;
static void *test_mmap(void *address,size_t size,int prot,int flags,int fd,off_t offset) {
    assert(!address && size==32768u*1024u && prot==(PROT_READ|PROT_WRITE));
    assert(flags==(MAP_PRIVATE|MAP_ANONYMOUS) && fd==-1 && offset==0);
    if(fail_allocation)return MAP_FAILED;
    void *allocation=malloc(size);
    return allocation?allocation:MAP_FAILED;
}
static int test_munmap(void *p,size_t size) {
    const unsigned char *bytes=p;
    assert(size==32768u*1024u);
    for(size_t i=0;i<size;i++) assert(bytes[i]==0);
    freed_wiped++;free(p);
    return 0;
}
#define mmap test_mmap
#define munmap test_munmap
#include "../userspace/localauth/crypto.c"
#undef mmap
#undef munmap

int main(void) {
    /* Independently generated with argon2-cffi's libargon2, v19. */
    static const uint8_t expected[32]={
        0xbb,0xe4,0xce,0x72,0x5d,0x8b,0x1a,0xe6,0xf8,0xc3,0xf7,0x0b,0x21,0xda,0xc6,0x76,
        0x81,0xbf,0x71,0xb5,0x3f,0xfc,0x4b,0x80,0x5c,0x35,0xe6,0xfa,0xa4,0x47,0x7e,0xab};
    static const char secret[]="correct horse battery staple";
    uint8_t salt[16],out[32];
    for(unsigned i=0;i<sizeof(salt);i++)salt[i]=(uint8_t)i;
    assert(local_auth_verifier(secret,sizeof(secret)-1,salt,out));
    assert(local_auth_equal(out,expected) && freed_wiped==1);
    out[31]^=1;assert(!local_auth_equal(out,expected));
    local_auth_wipe(out,sizeof(out));
    for(unsigned i=0;i<sizeof(out);i++)assert(!out[i]);
    memset(out,0xff,sizeof(out));fail_allocation=1;
    assert(!local_auth_verifier(secret,sizeof(secret)-1,salt,out));
    for(unsigned i=0;i<sizeof(out);i++)assert(!out[i]);
    assert(!local_auth_verifier(NULL,0,salt,out));
    assert(!local_auth_verifier(secret,(size_t)UINT32_MAX+1,salt,out));
    assert(!local_auth_verifier(secret,sizeof(secret)-1,NULL,out));
    assert(!local_auth_verifier(secret,sizeof(secret)-1,salt,NULL));
    assert(!local_auth_equal(NULL,expected) && freed_wiped==1);
    puts("local auth crypto: independent Argon2id vector, wipe and failure PASS");
}
