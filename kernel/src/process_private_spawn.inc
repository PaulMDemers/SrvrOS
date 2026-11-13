/* A copied exec request may construct a private child without entering the
 * namespace service only when its complete source is immutable RAM. All other
 * sources and descriptor actions retain the ordinary syscall gate. */
bool process_try_spawn_private(const char *path,uint64_t argc,const char *const *argv,
    uint64_t envc,const char *const *envp,int64_t *result) {
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PRIVATE_LIFECYCLE_HOST_TEST)
    uint64_t irq=process_irq_save();
    struct process *parent=process_current();
    uint64_t reason=LAUNCH_TRACE_PARENT,atomic=0,detail=0,mappings=parent?parent->mapping_count:0;
    if(!result || !parent || !parent->allocated || !parent->active || parent->killed || parent->exiting)goto refused;
    reason=LAUNCH_TRACE_MAPPINGS;if(parent->mapping_count>4096)goto refused;
    reason=LAUNCH_TRACE_PATH;if(!path || path[0]!='/')goto refused;
    reason=LAUNCH_TRACE_ATOMIC;atomic=scheduler_io_process_atomic_reason(parent);if(atomic!=LAUNCH_ATOMIC_OK)goto refused;
    reason=LAUNCH_TRACE_STDIO;
    for(unsigned i=0;i<3;i++) {
        struct process_file *f=process_file_at(parent,i);
        if(f && (f->type!=PROCESS_FILE_STDIO || f->handle>2)) {
            detail=i|((uint64_t)f->type<<8)|((uint64_t)(f->handle>2)<<16);goto refused;
        }
    }
    char logical[PROCESS_PATH_MAX],storage[PROCESS_PATH_MAX];
    reason=LAUNCH_TRACE_RESOLVE;detail=1;
    if(process_path_context_for(parent,SRV_PATH_RESOLVE,path,logical,sizeof(logical))<0)goto refused;
    detail=2;if(vfs_namespace_system_path(logical,storage,sizeof(storage))<0)goto refused;
    struct process_exec_view view={.node=vfs_lookup(storage)};
    reason=LAUNCH_TRACE_SOURCE;detail=1;if(!view.node)goto refused;
    detail=2;if(!(view.node->metadata.mode&0111))goto refused;
    detail=3;if(!exfat_readonly_memory_view(view.node,&view.data,&view.size))goto refused;
    /* Same atomic-attempt bound and loader; larger images retain fallback. */
    const struct elf64_header *header=NULL;
    reason=LAUNCH_TRACE_ELF;detail=0;if(!validate_elf(view.data,view.size,&header))goto refused;
    const struct elf64_program_header *programs=(const void *)(view.data+header->phoff);
    uint64_t pages=USER_STACK_PAGES+32;
    for(uint64_t i=0;i<header->phnum;i++)if(programs[i].type==PT_LOAD) {
        uint64_t n=(page_up(programs[i].vaddr+programs[i].memsz)-page_down(programs[i].vaddr))/PAGE_SIZE;
        reason=LAUNCH_TRACE_IMAGE_BOUND;detail=n;if(n>4096-pages)goto refused;
        pages+=n;
    }
    *result=process_spawn_exec_source(0,path,argc,argv,envc,envp,true,-1,-1,-1,0,false,
        NULL,0,SRV_EXEC_BACKGROUND,0,0,&view);
    /* Kernel allocations may add shared kernel mappings. Preserve the existing
     * parent refresh, still under IF0 and without a service/yield window. */
    process_refresh_mappings(parent);
    scheduler_launch_trace_exec(*result>=0?LAUNCH_TRACE_HANDLED:LAUNCH_TRACE_ERROR,0,mappings,atomic,0);
    process_irq_restore(irq);return true;
refused:
    scheduler_launch_trace_exec(reason,0,mappings,atomic,detail);
    process_irq_restore(irq);
#else
    (void)path;(void)argc;(void)argv;(void)envc;(void)envp;(void)result;
#endif
    return false;
}
