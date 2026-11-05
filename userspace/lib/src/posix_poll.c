#include "signal_runtime.h"
#include "socket_signal.h"
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

#include <srvros/sys.h>

int __posix_socket_poll_fd(int fd);
short __posix_socket_poll_update(int fd, short requested, short current_revents);
int __posix_signal_dispatch_pending(void);
int __posix_eventfd_poll(int fd, short requested, uint64_t *identity);
int __posix_eventfd_poll_locked(int fd, short requested, uint64_t *identity);
void __posix_event_lock(void);
void __posix_event_unlock(void);
uint64_t __posix_event_capture_identity_locked(int);
int __posix_event_identity_matches_locked(int,uint64_t);

#define POSIX_POLL_MAX 64

static int poll_trace_enabled(void) {
    const char *trace = getenv("SRVROS_POLL_TRACE");
    return trace != 0 && trace[0] != '\0' && trace[0] != '0';
}

static int poll_native_ms(struct pollfd *fds,nfds_t nfds,int64_t timeout) {
    struct srv_pollfd srv_fds[POSIX_POLL_MAX];
    if (nfds > POSIX_POLL_MAX || (nfds != 0 && fds == 0)) {
        errno = EINVAL;
        return -1;
    }

    int trace = poll_trace_enabled();
    int pre_ready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        int real_fd = fds[i].fd < 0 ? -1 : __posix_socket_poll_fd(fds[i].fd);
        srv_fds[i].fd = real_fd;
        srv_fds[i].events = fds[i].events;
        srv_fds[i].revents = 0;
        fds[i].revents = 0;
        if (trace) {
            fprintf(stderr,
                "srvros-poll: in fd=%d real=%d events=0x%x timeout=%lld\n",
                fds[i].fd,
                real_fd,
                (unsigned int)fds[i].events,
                (long long)timeout);
        }
        if (fds[i].fd >= 0 && real_fd < 0) {
            fds[i].revents = POLLNVAL;
            pre_ready++;
        }
    }

    int dispatched_before = __posix_signal_dispatch_pending();
    long result = srv_poll(srv_fds, nfds, pre_ready != 0 || dispatched_before != 0 ? 0 : timeout);
    int dispatched_after = __posix_signal_dispatch_pending();
    if (result < 0) {
        errno = result < -1 ? (int)-result : EINVAL;
        return -1;
    }
    if (trace) {
        fprintf(stderr, "srvros-poll: result=%ld\n", result);
    }
    if (result == 0 && (dispatched_before != 0 || dispatched_after != 0)) {
        errno = EINTR;
        return -1;
    }

    int ready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        if (fds[i].revents == POLLNVAL) {
            ready++;
            continue;
        }
        fds[i].revents = __posix_socket_poll_update(fds[i].fd,
            fds[i].events,
            srv_fds[i].revents);
        if (trace) {
            fprintf(stderr,
                "srvros-poll: out fd=%d real=%d revents=0x%x\n",
                fds[i].fd,
                srv_fds[i].fd,
                (unsigned int)fds[i].revents);
        }
        if (fds[i].revents != 0) {
            ready++;
        }
    }
    (void)result;
    return ready;
}

