/* Shared typed raw backends. The public CALL admission layer must snapshot and
 * validate all relevant operands before using any of these; this switch alone
 * does not authenticate a ticket or descriptor. */
static bool signal_io_local_backend(uint64_t number,const uint64_t a[6],int64_t *result) {
    switch(number) {
    case SYS_WRITE:*result=syscall_write(a[0],(const char *)a[1],a[2]);break;
    case SYS_READ:*result=syscall_read(a[0],(char *)a[1],a[2]);break;
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    case SYS_PREAD:*result=regular_user_io(a[0],(void *)a[1],a[2],false,true,(int64_t)a[3]);break;
    case SYS_PWRITE:*result=regular_user_io(a[0],(void *)a[1],a[2],true,true,(int64_t)a[3]);break;
#endif
    case SYS_VECTOR_IO:*result=syscall_vector_io(a[0],(const struct srv_iovec *)a[1],a[2],a[3],(int64_t)a[4]);break;
    case SYS_POLL:*result=syscall_poll((struct syscall_pollfd *)a[0],a[1],(int64_t)a[2]);break;
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    case SYS_FCNTL:*result=syscall_fcntl(a[0],a[1],a[2]);break;
    case SYS_PIPE_MESSAGE:*result=syscall_pipe_message(a[0],(const struct srv_pipe_message_layout *)a[1],(struct srv_pipe_message *)a[2],a[3]);break;
#endif
    case SYS_WAIT:*result=syscall_wait(a[0],(uint64_t *)a[1],a[2]);break;
    case SYS_SLEEP_TICKS:*result=syscall_sleep_ticks(a[0]);break;
    case SYS_YIELD:*result=syscall_yield();break;
    default:return false;
    }
    return true;
}

static bool signal_io_raw_backend(uint64_t number,const uint64_t a[6],int64_t *result) {
    if(signal_io_local_backend(number,a,result))return true;
    switch(number) {
    case SYS_SOCKET_TRY_IO:*result=syscall_socket_try_io(a[0],a[1],(void *)a[2],a[3],(struct srv_socket_peer *)a[4]);break;
    case SYS_NET_ACCEPT:*result=syscall_net_accept(a[0],(char *)a[1],a[2],(uint64_t *)a[3]);break;
    case SYS_NET_CONNECT:*result=syscall_net_connect((uint32_t)a[0],(uint16_t)a[1],a[2]);break;
    case SYS_NET_UDP_SENDTO:*result=syscall_net_udp_sendto(a[0],(uint32_t)a[1],(uint16_t)a[2],(const uint8_t *)a[3],a[4]);break;
    case SYS_NET_UDP_RECVFROM:*result=syscall_net_udp_recvfrom(a[0],(uint8_t *)a[1],a[2],(uint32_t *)a[3],(uint16_t *)a[4]);break;
    case SYS_NET_PEEK:*result=syscall_net_peek(a[0],(uint8_t *)a[1],a[2]);break;
    case SYS_UNIX_CONNECT:*result=syscall_unix_connect((const char *)a[0]);break;
    case SYS_UNIX_ACCEPT:*result=syscall_unix_accept(a[0]);break;
    default:return false;
    }
    return true;
}

#ifdef SRVROS_SIGNAL_PUBLIC_GUEST_TEST
/* This is bounded per-entry storage, never retained by a waiter after return. */
struct signal_io_snapshot {
    struct srv_signal_descriptor_v1 expected[64];
    struct syscall_pollfd poll[64];
    uint64_t poll_generation[64];
    struct srv_iovec iov[SRV_IOV_MAX];
    struct signal_pipe_snapshot pipe;
    struct srv_flock lock;
    char path[MAX_PATH_LENGTH];
};
static bool signal_io_has_scalar_fd(uint64_t n) {
    switch(n) {
    case SYS_WRITE:case SYS_READ:case SYS_PREAD:case SYS_PWRITE:case SYS_VECTOR_IO:
    case SYS_FCNTL:case SYS_SOCKET_TRY_IO:case SYS_NET_ACCEPT:case SYS_NET_UDP_SENDTO:
    case SYS_NET_UDP_RECVFROM:case SYS_NET_PEEK:case SYS_UNIX_ACCEPT:case SYS_PIPE_MESSAGE:return true;
    default:return false;
    }
}
/* Exact correspondence is checked on the same snapshots that backends use. */
static bool signal_io_coverage(const struct srv_signal_io_v1 *w,struct signal_io_snapshot *snapshot) {
    uint64_t used=0,count=w->descriptor_count;const uint64_t *a=w->args;
    if(count>64)return false;
    if(signal_io_has_scalar_fd(w->raw_syscall)) {
        if(!count || snapshot->expected[used++].fd!=(int64_t)a[0])return false;
    }
    if(w->raw_syscall==SYS_POLL) {
        if(a[1]>64)return false;
        for(uint64_t i=0;i<a[1];i++) {
            if(snapshot->poll[i].fd<0) {snapshot->poll_generation[i]=UINT64_MAX;continue;}
            if(used>=count || snapshot->expected[used].fd!=snapshot->poll[i].fd)return false;
            snapshot->poll_generation[i]=snapshot->expected[used++].value;
        }
    } else if(w->raw_syscall==SYS_PIPE_MESSAGE && a[3]==SRV_PIPE_MESSAGE_SEND) {
        if(snapshot->pipe.message.count>SRV_RIGHTS_MAX)return false;
        for(uint64_t i=0;i<snapshot->pipe.message.count;i++)
            if(used>=count || snapshot->expected[used++].fd!=snapshot->pipe.message.fds[i])return false;
    }
    if(used!=count)return false;
    for(uint64_t i=0;i<count;i++)for(uint64_t j=0;j<i;j++)
        if(snapshot->expected[i].fd==snapshot->expected[j].fd && snapshot->expected[i].value!=snapshot->expected[j].value)return false;
    return true;
}
/* Only kernel-owned snapshot storage is modified on refusal. Caller has
 * already checked the sticky ticket and keeps IRQ exclusion throughout. */
