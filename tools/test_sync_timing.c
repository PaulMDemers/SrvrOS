#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#ifdef SRVROS_WORKSPACE_SYNC_CONTENT_WIRE
#include "test_workspace_sync_content_wire.h"
#endif
static uint64_t irq=0x200,now,timer_reads;
static uint64_t sync_test_irq_save(void){uint64_t f=irq;irq=0;return f;}
static void sync_test_irq_restore(uint64_t f){irq=f;}
#define SRVROS_SYNC_TIMING_HOST_TEST
#include "../kernel/src/sync_timing.c"
static bool reference=true,busy;
static unsigned starts,ends;
uint64_t timer_monotonic_us(void){timer_reads++;return now;}
bool timer_reference_ready(void){return reference;}
bool usb_storage_timing_aux_begin(void){starts++;if(busy)return false;busy=true;return true;}
void usb_storage_timing_aux_end(struct usb_storage_timing *s){assert(busy);busy=false;ends++;*s=(struct usb_storage_timing){.valid=true};s->calls[1]=1;s->us[1]=4;}
/* Synthetic USB observer seam only. Real transport command behavior is covered
 * by test_usb_storage; this fixture tests retention and ownership boundaries. */
static bool content_refuse;
static unsigned usb_fixture_role=6,usb_fixture_depth;
static struct usb_storage_content_timing usb_fixture_content;
bool usb_storage_content_begin(void) {
 if(!busy || usb_fixture_content.active || content_refuse)return false;
 usb_fixture_content=(struct usb_storage_content_timing){.valid=1,.active=1};
 usb_fixture_role=6;usb_fixture_depth=0;return true;
}
void usb_storage_content_end(struct usb_storage_content_timing *out) {
 if(!usb_fixture_content.active){*out=(struct usb_storage_content_timing){0};return;}
 if(usb_fixture_depth){usb_fixture_content.valid=0;usb_fixture_content.invalid=1;}
 usb_fixture_content.active=0;*out=usb_fixture_content;
}
unsigned usb_storage_content_role_begin(unsigned role) {
 if(!usb_fixture_content.active)return USB_STORAGE_CONTENT_NO_ROLE;
 if(role>=USB_STORAGE_CONTENT_ROLES || usb_fixture_depth>=255){usb_fixture_content.valid=0;usb_fixture_content.invalid=1;return USB_STORAGE_CONTENT_NO_ROLE;}
 unsigned previous=(usb_fixture_depth<<8)|usb_fixture_role;
 usb_fixture_depth++;usb_fixture_role=role;return previous;
}
void usb_storage_content_role_end(unsigned previous) {
 if(previous==USB_STORAGE_CONTENT_NO_ROLE)return;
 if(!usb_fixture_content.active || !usb_fixture_depth || previous>>8!=usb_fixture_depth-1 || (previous&255)>=USB_STORAGE_CONTENT_ROLES){usb_fixture_content.valid=0;usb_fixture_content.invalid=1;return;}
 usb_fixture_depth--;usb_fixture_role=previous&255;
}
static void synthetic_command(unsigned role,uint64_t opcode,uint64_t bytes,uint64_t start,uint64_t end,bool ok) {
 assert(usb_fixture_content.active && role<USB_STORAGE_CONTENT_ROLES && end>=start);
 struct usb_storage_content_role *r=&usb_fixture_content.roles[role];
 if(opcode==0x28){r->read_commands++;r->read_bytes+=bytes;}
 else if(opcode==0x2a){r->write_commands++;r->write_bytes+=bytes;}
 else if(opcode==0x35)r->flush_commands++;
 else r->other_commands++;
 if(!ok)r->failed_commands++;
 r->csw_calls++;r->csw_us+=end-start;
 r->worst=(struct usb_storage_csw_timing){.start_us=start,.end_us=end,.opcode=opcode,
  .command_bytes=bytes,.request_bytes=13,.actual=13,.ok=ok,
  .sample={.submitted=start,.observed=end,.resumed=end,.seen=true,.service_valid=true,
   .service_checks=1,.service_samples=1,.service_gap_max_us=end-start,.completion_gap_us=end-start}};
}
static void call(uint64_t pid,uint64_t start,uint64_t mid,uint64_t end,bool file_ok,bool device_ok){
 now=start;assert(sync_timing_begin(pid));now=mid;sync_timing_files_done(file_ok);now=end;sync_timing_end(device_ok);
}
/* Exercise the actual C snapshot API; no process/backend success is inferred
 * from these observer-only invariants. Failed old committed extents can be
 * larger than a newly truncated captured RAM prefix. */