/* One real frame owns the root. The legacy adapter remains the capability-zero path. */
struct posix_signal_select_outputs { fd_set *read, *write, *except; };
__attribute__((noinline)) int __posix_poll_signal_ms(struct pollfd *fds,nfds_t nfds,int64_t timeout,const sigset_t *mask,struct posix_signal_select_outputs *outputs) {
    if (__posix_signal_info_depth()) { errno=EOPNOTSUPP; return -1; }
    if (nfds>POSIX_POLL_MAX || (nfds && !fds)) { errno=EINVAL; return -1; }
    struct posix_signal_io root;
    struct srv_pollfd native[POSIX_POLL_MAX];
    struct posix_socket_poll_snapshot initial[POSIX_POLL_MAX], current[POSIX_POLL_MAX];
    uint64_t identities[POSIX_POLL_MAX]={0},native_identity[POSIX_POLL_MAX]={0};
    int synthetic[POSIX_POLL_MAX];
    int64_t descriptors[POSIX_POLL_MAX];
    size_t count=0;
    int has_synthetic=0;
    uint64_t deadline=UINT64_MAX;
    long fallback_start=-1;
    if(timeout==0) deadline=0;
    else if(timeout>0) {
        long now=srv_monotonic_us();
        if(now>=0 && (uint64_t)timeout <= (UINT64_MAX-(uint64_t)now)/1000)
            deadline=(uint64_t)now+(uint64_t)timeout*1000;
        if(deadline==UINT64_MAX) fallback_start=srv_ticks();
        if(deadline==UINT64_MAX && fallback_start<0) { errno=EIO; return -1; }
    }
    __posix_event_lock();
    for(nfds_t i=0;i<nfds;i++) {
        synthetic[i]=__posix_eventfd_poll_locked(fds[i].fd,fds[i].events,&identities[i]);
        has_synthetic |= synthetic[i]>=0;
        __posix_socket_poll_prepare(fds[i].fd,&initial[i]);
        int fd=synthetic[i]>=0 ? -1 : initial[i].object ? initial[i].native_fd : fds[i].fd;
        if(synthetic[i]<0 && fds[i].fd>=0) native_identity[i]=__posix_event_capture_identity_locked(fds[i].fd);
        native[i]=(struct srv_pollfd){.fd=fd,.events=fds[i].events,.revents=0};
        if(fd>=0) descriptors[count++]=fd;
    }
    int status=__posix_signal_io_begin(&root,SRV_SIGNAL_ROOT_POLL,SYS_POLL,0,
        (uint64_t)(uintptr_t)__builtin_frame_address(0),deadline,descriptors,count,
        SRV_SIGNAL_IO_POLL_LIST|(mask?SRV_SIGNAL_IO_LEGACY_MASKED:0),mask?(uint64_t)*mask:0);
    __posix_event_unlock();
    if(status<0) { errno=-status; return -1; }
    for (;;) {
        int pre_ready=0;
        __posix_event_lock();
        for(nfds_t i=0;i<nfds;i++) {
            if(synthetic[i]>=0) {
                synthetic[i]=identities[i]?__posix_eventfd_poll_locked(fds[i].fd,fds[i].events,&identities[i]):POLLNVAL;
                pre_ready|=synthetic[i]!=0;
            }
            if(synthetic[i]<0 && initial[i].object && native[i].fd<0) pre_ready=1;
            native[i].revents=0;
        }
        __posix_event_unlock();
        int64_t slice=pre_ready || timeout==0 ? 0 : (has_synthetic || fallback_start>=0) ? 10 : timeout;
        if(deadline!=UINT64_MAX && deadline!=0) {
            long now=srv_monotonic_us();
            if(now>=0) {
                uint64_t left=(uint64_t)now>=deadline?0:deadline-(uint64_t)now;
                int64_t leftms=(int64_t)(left/1000+(left%1000!=0));
                if(slice<0 || leftms<slice) slice=leftms;
            }
        }
        const uint64_t args[6]={(uint64_t)(uintptr_t)native,nfds,(uint64_t)slice,0,0,0};
        long result=__posix_signal_io_call(&root,SYS_POLL,args,root.descriptors,root.count);
        int dispatched=__posix_signal_dispatch_pending();
        if(result<0) { __posix_signal_io_end(&root);errno=(int)-result;return -1; }
        struct srv_signal_io_result_v1 outcome={0};
        if(__posix_signal_io_result(&root,&outcome)<0 || outcome.size!=sizeof(outcome) || outcome.version!=1 ||
           !outcome.cookie || outcome.syscall_number!=SYS_POLL || outcome.result!=result)
            __posix_signal_runtime_failure();
        int completed=(outcome.validity&SRV_SIGNAL_IO_RESULT_COMPLETED_NATIVE)!=0;
        int fallback_expired=0;
        if(fallback_start>=0) {
            long now=srv_ticks();
            if(now<fallback_start) { __posix_signal_io_end(&root);errno=EIO;return -1; }
            fallback_expired=(uint64_t)(now-fallback_start)>=((uint64_t)timeout+9)/10+1;
        }
        __posix_event_lock();
        for(nfds_t i=0;i<nfds;i++) __posix_socket_poll_prepare(fds[i].fd,&current[i]);
        long guard=__posix_signal_io_local_begin(&root,completed?outcome.cookie:0,completed);
        if(guard<0) { __posix_event_unlock();__posix_signal_io_end(&root);errno=(int)-guard;return -1; }
        int ready=0, bits=0, badfd=0;
        short revents_out[POSIX_POLL_MAX];
        fd_set read_result,write_result,except_result;
        if(outputs) { FD_ZERO(&read_result);FD_ZERO(&write_result);FD_ZERO(&except_result); }
        for(nfds_t i=0;i<nfds;i++) {
            short revents=0;
            if(synthetic[i]>=0)
                revents=identities[i]?(short)__posix_eventfd_poll_locked(fds[i].fd,fds[i].events,&identities[i]):POLLNVAL;
            else if(fds[i].fd>=0 && (!native_identity[i] || !__posix_event_identity_matches_locked(fds[i].fd,native_identity[i]))) revents=POLLNVAL;
            else if(initial[i].object && native[i].fd<0) revents=POLLNVAL;
            else if(fds[i].fd>=0 && (current[i].descriptor_generation!=initial[i].descriptor_generation ||
                    current[i].object_generation!=initial[i].object_generation || current[i].object!=initial[i].object ||
                    current[i].native_fd!=initial[i].native_fd)) revents=POLLNVAL;
            else revents=__posix_socket_poll_commit(fds[i].fd,fds[i].events,native[i].revents,&current[i]);
            revents_out[i]=revents;
            ready+=revents!=0;
            if(outputs) {
                if(revents&POLLNVAL) badfd=1;
                if((fds[i].events&POLLIN) && (revents&(POLLIN|POLLHUP|POLLERR))) { FD_SET(fds[i].fd,&read_result);bits++; }
                if((fds[i].events&POLLOUT) && (revents&(POLLOUT|POLLERR))) { FD_SET(fds[i].fd,&write_result);bits++; }
                if((fds[i].events&POLLPRI) && (revents&POLLPRI)) { FD_SET(fds[i].fd,&except_result);bits++; }
            }
        }
        if(!outputs && (ready || completed || fallback_expired))
            for(nfds_t i=0;i<nfds;i++) fds[i].revents=revents_out[i];
        if(outputs && !badfd && (ready || completed || (fallback_expired && !dispatched))) {
            if(outputs->read) *outputs->read=read_result;
            if(outputs->write) *outputs->write=write_result;
            if(outputs->except) *outputs->except=except_result;
        }
        __posix_event_unlock();
        __posix_signal_io_local_end(&root,(uint64_t)guard);
        if(ready || completed || dispatched || fallback_expired) {
            __posix_signal_io_end(&root);
            if(badfd) { errno=EBADF;return -1; }
            if(!ready && !completed && dispatched) { errno=EINTR;return -1; }
            return outputs?bits:ready;
        }
    }
}

