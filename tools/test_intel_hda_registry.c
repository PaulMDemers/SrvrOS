/* Registry/reentry tests use the real HDA owner and its existing idle model. */
#define main old_hda_owner_fixture_main
#include "test_intel_hda_owner.c"
#undef main
struct registry_model {
 struct model base;
 struct intel_hda_owner_result *owner;
 struct intel_hda_owner_token *token;
 struct intel_hda_owner_borrow *borrow;
 bool reentry;
 unsigned attempts;
};
static void reenter(struct registry_model *m) {
 if(!m->reentry || m->owner->state!=INTEL_HDA_OWNER_LIVE)return;
 struct intel_hda_owner_result *r=m->owner;unsigned guards=m->base.guards,stores=m->base.stores,inspects=m->base.inspects;
 struct intel_hda_owner_borrow spare={0};struct intel_hda_result out={0};
 assert(r->busy);
 assert(!intel_hda_owner_pin(r,m->token,&spare));
 assert(!intel_hda_owner_validate(r,m->borrow));
 assert(!intel_hda_owner_check(r,m->borrow,&out));
 assert(!intel_hda_owner_unpin(r,m->borrow));
 assert(!intel_hda_owner_close(r,m->token));
 assert(guards==m->base.guards && stores==m->base.stores && inspects==m->base.inspects);
 ++m->attempts;
}
static bool registry_guard(void *p){struct registry_model *m=p;reenter(m);return guard(&m->base);}
static bool registry_inspect(void *p,struct intel_hda_result *out){struct registry_model *m=p;reenter(m);return inspect(&m->base,out);}
int main(int argc,char **argv) {
 const char *mode=argc>1?argv[1]:"normal";
 struct intel_hda_owner_result r={0};struct intel_hda_owner_token t={0};struct intel_hda_owner_borrow b[INTEL_HDA_OWNER_BORROWS]={0};
 struct registry_model m={.base={.mode="normal",.t=100,.command=6},.owner=&r,.token=&t,.borrow=&b[0],.reentry=!strcmp(mode,"reentry")};
 struct intel_hda_owner_facts f={true,true,true,true,true,true,true,true};
 struct intel_hda_owner_io io={&m,registry_inspect,registry_guard,endpoint,write16,now};
 assert(!intel_hda_owner_open(&r,f,io,(void *)&r.phase[0]));assert(!r.object && !m.base.inspects && !m.base.stores);
 assert(intel_hda_owner_open(&r,f,io,&t));
 struct intel_hda_owner_result copy=r;copy.object=&copy;struct intel_hda_owner_token copied_token={&copy,copy.epoch};
 unsigned reads=m.base.inspects,guards=m.base.guards,stores=m.base.stores;
 assert(!intel_hda_owner_live(&copy,&copied_token) && !intel_hda_owner_close(&copy,&copied_token));
 assert(!intel_hda_owner_pin(&copy,&copied_token,&b[0]));assert(reads==m.base.inspects && guards==m.base.guards && stores==m.base.stores);
 assert(!intel_hda_owner_pin(&r,&t,(void *)&r.phase[1]));
 assert(intel_hda_owner_pin(&r,&t,&b[0]));assert(intel_hda_owner_borrowed(&r,&b[0]));
 struct intel_hda_owner_borrow copied=b[0];copied.object=&copied;struct intel_hda_result mid={0};
 reads=m.base.inspects;guards=m.base.guards;
 assert(!intel_hda_owner_borrowed(&r,&copied) && !intel_hda_owner_validate(&r,&copied));
 assert(!intel_hda_owner_check(&r,&copied,&mid) && !intel_hda_owner_unpin(&r,&copied));
 assert(!intel_hda_owner_check(&r,&b[0],(void *)((char *)&r.phase[1]+8)));
 assert(!intel_hda_owner_check(&r,&b[0],(void *)&b[0]));
 assert(reads==m.base.inspects && guards==m.base.guards && m.base.stores==1 && r.pins==1);
 if(!strcmp(mode,"nested")) {
  b[0].dependent=(void *)&m;b[0].dependent_borrows=1;
  assert(!intel_hda_owner_unpin(&r,&b[0]));
  b[0].dependent=0;assert(!intel_hda_owner_unpin(&r,&b[0]));
  assert(guards==m.base.guards && m.base.stores==1);
  b[0].dependent_returns=1;
 }
 bool corrupt=!strcmp(mode,"drop-list") || !strcmp(mode,"drop-count") || !strcmp(mode,"cycle");
 if(!strcmp(mode,"drop-list"))r.dependents=0;
 if(!strcmp(mode,"drop-count"))r.pins=0;
 if(!strcmp(mode,"cycle"))b[0].next=&b[0];
 if(corrupt) {
  assert(!intel_hda_owner_validate(&r,&b[0]) && !intel_hda_owner_close(&r,&t));
  assert(r.retained && r.state==INTEL_HDA_OWNER_FAULT && m.base.command==2 && m.base.stores==1);
 } else if(!strcmp(mode,"late") || !strcmp(mode,"zero") || !strcmp(mode,"backward")) {
  if(!strcmp(mode,"late"))m.base.t+=400001;
  else m.base.mode=mode;
  assert(!intel_hda_owner_validate(&r,&b[0]));
  assert(r.retained && r.state==INTEL_HDA_OWNER_FAULT && r.clock_cause && r.clock_previous && r.pins==1);
  uint64_t at=r.clock_sample,previous=r.clock_previous;unsigned cause=r.clock_cause;
  reads=m.base.inspects;guards=m.base.guards;m.base.mode="normal";
  assert(!intel_hda_owner_validate(&r,&b[0]) && !intel_hda_owner_unpin(&r,&b[0]) && !intel_hda_owner_close(&r,&t));
  assert(at==r.clock_sample && previous==r.clock_previous && cause==r.clock_cause);
  assert(reads==m.base.inspects && guards==m.base.guards && m.base.stores==1);
 } else {
  assert(intel_hda_owner_validate(&r,&b[0]));assert(m.base.inspects==reads);
  unsigned count=!strcmp(mode,"capacity")?INTEL_HDA_OWNER_BORROWS:1;
  for(unsigned i=1;i<count;i++)assert(intel_hda_owner_pin(&r,&t,&b[i]));
  struct intel_hda_owner_borrow spare={0};
  if(count==INTEL_HDA_OWNER_BORROWS)assert(!intel_hda_owner_pin(&r,&t,&spare));
  assert(!intel_hda_owner_close(&r,&t) && m.base.stores==1);
  assert(intel_hda_owner_check(&r,&b[0],&mid));
  /* Exercise unlinking both an interior/head node and later list exhaustion. */
  for(unsigned parity=0;parity<2;parity++)for(unsigned i=parity;i<count;i+=2)assert(intel_hda_owner_unpin(&r,&b[i]));
  assert(!r.pins && !r.dependents && intel_hda_owner_close(&r,&t));
  assert(r.complete && !r.retained && !r.busy && m.base.command==6 && m.base.stores==2);
  if(m.reentry)assert(m.attempts>=3);
  /* A fresh owner may start only after clean terminal retirement. */
  struct intel_hda_owner_result second={0};struct intel_hda_owner_token second_token={0};m.owner=&second;m.token=&second_token;m.reentry=false;m.base.stores=0;m.base.inspects=0;m.base.guards=0;
  assert(intel_hda_owner_open(&second,f,io,&second_token) && second.epoch>r.epoch);
  assert(intel_hda_owner_close(&second,&second_token));
 }
 printf("hda-registry: PASS mode=%s guards=%u inspects=%u stores=%u\n",mode,m.base.guards,m.base.inspects,m.base.stores);
 return 0;
}
