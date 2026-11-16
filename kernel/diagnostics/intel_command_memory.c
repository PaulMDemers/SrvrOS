#include <srvros/intel_command_memory.h>
static bool io_same(const struct intel_command_io *a,const struct intel_command_io *b) {
 return a->ram.ctx==b->ram.ctx && a->ram.allocate==b->ram.allocate && a->ram.validate==b->ram.validate && a->ram.release==b->ram.release &&
 a->map==b->map && a->unmap==b->unmap && a->observe==b->observe && a->initialize==b->initialize && a->preserved==b->preserved &&
 a->guard==b->guard && a->retired==b->retired && a->now==b->now;
}
static bool lease_same(const struct intel_command_lease *a,const struct intel_command_lease *b) {
 return a->owner==b->owner && a->provider==b->provider && a->lifetime==b->lifetime && a->root==b->root &&
 a->epoch==b->epoch && a->sequence==b->sequence && a->prepare==b->prepare && a->gt==b->gt && a->hda==b->hda && a->outer==b->outer;
}
static bool identity(const struct intel_command_memory *r) {
 return r && r->object==r && !r->reason && !r->retained && io_same(&r->io,&r->original_io) &&
 r->root && !(r->root&4095) && r->virtual_base && !(r->virtual_base&4095) && r->virtual_base<=UINT64_MAX-INTEL_COMMAND_BYTES &&
 r->memory.object==&r->memory && r->memory.owner==INTEL_COMMAND_OWNER &&
 r->token.object==&r->memory && r->token.owner==INTEL_COMMAND_OWNER && r->token.epoch==r->memory.epoch &&
 r->physical==r->memory.physical && r->physical && !(r->physical&4095) && r->physical<=0x100000000ull-INTEL_COMMAND_BYTES &&
 r->memory.layout.width==1024 && r->memory.layout.height==4 && r->memory.layout.pitch==4096 &&
 r->memory.layout.pages==4 && r->memory.layout.rows==4 && r->memory.layout.bytes==INTEL_COMMAND_BYTES &&
 !r->memory.layout.tiled && r->memory.layout.visible_bytes==INTEL_COMMAND_BYTES &&
 r->memory.allocator.ctx==r->io.ram.ctx && r->memory.allocator.allocate==r->io.ram.allocate &&
 r->memory.allocator.validate==r->io.ram.validate && r->memory.allocator.release==r->io.ram.release;
}
static bool hold_live(const struct intel_command_memory *r) {
 return identity(r) && scanout_memory_hold_live(&r->hold) && r->hold.lifetime==r &&
 r->hold.memory.object==&r->memory && r->hold.physical==r->physical && r->hold.bytes==INTEL_COMMAND_BYTES && r->hold.serial==r->serial &&
 (r->registered?(r->hold.dependent==r && r->hold.registered==r):(!r->hold.dependent && !r->hold.registered));
}
static bool fail(struct intel_command_memory *r,unsigned reason) {
 r->reason|=reason;r->state=COMMAND_FAULT;r->retained=true;r->complete=false;r->busy=false;return false;
}
struct callback {
 struct intel_command_lease lease;
 struct intel_command_io io;
 const void *object;
 uint64_t root,va,pa,serial,last;
 uint32_t state,maps,retired,observations,guards,initializations,checks;
 bool registered;
 uint64_t prepare_begin,prepare_end,begin,end,close_begin,close_end;
 bool complete;
};
static struct callback capture(const struct intel_command_memory *r) {
 return (struct callback){r->lease,r->io,r->object,r->root,r->virtual_base,r->physical,r->serial,r->last,
 r->state,r->maps,r->retired,r->observations,r->guards,r->initializations,r->checks,r->registered,
 r->prepare_begin,r->prepare_end,r->begin,r->end,r->close_begin,r->close_end,r->complete};
}
static bool unchanged(const struct intel_command_memory *r,const struct callback *s) {
 return identity(r) && r->object==s->object && r->busy && lease_same(&r->lease,&s->lease) && io_same(&r->io,&s->io) &&
 r->root==s->root && r->virtual_base==s->va && r->physical==s->pa && r->serial==s->serial && r->last==s->last &&
 r->state==s->state && r->maps==s->maps && r->retired==s->retired && r->observations==s->observations &&
 r->guards==s->guards && r->initializations==s->initializations && r->checks==s->checks && r->registered==s->registered &&
 r->prepare_begin==s->prepare_begin && r->prepare_end==s->prepare_end && r->begin==s->begin && r->end==s->end &&
 r->close_begin==s->close_begin && r->close_end==s->close_end && r->complete==s->complete;
}
static bool tick(struct intel_command_memory *r) {
 if(!identity(r) || !r->busy)return fail(r,COMMAND_SOURCE);
 struct callback s=capture(r);uint64_t at=r->io.now(r->io.ram.ctx);
 if(!unchanged(r,&s))return fail(r,COMMAND_SOURCE);
 unsigned why=!at?1:at<r->last?2:0;
 if(!why && r->state==COMMAND_HELD){
  const struct intel_command_lease *l=&r->lease;
  why=at-l->prepare>INTEL_DIAGNOSTIC_PREPARE_US?3:at-l->gt>300000?4:at-l->hda>400000?5:at-l->outer>500000?6:0;
 } else if(!why && r->prepare_begin && r->state==COMMAND_PREPARING && at-r->prepare_begin>100000)why=7;
 else if(!why && r->close_begin && r->state==COMMAND_RETURNED && at-r->close_begin>100000)why=8;
 if(why){r->clock_at=at;r->clock_previous=r->last;r->clock_reason=why;return fail(r,COMMAND_CLOCK);}
 r->last=at;return true;
}
static bool observe(struct intel_command_memory *r) {
 if(!tick(r))return false;
 struct callback s=capture(r);struct intel_command_alias a={0};
 bool ok=r->io.observe(r->io.ram.ctx,r->root,r->virtual_base,r->physical,&a);
 if(!unchanged(r,&s))return fail(r,COMMAND_SOURCE);
 if(!tick(r))return false;
 if(!ok || a.pages!=4 || a.leaves!=4 || a.index || a.writable!=1 || a.user || a.nx!=1 || a.sizes!=1 ||
 a.mtrr!=6 || a.pat!=6 || a.effective!=6)return fail(r,COMMAND_MAPPING);
 r->alias=a;++r->observations;return true;
}
bool intel_command_memory_prepare(struct intel_command_memory *r,uint64_t root,uint64_t va,struct intel_command_io io) {
 if(!r || r->object || !root || (root&4095) || !va || (va&4095) || va>UINT64_MAX-INTEL_COMMAND_BYTES ||
 !io.ram.ctx || !io.ram.allocate || !io.ram.validate || !io.ram.release || !io.map || !io.unmap || !io.observe ||
 !io.initialize || !io.preserved || !io.guard || !io.retired || !io.now)return false;
 unsigned char *bytes=(unsigned char *)r;for(unsigned i=0;i<sizeof(*r);i++)bytes[i]=0;
 r->object=r;r->root=root;r->virtual_base=va;r->io=r->original_io=io;r->state=COMMAND_PREPARING;
 struct scanout_memory_layout l;
 if(!scanout_memory_layout(1024,4,4096,false,&l) ||
 !scanout_memory_create(&r->memory,&r->pool,INTEL_COMMAND_OWNER,&l,0x100000000ull,io.ram,&r->token))return fail(r,COMMAND_BACKING);
 if(r->object!=r || r->root!=root || r->virtual_base!=va || !io_same(&r->io,&io) || !io_same(&r->original_io,&io) ||
 r->state!=COMMAND_PREPARING || r->busy || r->complete || r->retained || r->reason || r->maps || r->retired ||
 r->observations || r->guards || r->initializations || r->checks || r->registered || r->physical || r->serial ||
 r->prepare_begin || r->prepare_end || r->begin || r->end || r->close_begin || r->close_end || r->last ||
 r->clock_at || r->clock_previous || r->clock_reason)return fail(r,COMMAND_SOURCE);
 r->physical=r->memory.physical;r->busy=true;
 if(!tick(r))return false;
 r->prepare_begin=r->last;
 for(unsigned i=0;i<4;i++){
  struct callback s=capture(r);bool ok=io.map(io.ram.ctx,root,va+4096*i,r->physical+4096*i);
  if(!unchanged(r,&s))return fail(r,COMMAND_SOURCE);
  if(ok)++r->maps;
  if(!tick(r))return false;
  if(!ok)return fail(r,COMMAND_MAPPING);
 }
 struct scanout_memory_cpu cpu;
 if(!scanout_memory_cpu_begin(&r->memory,&r->token,&cpu))return fail(r,COMMAND_BACKING);
 struct callback s=capture(r);bool ok=io.initialize(io.ram.ctx,va,INTEL_COMMAND_BYTES);
 if(!unchanged(r,&s))return fail(r,COMMAND_SOURCE);
 if(!ok)return fail(r,COMMAND_CONTENT);
 ++r->initializations;
 if(!tick(r) || !observe(r))return false;
 if(!scanout_memory_cpu_end(&r->memory,&cpu) || !scanout_memory_hold_begin(&r->memory,&r->token,&r->hold))return fail(r,COMMAND_BACKING);
 r->hold.lifetime=r;r->serial=r->hold.serial;
 if(!hold_live(r) || !tick(r))return fail(r,COMMAND_BACKING);
 r->prepare_end=r->last;if(r->prepare_end<=r->prepare_begin)return fail(r,COMMAND_CLOCK);
 r->state=COMMAND_PREPARED;r->busy=false;return true;
}
bool intel_command_memory_run(struct intel_command_memory *r,const struct intel_command_lease *lease) {
 if(!r || r->state!=COMMAND_PREPARED || r->busy || r->registered || r->complete || !hold_live(r) || !lease ||
 !lease->owner || !lease->provider || !lease->lifetime || lease->root!=r->root || !lease->epoch || lease->sequence!=1 ||
 !lease->outer || lease->gt<lease->outer || lease->hda<lease->outer || lease->prepare<lease->gt || lease->prepare<lease->hda ||
 lease->prepare<r->prepare_end)return false;
 r->lease=*lease;r->state=COMMAND_HELD;r->busy=true;r->registered=true;
 r->hold.dependent=r;r->hold.registered=r;r->last=lease->prepare;
 if(!tick(r))return false;
 r->begin=r->last;
 for(unsigned i=0;i<2;i++){
  if(!hold_live(r) || !tick(r))return fail(r,COMMAND_SOURCE);
  struct callback s=capture(r);bool ok=r->io.guard(r->io.ram.ctx,&r->lease);
  if(!unchanged(r,&s) || !hold_live(r))return fail(r,COMMAND_SOURCE);
  if(!tick(r))return false;
  if(!ok)return fail(r,COMMAND_PARENT);
  ++r->guards;
  if(!observe(r) || !hold_live(r))return fail(r,COMMAND_MAPPING);
 }
 if(!tick(r) || !hold_live(r))return fail(r,COMMAND_SOURCE);
 if(r->last<=r->begin){r->clock_at=r->last;r->clock_previous=r->last;r->clock_reason=9;return fail(r,COMMAND_CLOCK);}
 if(r->guards!=2 || r->observations!=3 || r->maps!=4 || r->retired || r->initializations!=1)return fail(r,COMMAND_SOURCE);
 r->end=r->last;r->complete=true;r->registered=false;r->hold.dependent=NULL;r->hold.registered=NULL;
 r->state=COMMAND_RETURNED;r->busy=false;return true;
}
bool intel_command_memory_returned(const struct intel_command_memory *r) {
 return r && r->state==COMMAND_RETURNED && !r->busy && !r->registered && r->complete && hold_live(r) &&
 r->alias.pages==4 && r->alias.leaves==4 && !r->alias.index && r->alias.writable==1 && !r->alias.user &&
 r->alias.nx==1 && r->alias.sizes==1 && r->alias.mtrr==6 && r->alias.pat==6 && r->alias.effective==6 &&
 r->maps==4 && !r->retired && r->observations==3 && r->guards==2 && r->initializations==1 && !r->checks &&
 r->begin>=r->lease.prepare && r->end>r->begin && r->end==r->last && r->end-r->lease.prepare<=INTEL_DIAGNOSTIC_PREPARE_US &&
 r->end-r->lease.gt<=300000 && r->end-r->lease.hda<=400000 && r->end-r->lease.outer<=500000;
}
bool intel_command_memory_close(struct intel_command_memory *r) {
 if(!intel_command_memory_returned(r))return false;
 r->busy=true;
 struct callback s=capture(r);bool ok=r->io.retired(r->io.ram.ctx,&r->lease);
 if(!unchanged(r,&s) || !hold_live(r))return fail(r,COMMAND_SOURCE);
 if(!ok)return fail(r,COMMAND_PARENT);
 if(!tick(r))return false;
 r->close_begin=r->last;
 if(!observe(r))return false;
 r->hold.lifetime=NULL;
 if(!scanout_memory_hold_end(&r->hold))return fail(r,COMMAND_BACKING);
 struct scanout_memory_cpu cpu;
 if(!scanout_memory_cpu_begin(&r->memory,&r->token,&cpu))return fail(r,COMMAND_BACKING);
 s=capture(r);ok=r->io.preserved(r->io.ram.ctx,r->virtual_base,INTEL_COMMAND_BYTES);
 if(!unchanged(r,&s))return fail(r,COMMAND_SOURCE);
 if(!ok)return fail(r,COMMAND_CONTENT);
 ++r->checks;
 if(!scanout_memory_cpu_end(&r->memory,&cpu))return fail(r,COMMAND_BACKING);
 for(unsigned i=0;i<4;i++){
  s=capture(r);uint64_t physical=0;
  ok=r->io.unmap(r->io.ram.ctx,r->root,r->virtual_base+4096*i,&physical);
  if(!unchanged(r,&s))return fail(r,COMMAND_SOURCE);
  if(!ok || physical!=r->physical+4096*i)return fail(r,COMMAND_RETIRE);
  ++r->retired;if(!tick(r))return false;
 }
 if(!scanout_memory_close(&r->memory,&r->token))return fail(r,COMMAND_BACKING);
 if(!tick(r))return false;
 r->close_end=r->last;
 if(r->close_end<=r->close_begin || r->memory.release_attempts!=1 || r->memory.releases!=1)return fail(r,COMMAND_RETIRE);
 r->state=COMMAND_CLOSED;r->busy=false;return true;
}