int __posix_poll_signal_masked_ms(struct pollfd *fds,nfds_t nfds,int64_t timeout,const sigset_t *mask) {
    return __posix_poll_signal_ms(fds,nfds,timeout,mask,0);
}

int __posix_poll_ms(struct pollfd *fds,nfds_t nfds,int64_t timeout) {
    if (__posix_signal_info_depth()) { errno=EOPNOTSUPP; return -1; }
    if (__posix_signal_public_available()) return __posix_poll_signal_ms(fds,nfds,timeout,0,0);
    if (nfds > POSIX_POLL_MAX || (nfds && !fds)) { errno = EINVAL; return -1; }
    uint64_t identities[POSIX_POLL_MAX] = {0};
    int synthetic[POSIX_POLL_MAX], has_synthetic = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        synthetic[i] = __posix_eventfd_poll(fds[i].fd, fds[i].events, &identities[i]);
        has_synthetic |= synthetic[i] >= 0;
    }
    if (!has_synthetic) return poll_native_ms(fds, nfds, timeout);
    struct timespec start = {0};
    if (timeout > 0 && clock_gettime(CLOCK_MONOTONIC, &start) < 0) return -1;
    int64_t remaining = timeout;
    for (;;) {
        struct pollfd native[POSIX_POLL_MAX];
        int pre_ready = 0;
        for (nfds_t i = 0; i < nfds; i++) {
            native[i] = fds[i];
            if (synthetic[i] >= 0) {
                synthetic[i] = __posix_eventfd_poll(fds[i].fd, fds[i].events, &identities[i]);
                native[i].fd = -1;
                pre_ready |= synthetic[i] != 0;
            }
        }
        int64_t slice = pre_ready || timeout == 0 ? 0 :
            remaining < 0 || remaining > 10 ? 10 : remaining;
        if (poll_native_ms(native, nfds, slice) < 0) return -1;
        int ready = 0;
        for (nfds_t i = 0; i < nfds; i++) {
            fds[i].revents = synthetic[i] >= 0 ?
                (short)__posix_eventfd_poll(fds[i].fd, fds[i].events, &identities[i]) : native[i].revents;
            ready += fds[i].revents != 0;
        }
        if (ready || timeout == 0) return ready;
        if (timeout > 0) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
            int64_t seconds = now.tv_sec - start.tv_sec;
            long ns = now.tv_nsec - start.tv_nsec;
            if (ns < 0) { seconds--; ns += 1000000000; }
            /* Compare before multiplication, including saturated ppoll/select
             * timeouts. Floor elapsed time so sub-ms waits never finish early. */
            if (seconds > timeout / 1000) return 0;
            int64_t elapsed = seconds < 0 ? 0 : seconds * 1000;
            if (elapsed >= timeout || ns / 1000000 >= timeout - elapsed) return 0;
            remaining = timeout - elapsed - ns / 1000000;
        }
    }
}

