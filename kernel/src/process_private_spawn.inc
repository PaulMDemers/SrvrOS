/* Private spawn may duplicate only existing simple endpoints. IF0 spans the
 * proof, image copy, refs and publication/rollback; no global rights collector
 * or generic descriptor release is allowed in this transaction. */
static inline bool process_private_spawn_pipe(const struct process_file *f,bool writing) {
    if(!f || f->type!=(writing?PROCESS_FILE_PIPE_WRITE:PROCESS_FILE_PIPE_READ))return false;
    struct process_pipe *p=pipe_at(f->handle);
    if(!p || !p->refs || !p->read_refs || !p->write_refs || p->refs>UINT64_MAX-2 ||
       p->read_refs>UINT64_MAX-2 || p->write_refs>UINT64_MAX-2)return false;
    for(unsigned i=0;i<PROCESS_PIPE_RIGHTS_QUEUE;i++)if(p->rights[i].used)return false;
    return true;
}
static bool process_private_spawn_cleanup(struct process *child) {
    bool ok=true;
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++)if(child->files[i].used) {
        struct process_file *f=&child->files[i];
        if(f->type==PROCESS_FILE_STDIO && f->handle<=2) *f=(struct process_file){0};
        else {int64_t result;if(!process_file_try_close_pipe(child,i,&result) || result)ok=false;}
    }
    /* Unreachable after the complete IF0 proof: never run generic teardown
     * under a live foreign owner if an invariant is violated. */
    if(!ok)for(;;)PROCESS_ARCH_HALT();
    return ok;
}
static bool process_private_spawn_stdio(struct process *child,struct process *parent,int64_t in,int64_t out,int64_t err) {
    int64_t fds[3]={in,out,err};
    /* Fresh child has exactly its constructor's standard descriptors. */
    for(unsigned i=0;i<PROCESS_MAX_OPEN_FILES;i++)if(child->files[i].used &&
        (i>=3 || child->files[i].type!=PROCESS_FILE_STDIO || child->files[i].handle>2))return false;
    for(unsigned i=0;i<3;i++) {
        int64_t fd=fds[i]>=0?fds[i]:(int64_t)i;
        struct process_file *src=process_file_at(parent,(uint64_t)fd);
        child->files[i]=(struct process_file){0};
        if(src && !process_file_clone_entry(&child->files[i],src))return false;
        if(src)child->files[i].flags&=~SRV_FD_CLOEXEC;
    }
    return true;
}
/* A copied exec request may construct a private child without entering the
 * namespace service only when its complete source is immutable RAM. All other
 * sources and descriptor actions retain the ordinary syscall gate. */
static bool process_try_spawn_private_impl(const char *path,uint64_t argc,const char *const *argv,
    uint64_t envc,const char *const *envp,int64_t in,int64_t out,bool helper,int64_t *result) {
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
        int64_t selected=i==0?in:i==1?out:-1;
        struct process_file *f=process_file_at(parent,selected>=0?(uint64_t)selected:i);
        bool pipe_ok=helper && selected>=3 && process_private_spawn_pipe(f,i==1);
        if((helper && selected>=0 && !pipe_ok) || (selected>=0 && !f) || (f && !pipe_ok && (f->type!=PROCESS_FILE_STDIO || f->handle>2))) {
            detail=i|((uint64_t)(f?f->type:0)<<8)|((uint64_t)(f && f->handle>2)<<16);goto refused;
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
    *result=process_spawn_exec_source(0,path,argc,argv,envc,envp,true,in,out,-1,0,false,
        NULL,0,SRV_EXEC_BACKGROUND,0,0,&view);
    /* Kernel allocations may add shared kernel mappings. Preserve the existing
     * parent refresh, still under IF0 and without a service/yield window. */
    process_refresh_mappings(parent);
    if(!helper)scheduler_launch_trace_exec(*result>=0?LAUNCH_TRACE_HANDLED:LAUNCH_TRACE_ERROR,0,mappings,atomic,0);
    process_irq_restore(irq);return true;
refused:
    if(!helper)scheduler_launch_trace_exec(reason,0,mappings,atomic,detail);
    process_irq_restore(irq);
#else
    (void)path;(void)argc;(void)argv;(void)envc;(void)envp;(void)in;(void)out;(void)helper;(void)result;
#endif
    return false;
}

bool process_try_spawn_private(const char *path,uint64_t argc,const char *const *argv,
    uint64_t envc,const char *const *envp,int64_t *result) {
    return process_try_spawn_private_impl(path,argc,argv,envc,envp,-1,-1,false,result);
}
bool process_try_spawn_private_helper(const char *path,const char *args,int64_t in,int64_t out,int64_t *result) {
    if((args && args[0]) || in<3 || out<3)return false;
    const char *argv[]={path};
    return process_try_spawn_private_impl(path,1,argv,0,NULL,in,out,true,result);
}
