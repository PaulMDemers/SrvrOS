#include <srvros/intel_display_capture.h>

/* Linux 028ef9c96e96 display register definitions, Broadwell only. All offsets
 * are in the first 2MiB MMIO block; no GGTT, status W1C, AUX or forcewake reads. */
const uint32_t intel_capture_pages[INTEL_CAPTURE_PAGES] = {
    0x45000, 0x60000, 0x61000, 0x62000, 0x6f000,
    0x70000, 0x71000, 0x72000, 0x7f000
};
/* Scanline, frame count, primary control/stride/surface/offset/live surface,
 * cursor control/base. Raw observations do not grant buffer ownership. */
const uint32_t intel_capture_pipe_offsets[INTEL_CAPTURE_PIPE_WORDS] = {
    0x000, 0x040, 0x180, 0x188, 0x19c, 0x1a4, 0x1ac, 0x080, 0x084
};
/* Horizontal/vertical total, vertical blank, DDI function, transcoder config. */
const uint32_t intel_capture_trans_offsets[INTEL_CAPTURE_TRANS_WORDS] = {
    0x000, 0x00c, 0x010, 0x400, 0x10008
};
static const uint32_t trans_base[INTEL_CAPTURE_TRANS] = {0x60000,0x61000,0x62000,0x6f000};

