#include <srvros/scanout_memory.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct scanout_memory a,b,copy;
static struct scanout_memory_pool pool;
static struct scanout_publish_pool publications;
static struct scanout_space space,space_copy;
static struct scanout_space_pool spaces;
static struct scanout_memory_token token;
static struct scanout_memory_layout layout;
static struct {
    uint64_t entries[SCANOUT_SPACE_PAGES],time,physical;
    unsigned alloc,valid,release,encode,write,flush,readers,operation,fail_at,fail_write_at;
    bool pending,release_fail,allocate_fail,validate_fail,reenter;
} model;
static bool step(void) {
    model.operation++;
    if(model.reenter) {
        struct scanout_memory_token t={&a,a.epoch,a.owner};struct scanout_memory_cpu cpu;
        assert(!scanout_memory_close(&a,&t));assert(!scanout_memory_apply(&a,&t));
        assert(!scanout_memory_cpu_begin(&a,&t,&cpu));
    }
    return model.operation!=model.fail_at;
}
static uint64_t allocate(void *ctx,uint32_t pages) {
    assert(ctx==&model && pages==layout.pages);model.alloc++;assert(a.busy);
    if(!step() || model.allocate_fail)return 0;
    return model.physical;
}
static bool validate(void *ctx,uint64_t pa,uint32_t pages) {
    assert(ctx==&model && pa==model.physical && pages==layout.pages);model.valid++;
    return step() && !model.validate_fail;
}
static bool release(void *ctx,uint64_t pa,uint32_t pages) {
    assert(ctx==&model && pa==model.physical && pages==layout.pages);model.release++;
    assert(!a.range.object && (!a.publication.exposed || a.publication.state==PUB_RETIRED));
    return step() && !model.release_fail;
}
static bool guard(void *ctx){assert(ctx==&model);return step();}
static bool read_entry(void *ctx,uint32_t index,uint64_t *v) {
    assert(ctx==&model && index<SCANOUT_SPACE_PAGES);if(!step())return false;*v=model.entries[index];return true;
}
static bool write_entry(void *ctx,uint32_t index,uint64_t v) {
    assert(ctx==&model && index<SCANOUT_SPACE_PAGES && a.publication.exposed);
    assert(!model.release);model.write++;model.entries[index]=v;
    return step() && model.write!=model.fail_write_at;
}
static bool flush(void *ctx){assert(ctx==&model);model.flush++;return step();}
static bool readers(void *ctx){assert(ctx==&model);model.readers++;return step() && !model.pending;}
static uint64_t now(void *ctx){assert(ctx==&model);return ++model.time;}
static bool encode(void *ctx,uint64_t pa,uint64_t *v) {
    assert(ctx==&model && pa==model.physical+(uint64_t)model.encode*4096);model.encode++;
    if(!step())return false;
    *v=pa|1;return true;
}
static struct scanout_memory_binding binding(void) {
    return (struct scanout_memory_binding){&model,encode,{&model,guard,read_entry,write_entry,flush,readers,now}};
}
static struct scanout_memory_io allocator(void){return (struct scanout_memory_io){&model,allocate,validate,release};}
static void setup(bool tiled) {
    memset(&a,0,sizeof a);memset(&b,0,sizeof b);memset(&space,0,sizeof space);memset(&model,0,sizeof model);
    pool.epoch=publications.epoch=spaces.epoch=0;model.physical=0x100000;model.time=100;
    assert(scanout_memory_layout(tiled?1440:1,tiled?900:1,tiled?6144:64,tiled,&layout));
    for(unsigned i=0;i<SCANOUT_SPACE_PAGES;i++)model.entries[i]=0x200000001ull+(uint64_t)i*4096;
}
static void create(void){assert(scanout_memory_create(&a,&pool,7,&layout,0x100000000ull,allocator(),&token));}
static void arena(void) {
    struct scanout_space_range grant={0,SCANOUT_SPACE_PAGES},protect={0,8704};
    assert(scanout_space_open(&space,&spaces,7,SCANOUT_SPACE_PAGES,&grant,1,&protect,1));
}
int main(void) {
    setup(true);assert(layout.visible_bytes==5529600 && layout.bytes==5554176 && layout.rows==904 && layout.pages==1356);
    struct scanout_memory_layout saved=layout;
    assert(!scanout_memory_layout(1440,900,6145,true,&layout) && !memcmp(&saved,&layout,sizeof layout));
    assert(!scanout_memory_layout(0,900,6144,true,&layout));assert(!scanout_memory_layout(4096,4096,65536,true,&layout));
    create();struct scanout_memory_cpu cpu;
    assert(scanout_memory_cpu_begin(&a,&token,&cpu));assert(cpu.bytes==5554176);
    assert(!scanout_memory_close(&a,&token) && !model.release);
    assert(!scanout_memory_cpu_begin(&a,&token,&(struct scanout_memory_cpu){0}));
    assert(scanout_memory_cpu_end(&a,&cpu));assert(!scanout_memory_cpu_end(&a,&cpu));
    assert(scanout_memory_close(&a,&token) && model.release==1 && a.releases==1);
    assert(!scanout_memory_close(&a,&token));struct scanout_memory_token old=token;
    create();assert(token.epoch>old.epoch && !scanout_memory_close(&a,&old));
    copy=a;struct scanout_memory_token forged={&copy,copy.epoch,copy.owner};
    assert(!scanout_memory_close(&copy,&forged));assert(!scanout_memory_cpu_begin(&copy,&forged,&cpu));
    assert(!scanout_memory_create(&copy,&pool,7,&layout,0x100000000ull,allocator(),&forged));
    assert(scanout_memory_close(&a,&token));
    setup(true);create();arena();model.reenter=true;
    assert(scanout_memory_prepare(&a,&token,&space,&publications,16,binding()));
    assert(a.range.first==8704 && a.range.pages==1356 && model.encode==1356);
    assert(!scanout_memory_cpu_begin(&a,&token,&cpu));assert(scanout_memory_apply(&a,&token));
    model.pending=true;assert(!scanout_memory_close(&a,&token) && !model.release && a.state==MEM_GPU);
    assert(!scanout_memory_cpu_begin(&a,&token,&cpu));model.pending=false;
    assert(scanout_memory_close(&a,&token) && model.release==1 && model.write==2*1356 && model.flush==2);
    for(unsigned i=8704;i<8704+1356;i++)assert(model.entries[i]==0x200000001ull+(uint64_t)i*4096);
    assert(scanout_space_close(&space,7,space.epoch));
    setup(false);create();arena();space_copy=space;struct scanout_space_token r;
    assert(!scanout_space_reserve(&space_copy,7,space_copy.epoch,1,1,&r));
    assert(!scanout_memory_prepare(&a,&token,&space_copy,&publications,1,binding()));
    assert(!scanout_space_open(&space_copy,&spaces,7,16,0,0,0,0));assert(scanout_memory_close(&a,&token));
    setup(false);create();arena();assert(scanout_memory_prepare(&a,&token,&space,&publications,1,binding()));
    assert(scanout_memory_close(&a,&token) && !model.write && model.release==1);
    setup(false);model.allocate_fail=true;assert(!scanout_memory_create(&a,&pool,7,&layout,0x100000000ull,allocator(),&token));
    assert(a.state==MEM_RELEASED && !model.release);
    setup(false);create();model.release_fail=true;assert(!scanout_memory_close(&a,&token));
    assert(a.state==MEM_FAULT && a.release_attempts==1 && !a.releases);
    assert(!scanout_memory_close(&a,&token) && model.release==1);
    /* Fail each callback operation in the actual joined mapping transaction.
     * Exposed failures must retain RAM; clean pre-exposure failures can cancel. */
    setup(false);create();arena();assert(scanout_memory_prepare(&a,&token,&space,&publications,1,binding()));
    assert(scanout_memory_apply(&a,&token));assert(scanout_memory_close(&a,&token));unsigned operations=model.operation;
    for(unsigned fail=1;fail<=operations;fail++) {
        setup(false);model.fail_at=fail;
        bool live=scanout_memory_create(&a,&pool,7,&layout,0x100000000ull,allocator(),&token);arena();
        if(live) {
            if(scanout_memory_prepare(&a,&token,&space,&publications,1,binding()))(void)scanout_memory_apply(&a,&token);
            (void)scanout_memory_close(&a,&token);
        }
        if(a.state==MEM_FAULT) {
            unsigned count=model.release;assert(!scanout_memory_close(&a,&token));assert(model.release==count);
            if(a.publication.exposed && a.publication.state!=PUB_RETIRED)assert(!model.release);
        }
        assert(model.release<=1);
    }
    for(unsigned index=1;index<=1356;index++) {
        setup(true);create();arena();
        assert(scanout_memory_prepare(&a,&token,&space,&publications,16,binding()));
        model.fail_write_at=index;
        assert(!scanout_memory_apply(&a,&token) && a.state==MEM_FAULT && !model.release);
        assert(a.publication.exposed && space.state==SPACE_FAULT && model.write==index);
        assert(!scanout_memory_close(&a,&token) && !model.release);
    }
    const unsigned restore_faults[]={1,678,1356};
    for(unsigned n=0;n<3;n++) {
        unsigned index=restore_faults[n];
        setup(true);create();arena();
        assert(scanout_memory_prepare(&a,&token,&space,&publications,16,binding()));
        assert(scanout_memory_apply(&a,&token));model.fail_write_at=1356+index;
        assert(!scanout_memory_close(&a,&token) && a.state==MEM_FAULT && !model.release);
    }
    setup(false);struct scanout_memory_token sentinel={0},unchanged=sentinel;
    struct scanout_memory_layout bad=layout;bad.pages++;
    assert(!scanout_memory_create(&a,&pool,7,&bad,0x100000000ull,allocator(),&sentinel));
    assert(!model.alloc && !memcmp(&sentinel,&unchanged,sizeof sentinel));
    assert(!scanout_memory_create(&a,(struct scanout_memory_pool *)&a,7,&layout,0x100000000ull,allocator(),&sentinel));
    assert(!scanout_memory_create(&a,&pool,7,&layout,0x100000000ull,allocator(),(struct scanout_memory_token *)&a));
    printf("scanout-memory: PASS padded1356-page storage, joined publication/restore, pending readers, CPU borrow/close, copies/stale/reentry, %u callback and1356 partial-write failures\n",operations);
}
