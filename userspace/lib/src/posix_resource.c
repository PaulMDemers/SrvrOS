#include <errno.h>
#include <srvros/sys.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

int getrlimit(int resource,struct rlimit *limit) {
    if(!limit) { errno=EFAULT;return -1; }
    struct srv_rlimit value;
    long result=srv_rlimit((uint64_t)resource,0,&value);
    if(result<0) { errno=(int)-result;return -1; }
    *limit=(struct rlimit){value.current,value.maximum};return 0;
}
int getrlimit64(int resource,struct rlimit *limit) { return getrlimit(resource,limit); }
int setrlimit(int resource,const struct rlimit *limit) {
    if(!limit) { errno=EFAULT;return -1; }
    struct srv_rlimit value={limit->rlim_cur,limit->rlim_max};
    long result=srv_rlimit((uint64_t)resource,&value,0);
    if(result<0) { errno=(int)-result;return -1; }
    return 0;
}
int setrlimit64(int resource,const struct rlimit *limit) { return setrlimit(resource,limit); }

static int priority_target(int which,int who) {
    if(which!=PRIO_PROCESS && which!=PRIO_PGRP && which!=PRIO_USER) { errno=EINVAL;return -1; }
    if(which!=PRIO_PROCESS) { errno=EOPNOTSUPP;return -1; }
    if(who<0) { errno=ESRCH;return -1; }
    return kill(who?who:getpid(),0);
}
int getpriority(int which,int who) {
    if(priority_target(which,who)<0) return -1;
    return 0;
}
int setpriority(int which,int who,int prio) {
    if(priority_target(which,who)<0) return -1;
    if(prio!=0) { errno=EOPNOTSUPP;return -1; }
    return 0;
}

int getrusage(int who, struct rusage *usage) {
    if (usage == 0 || (who != RUSAGE_SELF && who != RUSAGE_CHILDREN && who != RUSAGE_THREAD)) {
        errno = EINVAL;
        return -1;
    }
    struct srv_usage snapshot;
    long result=srv_usage(who,&snapshot);
    if(result<0) { errno=(int)-result;return -1; }
    memset(usage,0,sizeof(*usage));
    usage->ru_utime.tv_sec=(time_t)(snapshot.user_us/1000000);
    usage->ru_utime.tv_usec=(suseconds_t)(snapshot.user_us%1000000);
    usage->ru_stime.tv_sec=(time_t)(snapshot.system_us/1000000);
    usage->ru_stime.tv_usec=(suseconds_t)(snapshot.system_us%1000000);
    usage->ru_maxrss=(long)((snapshot.peak_mapped_bytes+1023)/1024);
    return 0;
}
