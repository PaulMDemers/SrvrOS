#include "crypto.h"
#include "vendor/monocypher.h"
#include <sys/mman.h>

#define LOCAL_AUTH_BLOCKS 32768u
#define LOCAL_AUTH_WORK_BYTES ((size_t)LOCAL_AUTH_BLOCKS * 1024u)

void local_auth_wipe(void *data,size_t length) {
    if(data) crypto_wipe(data,length);
}

bool local_auth_equal(const uint8_t a[32],const uint8_t b[32]) {
    return a && b && crypto_verify32(a,b)==0;
}

bool local_auth_verifier(const char *secret,size_t length,const uint8_t salt[16],uint8_t out[32]) {
    if(!out) return false;
    if(!secret || !salt || length>UINT32_MAX) {
        local_auth_wipe(out,32);
        return false;
    }
    void *work=mmap(NULL,LOCAL_AUTH_WORK_BYTES,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(work==MAP_FAILED) {
        local_auth_wipe(out,32);
        return false;
    }
    const crypto_argon2_config config={CRYPTO_ARGON2_ID,LOCAL_AUTH_BLOCKS,3,1};
    const crypto_argon2_inputs inputs={(const uint8_t *)secret,salt,(uint32_t)length,16};
    crypto_argon2(out,32,work,config,inputs,crypto_argon2_no_extras);
    /* Monocypher also wipes this on success; retain the explicit guarantee at
     * our allocation boundary. */
    local_auth_wipe(work,LOCAL_AUTH_WORK_BYTES);
    if(munmap(work,LOCAL_AUTH_WORK_BYTES)==0) return true;
    local_auth_wipe(out,32);
    return false;
}
