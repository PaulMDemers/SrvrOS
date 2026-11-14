/* v35 value-only census accounting. Uses only timestamps/counts already
 * captured by the original backing observer; never invokes a provider/clock. */
#define ADMISSION_COST_CAP 512u
static struct {
 struct admission_cost_row {uint64_t begin,end,sample_us;uint32_t reads,ptes,status;} row[ADMISSION_COST_CAP];
 unsigned count,flags;
 uint64_t previous;
} admission_cost_capture;
static void admission_cost_reset(void){admission_cost_capture=(__typeof__(admission_cost_capture)){0};}
static void admission_cost_record(const struct intel_backing_result *r,bool ok) {
 if(!admission_active)return;
 if(admission_cost_capture.count==ADMISSION_COST_CAP){admission_cost_capture.flags|=1;return;}
 struct admission_cost_row *v=&admission_cost_capture.row[admission_cost_capture.count++];
 v->reads=r->sample[0].reads+r->sample[1].reads;v->ptes=r->sample[0].ptes+r->sample[1].ptes;
 v->status=ok && r->status==1;v->begin=r->sample[0].begin_us;v->end=r->sample[1].end_us;
 if(!v->status)return;
 if(!v->begin || r->sample[0].end_us<=v->begin || r->sample[1].begin_us<r->sample[0].end_us ||
    v->end<=r->sample[1].begin_us || v->begin<admission_cost_capture.previous || v->end-v->begin>300000) {
  admission_cost_capture.flags|=2;v->status=0;return;
 }
 v->sample_us=r->sample[0].end_us-v->begin+v->end-r->sample[1].begin_us;
 admission_cost_capture.previous=v->end;
}
struct admission_cost_sum {uint64_t count,reads,ptes,sample_us,envelope_us,max_us,failed;};
static void admission_cost_add(struct admission_cost_sum *s,const struct admission_cost_row *r) {
 ++s->count;s->reads+=r->reads;s->ptes+=r->ptes;
 if(!r->status){++s->failed;return;}
 uint64_t elapsed=r->end-r->begin;s->sample_us+=r->sample_us;s->envelope_us+=elapsed;
 if(elapsed>s->max_us)s->max_us=elapsed;
}
static void admission_cost_report(uint64_t begin,uint64_t end) {
 struct admission_cost_sum total={0},prepare={0};
 for(unsigned i=0;i<admission_cost_capture.count;i++) {
  const struct admission_cost_row *r=&admission_cost_capture.row[i];admission_cost_add(&total,r);
  if(begin && end>=begin && r->begin>=begin && r->end<=end && r->end>=r->begin)admission_cost_add(&prepare,r);
 }
 static uint8_t raw[76];static char encoded[105];unsigned pos=0,k=0;
#define COST_U64(v) do {uint64_t x=(v);for(unsigned b=0;b<8;b++)raw[pos++]=(uint8_t)(x>>(8*b));} while(0)
#define COST_U32(v) do {uint32_t x=(uint32_t)(v);for(unsigned b=0;b<4;b++)raw[pos++]=(uint8_t)(x>>(8*b));} while(0)
 COST_U64(begin);COST_U64(end);COST_U32(admission_cost_capture.flags);
 const struct admission_cost_sum *groups[2]={&total,&prepare};
 for(unsigned n=0;n<2;n++) {const struct admission_cost_sum *s=groups[n];
  COST_U32(s->count);COST_U32(s->reads);COST_U32(s->ptes);COST_U32(s->failed);
  COST_U32(s->sample_us);COST_U32(s->envelope_us);COST_U32(s->max_us);
 }
#undef COST_U64
#undef COST_U32
 static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 for(unsigned i=0;i<76;i+=3){unsigned left=76-i;uint32_t v=(uint32_t)raw[i]<<16;
  if(left>1)v|=(uint32_t)raw[i+1]<<8;
  if(left>2)v|=raw[i+2];
  encoded[k++]=abc[v>>18];encoded[k++]=abc[(v>>12)&63];encoded[k++]=left>1?abc[(v>>6)&63]:'=';encoded[k++]=left>2?abc[v&63]:'=';
 }
 encoded[k]=0;console_printf("intel-census-cost: v=1 b=%s\n",encoded);
}