static bool snapshot_observation(const struct sync_content_prefix *prefix,bool ok,bool dirty) {
 observed=(struct sync_timing_snapshot){.enabled=1};now=100;assert(sync_timing_begin(7));
 now=101;assert(sync_timing_content_begin(0,10,42,65536));
 sync_timing_content_io(0,2,0,2,true);
 now=130;sync_timing_content_end_snapshot(ok,dirty,prefix);
 now=140;sync_timing_files_done(ok);now=150;sync_timing_end(true);
 assert(irq==0x200 && !observed.active && !content_active && !commands_active && !busy);
 return !observed.invalid && !observed.records[0].invalid;
}
static void snapshot_tests(void) {
 struct sync_content_prefix p={.valid=1,.start_generation=11,.end_generation=12,
  .live_size=65573,.committed_size=65536,.append_calls=1,.append_bytes=37};
 assert(snapshot_observation(&p,true,true));
 struct sync_content_call retained=observed.records[0].worst.content.worst;
 assert(retained.size==65536 && retained.prefix.valid && retained.prefix.live_size==65573 && retained.dirty_after);
 assert(retained.prefix.start_generation==11 && retained.prefix.end_generation==12);
 /* Emit every content row from the actual retained producer state. Only
  * the parent USB sync envelope uses the existing Python fixture adapter. */
 now=1000;uint64_t snapshot_us=timer_monotonic_us();
#ifdef SRVROS_WORKSPACE_SYNC_CONTENT_WIRE
 struct sync_timing_snapshot wire;sync_timing_snapshot(&wire);
 workspace_sync_content_wire(snapshot_us,&wire);
#else
 const struct sync_timing_record *record=&observed.records[0];
 const struct sync_content_summary *summary=&record->worst.content;
#define U(value) ((unsigned long long)(value))
 printf("sync-content-summary: version=1 snapshot_us=%llu pid=%llu id=%llu calls=%llu bytes=%llu failures=%llu final=1\n",
  U(snapshot_us),U(record->pid),U(record->worst.start_us),U(summary->calls),U(summary->bytes),U(summary->failures));
 printf("sync-content-selection: version=1 snapshot_us=%llu pid=%llu id=%llu final=1\n",
  U(snapshot_us),U(record->pid),U(record->worst.start_us));
 printf("sync-content-worst: version=%llu snapshot_us=%llu pid=%llu id=%llu slot=%llu identity=%llu inode=%llu size=%llu start_us=%llu end_us=%llu ok=%llu dirty_after=%llu final=1\n",
  U(retained.prefix.valid?2:1),U(snapshot_us),U(record->pid),U(record->worst.start_us),
  U(retained.slot),U(retained.identity),U(retained.inode),U(retained.size),U(retained.start_us),U(retained.end_us),
  U(retained.ok),U(retained.dirty_after));
 printf("sync-content-prefix: version=1 snapshot_us=%llu pid=%llu id=%llu identity=%llu start_generation=%llu end_generation=%llu live_size=%llu committed_size=%llu append_calls=%llu append_bytes=%llu final=1\n",
  U(snapshot_us),U(record->pid),U(record->worst.start_us),U(retained.identity),U(retained.prefix.start_generation),
  U(retained.prefix.end_generation),U(retained.prefix.live_size),U(retained.prefix.committed_size),
  U(retained.prefix.append_calls),U(retained.prefix.append_bytes));
 for(unsigned role=0;role<SYNC_CONTENT_ROLES;role++) {
  const struct sync_content_io *io=&retained.io[role];
  printf("sync-content-io: version=1 snapshot_us=%llu pid=%llu id=%llu role=%u reads=%llu read_bytes=%llu read_us=%llu writes=%llu write_bytes=%llu write_us=%llu flushes=%llu flush_us=%llu failures=%llu final=1\n",
   U(snapshot_us),U(record->pid),U(record->worst.start_us),role,U(io->reads),U(io->read_bytes),U(io->read_us),
   U(io->writes),U(io->write_bytes),U(io->write_us),U(io->flushes),U(io->flush_us),U(io->failures));
 }
#undef U
#endif
 p=(struct sync_content_prefix){.valid=1,.start_generation=12,.end_generation=12,.live_size=65536,.committed_size=65536};
 assert(snapshot_observation(&p,true,false));
 assert(!observed.records[0].worst.content.worst.dirty_after);
 struct sync_content_prefix bad[13];for(unsigned i=0;i<13;i++)bad[i]=p;
 bad[0].valid=0;bad[1].valid=2;bad[2].end_generation=11;
 bad[3].live_size=65535;bad[4].live_size=16u*1024u*1024u+1;
 bad[5].committed_size=65535;bad[6].committed_size=65537;
 bad[7].append_calls=1;bad[8].append_bytes=1;
 bad[9].end_generation=13;bad[9].append_calls=1; /* Missing appended byte. */
 bad[10].end_generation=13;bad[10].append_calls=1;bad[10].append_bytes=65537;bad[10].live_size=131073;
 bad[11].end_generation=14;bad[11].append_calls=1;bad[11].append_bytes=1;bad[11].live_size=65537;
 bad[12].committed_size=16u*1024u*1024u+1;
 for(unsigned i=0;i<13;i++)assert(!snapshot_observation(&bad[i],true,false));
 assert(!snapshot_observation(0,true,false));
 assert(!snapshot_observation(&p,true,true));
 p=(struct sync_content_prefix){.valid=1,.start_generation=UINT64_MAX-1,.end_generation=UINT64_MAX,
  .live_size=131072,.committed_size=65536,.append_calls=1,.append_bytes=65536};
 assert(snapshot_observation(&p,true,true));
 p.append_bytes=65537;p.live_size=131073;assert(!snapshot_observation(&p,true,true));
 p=(struct sync_content_prefix){.valid=1,.start_generation=0,.end_generation=UINT64_MAX,
  .live_size=65537,.committed_size=65536,.append_calls=UINT64_MAX,.append_bytes=1};
 assert(snapshot_observation(&p,true,true)); /* No append_calls*65536 overflow. */
 p=(struct sync_content_prefix){.valid=1,.start_generation=12,.end_generation=12,
  .live_size=65536,.committed_size=131072};
 assert(snapshot_observation(&p,false,true));
 assert(observed.records[0].worst.content.failures==1 && observed.records[0].errors==1);
 assert(observed.records[0].worst.content.worst.prefix.committed_size==131072);
 assert(!snapshot_observation(&p,false,false));
 /* The old API remains a v1 observation with no accidental stale companion. */
 observed=(struct sync_timing_snapshot){.enabled=1};now=200;assert(sync_timing_begin(7));
 now=201;assert(sync_timing_content_begin(0,11,42,1));now=202;sync_timing_content_end(true,false);
 now=203;sync_timing_files_done(true);now=204;sync_timing_end(true);
 assert(!observed.records[0].worst.content.worst.prefix.valid);
 assert(retained.prefix.live_size==65573 && retained.size==65536);
}
/* Observer-only: no extra transfers/clock reads and no historical maximum may
 * stand in for a currently enclosing parent/content operation. */