int poll(struct pollfd *fds,nfds_t nfds,int timeout) { return __posix_poll_ms(fds,nfds,timeout); }

static int64_t timeval_to_ms(const struct timeval *timeout) {
    if (timeout == 0) {
        return -1;
    }
    int64_t part=(timeout->tv_usec+999)/1000;
    if(timeout->tv_sec>(INT64_MAX-part)/1000) return INT64_MAX;
    return timeout->tv_sec*1000+part;
}

static int select_ms_impl(int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    int64_t timeout, const sigset_t *mask) {
    struct pollfd fds[POSIX_POLL_MAX];
    fd_set read_result;
    fd_set write_result;
    fd_set except_result;
    int count = 0;

    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }

    for (int fd = 0; fd < nfds; fd++) {
        short events = 0;
        if (readfds != 0 && FD_ISSET(fd, readfds)) {
            events |= POLLIN;
        }
        if (writefds != 0 && FD_ISSET(fd, writefds)) {
            events |= POLLOUT;
        }
        if (exceptfds != 0 && FD_ISSET(fd, exceptfds)) events |= POLLPRI;
        if (events != 0) {
            if (count >= POSIX_POLL_MAX) {
                errno = EINVAL;
                return -1;
            }
            fds[count].fd = fd;
            fds[count].events = events;
            fds[count].revents = 0;
            count++;
        }
    }

    if (__posix_signal_public_available()) {
        struct posix_signal_select_outputs outputs={readfds,writefds,exceptfds};
        return __posix_poll_signal_ms(fds,(nfds_t)count,timeout,mask,&outputs);
    }
    int result = __posix_poll_ms(fds,(nfds_t)count,timeout);
    if (result < 0) {
        return -1;
    }

    FD_ZERO(&read_result);
    FD_ZERO(&write_result);
    FD_ZERO(&except_result);
    int bits=0;
    for (int i = 0; i < count; i++) {
        if (fds[i].revents & POLLNVAL) { errno=EBADF; return -1; }
        if ((fds[i].events & POLLIN) && (fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
            FD_SET(fds[i].fd, &read_result);
            bits++;
        }
        if ((fds[i].events & POLLOUT) && (fds[i].revents & (POLLOUT | POLLERR))) {
            FD_SET(fds[i].fd, &write_result);
            bits++;
        }
        if ((fds[i].events & POLLPRI) && (fds[i].revents & POLLPRI)) {
            FD_SET(fds[i].fd,&except_result); bits++;
        }
    }

    if (readfds != 0) {
        memcpy(readfds, &read_result, sizeof(*readfds));
    }
    if (writefds != 0) {
        memcpy(writefds, &write_result, sizeof(*writefds));
    }
    if (exceptfds != 0) memcpy(exceptfds,&except_result,sizeof(*exceptfds));
    return bits;
}

int __posix_select_ms(int nfds,fd_set *readfds,fd_set *writefds,fd_set *exceptfds,int64_t timeout) {
    return select_ms_impl(nfds,readfds,writefds,exceptfds,timeout,0);
}
int __posix_select_signal_masked_ms(int nfds,fd_set *readfds,fd_set *writefds,fd_set *exceptfds,int64_t timeout,const sigset_t *mask) {
    return select_ms_impl(nfds,readfds,writefds,exceptfds,timeout,mask);
}

int select(int nfds,fd_set *readfds,fd_set *writefds,fd_set *exceptfds,struct timeval *timeout) {
    if (__posix_signal_info_depth()) { errno=EOPNOTSUPP; return -1; }
    if(timeout && (timeout->tv_sec<0 || timeout->tv_usec<0 || timeout->tv_usec>=1000000)) {
        errno=EINVAL;return -1;
    }
    return __posix_select_ms(nfds,readfds,writefds,exceptfds,timeval_to_ms(timeout));
}
