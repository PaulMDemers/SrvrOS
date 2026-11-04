/* Deterministic CPU0 schedule model around real process_exit and file cleanup.
 * The scheduler/device callbacks enforce the ordering, not timing or sleeps. */
static bool teardown_running, teardown_irq=true, teardown_safe, teardown_foreign;
static bool teardown_workers_killed;
static unsigned teardown_expected_clears;
static unsigned teardown_waits, teardown_clears, teardown_frames, teardown_spaces, teardown_invalidates;
static struct process *teardown_target;
static jmp_buf teardown_return;
static void teardown_writeback_check(void) {
    if(!teardown_running)return;
    assert(!teardown_irq && teardown_target->active);
    if(!teardown_foreign)assert(teardown_workers_killed);
    assert(!teardown_frames && !teardown_spaces);
}
static uint64_t process_test_irq_save(void) {
    uint64_t flags=teardown_irq?1ull<<9:0;teardown_irq=false;return flags;
}
static void process_test_irq_restore(uint64_t flags) { if(flags&(1ull<<9))teardown_irq=true; }
static uint64_t process_test_rflags(void) { return teardown_irq?1ull<<9:0; }
void scheduler_invalidate_current_user_trap(void) {
    assert(teardown_running && !teardown_irq && !teardown_foreign && !teardown_workers_killed);
    assert(teardown_target->image_generation==19);
    teardown_invalidates++;
}
void scheduler_io_set_user_safe(bool safe) {
    assert(teardown_running && !teardown_irq && !safe);teardown_safe=safe;
}
void scheduler_io_wait(void) {
    assert(teardown_running && !teardown_irq && !teardown_safe);
    teardown_waits++;
    if(teardown_foreign) {
        /* The victim's worker, descriptors, futex and mapping remain live
         * until the foreign writeback completes and releases ownership. */
        assert(!teardown_workers_killed && teardown_clears<=1 && !teardown_frames);
        assert(teardown_target->active && teardown_target->files[1].used);
        assert(teardown_target->mapping_count==1 && futexes[0].used);
        assert(write_files[0].content && write_files[0].content->dirty);
        (void)content_flush(write_files[0].content);
        assert(!teardown_workers_killed && !teardown_frames);
        teardown_foreign=false;
    }
}
void scheduler_kill_user_threads(void *p,void *except) {
    assert(teardown_running && p==teardown_target && !except);
    assert(!teardown_irq && !teardown_foreign && teardown_waits && teardown_invalidates);
    teardown_workers_killed=true;
}
void scheduler_clear_user_context(void) {
    assert(teardown_running && !teardown_irq && !teardown_foreign);
    teardown_clears++;current_subject=NULL;current_worker=NULL;
}
void scheduler_exit_current(void) { assert(teardown_irq);longjmp(teardown_return,1); }
void audio_process_cleanup(uint64_t pid) {
    assert(pid==teardown_target->pid && teardown_workers_killed && !futexes[0].used);
}
void gui_process_cleanup(uint64_t pid) {
    assert(pid==teardown_target->pid && !teardown_target->files[1].used);
}
void console_release_framebuffer_mute_owner(uint64_t pid) { assert(pid==teardown_target->pid); }
void net_process_cleanup(struct process *p) { assert(p==teardown_target); }
bool vmm_unmap_page_in_space(uint64_t root,uint64_t page,uint64_t *physical) {
    assert(root==0x1000 && page==0x5000 && teardown_workers_killed);
    assert(!teardown_irq && !teardown_foreign && !teardown_target->active && !teardown_target->image_generation);
    assert(!teardown_target->files[1].used && teardown_clears==teardown_expected_clears);
    *physical=0x9000;return true;
}
void pmm_free_frame(uint64_t frame) { assert(frame==0x9000);teardown_frames++; }
void vmm_destroy_address_space(uint64_t root) {
    assert(root==0x1000 && teardown_frames==1 && !teardown_target->mapping_count);
    teardown_spaces++;
}
void process_context_restore(struct process_context *context,uint64_t result) {
    assert(context==&teardown_target->context && result==1 && !teardown_irq);
    assert(!teardown_target->active && !teardown_target->address_space && teardown_spaces==1);
    longjmp(teardown_return,2);
}
void test_process_teardown_fault(void);
void halt_forever(void) { assert(0 && "unexpected kernel halt");abort(); }
static void teardown_case(bool survivor,unsigned failure,bool fatal) {
    setup(1);subject.pid=77;subject.active=subject.allocated=true;
    subject.address_space=0x1000;subject.mapping_count=1;subject.image_generation=19;
    subject.mappings[0]=(struct process_mapping){.virtual_address=0x5000,.physical_address=0x9000};
    subject.main_thread_exited=true;
    subject.user_threads[0]=(struct process_user_thread){.used=true,.active=true,.owner=&subject};
    futexes[0]=(struct process_futex){.used=true,.process=&subject,.address=0x5000};
    assert(process_file_write(&subject,1,(const uint8_t *)"dirty",5)==5);
    struct process parent={0};
    if(survivor)assert(process_file_clone_entry(&parent.files[3],&subject.files[3]));
    fail_save=failure!=0;clean_refusal=failure==1;
    teardown_running=true;teardown_target=&subject;
    teardown_irq=true;teardown_safe=true;teardown_foreign=true;
    teardown_waits=teardown_clears=teardown_frames=teardown_spaces=teardown_invalidates=0;teardown_workers_killed=false;
    current_subject=&subject;current_worker=&subject.user_threads[0];
    teardown_expected_clears=fatal?2:1;
    if(fatal) {
        teardown_irq=false; /* Interrupt-gate entry, before C fault dispatch. */
        if(!setjmp(teardown_return))test_process_teardown_fault();
        assert(subject.exiting && subject.active && subject.mapping_count==1);
        assert(subject.image_generation==19 && teardown_invalidates==1);
        assert(subject.user_threads[0].used && !subject.user_threads[0].active);
        assert(subject.files[1].used && !teardown_workers_killed && !teardown_frames);
    }
    /* Resume the parked initial thread after another owner takes a dirty turn.
     * The initial continuation starts with IRQs enabled, like pthread_exit. */
    write_files[0].content->dirty=true;teardown_foreign=true;
    current_subject=&subject;current_worker=NULL;teardown_irq=true;
    if(!setjmp(teardown_return))process_exit(fatal?process_current_kill_status():0);
    assert(teardown_workers_killed && teardown_spaces==1 && !subject.user_threads[0].used);
    assert(!subject.image_generation && teardown_invalidates==(fatal?2U:1U));
    assert(subject.exit_status==(fatal?128+SRV_SIGNAL_ILL:(failure?1:0)) && teardown_waits==(fatal?3U:1U));
    uint64_t status=0;assert(process_exit_status(subject.pid,&status) && status==subject.exit_status);
    teardown_running=false; /* Subsequent close belongs to the surviving parent. */
    if(survivor) {
        assert(write_files[0].used && write_files[0].refs==1);
        assert(write_files[0].content->failed==(failure==2));
        if(failure==1) { assert(write_files[0].content->dirty);fail_save=false; }
        assert(process_file_close(&parent,3)==(failure==2?SRV_ERR_IO:0));
    }
    if(!failure || (survivor && failure==1))
        assert(saved_size==5 && !memcmp(saved,"dirty",5));
    assert(!content_find("/test/output"));
    teardown_running=false;teardown_irq=true;clean_refusal=false;fail_save=false;
}
static void teardown_regression(void) {
    for(unsigned survivor=0;survivor<2;survivor++)
        for(unsigned failure=0;failure<3;failure++)
            for(unsigned fatal=0;fatal<2;fatal++)teardown_case(survivor,failure,fatal);
    puts("process-teardown: PASS 12 schedules: foreign writeback, worker fault, initial continuation, inherited descriptions, clean/sticky errors, retirement order");
}
