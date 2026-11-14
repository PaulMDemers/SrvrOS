/* Explicit synthetic lifetime-fixture only. These arrays are NOT GGTT MMIO.
 * Faulted model bank remains pinned in static RAM until reboot. */
#include <srvros/scanout_publish.h>
#define PUB_MODEL_PAGES 32u
struct pub_model_bank {
    struct scanout_space space;
    struct scanout_space_pool ranges;
    struct scanout_publication pub;
    struct scanout_publish_pool epochs;
    struct scanout_space_token lease;
    struct scanout_publish_token token;
    uint64_t visible[PUB_MODEL_PAGES],translation[PUB_MODEL_PAGES],posted[PUB_MODEL_PAGES],planned[PUB_MODEL_PAGES];
    bool readers,fail_write;
};
static struct pub_model_bank pub_model_banks[2];
static bool pub_model_guard(void *p){return p==&pub_model_banks[0] || p==&pub_model_banks[1];}
static bool pub_model_index(struct pub_model_bank *b,uint32_t i){return i>=b->lease.first && i-b->lease.first<PUB_MODEL_PAGES;}
static bool pub_model_read(void *p,uint32_t i,uint64_t *v) {
    struct pub_model_bank *b=p;if(!pub_model_index(b,i))return false;
    *v=b->visible[i-b->lease.first];return true;
}
static bool pub_model_write(void *p,uint32_t i,uint64_t v) {
    struct pub_model_bank *b=p;
    if(!pub_model_index(b,i) || !b->pub.exposed || b->space.leases[b->lease.slot].use!=SPACE_EXPOSED)return false;
    b->posted[i-b->lease.first]=v;return !b->fail_write;
}
static bool pub_model_flush(void *p) {
    struct pub_model_bank *b=p;
    for(unsigned i=0;i<PUB_MODEL_PAGES;i++)b->visible[i]=b->translation[i]=b->posted[i];
    return true;
}
static bool pub_model_readers(void *p){return ((struct pub_model_bank *)p)->readers;}
static uint64_t pub_model_now(void *p){(void)p;return clock_us();}
static bool pub_model_view(struct pub_model_bank *b,bool after) {
    for(unsigned i=0;i<PUB_MODEL_PAGES;i++) {
        uint64_t expected=after?b->planned[i]:0x100000001ull+4096ull*i;
        if(b->visible[i]!=expected || b->translation[i]!=expected)return false;
    }
    return true;
}
static bool pub_model_run(void) {
    bool ok=true;unsigned completed=0;
    for(unsigned n=0;n<2 && ok;n++) {
        struct pub_model_bank *b=&pub_model_banks[n];
        struct scanout_space_range managed={0,SCANOUT_SPACE_PAGES},protected={0,8704};
        for(unsigned i=0;i<PUB_MODEL_PAGES;i++) {
            b->visible[i]=b->translation[i]=b->posted[i]=0x100000001ull+4096ull*i;
            b->planned[i]=0x200000001ull+4096ull*i;
        }
        struct scanout_publish_io io={b,pub_model_guard,pub_model_read,pub_model_write,pub_model_flush,pub_model_readers,pub_model_now};
        ok=scanout_space_open(&b->space,&b->ranges,9,SCANOUT_SPACE_PAGES,&managed,1,&protected,1) &&
           scanout_space_reserve(&b->space,9,b->space.epoch,PUB_MODEL_PAGES,1,&b->lease) && b->lease.first==8704 &&
           scanout_publish_prepare(&b->pub,&b->epochs,9,&b->space,&b->lease,b->planned,PUB_MODEL_PAGES,&io,&b->token);
        if(!ok)break;
        if(!n) {
            ok=scanout_publish_apply(&b->pub,&b->token) && pub_model_view(b,true);
            uint32_t writes=b->pub.writes;
            ok=ok && !scanout_publish_restore(&b->pub,&b->token) && b->pub.state==PUB_PUBLISHED && b->pub.writes==writes;
            b->readers=true;
            ok=ok && scanout_space_close(&b->space,9,b->space.epoch) && scanout_publish_restore(&b->pub,&b->token) &&
               pub_model_view(b,false) && scanout_space_release(&b->space,&b->lease) && b->space.state==SPACE_CLOSED;
        } else {
            b->fail_write=true;
            ok=!scanout_publish_apply(&b->pub,&b->token) && b->pub.state==PUB_FAULT && b->space.state==SPACE_FAULT &&
               b->space.leases[b->lease.slot].use==SPACE_QUARANTINED && !scanout_space_release(&b->space,&b->lease);
        }
        if(ok)completed++;
    }
    console_printf("scanout-publish-model: v=1 ok=%u cases=%u pages=32 writes=%u flushes=%u quarantined=%u hardware_writes=0 dma_authority=0 ownership=model\n",
        (uint64_t)ok,(uint64_t)completed,(uint64_t)(pub_model_banks[0].pub.writes+pub_model_banks[1].pub.writes),
        (uint64_t)(pub_model_banks[0].pub.flushes+pub_model_banks[1].pub.flushes),
        (pub_model_banks[0].pub.state==PUB_FAULT?32ull:0ull)+(pub_model_banks[1].pub.state==PUB_FAULT?32ull:0ull));
    return ok;
}
