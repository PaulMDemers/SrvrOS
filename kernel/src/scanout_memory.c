#include <srvros/scanout_memory.h>

bool scanout_memory_layout(uint32_t width,uint32_t height,uint32_t pitch,bool tiled,struct scanout_memory_layout *out) {
    if(!out || !width || width>4096 || !height || height>4096 || pitch>65536 ||
       (uint64_t)width*4>pitch || pitch%(tiled?512u:64u))return false;
    uint32_t rows=tiled?(height+7u)&~7u:height;
    uint64_t bytes=(uint64_t)pitch*rows,pages=bytes/4096+!!(bytes%4096);
    if(!pages || pages>SCANOUT_PUBLISH_PAGES)return false;
    *out=(struct scanout_memory_layout){(uint64_t)pitch*height,bytes,width,height,pitch,rows,(uint32_t)pages,tiled};return true;
}
static bool aliases(const void *a,uint64_t bytes,const struct scanout_memory *m) {
    uintptr_t p=(uintptr_t)a,b=(uintptr_t)m;
    return !a || p>UINTPTR_MAX-bytes || b>UINTPTR_MAX-sizeof(*m) || (p<b+sizeof(*m) && b<p+bytes);
}
static bool auth(const struct scanout_memory *m,const struct scanout_memory_token *t) {
    return m && t && m->object==m && !m->busy && m->epoch && m->owner &&
        t->object==m && t->epoch==m->epoch && t->owner==m->owner;
}
static bool fault(struct scanout_memory *m,unsigned why) {
    m->state=MEM_FAULT;m->error=why;m->busy=false;
    if(m->publication.exposed)(void)scanout_space_quarantine(m->space,&m->range);
    return false;
}
static bool backing(struct scanout_memory *m) {
    if(!m->allocator.validate(m->allocator.ctx,m->physical,m->layout.pages))return fault(m,MEM_BACKING);
    return true;
}
bool scanout_memory_create(struct scanout_memory *m,struct scanout_memory_pool *pool,uint64_t owner,
    const struct scanout_memory_layout *layout,uint64_t limit,struct scanout_memory_io io,struct scanout_memory_token *out) {
    if(!m || (m->object && m->object!=m) || m->busy || (m->state!=MEM_EMPTY && m->state!=MEM_RELEASED) ||
       !owner || !layout || !io.allocate || !io.validate || !io.release || aliases(pool,sizeof(*pool),m) ||
       aliases(out,sizeof(*out),m) || pool->epoch==UINT64_MAX || pool->epoch<m->epoch)return false;
    struct scanout_memory_layout checked;
    if(layout->tiled>1 || !scanout_memory_layout(layout->width,layout->height,layout->pitch,layout->tiled,&checked) ||
       layout->rows!=checked.rows || layout->bytes!=checked.bytes || layout->visible_bytes!=checked.visible_bytes ||
       layout->pages!=checked.pages || checked.pages>limit/4096)return false;
    uint64_t epoch=pool->epoch+1;
    /* Clear directly in owned storage, never by copying a large stack object.
     * Capture all small inputs before clearing possible caller aliases. */
    unsigned char *p=(unsigned char *)m;
    for(uint64_t i=0;i<sizeof(*m);i++)p[i]=0;
    m->object=m;m->epoch=epoch;pool->epoch=epoch;m->owner=owner;m->layout=checked;m->allocator=io;
    m->state=MEM_ALLOCATING;m->busy=true;
    m->allocations=1;m->physical=io.allocate(io.ctx,checked.pages);
    if(!m->physical){m->state=MEM_RELEASED;m->error=MEM_ALLOC;m->busy=false;return false;}
    if((m->physical&4095) || m->physical>limit-(uint64_t)checked.pages*4096)return fault(m,MEM_BACKING);
    if(!backing(m))return false;
    m->state=MEM_RAM;m->busy=false;*out=(struct scanout_memory_token){m,epoch,owner};return true;
}
bool scanout_memory_cpu_begin(struct scanout_memory *m,const struct scanout_memory_token *t,struct scanout_memory_cpu *out) {
    if(!auth(m,t) || m->state!=MEM_RAM || m->closing || m->cpu_serial || m->range.object ||
       m->serial==UINT64_MAX || aliases(out,sizeof(*out),m))return false;
    m->busy=true;if(!backing(m))return false;
    m->cpu_serial=++m->serial;*out=(struct scanout_memory_cpu){*t,m->cpu_serial,m->physical,(uint64_t)m->layout.pages*4096};m->busy=false;return true;
}
bool scanout_memory_cpu_end(struct scanout_memory *m,const struct scanout_memory_cpu *cpu) {
    if(!cpu || !auth(m,&cpu->memory) || m->state!=MEM_RAM || !m->cpu_serial ||
       cpu->serial!=m->cpu_serial || cpu->physical!=m->physical || cpu->bytes!=(uint64_t)m->layout.pages*4096)return false;
    m->busy=true;if(!backing(m))return false;
    m->cpu_serial=0;m->busy=false;return true;
}
bool scanout_memory_prepare(struct scanout_memory *m,const struct scanout_memory_token *t,struct scanout_space *space,
    struct scanout_publish_pool *pool,uint32_t align,struct scanout_memory_binding binding) {
    if(!auth(m,t) || m->state!=MEM_RAM || m->closing || m->cpu_serial || m->range.object || !space ||
       space->object!=space || space->state!=SPACE_ACTIVE || space->owner!=m->owner || !pool ||
       aliases(pool,sizeof(*pool),m) || aliases(space,sizeof(*space),m) || !binding.encode)return false;
    m->busy=true;if(!backing(m))return false;
    if(!scanout_space_reserve(space,m->owner,space->epoch,m->layout.pages,align,&m->range)){m->busy=false;return false;}
    m->space=space;
    for(uint32_t i=0;i<m->layout.pages;i++)if(!binding.encode(binding.ctx,m->physical+(uint64_t)i*4096,&m->entries[i])) {
        m->error=MEM_MAPPING;m->busy=false;return false;
    }
    bool ok=scanout_publish_prepare(&m->publication,pool,m->owner,space,&m->range,m->entries,m->layout.pages,
        &binding.publication,&m->publication_token);
    if(!ok){m->error=MEM_MAPPING;m->busy=false;return false;}
    if(!backing(m))return false;
    m->state=MEM_PREPARED;m->busy=false;return true;
}
bool scanout_memory_apply(struct scanout_memory *m,const struct scanout_memory_token *t) {
    if(!auth(m,t) || m->state!=MEM_PREPARED || m->closing || m->cpu_serial)return false;
    m->busy=true;if(!backing(m))return false;
    bool ok=scanout_publish_apply(&m->publication,&m->publication_token);
    if(!ok) {
        if(m->publication.exposed)return fault(m,MEM_MAPPING);
        m->error=MEM_MAPPING;m->busy=false;return false;
    }
    if(!backing(m))return false;
    m->state=MEM_GPU;m->busy=false;return true;
}
bool scanout_memory_close(struct scanout_memory *m,const struct scanout_memory_token *t) {
    if(!auth(m,t) || (m->state!=MEM_RAM && m->state!=MEM_PREPARED && m->state!=MEM_GPU))return false;
    m->closing=true;if(m->cpu_serial)return false;
    m->busy=true;if(!backing(m))return false;
    if(m->publication.exposed && m->publication.state!=PUB_RETIRED) {
        if(!scanout_publish_restore(&m->publication,&m->publication_token)) {
            if(m->publication.state==PUB_FAULT)return fault(m,MEM_MAPPING);
            m->busy=false;return false;
        }
    } else if(m->publication.state==PUB_PREPARED && !scanout_publish_cancel(&m->publication,&m->publication_token))return fault(m,MEM_MAPPING);
    if(m->range.object && !scanout_space_release(m->space,&m->range))return fault(m,MEM_MAPPING);
    m->range.object=0;
    if(!backing(m))return false;
    m->release_attempts++;
    if(!m->allocator.release(m->allocator.ctx,m->physical,m->layout.pages))return fault(m,MEM_RELEASE);
    m->releases++;m->state=MEM_RELEASED;m->busy=false;return true;
}
