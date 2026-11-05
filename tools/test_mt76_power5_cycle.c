#include <srvros/mt76_power5_cycle.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint32_t addresses[]={0x13b0,0x1314,0x1318,0x131c,0x1320,0x1324,0x13d4,0x13d8,0x13dc};
struct fake {
 uint64_t context_identity; /* Context address is distinct from cleared state. */
 struct mt76_power5_cycle state;
 uint32_t words[9],original[9],selected[9];
 unsigned calls,owners,fail,cancel,bad_read,readbacks,forward,restore_order;
 bool uncertain,inspect_fail,unsafe,mac_on,retired,reentered,injected;
};
static struct mt76_power5_capture capture(void){return (struct mt76_power5_capture){.attempted=true,.complete=true,.reads=60,.writes=5,.polls=5,.blocks=5,.fields={.generation=1,.chip=0x7610,.nic0=0xfd11,.nic1=0x1004,.channel_pair=19u<<8,.cck=0x0201,.ofdm_lo=0x0405,.ofdm_hi=4,.ht_lo=0x0607,.ht_hi=0x0203,.vht=0x0809}};}
static unsigned index_of(uint32_t a){for(unsigned i=0;i<9;i++)if(addresses[i]==a)return i;assert(0);return 0;}
static bool owned(void *c,bool cleanup){struct fake *f=c;f->owners++;if(f->retired)return false;return !(f->cancel && f->owners>=f->cancel && !cleanup);}
static bool read_word(void *c,uint32_t a,uint32_t *v,bool cleanup){
 struct fake *f=c;(void)cleanup;if(++f->calls==f->fail)return false;
 if(a==0x1004)*v=f->mac_on?12:0;
 else if(a==0x2000)*v=0xf000f200;
 else if(a==0x208)*v=0x40000050;
 else {*v=f->words[index_of(a)];f->readbacks++;}
 if(f->calls==f->bad_read && (a==0x1004 || a==0x2000 || a==0x208 || f->state.snapshot_complete)){*v^=1;f->injected=true;}
 return true;
}
static bool write_word(void *c,uint32_t a,uint32_t v,bool cleanup){
 struct fake *f=c;unsigned i=index_of(a);assert(!f->mac_on);bool failure=++f->calls==f->fail;
 if(!cleanup){assert(f->state.snapshot_complete && f->state.saved_mask==511);assert(i==f->forward++);assert(v==f->selected[i]);assert(f->state.modified_mask&(1u<<i));}
 else {assert(i<f->restore_order);f->restore_order=i;assert(v==f->original[i]);}
 if(!failure || f->uncertain)f->words[i]=v;
 return !failure;
}
static bool inspect(void *);
static struct mt76_power5_cycle_io io_for(struct fake *f){return (struct mt76_power5_cycle_io){f,owned,read_word,write_word,inspect};}
static bool inspect(void *c){
 struct fake *f=c;assert(f->state.selected && f->state.verified_mask==511 && f->forward==9);
 assert(!memcmp(f->words,f->selected,sizeof f->words));
 /* Reentry refuses before issuing I/O,even during the selected lifetime. */
 unsigned calls=f->calls;struct mt76_power5_cycle_io io=io_for(f);struct mt76_power5_capture fields=capture();
 assert(!mt76_power5_cycle_run(&f->state,&io,&fields,30) && calls==f->calls);f->reentered=true;
 if(f->unsafe)f->mac_on=true;
 return !f->inspect_fail;
}
static void setup(struct fake *f){
 *f=(struct fake){.restore_order=9};struct mt76_power5_capture fields=capture();const uint8_t us[2]={'U','S'};struct mt76_power5_plan plan={0};
 assert(mt76_power5_plan(&fields.fields,us,161,20,30,&plan)==MT76_POWER5_OK);
 for(unsigned i=0;i<9;i++){f->original[i]=f->words[i]=i?0x02020202:0x2f2f1b1b;f->selected[i]=(f->original[i]&~plan.write[i].mask)|plan.write[i].value;}
}
static bool run(struct fake *f){struct mt76_power5_capture fields=capture();struct mt76_power5_cycle_io io=io_for(f);return mt76_power5_cycle_run(&f->state,&io,&fields,30);}
int main(void){
 struct fake f;setup(&f);assert(run(&f));assert(f.calls==132 && f.state.writes==18 && f.state.reads==114 && f.state.restored_mask==511);
 assert(f.state.cleanup_complete && f.state.complete && !f.state.failed && !f.state.selected && f.reentered && !memcmp(f.words,f.original,sizeof f.words));
 unsigned calls=f.calls,owners=f.owners;
 for(unsigned n=1;n<=calls;n++)for(unsigned uncertain=0;uncertain<2;uncertain++){
  setup(&f);f.fail=n;f.uncertain=uncertain;assert(!run(&f));assert(f.state.failed && !f.state.complete && !f.state.selected);
  /* Forward failures restore every possibly changed word. Cleanup failure
   * preserves the failed receipt,even when a later read sees original bytes. */
  if(n<=84)assert(!memcmp(f.words,f.original,sizeof f.words));
 }
 for(unsigned n=1;n<=owners;n++){
  setup(&f);f.cancel=n;assert(!run(&f));assert(f.state.failed && !f.state.selected && !memcmp(f.words,f.original,sizeof f.words));
 }
 for(unsigned n=1;n<=calls;n++){
  setup(&f);f.bad_read=n;bool ok=run(&f);
  /* Initial arbitrary register contents are a trusted transport snapshot;
   * inject mismatches only into guards or selected/restored readbacks. */
  if(f.injected)assert(!ok && f.state.failed && !f.state.complete && !f.state.selected);
 }
 setup(&f);f.inspect_fail=true;assert(!run(&f) && f.state.cleanup_complete && f.state.restored_mask==511 && !memcmp(f.words,f.original,sizeof f.words));
 setup(&f);f.unsafe=true;assert(!run(&f) && !f.state.cleanup_complete && !f.state.restored_mask && f.state.writes==9 && !memcmp(f.words,f.selected,sizeof f.words));
 setup(&f);f.retired=true;assert(!run(&f) && !f.calls && !f.state.writes);
 setup(&f);struct mt76_power5_capture fields=capture();struct mt76_power5_cycle_io io=io_for(&f);
 fields.complete=false;assert(!mt76_power5_cycle_run(&f.state,&io,&fields,30) && !f.state.attempted && !f.calls);
 fields=capture();assert(!mt76_power5_cycle_run(&f.state,&io,&fields,31) && !f.state.attempted && !f.calls);
 fields.fields.chip=0xffff;assert(!mt76_power5_cycle_run(&f.state,&io,&fields,30) && !f.state.attempted);
 union {struct mt76_power5_cycle s;struct mt76_power5_cycle_io io;struct mt76_power5_capture fields;} alias;
 memset(&alias,0,sizeof alias);alias.io=io;fields=capture();assert(!mt76_power5_cycle_run(&alias.s,&alias.io,&fields,30));
 memset(&alias,0,sizeof alias);alias.fields=fields;assert(!mt76_power5_cycle_run(&alias.s,&io,&alias.fields,30));
 printf("power5-cycle: PASS;132-operation before/after-uncertain faults;%u ownership cancellations;readback/guard/unsafe cleanup/reentry/alias refusals;no native caller\n",owners);
 return 0;
}
