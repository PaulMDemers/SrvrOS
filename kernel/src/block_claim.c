/* CPU0 metadata-only admission. Claims cover physical byte ranges, not names or
 * callback identity. Callers must keep device/view descriptors stable until
 * release; saved driver callbacks remain trusted kernel capabilities. */
#include <srvros/block.h>
#include <stddef.h>

#define BLOCK_MAX_CLAIMS 40
struct block_claim {
    uint64_t token, offset, length, lease;
    struct block_device *root, *device, *allowed_view;
    bool raw;
};
static struct block_claim claims[BLOCK_MAX_CLAIMS];
static uint64_t claim_serial;
static uint64_t claim_lock(void) {
    uint64_t flags=0;
#ifndef SRVROS_BLOCK_CLAIM_HOST_TEST
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
#endif
    return flags;
}
static void claim_unlock(uint64_t flags) {
#ifndef SRVROS_BLOCK_CLAIM_HOST_TEST
    if(flags&(1ull<<9))__asm__ volatile("sti" : : : "memory");
#else
    (void)flags;
#endif
}
bool block_raw_range(struct block_device *device,uint64_t offset,uint64_t length,
    struct block_device **parent,uint64_t *raw_offset) {
    if(!parent || !raw_offset || !length)return false;
    /* Bound malformed/cyclic chains. Validate the entire view at every edge,
     * not just this request, so aliases cannot claim an out-of-parent tail. */
    for(unsigned depth=0;depth<BLOCK_MAX_DEVICES;depth++) {
        if(!device || !device->used || !device->block_size ||
           device->block_count>UINT64_MAX/device->block_size)return false;
        uint64_t size=device->block_count*device->block_size;
        if(offset>size || length>size-offset)return false;
        if(!device->backing) { *parent=device;*raw_offset=offset;return true; }
        struct block_device *backing=device->backing;
        if(!backing->used || !backing->block_size ||
           backing->block_count>UINT64_MAX/backing->block_size)return false;
        uint64_t total=backing->block_size*backing->block_count;
        if(device->backing_offset>total || size>total-device->backing_offset)return false;
        offset+=device->backing_offset;
        device=backing;
    }
    return false;
}
bool block_device_online(struct block_device *device) {
    struct block_device *root;uint64_t offset;
    if(!block_raw_range(device,0,block_device_size(device),&root,&offset))return false;
    for(unsigned depth=0;depth<BLOCK_MAX_DEVICES;depth++) {
        if(device->offline)return false;
        if(device==root)return true;
        device=device->backing;
    }
    return false;
}
static bool overlaps(const struct block_claim *a,const struct block_claim *b) {
    return a->root==b->root && a->offset<b->offset+b->length && b->offset<a->offset+a->length;
}
static bool describe(struct block_device *d,struct block_claim *c) {
    if(!block_device_online(d))return false;
    c->device=d;c->length=block_device_size(d);
    return block_raw_range(d,0,c->length,&c->root,&c->offset);
}
/* A whole-transport handoff must not revoke another partition's callbacks. */
bool block_claim_available(struct block_device *device,struct block_device *except_mount) {
    uint64_t flags=claim_lock();struct block_claim candidate={0};
    bool ok=describe(device,&candidate);
    for(unsigned i=0;ok && i<BLOCK_MAX_CLAIMS;i++)
        if(claims[i].token && overlaps(&candidate,&claims[i]) &&
           (claims[i].raw || claims[i].device!=except_mount))ok=false;
    claim_unlock(flags);return ok;
}
static uint64_t acquire(struct block_device *d,bool raw,struct block_device *allowed) {
    uint64_t flags=claim_lock(),token=0;struct block_claim c={0};
    if(!describe(d,&c) || claim_serial==UINT64_MAX)goto out;
    c.raw=raw;c.allowed_view=allowed;
    if(allowed) {
        struct block_claim child={0};
        if(!raw || !describe(allowed,&child) || child.root!=c.root ||
           child.offset<c.offset || child.offset-c.offset>c.length || child.length>c.length-(child.offset-c.offset))goto out;
    }
    unsigned slot=BLOCK_MAX_CLAIMS;
    for(unsigned i=0;i<BLOCK_MAX_CLAIMS;i++) {
        if(!claims[i].token) {slot=i;continue;}
        if(!overlaps(&c,&claims[i]))continue;
        if(!raw && claims[i].raw && claims[i].allowed_view==d &&
           c.offset>=claims[i].offset && c.offset-claims[i].offset<=claims[i].length &&
           c.length<=claims[i].length-(c.offset-claims[i].offset))c.lease=claims[i].token;
        else goto out;
    }
    if(slot==BLOCK_MAX_CLAIMS)goto out;
    token=c.token=++claim_serial;claims[slot]=c;
out:
    claim_unlock(flags);return token;
}
uint64_t block_claim_mount(struct block_device *device) { return acquire(device,false,NULL); }
uint64_t block_claim_raw(struct block_device *device,struct block_device *allowed_view) {
    return acquire(device,true,allowed_view);
}
bool block_claim_release(uint64_t token) {
    uint64_t flags=claim_lock();bool ok=false;
    if(!token)goto out;
    for(unsigned i=0;i<BLOCK_MAX_CLAIMS;i++)if(claims[i].token && claims[i].lease==token)goto out;
    for(unsigned i=0;i<BLOCK_MAX_CLAIMS;i++)if(claims[i].token==token) {
        claims[i]=(struct block_claim){0};ok=true;break;
    }
out:
    claim_unlock(flags);return ok;
}
