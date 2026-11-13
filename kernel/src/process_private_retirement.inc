#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PRIVATE_LIFECYCLE_HOST_TEST)
static bool process_private_mappings_valid(struct process *p) {
    if(!p || !p->allocated || !p->active || p->killed || p->exiting || !p->address_space ||
       p->address_space==vmm_kernel_address_space() || p->address_space!=vmm_current_address_space() ||
       p->mapping_count>4096 || (p->mapping_count && !p->mappings))return false;
    for(uint64_t i=0;i<p->mapping_count;i++) {
        const struct process_mapping *m=&p->mappings[i];uint64_t physical=0;
        if(!m->physical_address || m->virtual_address>=(1ull<<47) ||
           m->virtual_address%PAGE_SIZE || m->physical_address%PAGE_SIZE ||
           !vmm_virt_to_phys_in_space(p->address_space,m->virtual_address,&physical) || physical!=m->physical_address)return false;
    }
    return true;
}
#endif

static bool process_private_thread_retire(void) {
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PRIVATE_LIFECYCLE_HOST_TEST)
    uint64_t irq=process_irq_save();
    struct process *p=process_current();struct process_user_thread *t=process_current_user_thread();
    if(!t || !p || !t->used || !t->active || t->owner!=p ||
       !scheduler_io_process_atomic(p) || !process_private_mappings_valid(p))goto refused;
    unsigned slot=PROCESS_MAX_USER_THREADS;
    for(unsigned i=0;i<PROCESS_MAX_USER_THREADS;i++)if(t==&p->user_threads[i])slot=i;
    if(slot==PROCESS_MAX_USER_THREADS)goto refused;
    if(t->tls_base) {
        if(!p->tls_align || p->tls_align>PAGE_SIZE || (p->tls_align&(p->tls_align-1)) ||
           p->tls_memsz>PROCESS_TLS_TEMPLATE_MAX)goto refused;
        uint64_t size=page_up(align_up_u64(p->tls_memsz,p->tls_align)+USER_TLS_TCB_SIZE);
        if(t->tls_base!=USER_TLS_BASE+(slot+1)*size || t->tls_thread_pointer!=t->tls_base+align_up_u64(p->tls_memsz,p->tls_align))goto refused;
        for(uint64_t at=0;at<size;at+=PAGE_SIZE)if(process_mapping_index(p,t->tls_base+at)<0)goto refused;
        for(uint64_t at=0;at<size;at+=PAGE_SIZE)
            if(!process_unmap_tracked_page_checked(p,t->tls_base+at))goto refused;
        t->tls_base=0;t->tls_thread_pointer=0;
    } else if(t->tls_thread_pointer || p->tls_memsz)goto refused;
    /* Keep IF0 through the worker's DEAD publication. No kernel continuation
     * with cleared user context may be stranded behind the foreign owner. */
    return true;
refused:
    process_irq_restore(irq);
#endif
    return false;
}

static bool process_private_exit_admit(void) {
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PRIVATE_LIFECYCLE_HOST_TEST)
    uint64_t irq=process_irq_save();struct process *p=process_current();
    /* Only background_process_thread has the audited atomic return/reap
     * continuation. Foreground calls must restore their waiting parent. */
    if(!p || !p->detached || !p->quiet || process_current_user_thread() || !scheduler_io_process_atomic(p) ||
       !process_private_mappings_valid(p) || p->owned_user_scope ||
       file_stream_owned(p->stream_instance) || net_process_owned(p) ||
       gui_server_is_owner(p->pid) || gui_process_surfaces_owned(p->pid) ||
       console_framebuffer_owned_by(p->pid))goto refused;
    uint64_t audio_owner=0;audio_stream_snapshot(NULL,&audio_owner);
    if(audio_owner==p->pid)goto refused;
    /* Even a clean-looking VFS fd or a pipe with queued rights may flush on
     * final release. Retain all of those descriptors on the gated path. */
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++)if(p->files[i].used &&
        (p->files[i].type!=PROCESS_FILE_STDIO || p->files[i].handle>2) &&
        !process_private_closeable_pipe(&p->files[i]))goto refused;
    /* Prove the complete resource set before changing any descriptor. Closing
     * only these simple endpoints is the same audited atomic close operation;
     * VFS-backed stdio must retain finish_process_files' explicit flush even
     * while another process still owns a reference. Keep IF0 through the final
     * resource-free exit, without global rights collection or callbacks. */
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++)if(p->files[i].used &&
        (p->files[i].type==PROCESS_FILE_PIPE_READ || p->files[i].type==PROCESS_FILE_PIPE_WRITE)) {
        int64_t result;
        if(!process_file_try_close_pipe(p,i,&result) || result)for(;;)PROCESS_ARCH_HALT();
    }
    p->private_retiring=true;
    return true;
refused:
    process_irq_restore(irq);
#endif
    return false;
}
