/* A copied exec request may construct a private child without entering the
 * namespace service only when its complete source is immutable RAM. All other
 * sources and descriptor actions retain the ordinary syscall gate. */
bool process_try_spawn_private(const char *path,uint64_t argc,const char *const *argv,
    uint64_t envc,const char *const *envp,int64_t *result) {
#if !defined(SRVROS_PROCESS_HOST_TEST) || defined(SRVROS_PRIVATE_LIFECYCLE_HOST_TEST)
    uint64_t irq=process_irq_save();
    struct process *parent=process_current();
    if(!result || !parent || !parent->allocated || !parent->active || parent->killed || parent->exiting ||
       parent->mapping_count>4096 || !path || path[0]!='/' || !scheduler_io_process_atomic(parent))goto refused;
    for(unsigned i=0;i<3;i++) {
        struct process_file *f=process_file_at(parent,i);
        if(f && (f->type!=PROCESS_FILE_STDIO || f->handle>2))goto refused;
    }
    char logical[PROCESS_PATH_MAX],storage[PROCESS_PATH_MAX];
    if(process_path_context_for(parent,SRV_PATH_RESOLVE,path,logical,sizeof(logical))<0 ||
       vfs_namespace_system_path(logical,storage,sizeof(storage))<0)goto refused;
    struct process_exec_view view={.node=vfs_lookup(storage)};
    if(!view.node || !(view.node->metadata.mode&0111) ||
       !exfat_readonly_memory_view(view.node,&view.data,&view.size))goto refused;
    /* This is an atomic-attempt bound, not a new executable size limit. Larger
     * images use the original loader, with its unchanged limits/checkpoints. */
    const struct elf64_header *header=NULL;
    if(!validate_elf(view.data,view.size,&header))goto refused;
    const struct elf64_program_header *programs=(const void *)(view.data+header->phoff);
    uint64_t pages=USER_STACK_PAGES+32;
    for(uint64_t i=0;i<header->phnum;i++)if(programs[i].type==PT_LOAD) {
        uint64_t n=(page_up(programs[i].vaddr+programs[i].memsz)-page_down(programs[i].vaddr))/PAGE_SIZE;
        if(n>4096-pages)goto refused;
        pages+=n;
    }
    *result=process_spawn_exec_source(0,path,argc,argv,envc,envp,true,-1,-1,-1,0,false,
        NULL,0,SRV_EXEC_BACKGROUND,0,0,&view);
    /* Kernel allocations may add shared kernel mappings. Preserve the existing
     * parent refresh, still under IF0 and without a service/yield window. */
    process_refresh_mappings(parent);
    process_irq_restore(irq);return true;
refused:
    process_irq_restore(irq);
#else
    (void)path;(void)argc;(void)argv;(void)envc;(void)envp;(void)result;
#endif
    return false;
}
