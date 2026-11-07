#include <srvros/intel_gt_preflight.h>
static bool same(const struct intel_gt_endpoint *a,const struct intel_gt_endpoint *b) {
    return a->mmio==b->mmio && a->asls==b->asls && a->swsci==b->swsci;
}
static bool tick(struct intel_gt_preflight_result *r,const struct intel_gt_preflight_io *io,struct intel_gt_sample *s) {
    uint64_t t=io->now(io->context);
    if(!t || t<s->end_us || t-s->begin_us>100000){r->status=GTP_CLOCK;return false;}
    s->end_us=t;return true;
}
bool intel_gt_preflight(struct intel_gt_preflight_result *r,const struct intel_gt_preflight_io *io) {
    if(!r)return false;
    *r=(struct intel_gt_preflight_result){0};
    if(!io || !io->guard || !io->endpoint || !io->map || !io->read || !io->unmap || !io->now)return false;
    if(!io->guard(io->context) || !io->endpoint(io->context,&r->endpoint) ||
       r->endpoint.mmio!=0xc0000000 || r->endpoint.asls==UINT32_MAX || r->endpoint.swsci==UINT32_MAX)return false;
    bool mapped=false;r->status=GTP_ACCESS;
    const uint32_t offsets[2]={INTEL_GT_REQUEST,INTEL_GT_ACK};
    for(unsigned n=0;n<2;n++) {
        struct intel_gt_sample *s=&r->sample[n];s->begin_us=s->end_us=io->now(io->context);
        if(!s->begin_us || (n && (s->begin_us<r->sample[0].end_us || s->begin_us-r->sample[0].end_us>100000))){r->status=GTP_CLOCK;goto done;}
        struct intel_gt_endpoint e={0};
        if(!io->guard(io->context) || !io->endpoint(io->context,&e) || !same(&e,&r->endpoint)){r->status=GTP_CHANGED;goto done;}
        for(unsigned k=0;k<2;k++) {
            if(!tick(r,io,s) || !io->map(io->context,offsets[k]&~4095u))goto done;
            mapped=true;r->maps++;
            uint32_t value;
            if(!tick(r,io,s) || !io->read(io->context,offsets[k],&value)){if(r->status!=GTP_CLOCK)r->status=GTP_READ;goto done;}
            s->reads++;if(k)s->ack=value;else s->requests=value;
            if(value==UINT32_MAX){r->status=GTP_READ;goto done;}
            if(!io->unmap(io->context)){r->status=GTP_RETAINED;r->retained=1;return false;}
            mapped=false;
        }
        if(!tick(r,io,s) || !io->guard(io->context) || !io->endpoint(io->context,&e) || !same(&e,&r->endpoint)){
            if(r->status!=GTP_CLOCK)r->status=GTP_CHANGED;
            goto done;
        }
        if(!tick(r,io,s) || s->end_us==s->begin_us){r->status=GTP_CLOCK;goto done;}
        if(n && (s->requests!=r->sample[0].requests || s->ack!=r->sample[0].ack)){r->status=GTP_CHANGED;goto done;}
    }
    r->status=GTP_OBSERVED;
done:
    if(mapped && !io->unmap(io->context)){r->status=GTP_RETAINED;r->retained=1;}
    return r->status==GTP_OBSERVED;
}