bool intel_capture_offset_allowed(uint32_t off) {
    for(unsigned i=0;i<4;i++) if(off==0x45400+4*i) return true;
    for(unsigned i=0;i<INTEL_CAPTURE_PIPES;i++) if(off==0x6001c+0x1000*i) return true;
    for(unsigned i=0;i<INTEL_CAPTURE_PIPES;i++)
        for(unsigned j=0;j<INTEL_CAPTURE_PIPE_WORDS;j++)
            if(off==0x70000+0x1000*i+intel_capture_pipe_offsets[j]) return true;
    for(unsigned i=0;i<INTEL_CAPTURE_TRANS;i++)
        for(unsigned j=0;j<INTEL_CAPTURE_TRANS_WORDS;j++)
            if(off==trans_base[i]+intel_capture_trans_offsets[j]) return true;
    return false;
}
static enum intel_capture_status admit(const struct intel_capture_identity *id, uint64_t *base) {
    if(!id->firmware_match || id->id!=0x16268086 || id->class_rev!=0x03000009 ||
       id->subsystem!=0x011b106b || id->pch!=0x9cc38086) return INTEL_CAPTURE_IDENTITY;
    *base=((uint64_t)id->bar[1]<<32)|(id->bar[0]&~15u);
    /* Captured BAR0 alignment; conservative fixed 2MiB display-MMIO prefix.
     * No BAR sizing writes and no interpretation of the remaining GGTT range. */
    if((id->command&2)==0 || !id->power_valid || (id->pm&3) || !id->mmio_safe ||
       (id->bar[0]&0xffffff)!=4 || !*base || *base>((1ull<<39)-0x1000000))
        return INTEL_CAPTURE_ACCESS;
    return INTEL_CAPTURE_OBSERVED;
}
static bool same(const struct intel_capture_identity *a,const struct intel_capture_identity *b) {
    if(a->id!=b->id || a->class_rev!=b->class_rev || a->subsystem!=b->subsystem ||
       a->pch!=b->pch || a->command!=b->command || a->pm!=b->pm ||
       a->power_valid!=b->power_valid || a->firmware_match!=b->firmware_match ||
       a->mmio_safe!=b->mmio_safe) return false;
    for(unsigned i=0;i<4;i++) if(a->bar[i]!=b->bar[i]) return false;
    return true;
}
static bool fb_valid(const struct intel_capture_fb *f,uint64_t base) {
    if(!f->physical || !f->width || f->width>4096 || !f->height || f->height>4096 ||
       f->pitch<f->width*4 || f->pitch>65536 || (f->pitch&3) || f->bpp!=32 ||
       f->model!=1 || f->red_size!=8 || f->red_shift!=16 || f->green_size!=8 ||
       f->green_shift!=8 || f->blue_size!=8 || f->blue_shift!=0) return false;
    uint64_t bytes=f->pitch*f->height;
    if(f->physical>((1ull<<52)-bytes)) return false;
    /* Keep the boot framebuffer out of the entire captured BAR0 window,
     * including reserved/GGTT space that the diagnostic itself never maps. */
    return f->physical+bytes<=base || f->physical>=base+0x1000000;
}
static bool read_word(const struct intel_capture_io *io,struct intel_capture_sample *s,
                      uint32_t off,uint32_t *value) {
    if(!intel_capture_offset_allowed(off) || !io->read(io->ctx,off,value)) return false;
    s->reads++;
    return *value!=UINT32_MAX;
}
static enum intel_capture_status sample(const struct intel_capture_io *io,
                                       struct intel_capture_sample *s) {
    s->begin_us=io->now(io->ctx);
    for(unsigned i=0;i<4;i++) {
        if(!read_word(io,s,0x45400+4*i,&s->well_before[i])) return INTEL_CAPTURE_REGISTER;
        if(!(s->well_before[i]&(1u<<30))) return INTEL_CAPTURE_WELL_OFF;
    }
    for(unsigned i=0;i<INTEL_CAPTURE_PIPES;i++)
        for(unsigned j=0;j<INTEL_CAPTURE_PIPE_WORDS;j++)
            if(!read_word(io,s,0x70000+0x1000*i+intel_capture_pipe_offsets[j],&s->pipe[i][j]))
                return INTEL_CAPTURE_REGISTER;
    for(unsigned i=0;i<INTEL_CAPTURE_PIPES;i++)
        if(!read_word(io,s,0x6001c+0x1000*i,&s->source[i])) return INTEL_CAPTURE_REGISTER;
    for(unsigned i=0;i<INTEL_CAPTURE_TRANS;i++)
        for(unsigned j=0;j<INTEL_CAPTURE_TRANS_WORDS;j++)
            if(!read_word(io,s,trans_base[i]+intel_capture_trans_offsets[j],&s->trans[i][j]))
                return INTEL_CAPTURE_REGISTER;
    for(unsigned i=0;i<4;i++) {
        if(!read_word(io,s,0x45400+4*i,&s->well_after[i])) return INTEL_CAPTURE_REGISTER;
        if(s->well_after[i]!=s->well_before[i]) return INTEL_CAPTURE_UNSTABLE;
    }
    s->end_us=io->now(io->ctx);
    if(s->end_us<s->begin_us || s->end_us-s->begin_us>100000) return INTEL_CAPTURE_CLOCK;
    return INTEL_CAPTURE_OBSERVED;
}
bool intel_display_capture(struct intel_capture_result *r,const struct intel_capture_fb *fb,
                           const struct intel_capture_io *io) {
    if(!r) return false;
    *r=(struct intel_capture_result){0};
    if(!fb || !io || !io->identity || !io->map || !io->unmap || !io->read || !io->now) return false;
    r->fb=*fb;
    if(!io->identity(io->ctx,&r->before)) {r->status=INTEL_CAPTURE_IDENTITY;return false;}
    r->status=admit(&r->before,&r->mmio_base);
    if(r->status!=INTEL_CAPTURE_OBSERVED) return false;
    if(!fb_valid(fb,r->mmio_base)) {r->status=INTEL_CAPTURE_FRAMEBUFFER;return false;}
    for(unsigned i=0;i<INTEL_CAPTURE_PAGES;i++) {
        if(!io->map(io->ctx,i,r->mmio_base+intel_capture_pages[i])) {r->status=INTEL_CAPTURE_MAP;goto done;}
        r->mapped++;
    }
    if(!io->identity(io->ctx,&r->after) || !same(&r->before,&r->after)) {
        r->status=INTEL_CAPTURE_UNSTABLE;goto done;
    }
    r->status=sample(io,&r->sample[0]);
    if(r->status!=INTEL_CAPTURE_OBSERVED) goto done;
    uint64_t start=io->now(io->ctx),previous=start;
    if(start<r->sample[0].end_us) {r->status=INTEL_CAPTURE_CLOCK;goto done;}
    bool elapsed=false;
    for(unsigned spin=0;spin<1000000;spin++) {
        uint64_t now=io->now(io->ctx);
        if(now<previous || now-start>100000) {r->status=INTEL_CAPTURE_CLOCK;goto done;}
        previous=now;
        if(now-start>=20000) {elapsed=true;break;}
    }
    if(!elapsed) {r->status=INTEL_CAPTURE_CLOCK;goto done;}
    if(!io->identity(io->ctx,&r->after) || !same(&r->before,&r->after)) {
        r->status=INTEL_CAPTURE_UNSTABLE;goto done;
    }
    r->status=sample(io,&r->sample[1]);
    if(r->status!=INTEL_CAPTURE_OBSERVED) goto done;
    if(r->sample[1].begin_us<previous || r->sample[1].begin_us-r->sample[0].end_us<20000 ||
       r->sample[1].begin_us-r->sample[0].end_us>100000) {r->status=INTEL_CAPTURE_CLOCK;goto done;}
    if(!io->identity(io->ctx,&r->after) || !same(&r->before,&r->after)) {
        r->status=INTEL_CAPTURE_UNSTABLE;goto done;
    }
    for(unsigned k=0;k<4;k++)
        if(r->sample[0].well_before[k]!=r->sample[1].well_before[k]) {r->status=INTEL_CAPTURE_UNSTABLE;goto done;}
    for(unsigned i=0;i<INTEL_CAPTURE_PIPES;i++) {
        if(r->sample[0].source[i]!=r->sample[1].source[i]) {r->status=INTEL_CAPTURE_UNSTABLE;goto done;}
        for(unsigned j=2;j<INTEL_CAPTURE_PIPE_WORDS;j++)
            if(r->sample[0].pipe[i][j]!=r->sample[1].pipe[i][j]) {r->status=INTEL_CAPTURE_UNSTABLE;goto done;}
        uint32_t *p=r->sample[1].pipe[i];
        if(p[2]&(1u<<31)) r->active_planes|=1u<<i;
        uint64_t aperture=((uint64_t)r->before.bar[3]<<32)|(r->before.bar[2]&~15u);
        /* A numerical relation only: aperture extent and GGTT PTE backing are
         * deliberately unproven. No backing address or PTE is dereferenced. */
        if((p[2]&(1u<<31)) && ((p[2]>>26)&15)==6 && !(p[2]&((1u<<10)|(1u<<15))) &&
           ((r->sample[1].source[i]>>16)&0x1fff)+1==fb->width &&
           (r->sample[1].source[i]&0xfff)+1==fb->height &&
           p[3]==fb->pitch && !p[5] && p[4]==p[6] && (r->before.bar[2]&15)==12 &&
           aperture && fb->physical>=aperture && fb->physical-aperture==(p[4]&~4095u))
            r->linear_relation|=1u<<i;
    }
    for(unsigned i=0;i<INTEL_CAPTURE_TRANS;i++)
        for(unsigned j=0;j<INTEL_CAPTURE_TRANS_WORDS;j++)
            if(r->sample[0].trans[i][j]!=r->sample[1].trans[i][j]) {r->status=INTEL_CAPTURE_UNSTABLE;goto done;}
done:
    for(unsigned i=r->mapped;i>0;i--) if(!io->unmap(io->ctx,i-1)) r->retained|=1u<<(i-1);
    if(r->retained) r->status=INTEL_CAPTURE_RETAINED_MAPPING;
    return r->status==INTEL_CAPTURE_OBSERVED;
}
const char *intel_capture_status_name(enum intel_capture_status s) {
    static const char *names[]={"invalid","observed","identity-refused","access-refused",
        "framebuffer-refused","map-failed","well-off","register-invalid","unstable",
        "clock-invalid","mapping-retained"};
    return (unsigned)s<sizeof(names)/sizeof(names[0])?names[s]:"invalid";
}
