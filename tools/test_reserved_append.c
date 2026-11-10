/* Reuse established process/PTY/signal host adapters, but execute the actual
 * production reservation/attempt/mutation functions below. Backend-overlap and
 * media invariants have a separate real-exFAT fixture. */
#define SRVROS_PROCESS_SERVICE_HOST_TEST
#define main reserved_append_unused_output_main
#pragma GCC diagnostic push
/* The renamed legacy C main relies on its implicit return0; it is never run. */
#pragma GCC diagnostic ignored "-Wreturn-type"
#include "test_process_output.c"
#pragma GCC diagnostic pop
#undef main
static bool reserved_entry_ok=true,reserved_actual_ok=true;
static unsigned reserved_proofs;
bool scheduler_io_entry_process_independent(void *p) {
    assert(!teardown_irq);return reserved_entry_ok && p==current_subject && !current_worker;
}
bool scheduler_io_process_independent(void *p) {
    assert(!teardown_irq);reserved_proofs++;return reserved_actual_ok && p==current_subject && !current_worker;
}
static bool reserved_gate_admit=true;
static unsigned reserved_gate_depth;
bool scheduler_io_begin(void) {assert(!teardown_irq);if(!reserved_gate_admit)return false;reserved_gate_depth++;return true;}
void scheduler_io_end(void) {assert(!teardown_irq && reserved_gate_depth);reserved_gate_depth--;}
void scheduler_io_checkpoint(void) {assert(!"private copy must not service or yield");}
bool scheduler_io_active(void) {return reserved_gate_depth!=0;}
struct reserved_copy {
    const uint8_t *source;
    uint64_t mapped,partial,requested,count;
    unsigned calls;
    bool fail_after_partial;
};
static bool reserved_copy(void *opaque,void *destination,uint64_t count,uint64_t requested) {
    struct reserved_copy *c=opaque;assert(!teardown_irq && count<=requested && requested<=65536);
    c->calls++;c->requested=requested;c->count=count;
    if(!c->source || c->mapped<requested)return false;
    if(c->fail_after_partial) {assert(c->partial<count);memcpy(destination,c->source,c->partial);return false;}
    memcpy(destination,c->source,(size_t)count);return true;
}
static uint8_t payload[65536];
static uint64_t fresh(unsigned limit) {
    for(unsigned i=0;i<PROCESS_MAX_WRITE_FILES+PROCESS_MAX_READ_FILES;i++)
        if(file_contents[i].owned)free((void *)file_contents[i].data);
    memset(file_contents,0,sizeof(file_contents));memset(write_files,0,sizeof(write_files));
    memset(read_files,0,sizeof(read_files));memset(&subject,0,sizeof(subject));
    subject.allocated=subject.active=true;subject.image_generation=17;subject.pid=400;
    subject.fsize_limit.current=UINT64_MAX;subject.signal_catch_mask=1ull<<SRV_SIGNAL_XFSZ;
    for(unsigned i=0;i<3;i++)subject.files[i]=(struct process_file){.used=true,.type=PROCESS_FILE_STDIO,.handle=i};
    current_subject=&subject;current_worker=NULL;opening=true;fixture_exists=false;
    fail_alloc=fail_save=clean_refusal=false;saved_size=0;saves=0;teardown_irq=true;
    reserved_entry_ok=reserved_actual_ok=true;reserved_gate_admit=true;reserved_gate_depth=0;
    int64_t fd=process_file_open_write_raw(&subject,"/test/output",SRV_OPEN_WRITE|SRV_OPEN_CREATE|SRV_OPEN_EXCL);
    assert(fd==3 && write_files[0].fresh_exclusive && !write_files[0].content->node);
    struct process_file_content before=*write_files[0].content;
    uint64_t cursor=write_files[0].offset,media=saves;
    assert(process_file_reserve_append_ram(&subject,3,limit)==0);
    assert(!write_files[0].content->size && !write_files[0].content->dirty && write_files[0].offset==cursor);
    assert(write_files[0].content->dirty_generation==before.dirty_generation && saves==media);
    assert(write_files[0].append.valid && write_files[0].content->capacity>=limit && !reserved_gate_depth && teardown_irq);
    return 3;
}
static void refused(uint64_t fd,uint64_t amount) {
    struct process_file_content *content=write_files[0].content;
    struct process_file_content before=*content;
    struct process_write_file description=write_files[0];
    struct process_file descriptors[PROCESS_MAX_OPEN_FILES];memcpy(descriptors,subject.files,sizeof(descriptors));
    uint8_t *bytes=malloc(content->capacity);assert(bytes);memcpy(bytes,content->data,content->capacity);
    struct reserved_copy copy={.source=payload,.mapped=sizeof(payload)};int64_t result=12345;
    assert(!process_file_try_reserved_append(&subject,fd,amount,reserved_copy,&copy,&result));
    assert(result==12345 && !copy.calls && !memcmp(&before,content,sizeof(before)) &&
        !memcmp(&description,&write_files[0],sizeof(description)) &&
        !memcmp(descriptors,subject.files,sizeof(descriptors)) && !memcmp(bytes,content->data,content->capacity));
    free(bytes);
}
static void guards(void) {
    fresh(1024*1024);struct process_write_file original=write_files[0];
    struct process_file_content *c=write_files[0].content,content=*c;
#define REFUSE_CHANGE(statement) do {statement;refused(3,64);write_files[0]=original;*c=content;} while(0)
    refused(3,0);refused(3,65537);refused(PROCESS_MAX_OPEN_FILES,64);
    REFUSE_CHANGE(write_files[0].refs=2);
    REFUSE_CHANGE(c->refs=2);
    REFUSE_CHANGE(c->owned=false);
    REFUSE_CHANGE(c->failed=true);
    REFUSE_CHANGE(c->unlinked=true);
    REFUSE_CHANGE(c->dirty_generation=UINT64_MAX);
    REFUSE_CHANGE(c->dirty_generation=0);
    REFUSE_CHANGE(c->committed_size=1);
    REFUSE_CHANGE(write_files[0].append.valid=false);
    REFUSE_CHANGE(write_files[0].append.fd=4);
    REFUSE_CHANGE(write_files[0].append.fd_generation=1);
    REFUSE_CHANGE(write_files[0].append.image_generation=16);
    REFUSE_CHANGE(write_files[0].append.owner=NULL);
    REFUSE_CHANGE(write_files[0].append.content=NULL);
    REFUSE_CHANGE(write_files[0].append.data=NULL);
    REFUSE_CHANGE(write_files[0].append.capacity--);
    REFUSE_CHANGE(write_files[0].append.limit=63);
    REFUSE_CHANGE(write_files[0].open_flags|=SRV_OPEN_APPEND);
    REFUSE_CHANGE(write_files[0].offset=1);
    REFUSE_CHANGE(c->writeback.active=true;c->writeback.data=c->data;c->writeback.size=1);
    reserved_actual_ok=false;refused(3,64);reserved_actual_ok=true;
    reserved_entry_ok=false;assert(!process_file_reserved_append_entry_candidate(&subject,3,64));reserved_entry_ok=true;
    subject.image_generation++;refused(3,64);subject.image_generation--;
    subject.file_generations[3]=UINT64_MAX;refused(3,64);subject.file_generations[3]=0;
    subject.active=false;refused(3,64);subject.active=true;
    subject.allocated=false;refused(3,64);subject.allocated=true;
    struct process_user_thread worker={0};current_worker=&worker;refused(3,64);current_worker=NULL;
    assert(process_file_reserved_append_entry_candidate(&subject,3,64));
    int64_t result=0;struct reserved_copy copy={.source=payload,.mapped=sizeof(payload)};
    unsigned media=saves;fail_alloc=true;
    assert(process_file_try_reserved_append(&subject,3,65536,reserved_copy,&copy,&result) && result==65536);
    assert(saves==media && c->size==65536 && write_files[0].offset==65536 && c->dirty);
    fail_alloc=false;
#undef REFUSE_CHANGE
}
static void window_and_original_tail(void) {
    fresh(1024*1024);struct process_file_content *c=write_files[0].content;
    struct reserved_copy copy={.source=payload,.mapped=sizeof(payload)};int64_t result;
    for(unsigned n=0;n<4;n++)assert(process_file_try_reserved_append(&subject,3,65536,reserved_copy,&copy,&result) && result==65536);
    refused(3,1);assert(!process_file_reserved_append_entry_candidate(&subject,3,65536));
    assert(process_file_write(&subject,3,payload,65536)==65536 && write_files[0].append.valid);
    assert(c->size==327680 && !c->committed_size);
    c->writeback=(__typeof__(c->writeback)){.active=true,.data=c->data,.size=c->size,.generation=c->dirty_generation};
    for(unsigned n=0;n<4;n++)assert(process_file_try_reserved_append(&subject,3,65536,reserved_copy,&copy,&result));
    assert(c->writeback.append_calls==4 && c->writeback.append_bytes==262144 && c->size==589824);
    refused(3,1);c->writeback.active=false;
    /* Quiet original buffering is unchanged; this is not a global flush cap. */
    assert(process_file_write(&subject,3,payload,65536)==65536 && write_files[0].append.valid);
    c->committed_size=c->size;
    assert(process_file_try_reserved_append(&subject,3,1,reserved_copy,&copy,&result));
}
static void hidden_fault_and_rlimit(void) {
    fresh(4096);struct process_file_content *c=write_files[0].content;
    struct reserved_copy copy={.source=payload,.mapped=sizeof(payload),.partial=7,.fail_after_partial=true};
    int64_t result=0;uint64_t generation=c->dirty_generation;
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result) && result==SRV_ERR_FAULT);
    assert(!c->size && !c->dirty && !write_files[0].offset && c->dirty_generation==generation);
    assert(!memcmp(c->data,payload,7)); /* Hidden private tail, never public prefix. */
    assert(process_file_truncate(&subject,3,64)==0 && c->size==64);
    for(unsigned n=0;n<64;n++)assert(c->data[n]==0);
    fresh(4096);c=write_files[0].content;
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result) && result==SRV_ERR_FAULT);
    assert(process_file_pwrite(&subject,3,(const uint8_t *)"X",1,63)==1);
    for(unsigned n=0;n<63;n++)assert(c->data[n]==0);
    assert(c->data[63]=='X');
    fresh(4096);c=write_files[0].content;
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result) && result==SRV_ERR_FAULT);
    assert(process_file_write(&subject,3,(const uint8_t *)"OK",2)==2 && !memcmp(c->data,"OK",2));
    assert(process_file_truncate(&subject,3,64)==0);for(unsigned n=2;n<64;n++)assert(c->data[n]==0);
    fresh(4096);c=write_files[0].content;subject.fsize_limit.current=2;
    copy=(struct reserved_copy){.source=payload,.mapped=2};generation=c->dirty_generation;
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result) && result==SRV_ERR_FAULT);
    assert(copy.requested==64 && copy.count==2 && !c->size && c->dirty_generation==generation);
    copy.mapped=64;assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result) && result==2);
    uint64_t pending=subject.main_pending_signals;refused(3,1);assert(subject.main_pending_signals==pending);
    assert(process_file_write(&subject,3,payload,1)==SRV_ERR_FBIG);
    assert(subject.main_pending_signals&(1ull<<SRV_SIGNAL_XFSZ));
}
static void revocation_and_saturation(void) {
    fresh(4096);assert(process_file_seek(&subject,3,0,0)==0 && !write_files[0].append.valid);refused(3,1);
    fresh(4096);assert(!process_file_set_flags(&subject,3,SRV_FD_APPEND) && !write_files[0].append.valid);refused(3,1);
    fresh(4096);assert(!process_file_set_fd_flags(&subject,3,SRV_FD_CLOEXEC) && !write_files[0].append.valid);refused(3,1);
    fresh(4096);assert(process_file_dup_min(&subject,3,4,false)==4 && !write_files[0].append.valid);refused(3,1);
    fresh(4096);struct process_file_content *c=write_files[0].content;
    assert(content_acquire(NULL,"/test/output",NULL,0)==c && c->refs==2 && !write_files[0].append.valid);refused(3,1);
    fresh(4096);c=write_files[0].content;c->dirty_generation=UINT64_MAX-1;
    struct reserved_copy copy={.source=payload,.mapped=sizeof(payload)};int64_t result;
    assert(process_file_try_reserved_append(&subject,3,1,reserved_copy,&copy,&result) && c->dirty_generation==UINT64_MAX);
    assert(!write_files[0].append.valid);refused(3,1);
    assert(content_flush(c)==0 && !c->dirty && c->committed_size==1);
    assert(process_file_write(&subject,3,payload,1)==1 && c->dirty_generation==UINT64_MAX);
    assert(content_flush(c)==0 && !c->dirty && c->committed_size==2);
    fresh(4096);c=write_files[0].content;
    struct process_file_content unchanged=*c;struct process_write_file descriptor=write_files[0];
    reserved_gate_admit=false;assert(process_file_reserve_append_ram(&subject,3,65536)==SRV_ERR_IO);
    assert(!memcmp(&unchanged,c,sizeof(unchanged)) && !memcmp(&descriptor,&write_files[0],sizeof(descriptor)));
    reserved_gate_admit=true;
    uint64_t capacity=c->capacity;
    write_files[0].append.valid=false;fail_alloc=true;
    assert(process_file_reserve_append_ram(&subject,3,65536)==SRV_ERR_NOMEM);
    assert(!c->size && !c->dirty && c->capacity==capacity && !write_files[0].offset);fail_alloc=false;
}
static void failure_and_borrow_guards(void) {
    fresh(4096);struct process_file_content *c=write_files[0].content;
    struct reserved_copy copy={.source=payload,.mapped=sizeof(payload)};int64_t result;
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result));
    uint64_t generation=c->dirty_generation;
    fail_save=clean_refusal=true;
    assert(content_flush(c)==SRV_ERR_IO && c->dirty && !c->failed && !c->writeback.active && !c->committed_size);
    assert(c->dirty_generation==generation && !memcmp(c->data,payload,64));
    fail_save=clean_refusal=false;assert(content_flush(c)==0 && !c->dirty && c->committed_size==64);
    c->writeback=(__typeof__(c->writeback)){.active=true,.data=c->data,.size=64,.generation=c->dirty_generation};
    const uint8_t *data=c->data;uint64_t capacity=c->capacity;
    assert(!ensure_write_capacity(&write_files[0],capacity+1) && c->data==data && c->capacity==capacity);
    assert(content_release(c)==SRV_ERR_BUSY && c->refs==1);
    assert(process_file_write(&subject,3,payload,1)==SRV_ERR_BUSY);
    struct process_file descriptor=subject.files[3];
    assert(process_file_close(&subject,3)==SRV_ERR_BUSY && !memcmp(&descriptor,&subject.files[3],sizeof(descriptor)));
    copy.fail_after_partial=true;copy.partial=5;
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result) && result==SRV_ERR_FAULT);
    assert(c->size==64 && c->dirty_generation==generation && !memcmp(c->data,payload,64) && !c->writeback.append_calls);
    c->writeback.active=false;copy.fail_after_partial=false;
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result));
    fail_save=true;clean_refusal=false;
    assert(content_flush(c)==SRV_ERR_IO && c->dirty && c->failed && c->committed_size==64 && c->size==128);
    refused(3,1);assert(!memcmp(c->data,payload,64) && !memcmp(c->data+64,payload,64));
    c->refs++;assert(write_file_close(1)==SRV_ERR_IO && c->refs==1 && c->dirty && c->failed && c->size==128);
    assert(content_release(c)==SRV_ERR_IO && !c->refs);
    fail_save=false;
}
static void traced_copy_paths(void) {
    fresh(4096);capture_writes=(struct capture_write_snapshot){0};
    strcpy(subject.executable_path,"/fat/bin/shotworker");assert(process_capture_write_timing_enable());
    struct reserved_copy copy={.source=payload,.mapped=sizeof(payload)};int64_t result;
    uint64_t start=process_capture_write_begin();
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result) && result==64);
    process_capture_write_fast_result(64,result);process_capture_write_result(start,64,result);
    struct capture_write_record before=capture_writes.records[0];
    assert(before.calls==1 && before.fast_calls==1 && before.site[CAP_WRITE_RESERVE].count==1 && before.site[CAP_WRITE_COPY].count==1 && !before.gate.total.calls);
    write_files[0].offset=0;refused(3,1);
    assert(!memcmp(&before,&capture_writes.records[0],sizeof(before)));
    write_files[0].offset=64;copy.fail_after_partial=true;copy.partial=3;
    start=process_capture_write_begin();
    assert(process_file_try_reserved_append(&subject,3,64,reserved_copy,&copy,&result) && result==SRV_ERR_FAULT);
    process_capture_write_fast_result(64,result);process_capture_write_result(start,64,result);
    const struct capture_write_record *record=&capture_writes.records[0];
    assert(record->calls==2 && record->fast_calls==2 && record->fast_failures==1 && record->failures==1 &&
        record->site[CAP_WRITE_RESERVE].count==2 && record->site[CAP_WRITE_COPY].count==2 && !record->invalid);
    capture_writes=(struct capture_write_snapshot){0};
}
int main(void) {
    for(unsigned n=0;n<sizeof(payload);n++)payload[n]=(uint8_t)(n*37+11);
    guards();window_and_original_tail();hidden_fault_and_rlimit();revocation_and_saturation();failure_and_borrow_guards();traced_copy_paths();
    puts("reserved-append: PASS actual process reservation/private copy, refusal identity, windows, original tail retention, hidden-fault zeroing, RLIMIT, revocation and saturated ordinary fallback");
    return 0;
}