static void gate_tests(void) {
 observed=(struct sync_timing_snapshot){0};content_active=commands_active=tracing=busy=false;
 usb_fixture_content=(struct usb_storage_content_timing){0};slot=phase=0;
 struct sync_gate_context q;memset(&q,0xa5,sizeof(q));
 sync_timing_gate_snapshot(7,120,160,&q);assert(!q.state && !q.identity);
 observed.enabled=1;now=100;assert(sync_timing_begin(7));
 sync_timing_gate_snapshot(8,120,160,&q);assert(q.state==1 && !q.pid && !q.id);
 sync_timing_gate_snapshot(7,120,160,&q);assert(q.state==2 && q.pid==7 && q.id==100 && !q.identity);
 now=110;assert(sync_timing_content_begin(2,444,42,65536));
 sync_timing_content_io(3,0,4096,8,true);sync_timing_content_io(2,1,1024,12,true);
 now=160;uint64_t before_clock=timer_reads;
 unsigned before_starts=starts,before_ends=ends;
 struct sync_timing_call parent=current;struct sync_content_call content=content_current;
 sync_timing_gate_snapshot(7,120,160,&q);
 assert(q.state==3 && !q.invalid && q.pid==7 && q.id==100 && q.identity==444 && q.slot==2 && q.size==65536 && q.content_start_us==110);
 assert(q.io[3].reads==1 && q.io[3].read_bytes==4096 && q.io[2].write_us==12 && irq==0x200);
 assert(timer_reads==before_clock && starts==before_starts && ends==before_ends && !memcmp(&parent,&current,sizeof(parent)) && !memcmp(&content,&content_current,sizeof(content)));
#ifdef SRVROS_CAPTURE_GATE_SYNC_WIRE
 extern void workspace_gate_fixture(const struct sync_gate_context *context);
 workspace_gate_fixture(&q);
#endif
 struct sync_gate_context retained=q;
 sync_timing_content_io(2,1,512,1,true);assert(retained.io[2].write_bytes==1024);
 sync_timing_gate_snapshot(7,105,160,&q);assert(q.state==2 && !q.identity && !q.io[2].writes);
 sync_timing_gate_snapshot(7,99,160,&q);assert(q.state==1 && !q.id);
 sync_timing_gate_snapshot(7,170,160,&q);assert(q.invalid);
 now=170;sync_timing_content_end(true,false);
 sync_timing_gate_snapshot(7,120,180,&q);assert(q.state==2 && !q.identity); /* Not the completed maximum. */
 now=180;sync_timing_files_done(true);
 sync_timing_gate_snapshot(7,185,190,&q);assert(q.state==2 && q.phase==1 && !q.identity);
 now=200;sync_timing_end(true);sync_timing_gate_snapshot(7,120,160,&q);assert(q.state==1 && !q.id);
 now=300;assert(sync_timing_begin(7));sync_timing_gate_snapshot(7,120,160,&q);assert(q.state==1 && !q.id);
 observed.records[slot].invalid=1;sync_timing_gate_snapshot(7,310,320,&q);assert(q.state==2 && q.invalid);
 now=330;sync_timing_files_done(true);now=340;sync_timing_end(true);
 sync_timing_gate_snapshot(7,310,320,NULL);assert(!observed.active && irq==0x200);
 puts("sync-gate: PASS exact live parent/content binding, no previous-maximum substitution, immutable copied counters, disabled/missing/phase/clock/failure refusal and unchanged observer state");
}
int main(void){
 assert(sync_timing_content_role_begin(2)==USB_STORAGE_CONTENT_NO_ROLE);sync_timing_content_role_end(USB_STORAGE_CONTENT_NO_ROLE);
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
 unsigned outer=sync_timing_content_role_begin(3);assert(usb_fixture_role==3);
 unsigned inner=sync_timing_content_role_begin(4);assert(usb_fixture_role==4);
 sync_timing_content_role_end(inner);assert(usb_fixture_role==3);
 sync_timing_content_role_end(outer);assert(usb_fixture_role==6 && !usb_fixture_depth);
 synthetic_command(3,0x28,16384,8004,8009,true);
 sync_timing_content_io(3,0,16384,5,true);sync_timing_content_io(4,1,512,3,true);
 now=8020;sync_timing_content_end(true,false);
 now=8021;assert(sync_timing_content_begin(4,b,23,7));
 synthetic_command(4,0x2a,512,8023,8025,true);
 sync_timing_content_io(0,2,0,6,true);now=8029;sync_timing_content_end(true,false);
 now=8030;sync_timing_files_done(true);now=8040;sync_timing_end(true);
 struct sync_content_summary *c=&observed.records[0].worst.content;
 assert(c->calls==2 && c->bytes==65543 && !c->failures && c->worst.identity==a && c->worst.slot==3);
 assert(c->worst.io[3].read_bytes==16384 && c->worst.io[4].write_bytes==512 && !c->worst.io[0].flushes);
 assert(c->worst.commands.valid && !c->worst.commands.active && !c->worst.commands.invalid);
 assert(c->worst.commands.roles[3].read_commands==1 && c->worst.commands.roles[3].read_bytes==16384);
 assert(c->worst.commands.roles[3].csw_us==5 && c->worst.commands.roles[3].worst.start_us==8004);
 assert(!c->worst.commands.roles[4].write_commands); /* Later subcall cannot replace the copied command context. */
 call(7,8100,8110,8120,true,true);assert(c->calls==2 && c->worst.identity==a && c->worst.commands.roles[3].csw_us==5);
 now=8200;assert(sync_timing_begin(7));now=8201;assert(sync_timing_content_begin(4,b,23,7));
 synthetic_command(0,0x2a,512,8202,8204,false);
 sync_timing_content_io(0,1,1,2,false);now=8250;sync_timing_content_end(false,true);
 now=8260;sync_timing_files_done(false);now=8270;sync_timing_end(true);
 assert(c->calls==1 && c->bytes==7 && c->failures==1 && c->worst.identity==b && c->worst.io[0].failures==1 && c->worst.dirty_after);
 assert(c->worst.commands.roles[0].failed_commands==1 && !c->worst.commands.roles[3].read_commands);
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
 /* A refused nested command window invalidates this observer without
  * ending or copying another owner's window or changing content success. */
 observed=(struct sync_timing_snapshot){.enabled=1};now=9300;assert(sync_timing_begin(7));
 assert(usb_storage_content_begin());usb_fixture_content.roles[2].write_commands=99;
 assert(sync_timing_content_begin(1,a,0,1));
 now=9310;sync_timing_content_end(true,false);
 assert(usb_fixture_content.active && usb_fixture_content.roles[2].write_commands==99);
 assert(current.content.calls==1 && !current.content.failures && !current.content.worst.commands.valid);
 assert(observed.invalid || observed.records[0].invalid);
 struct usb_storage_content_timing foreign;usb_storage_content_end(&foreign);
 assert(foreign.valid && foreign.roles[2].write_commands==99);
 now=9320;sync_timing_files_done(true);now=9330;sync_timing_end(true);
 assert(!observed.records[0].errors);
 observed=(struct sync_timing_snapshot){.enabled=1};now=9400;assert(sync_timing_begin(7));content_refuse=true;
 assert(sync_timing_content_begin(1,a,0,1));assert(!usb_fixture_content.active);
 now=9410;sync_timing_content_end(true,false);content_refuse=false;
 assert(!current.content.worst.commands.valid && (observed.invalid || observed.records[0].invalid));
 now=9420;sync_timing_files_done(true);now=9430;sync_timing_end(true);assert(!observed.records[0].errors);
 observed=(struct sync_timing_snapshot){.enabled=1};now=9500;assert(sync_timing_begin(7));
 assert(sync_timing_content_begin(1,a,0,1));outer=sync_timing_content_role_begin(2);inner=sync_timing_content_role_begin(4);
 sync_timing_content_role_end(outer); /* Out-of-order: observation only is poisoned. */
 sync_timing_content_role_end(inner);sync_timing_content_role_end(outer);
 now=9510;sync_timing_content_end(true,false);
 assert(!current.content.worst.commands.valid && current.content.worst.commands.invalid);
 assert(observed.invalid || observed.records[0].invalid);
 now=9520;sync_timing_files_done(true);now=9530;sync_timing_end(true);assert(!observed.records[0].errors);
 struct usb_storage_content_timing missing={.valid=1};usb_storage_content_end(&missing);assert(!missing.valid && !missing.active);
 snapshot_tests();
 gate_tests();
 puts("sync-timing: PASS opt-in, complete phases, all failure combinations, copied worst content/command retention, PID capacity, nested trace/role refusal and foreign preservation, invalid clock/overflow and incomplete evidence");
}