static int64_t signal_io_snapshot_operands(const struct srv_signal_io_v1 *w,struct signal_io_snapshot *snapshot) {
    uint64_t count=w->descriptor_count;const uint64_t *a=w->args;
    if(count>64 || (count && !copy_from_user(snapshot->expected,(const void *)w->descriptor_vector,count*sizeof(snapshot->expected[0]))))return SRV_ERR_FAULT;
    if(w->raw_syscall==SYS_POLL) {
        if(a[1]>64)return SRV_ERR_INVAL;
        if(a[1] && (!user_buffer_ok((void *)a[0],a[1]*sizeof(snapshot->poll[0]),true) ||
           !copy_from_user(snapshot->poll,(void *)a[0],a[1]*sizeof(snapshot->poll[0]))))return SRV_ERR_FAULT;
    } else if(w->raw_syscall==SYS_VECTOR_IO) {
        if(a[2]>SRV_IOV_MAX)return SRV_ERR_INVAL;
        if(a[2] && !copy_from_user(snapshot->iov,(void *)a[1],a[2]*sizeof(snapshot->iov[0])))return SRV_ERR_FAULT;
    } else if(w->raw_syscall==SYS_UNIX_CONNECT) {
        if(!copy_user_string((const char *)a[0],snapshot->path,sizeof(snapshot->path)))return SRV_ERR_FAULT;
    } else if(w->raw_syscall==SYS_FCNTL) {
        if(!copy_from_user(&snapshot->lock,(void *)a[2],sizeof(snapshot->lock)))return SRV_ERR_FAULT;
    } else if(w->raw_syscall==SYS_PIPE_MESSAGE) {
        if(!copy_from_user(&snapshot->pipe.message,(void *)a[2],sizeof(snapshot->pipe.message)) ||
           !copy_from_user(&snapshot->pipe.layout,(void *)a[1],sizeof(snapshot->pipe.layout)))return SRV_ERR_FAULT;
        if(a[3]!=SRV_PIPE_MESSAGE_SEND && a[3]!=SRV_PIPE_MESSAGE_RECV)return SRV_ERR_INVAL;
        bool receiving=a[3]==SRV_PIPE_MESSAGE_RECV;
        uint32_t valid_flags=receiving?(SRV_PIPE_MSG_DONTWAIT|SRV_PIPE_MSG_CLOEXEC|SRV_PIPE_MSG_PEEK):
            (SRV_PIPE_MSG_DONTWAIT|SRV_PIPE_MSG_NOSIGNAL);
        if(snapshot->pipe.message.count>SRV_RIGHTS_MAX || (snapshot->pipe.message.flags&~valid_flags) ||
           snapshot->pipe.layout.iov_count>SRV_IOV_MAX ||
           (receiving && snapshot->pipe.layout.header_length>SRV_PIPE_MESSAGE_AUX_MAX))return SRV_ERR_INVAL;
        if(snapshot->pipe.layout.iov_count && !copy_from_user(snapshot->pipe.iov,snapshot->pipe.layout.iov,
            snapshot->pipe.layout.iov_count*sizeof(snapshot->pipe.iov[0])))return SRV_ERR_FAULT;
    }
    if(!signal_io_coverage(w,snapshot))return SRV_ERR_INVAL;
    if((w->raw_syscall==SYS_PREAD || w->raw_syscall==SYS_PWRITE) && (int64_t)a[3]<0)return SRV_ERR_INVAL;
    switch(w->raw_syscall) {
    case SYS_READ:case SYS_PREAD:case SYS_NET_PEEK:
        if(!user_buffer_ok((void *)a[1],a[2],true))return SRV_ERR_FAULT;
        break;
    case SYS_WRITE:case SYS_PWRITE:
        if(!user_buffer_ok((void *)a[1],a[2],false))return SRV_ERR_FAULT;
        break;
    case SYS_VECTOR_IO: {
        if(a[3]>SRV_VECTOR_PWRITE || (a[3]>=SRV_VECTOR_PREAD && (int64_t)a[4]<0))return SRV_ERR_INVAL;
        bool writing=a[3]==SRV_VECTOR_WRITE || a[3]==SRV_VECTOR_PWRITE;uint64_t total=0;
        for(uint64_t i=0;i<a[2];i++) {
            if(snapshot->iov[i].length>(uint64_t)INT64_MAX-total)return SRV_ERR_INVAL;
            total+=snapshot->iov[i].length;
            if(!user_buffer_ok(snapshot->iov[i].base,snapshot->iov[i].length,!writing))return SRV_ERR_FAULT;
        }
        break;
    }
    case SYS_SOCKET_TRY_IO: {
        bool writing=a[1]==SRV_SOCKET_SEND || a[1]==SRV_SOCKET_UDP_SEND;
        if(!user_buffer_ok((void *)a[2],a[3],!writing))return SRV_ERR_FAULT;
        if(a[1]==SRV_SOCKET_UDP_SEND) {
            struct srv_socket_peer peer;
            if(!copy_from_user(&peer,(void *)a[4],sizeof(peer)))return SRV_ERR_FAULT;
            if(peer.reserved)return SRV_ERR_INVAL;
        } else if(a[1]==SRV_SOCKET_UDP_RECV && a[4] && !user_buffer_ok((void *)a[4],sizeof(struct srv_socket_peer),true))return SRV_ERR_FAULT;
        break;
    }
    case SYS_NET_ACCEPT:
        if(!user_buffer_ok((void *)a[1],a[2],true) || (a[3] && !user_buffer_ok((void *)a[3],sizeof(uint64_t),true)))return SRV_ERR_FAULT;
        break;
    case SYS_NET_UDP_SENDTO:if(!user_buffer_ok((void *)a[3],a[4],false))return SRV_ERR_FAULT;break;
    case SYS_NET_UDP_RECVFROM:
        if(!user_buffer_ok((void *)a[1],a[2],true) || (a[3] && !user_buffer_ok((void *)a[3],sizeof(uint32_t),true)) ||
           (a[4] && !user_buffer_ok((void *)a[4],sizeof(uint16_t),true)))return SRV_ERR_FAULT;
        break;
    case SYS_WAIT:if(a[1] && !user_buffer_ok((void *)a[1],sizeof(uint64_t),true))return SRV_ERR_FAULT;break;
    default:break;
    }
    return 0;
}
static int64_t signal_io_call(struct isr_frame *frame,const struct srv_signal_io_v1 *w) {
    uint64_t entry_flags=irq_save();struct signal_io_slot root;int64_t result=process_signal_io_check(frame,w,&root);
    if(result) {irq_restore(entry_flags);return result;}
    struct signal_io_snapshot snapshot={0};uint64_t count=w->descriptor_count;const uint64_t *a=w->args;
    result=signal_io_snapshot_operands(w,&snapshot);
    if(result) {irq_restore(entry_flags);return result;}
    uint64_t cookie=0,sequence=0;
    result=process_signal_io_reserve(frame,w,snapshot.expected,count,&cookie,&sequence);
    if(result) {irq_restore(entry_flags);return result;}
    struct signal_sleep_elapsed elapsed={0};bool sleep=w->raw_syscall==SYS_SLEEP_TICKS;
    if(w->raw_syscall==SYS_POLL)
        result=syscall_poll_prepared((struct syscall_pollfd *)a[0],a[1],(int64_t)a[2],snapshot.poll,snapshot.poll_generation);
    else if(w->raw_syscall==SYS_VECTOR_IO)
        result=syscall_vector_io_prepared(a[0],(const struct srv_iovec *)a[1],a[2],a[3],(int64_t)a[4],snapshot.iov);
#ifndef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    else if(w->raw_syscall==SYS_PIPE_MESSAGE)
        result=syscall_pipe_message_prepared(a[0],(const struct srv_pipe_message_layout *)a[1],(struct srv_pipe_message *)a[2],a[3],&snapshot.pipe);
    else if(w->raw_syscall==SYS_UNIX_CONNECT)
        result=process_unix_connect(process_current(),snapshot.path);
    else if(w->raw_syscall==SYS_FCNTL)
        result=process_file_set_lock(process_current(),a[0],&snapshot.lock,true);
#endif
    else if(sleep)result=syscall_sleep_ticks_measured(a[0],&elapsed);
#ifdef SRVROS_SIGNAL_EXPERIMENTAL_INFO
    else if(!signal_io_local_backend(w->raw_syscall,a,&result))result=SRV_ERR_NOTSUP;
#else
    else if(!signal_io_raw_backend(w->raw_syscall,a,&result))result=SRV_ERR_NOTSUP;
#endif
    (void)irq_save();
    if(!process_signal_io_capture(frame,w,cookie,sequence,result,elapsed.us,elapsed.ticks,elapsed.mono_valid,sleep)) {
        (void)process_record_user_fault(SRV_SIGNAL_SEGV);result=SRV_ERR_FAULT;
    }
    irq_restore(entry_flags);return result;
}
#endif
