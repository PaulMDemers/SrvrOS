#include <srvros/intel_gt_firmware.h>
const uint32_t intel_fw_offsets[INTEL_FW_WORDS]={0,4,8,12,16,20,0x58,0x100,0x104,0x108,0x200,0x300,0x304};
static bool same(const struct intel_fw_endpoint *a,const struct intel_fw_endpoint *b) {
 return a->base==b->base && a->length==b->length && a->type==b->type && a->asls==b->asls && a->swsci==b->swsci;
}
static bool tick(struct intel_fw_result *r,const struct intel_fw_io *io,struct intel_fw_sample *s) {
 uint64_t t=io->now(io->context);
 if(!t || t<s->end_us || t-s->begin_us>100000){r->status=IFW_CLOCK;return false;}s->end_us=t;return true;
}
bool intel_fw_observe(struct intel_fw_result *r,const struct intel_fw_io *io) {
 if(!r)return false;
 *r=(struct intel_fw_result){0};
 if(!io || !io->guard || !io->endpoint || !io->map || !io->read || !io->unmap || !io->now)return false;
 if(!io->guard(io->context) || !io->endpoint(io->context,&r->endpoint))return false;
 const struct intel_fw_endpoint *e=&r->endpoint;
 /* This first profile supports a single containing ACPI NVS resource and a
  * single header/readiness page; do not round a pointer down and trust a gap. */
 if(e->type!=3 || !e->length || e->base>UINT64_MAX-e->length || e->asls<4096 || (e->asls&3) ||
    (e->asls&4095)>4096-0x308 || e->swsci==UINT32_MAX || e->base>(e->asls&~4095ull) ||
    (uint64_t)e->asls+8192>e->base+e->length)return false;
 bool mapped=false;r->status=IFW_ACCESS;
 for(unsigned n=0;n<2;n++) {
  struct intel_fw_sample *s=&r->sample[n];struct intel_fw_endpoint fresh={0};
  s->begin_us=s->end_us=io->now(io->context);
  if(!s->begin_us || (n && (s->begin_us<r->sample[0].end_us || s->begin_us-r->sample[0].end_us>100000))){r->status=IFW_CLOCK;goto done;}
  if(!io->guard(io->context) || !io->endpoint(io->context,&fresh) || !same(e,&fresh)){r->status=IFW_CHANGED;goto done;}
  if(!tick(r,io,s) || !io->map(io->context,e->asls&~4095ull))goto done;
  mapped=true;r->maps++;
  for(unsigned i=0;i<INTEL_FW_WORDS;i++) {
   if(i>=7) {
    uint32_t bit=i<10?1u:i==10?2u:4u;
    if(!(s->words[6]&bit))continue;
   }
   if(!tick(r,io,s) || !io->read(io->context,intel_fw_offsets[i],&s->words[i])){if(r->status!=IFW_CLOCK)r->status=IFW_READ;goto done;}s->reads++;
   if(i==6 && (s->words[0]!=0x65746e49u || s->words[1]!=0x6172476cu || s->words[2]!=0x63696870u || s->words[3]!=0x6d654d73u ||
       s->words[4]!=8 || (s->words[5]>>24)<1 || (s->words[5]>>24)>2 || (s->words[6]&~31u))){r->status=IFW_REFUSED;goto done;}
  }
  if(!io->unmap(io->context)){r->status=IFW_RETAINED;r->retained=1;return false;}mapped=false;
  if(!tick(r,io,s) || !io->guard(io->context) || !io->endpoint(io->context,&fresh) || !same(e,&fresh)){if(r->status!=IFW_CLOCK)r->status=IFW_CHANGED;goto done;}
  if(!tick(r,io,s) || s->end_us==s->begin_us){r->status=IFW_CLOCK;goto done;}
  if(n)for(unsigned i=0;i<INTEL_FW_WORDS;i++)if(s->words[i]!=r->sample[0].words[i]){r->status=IFW_CHANGED;goto done;}
 }
 r->status=IFW_OBSERVED;
done:
 if(mapped && !io->unmap(io->context)){r->status=IFW_RETAINED;r->retained=1;}
 return r->status==IFW_OBSERVED;
}
