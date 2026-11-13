#define main memory_base_main
#include "test_scanout_memory.c"
#undef main
static struct scanout_memory_hold held,held_copy,other;
static unsigned drift;
static bool drift_validate(void *ctx,uint64_t physical,uint32_t pages) {
 bool ok=validate(ctx,physical,pages);
 if(drift==1)a.physical+=4096;
 if(drift==2)a.layout.bytes--;
 if(drift==3)held.lifetime=&other;
 if(drift==4)held.dependent=held.registered=&other;
 if(drift==5)a.publication.object=&a.publication;
 return ok;
}
static void start(void) {
 setup(true);create();memset(&held,0,sizeof held);memset(&other,0,sizeof other);
 assert(scanout_memory_hold_begin(&a,&token,&held));assert(scanout_memory_hold_live(&held));
}
int main(void) {
 start();unsigned checked=model.valid;struct scanout_memory_cpu cpu;
 held.lifetime=&model;assert(!scanout_memory_hold_end(&held));held.lifetime=0;
 assert(!scanout_memory_cpu_begin(&a,&token,&cpu));assert(!scanout_memory_hold_begin(&a,&token,&other));
 arena();assert(!scanout_memory_prepare(&a,&token,&space,&publications,1,binding()));
 assert(!model.encode && model.valid==checked && !model.release);
 held_copy=held;assert(!scanout_memory_hold_live(&held_copy) && !scanout_memory_hold_end(&held_copy));
 held.dependent=held.registered=&other;
 assert(!scanout_memory_hold_end(&held) && scanout_memory_hold_live(&held));
 assert(!scanout_memory_close(&a,&token) && !model.release && a.closing);
 assert(!scanout_memory_hold_begin(&a,&token,&other));
 held.dependent=held.registered=0;assert(scanout_memory_hold_end(&held));
 assert(!scanout_memory_hold_end(&held) && !scanout_memory_hold_live(&held));
 assert(!scanout_memory_cpu_begin(&a,&token,&cpu)); // pending close remains closed to admission
 assert(scanout_memory_close(&a,&token) && model.release==1);
 assert(!scanout_memory_close(&a,&token) && model.release==1);
 // Returned stale/copy holds cannot act on a fresh allocation or re-register.
 struct scanout_memory_token fresh;assert(scanout_memory_create(&a,&pool,7,&layout,0x100000000ull,allocator(),&fresh));
 assert(!scanout_memory_hold_begin(&a,&fresh,&held));assert(!scanout_memory_hold_end(&held_copy));
 assert(scanout_memory_hold_begin(&a,&fresh,&other));assert(scanout_memory_hold_end(&other));assert(scanout_memory_close(&a,&fresh));
 for(unsigned n=0;n<17;n++) {
  start();
  switch(n) {
   case 0:held.memory.epoch++;break;case 1:held.memory.owner++;break;
   case 2:held.physical+=4096;break;case 3:held.bytes-=4096;break;
   case 4:held.serial++;break;case 5:a.physical+=4096;break;
   case 6:a.layout.pages--;break;case 7:a.layout.rows--;break;
   case 8:a.allocator.ctx=&held;break;case 9:held.allocator.release=0;break;
   case 10:a.registered=&held_copy;break;case 11:a.hold_serial++;break;
   case 12:a.cpu_serial=1;break;case 13:a.range.object=&space;break;
   case 14:a.release_attempts=1;break;case 15:a.publication.exposed=true;break;
   case 16:a.layout.visible_bytes--;break;
  }
  assert(!scanout_memory_hold_live(&held));assert(!scanout_memory_hold_end(&held));
  assert(held.fault && held.active);assert(!scanout_memory_close(&a,&token) && !model.release);
 }
 start();model.validate_fail=true;assert(!scanout_memory_hold_end(&held));
 assert(a.state==MEM_FAULT && held.active && held.fault && !model.release);
 model.validate_fail=false;assert(!scanout_memory_hold_end(&held));assert(!scanout_memory_close(&a,&token));
 start();model.reenter=true;assert(scanout_memory_hold_end(&held));assert(scanout_memory_close(&a,&token));
 setup(false);create();assert(scanout_memory_cpu_begin(&a,&token,&cpu));memset(&held,0,sizeof held);
 assert(!scanout_memory_hold_begin(&a,&token,&held));assert(scanout_memory_cpu_end(&a,&cpu));assert(scanout_memory_close(&a,&token));
 for(unsigned n=1;n<=5;n++) {
  setup(true);create();memset(&held,0,sizeof held);a.allocator.validate=drift_validate;drift=0;
  if(n<=2 || n==5) {
   drift=n;assert(!scanout_memory_hold_begin(&a,&token,&held));assert(a.state==MEM_FAULT && !held.object);
  } else {
   assert(scanout_memory_hold_begin(&a,&token,&held));drift=n;
   assert(!scanout_memory_hold_end(&held) && held.fault && held.active);
  }
  assert(!scanout_memory_close(&a,&token) && !model.release);
 }
 puts("scanout-memory-hold: PASS original registration, close drain, copy/stale/refusal, reentry, backing loss and no premature RAM release");
}
