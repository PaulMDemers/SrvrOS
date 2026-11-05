#include <srvros/mt76_handoff5.h>
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static bool page(uint64_t p){return p && !(p&4095) && p<=UINT64_MAX-4096;}
bool mt76_handoff5_plan_valid(const struct mt76_handoff5_plan *p){
 if(!p || !p->generation || p->generation==UINT64_MAX || !p->slot || !page(p->old_ring) || !page(p->new_ring) ||
 p->old_ring==p->new_ring || !p->old_posts || p->old_posts>8 || p->new_slots!=63 || p->protected_count>MT76_HANDOFF5_PROTECTED ||
 (p->old_dequeue&15)!=1 || (p->old_dequeue&~15ull)<p->old_ring || (p->old_dequeue&~15ull)>p->old_ring+16*p->old_posts)return false;
 for(unsigned i=0;i<p->protected_count;i++){
  uint64_t a=p->protected_pages[i];if(!page(a) || a==p->old_ring || a==p->new_ring)return false;
  for(unsigned j=0;j<i;j++)if(a==p->protected_pages[j])return false;
 }
 for(unsigned i=p->protected_count;i<MT76_HANDOFF5_PROTECTED;i++)if(p->protected_pages[i])return false;
 return true;
}
static uint64_t dequeue(const uint32_t *w){return w[2]|((uint64_t)w[3]<<32);}
static bool initial(const uint32_t *w,const struct mt76_handoff5_plan *p){
 return (w[0]&7)==3 && ((w[1]>>3)&7)==6 && (w[1]>>16)==512 && dequeue(w)==p->old_dequeue;
}
static bool fresh(const uint32_t *w,const uint32_t *old,const struct mt76_handoff5_plan *p,bool start){
 for(unsigned i=0;i<8;i++)if(i!=2 && i!=3 && w[i]!=old[i])return false;
 uint64_t d=dequeue(w),ptr=d&~15ull;
 return (d&15)==1 && ptr>=p->new_ring && ptr<=p->new_ring+(start?0:16*p->new_slots);
}
static bool read_words(struct mt76_handoff5 *s,const struct mt76_handoff5_io *io,uint32_t *w,bool cleanup){
 if(!io->owned(io->context,cleanup))return false;
 s->reads++;return io->read_context(io->context,w) && io->owned(io->context,cleanup);
}
static bool command(struct mt76_handoff5 *s,const struct mt76_handoff5_io *io,uint64_t d,bool cleanup){
 if(!io->owned(io->context,cleanup))return false;
 s->commands++;return io->set_dequeue(io->context,d) && io->owned(io->context,cleanup);
}
bool mt76_handoff5_run(struct mt76_handoff5 *s,const struct mt76_handoff5_plan *input,const struct mt76_handoff5_io *transport){
 if(!s || !input || !transport || overlap(s,sizeof *s,input,sizeof *input) || overlap(s,sizeof *s,transport,sizeof *transport) ||
 (transport->context && overlap(s,sizeof *s,transport->context,1)) || s->attempted || !mt76_handoff5_plan_valid(input) ||
 !transport->owned || !transport->read_context || !transport->set_dequeue || !transport->inspect)return false;
 const struct mt76_handoff5_plan p=*input;const struct mt76_handoff5_io io=*transport;
 *s=(struct mt76_handoff5){.attempted=true,.generation=p.generation};
 uint32_t old[8]={0},now[8]={0};bool ok=false;
 if(!read_words(s,&io,old,false) || !initial(old,&p))goto done;
 if(!io.owned(io.context,false))goto done;
 s->retarget_attempted=true;
 if(!command(s,&io,p.new_ring|1,false))goto done;
 s->retargeted=true;
 if(!read_words(s,&io,now,false) || !fresh(now,old,&p,true))goto done;
 if(!io.owned(io.context,false))goto done;
 s->inspected=io.inspect(io.context);
 /* Only an independently observed stopped fresh endpoint permits restoring
  * the saved old dequeue. Pending/running/unknown state never gets a write. */
 if(!read_words(s,&io,now,true) || !fresh(now,old,&p,false))goto done;
 if(!io.owned(io.context,true))goto done;
 s->restore_attempted=true;
 if(!command(s,&io,p.old_dequeue,true))goto done;
 if(!read_words(s,&io,now,true))goto done;
 for(unsigned i=0;i<8;i++)if(now[i]!=old[i])goto done;
 s->restored=true;ok=s->inspected && io.owned(io.context,false);
done:
 s->unsafe=s->retarget_attempted && !s->restored;
 s->complete=ok;s->failed=!ok;return ok;
}
