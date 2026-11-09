#include <assert.h>
#include <stdio.h>
#include <stdint.h>
static uint64_t irq=0x200,now;
static uint64_t sync_test_irq_save(void){uint64_t f=irq;irq=0;return f;}
static void sync_test_irq_restore(uint64_t f){irq=f;}
#define SRVROS_SYNC_TIMING_HOST_TEST
#include "../kernel/src/sync_timing.c"
static bool reference=true,busy;
static unsigned starts,ends;
uint64_t timer_monotonic_us(void){return now;}
bool timer_reference_ready(void){return reference;}
bool usb_storage_timing_aux_begin(void){starts++;if(busy)return false;busy=true;return true;}
void usb_storage_timing_aux_end(struct usb_storage_timing *s){assert(busy);busy=false;ends++;*s=(struct usb_storage_timing){.valid=true};s->calls[1]=1;s->us[1]=4;}
static void call(uint64_t pid,uint64_t start,uint64_t mid,uint64_t end,bool file_ok,bool device_ok){
 now=start;assert(sync_timing_begin(pid));now=mid;sync_timing_files_done(file_ok);now=end;sync_timing_end(device_ok);
}
int main(void){
 assert(!sync_timing_begin(1) && !starts);reference=false;assert(!sync_timing_enable());reference=true;
 assert(sync_timing_enable() && !sync_timing_enable());
 call(7,100,130,140,true,true);assert(irq==0x200 && starts==2 && ends==2 && !busy);
 struct sync_timing_snapshot s;sync_timing_snapshot(&s);assert(s.enabled && !s.active && !s.invalid);
 assert(s.records[0].pid==7 && s.records[0].calls==1 && s.records[0].total_us==40 && s.records[0].files_us==30 && s.records[0].devices_us==10);
 assert(s.records[0].worst.usb[0].calls[1]==1 && s.records[0].worst.usb[1].us[1]==4);
 call(7,200,210,220,false,true);call(7,300,310,320,true,false);call(7,400,410,420,false,false);
 assert(observed.records[0].calls==4 && observed.records[0].errors==3 && observed.records[0].worst.start_us==100);
 assert(s.records[0].calls==1); /* copied metadata survives later updates */
 for(unsigned i=1;i<SYNC_TIMING_RECORDS;i++)call(100+i,500+i*100,510+i*100,520+i*100,true,true);
 now=2000;assert(!sync_timing_begin(999) && observed.dropped==1 && !observed.active);
 observed=(struct sync_timing_snapshot){.enabled=1};now=3000;assert(sync_timing_begin(7));assert(!sync_timing_begin(8) && observed.invalid && busy);
 now=3010;sync_timing_files_done(true);now=3020;sync_timing_end(true);assert(!busy && !observed.active);
 observed=(struct sync_timing_snapshot){.enabled=1};busy=true;call(7,4000,4010,4020,true,true);
 assert(observed.records[0].conflicts==2 && observed.records[0].invalid && busy);busy=false; /* foreign trace untouched */
 observed=(struct sync_timing_snapshot){.enabled=1};call(7,5000,4999,5010,true,true);assert(observed.records[0].invalid);
 observed=(struct sync_timing_snapshot){.enabled=1};call(7,6000,6010,6020,true,true);observed.records[0].total_us=UINT64_MAX;
 call(7,6100,6110,6120,true,true);assert(observed.records[0].invalid);
 observed=(struct sync_timing_snapshot){.enabled=1};now=7000;assert(sync_timing_begin(7));sync_timing_snapshot(&s);assert(s.active && !s.records[0].calls);
 now=7010;sync_timing_files_done(true);now=7020;sync_timing_end(true);
 /* Real content observers retain the slowest subcall WITH its parent call,
  * not the globally slowest content from a different sync. */
 observed=(struct sync_timing_snapshot){.enabled=1};now=8000;assert(sync_timing_begin(7));
 uint64_t a=sync_timing_content_identity(),b=sync_timing_content_identity();assert(a && b>a);
 now=8001;assert(sync_timing_content_begin(3,a,22,65536));
 sync_timing_content_io(3,0,16384,5,true);sync_timing_content_io(4,1,512,3,true);
 now=8020;sync_timing_content_end(true,false);
 now=8021;assert(sync_timing_content_begin(4,b,23,7));
 sync_timing_content_io(0,2,0,6,true);now=8029;sync_timing_content_end(true,false);
 now=8030;sync_timing_files_done(true);now=8040;sync_timing_end(true);
 struct sync_content_summary *c=&observed.records[0].worst.content;
 assert(c->calls==2 && c->bytes==65543 && !c->failures && c->worst.identity==a && c->worst.slot==3);
 assert(c->worst.io[3].read_bytes==16384 && c->worst.io[4].write_bytes==512 && !c->worst.io[0].flushes);
 call(7,8100,8110,8120,true,true);assert(c->calls==2 && c->worst.identity==a);
 now=8200;assert(sync_timing_begin(7));now=8201;assert(sync_timing_content_begin(4,b,23,7));
 sync_timing_content_io(0,1,1,2,false);now=8250;sync_timing_content_end(false,true);
 now=8260;sync_timing_files_done(false);now=8270;sync_timing_end(true);
 assert(c->calls==1 && c->bytes==7 && c->failures==1 && c->worst.identity==b && c->worst.io[0].failures==1 && c->worst.dirty_after);
 observed=(struct sync_timing_snapshot){.enabled=1};now=9000;assert(sync_timing_begin(7));
 assert(sync_timing_content_begin(1,a,0,0));assert(!sync_timing_content_begin(2,b,0,0) && observed.invalid);
 sync_timing_content_io(9,0,0,0,true);assert(observed.records[0].invalid);
 now=8999;sync_timing_content_end(true,false);assert(observed.records[0].invalid);
 now=9010;sync_timing_files_done(true);now=9020;sync_timing_end(true);
 observed=(struct sync_timing_snapshot){.enabled=1};now=9100;assert(sync_timing_begin(7));
 assert(sync_timing_content_begin(1,a,0,1));content_current.io[0].reads=UINT64_MAX;
 sync_timing_content_io(0,0,1,1,true);assert(observed.records[0].invalid);
 now=9110;sync_timing_content_end(true,false);now=9120;sync_timing_files_done(true);now=9130;sync_timing_end(true);
 observed=(struct sync_timing_snapshot){.enabled=1};now=9200;assert(sync_timing_begin(7));
 assert(sync_timing_content_begin(1,a,0,1));sync_timing_content_fault();assert(observed.records[0].invalid);
 now=9210;sync_timing_content_end(true,false);now=9220;sync_timing_files_done(true);now=9230;sync_timing_end(true);
 puts("sync-timing: PASS opt-in, complete phases, all failure combinations, copied worst retention, PID capacity, nested trace refusal/foreign preservation, invalid clock/overflow and incomplete evidence");
}
