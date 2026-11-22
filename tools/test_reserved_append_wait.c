/* Actual process lease/window/borrow code; scheduler turns are controlled here.
 * The separate real-scheduler regression verifies eligibility and queue wake. */
#define SRVROS_RESERVED_APPEND_WAIT_HOST_TEST
#define RESERVED_APPEND_TEST_MAIN queued_unused_main
#include "test_reserved_append.c"
static bool foreign_busy;
static unsigned wait_turns,notifications,action;
bool scheduler_io_busy(void) {assert(!teardown_irq);return foreign_busy;}
void scheduler_io_private_append_progress(void) {assert(!teardown_irq);notifications++;}
bool scheduler_io_private_append_wait(void *p,scheduler_wait_condition_fn ready,void *arg) {
    assert(p==&subject && foreign_busy && !teardown_irq && !ready(arg));
    wait_turns++;
    struct process_file_content *c=write_files[0].content;
    if(wait_turns==1)return true; /* Spurious progress from a different content. */
    if(action==1)foreign_busy=false;
    else if(action==2)subject.killed=true;
    else if(action==3)subject.file_generations[3]++;
    else if(action==4)reserved_actual_ok=false;
    else if(action==5)c->failed=true;
    else if(action==6)write_files[0].append.valid=false;
    else c->writeback=(__typeof__(c->writeback)){.active=true,.data=c->data,.size=c->size,.generation=c->dirty_generation};
    assert(ready(arg));return true;
}
static void full_window(void) {
    fresh(1024*1024);foreign_busy=true;wait_turns=0;
    struct reserved_copy copy={.source=payload,.mapped=sizeof(payload)};int64_t result;
    for(unsigned n=0;n<4;n++)assert(process_file_try_reserved_append(&subject,3,65536,reserved_copy,&copy,&result));
    uint64_t irq=process_irq_save(),count;
    /* This is precisely the old raw-entry window test: it rejects the writer
     * before a later snapshot starts. Actual bounded copying must still reject. */
    assert(!reserved_append_count(&subject,reserved_append_binding(&subject,3),65536,true,&count));
    process_irq_restore(irq);
    assert(process_file_reserved_append_entry_candidate(&subject,3,65536));refused(3,65536);
}
int main(void) {
    for(unsigned i=0;i<sizeof(payload);i++)payload[i]=(uint8_t)(i*37+11);
    action=0;full_window();struct process_file_content *c=write_files[0].content;
    uint8_t prefix[262144];memcpy(prefix,c->data,sizeof(prefix));
    strcpy(subject.executable_path,"/fat/bin/shotworker");assert(process_capture_write_timing_enable());
    assert(process_file_wait_reserved_append(&subject,3,65536) && wait_turns==2 && foreign_busy);
    const uint8_t *data=c->data;uint64_t generation=c->writeback.generation;
    struct reserved_copy copy={.source=payload,.mapped=sizeof(payload)};int64_t result;
    fail_alloc=true;unsigned media=saves;
    for(unsigned n=0;n<4;n++)assert(process_file_try_reserved_append(&subject,3,65536,reserved_copy,&copy,&result) && result==65536);
    assert(c->size==524288 && c->writeback.size==262144 && c->writeback.append_calls==4 && c->writeback.append_bytes==262144);
    assert(c->data==data && c->dirty_generation==generation+4 && c->dirty && !memcmp(prefix,c->data,sizeof(prefix)) && media==saves);
    refused(3,1);fail_alloc=false;
    assert(capture_writes.records[0].append_wait_calls==1 && capture_writes.records[0].append_wait_wakes==2 && capture_writes.records[0].append_wait_us>0);
    capture_writes=(struct capture_write_snapshot){0};
    for(action=1;action<=6;action++) {
        full_window();c=write_files[0].content;uint64_t size=c->size,gen=c->dirty_generation;
        memcpy(prefix,c->data,sizeof(prefix));
        assert(!process_file_wait_reserved_append(&subject,3,65536) && wait_turns==2);
        assert(c->size==size && c->dirty_generation==gen && !memcmp(prefix,c->data,sizeof(prefix)));
    }
    action=0;full_window();foreign_busy=false;
    assert(!process_file_wait_reserved_append(&subject,3,65536) && !wait_turns);
    /* The production borrow publication/retirement emits both notifications. */
    fresh(4096);unsigned notices=notifications;
    assert(process_file_write(&subject,3,payload,64)==64);
    assert(content_flush(write_files[0].content)==0 && notifications==notices+2);
    puts("reserved-queued: PASS missed snapshot window, spurious wakes, unchanged256KiB bound/prefix, no allocation/media, owner-end/kill/FD/owner/failed/revoked fallback");
    return 0;
}
