/* Actual sync observer and production wire; USB/time are explicit host models. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "test_workspace_sync_content_wire.h"
static uint64_t now,irq=0x200,clocks;
static uint64_t sync_test_irq_save(void){uint64_t f=irq;irq=0;return f;}
static void sync_test_irq_restore(uint64_t f){irq=f;}
#define SRVROS_SYNC_TIMING_HOST_TEST
#include "../kernel/src/sync_timing.c"
static bool busy;
static struct usb_storage_content_timing usb;
uint64_t timer_monotonic_us(void){clocks++;return now;}
bool timer_reference_ready(void){return true;}
bool usb_storage_timing_aux_begin(void){assert(!busy);busy=true;return true;}
void usb_storage_timing_aux_end(struct usb_storage_timing *out){assert(busy);busy=false;*out=(struct usb_storage_timing){.valid=true};}
bool usb_storage_content_begin(void){usb=(struct usb_storage_content_timing){.valid=1,.active=1};return true;}
void usb_storage_content_end(struct usb_storage_content_timing *out){usb.active=0;*out=usb;}
static void reset(void){commit_owner_active=(struct commit_owner_timing){0};observed=(struct sync_timing_snapshot){0};current=(struct sync_timing_call){0};content_current=(struct sync_content_call){0};content_active=commands_active=tracing=busy=false;memset(commit_wait_live,0,sizeof(commit_wait_live));commit_wait_retained=(struct commit_wait_snapshot){0};now=1;}
static void enable(void){reset();assert(sync_timing_enable());}
static void content(uint64_t begin,uint64_t identity,uint64_t end) {
 now=begin;assert(sync_timing_content_begin(2,identity,42,16384));
 if(end>begin+10){
  uint64_t a=begin+2,b=end-2;
  usb.roles[2]=(struct usb_storage_content_role){.write_commands=1,.write_bytes=16384,.csw_calls=1,.csw_us=b-a,
   .worst={.start_us=a,.end_us=b,.opcode=0x2a,.command_bytes=16384,.request_bytes=13,.actual=13,.ok=1,
    .sample={.submitted=a,.observed=b,.resumed=b,.seen=true,.service_valid=true,.service_checks=2,.service_samples=1,.service_gap_max_us=b-a,.completion_gap_us=b-a,.empty_checks=1,.event_checks=1,.last_empty_sample_us=a}}};
  usb.command=(struct usb_storage_command_timing){.valid=1,.complete=1,.ok=1,.role=2,.start_us=begin,.end_us=end,
   .csw_start_us=a,.csw_end_us=b,.checkpoint_calls=4,.checkpoint_us=1,.checkpoint_first_us=begin+1,.checkpoint_last_us=end,
   .phase={{.calls=1,.requested=31,.actual=31,.us=1,.first_us=begin,.last_us=begin+1},
           {.calls=4,.requested=16384,.actual=16384,.us=1,.first_us=begin+1,.last_us=a},
           {.calls=1,.requested=13,.actual=13,.us=b-a,.first_us=a,.last_us=b}}};
  sync_timing_content_io(2,1,16384,end-begin,true);
 }
 now=end;sync_timing_content_end(true,false);
}
static void parent(uint64_t start){now=start;assert(sync_timing_begin(7));}
static void finish(uint64_t end){now=end;sync_timing_files_done(true);now=end+1;sync_timing_end(true);}
int main(void){
 assert(sizeof(struct commit_wait_snapshot)<4096);
 reset();uint64_t before=clocks;assert(!sync_commit_wait_begin(11,100,99,0,262144,65536,false));assert(clocks==before);
 struct commit_wait_snapshot s;sync_commit_wait_snapshot(&s);assert(!s.enabled&&!s.active&&!s.invalid);
 enable();parent(10);
 /* A very long unrelated content completes before wait entry. Never join it. */
 content(20,100,1000);
 now=1010;assert(sync_commit_wait_begin(11,1010,999,0,262144,65536,false));
 content(1020,101,1060);content(1065,102,1095);
 sync_commit_wait_end(11,1010,1100,true);finish(1110);
 sync_commit_wait_snapshot(&s);assert(!s.invalid&&!s.active&&s.waits==1);
 assert(s.worst.pid==11&&s.worst.calls==2&&s.worst.overlap_us==70&&s.worst.content.identity==101);
 assert(s.worst.parent_pid==7&&s.worst.parent_id==10&&s.worst.target_identity==999);
 assert(observed.records[0].worst.content.worst.identity==100); /* Different global maximum. */
 assert(s.worst.content.commands.roles[2].write_bytes==16384&&s.worst.content.commands.command.complete);
 workspace_commit_wait_wire(1300,&s);
 /* The loaded VM found a kernel owner without completed SYS_SYNC content.
  * Retain the actual owner/status, never substitute the independent maximum. */
 enable();sync_commit_owner_begin(1,55,0,0,1000,"workspace-log");
 assert(sync_commit_wait_begin(11,1010,999,0,262144,65536,false));
 struct usb_bulk_timing status={.submitted=1020,.observed=1060,.resumed=1060,
  .seen=true,.service_valid=true,.service_checks=2,.service_samples=1,
  .service_gap_max_us=40,.completion_gap_us=40,.empty_checks=1,.event_checks=1,.last_empty_sample_us=1020};
 sync_commit_wait_status(77,0x2a,16384,1020,1060,13,13,true,&status);
 sync_commit_owner_end(1,55,1000,1080);sync_commit_wait_end(11,1010,1100,false);
 sync_commit_wait_snapshot(&s);assert(!s.invalid&&s.worst.owner.thread==55&&s.worst.owner.pid==0);
 assert(s.worst.owner.released&&s.worst.owned_us==70&&s.worst.owner.csw_overlap_us==40&&s.worst.calls==0);
 workspace_commit_wait_wire(1200,&s);
 /* A live owner is clipped at wait return, with no invented release/backfill. */
 enable();sync_commit_owner_begin(1,55,7,87,5,"disk-worker");
 assert(sync_commit_wait_begin(11,10,9,0,262144,65536,false));
 status=(struct usb_bulk_timing){.submitted=8,.observed=30,.resumed=30,
  .seen=true,.service_valid=true,.service_checks=2,.service_samples=1,
  .service_gap_max_us=22,.completion_gap_us=22,.empty_checks=1,.event_checks=1,.last_empty_sample_us=8};
 sync_commit_wait_status(78,0x2a,512,8,30,13,13,true,&status);
 sync_commit_wait_end(11,10,40,true);sync_commit_wait_snapshot(&s);
 assert(!s.invalid&&!s.worst.owner.released&&!s.worst.owner.end_us&&s.worst.owner.overlap_end==40&&s.worst.owner.csw_overlap_us==20);
 workspace_commit_wait_wire(41,&s);sync_commit_owner_end(1,55,5,50);
 struct commit_wait_snapshot later;sync_commit_wait_snapshot(&later);assert(!memcmp(&later.worst,&s.worst,sizeof(s.worst)));
 /* Multiple real owner intervals are disjoint; retain their actual maximum. */
 enable();sync_commit_owner_begin(1,55,0,0,1,"workspace-log");
 assert(sync_commit_wait_begin(11,10,9,0,262144,65536,false));
 sync_commit_owner_end(1,55,1,20);sync_commit_owner_begin(2,56,7,87,25,"disk-worker");
 sync_commit_owner_end(2,56,25,40);sync_commit_wait_end(11,10,50,false);
 sync_commit_wait_snapshot(&s);assert(!s.invalid&&s.worst.owner_calls==2&&s.worst.owned_us==25&&s.worst.owner.thread==56);
 workspace_commit_wait_wire(51,&s);
 /* Start inside an active call; retain clipped overlap and full-call counters. */
 enable();parent(1);now=10;assert(sync_timing_content_begin(0,77,0,5));
 assert(sync_commit_wait_begin(11,20,9,0,262144,65536,false));assert(sync_commit_wait_begin(12,25,10,0,262144,65536,false));
 now=50;sync_timing_content_end(true,false);sync_commit_wait_end(12,25,55,false);sync_commit_wait_end(11,20,60,true);
 sync_commit_wait_snapshot(&s);assert(s.worst.pid==11&&s.worst.content.start_us==10&&s.worst.overlap_start_us==20&&s.worst.overlap_us==30&&s.waits==2);
 workspace_commit_wait_wire(61,&s);
 /* No retroactive attachment of a call completed after the waiter returned. */
 now=65;assert(sync_commit_wait_begin(13,65,88,0,262144,65536,false));now=70;assert(sync_timing_content_begin(0,78,0,5));
 sync_commit_wait_end(13,65,120,true);sync_commit_wait_snapshot(&s);
 assert(s.worst.pid==13&&s.worst.calls==0&&s.worst.open_at_end==1&&!s.worst.content.identity);
 workspace_commit_wait_wire(121,&s);
 now=130;sync_timing_content_end(true,false);sync_commit_wait_snapshot(&s);assert(!s.worst.content.identity);
 finish(140);
 /* Content ties keep the first actual overlap, independently of wait ties. */
 enable();parent(1);assert(sync_commit_wait_begin(11,10,9,0,262144,65536,false));
 content(20,101,40);content(50,102,70);sync_commit_wait_end(11,10,80,true);
 sync_commit_wait_snapshot(&s);assert(s.worst.content.identity==101&&s.worst.calls==2&&s.worst.overlap_us==40);finish(81);
 /* Failed content is retained as failed; observation never changes results. */
 enable();parent(1);assert(sync_commit_wait_begin(11,10,9,0,262144,65536,false));now=20;
 assert(sync_timing_content_begin(0,103,0,5));now=40;sync_timing_content_end(false,true);
 sync_commit_wait_end(11,10,45,false);now=50;sync_timing_files_done(false);now=60;sync_timing_end(true);
 sync_commit_wait_snapshot(&s);assert(!s.invalid&&!s.worst.content.ok&&s.worst.content.dirty_after&&observed.records[0].errors==1);
 workspace_commit_wait_wire(62,&s);
 /* Capacity, duplicate PID, malformed end and overflow invalidate only evidence. */
 enable();for(unsigned i=0;i<SYNC_TIMING_RECORDS;i++)assert(sync_commit_wait_begin(i+1,10,1,0,262144,65536,false));
 assert(!sync_commit_wait_begin(9,10,1,0,262144,65536,false));sync_commit_wait_snapshot(&s);assert(s.invalid&&s.active==8);
 enable();assert(sync_commit_wait_begin(11,10,1,0,262144,65536,false));assert(!sync_commit_wait_begin(11,11,2,0,262144,65536,false));
 sync_commit_wait_end(11,10,9,false);sync_commit_wait_snapshot(&s);assert(s.invalid&&!s.active);
 enable();parent(1);assert(sync_commit_wait_begin(11,10,1,0,262144,65536,false));commit_wait_live[0].calls=UINT64_MAX;
 content(20,42,40);sync_commit_wait_end(11,10,50,false);finish(60);sync_commit_wait_snapshot(&s);assert(s.invalid);
 enable();assert(sync_commit_wait_begin(11,10,1,0,262144,65536,false));sync_commit_wait_end(11,11,30,false);sync_commit_wait_snapshot(&s);assert(s.invalid&&s.active==1);
 /* First strict wait maximum survives PID reuse and equal-duration ties. */
 enable();assert(sync_commit_wait_begin(11,10,1,0,262144,65536,false));sync_commit_wait_end(11,10,30,false);
 assert(sync_commit_wait_begin(11,40,2,0,262144,65536,false));sync_commit_wait_end(11,40,60,true);sync_commit_wait_snapshot(&s);assert(s.worst.target_identity==1&&s.waits==2&&!s.invalid);
 assert(irq==0x200);
 puts("commit-wait: PASS exact intersection, unrelated maximum exclusion, full copied command, partial/unavailable completion, concurrent waiters, no late backfill, capacity/duplicate/clock/overflow and retirement ties");
 return 0;
}
