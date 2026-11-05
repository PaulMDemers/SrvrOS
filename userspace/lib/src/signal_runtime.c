#include "signal_runtime.h"
#include <srvros/sys.h>
#include <errno.h>
#include <signal.h>

static int public_available;
void __posix_signal_runtime_failure(void) {
    (void)srv_syscall6(SYS_EXIT,128+SIGABRT,0,0,0,0,0);
    __builtin_trap();
}
long __posix_signal_control(const struct srv_signal_control_v1 *wire,void *out,size_t size) {
    return srv_syscall6(SYS_SIGNAL_CONTROL,(long)wire,sizeof(*wire),(long)out,(long)size,0,0);
}
int __posix_signal_public_available(void) {
    int value=__atomic_load_n(&public_available,__ATOMIC_ACQUIRE);
    if(value)return value>0;
    struct srv_signal_control_v1 request={.size=96,.version=1,.operation=SRV_SIGNAL_CONTROL_FEATURES};
    struct srv_signal_features_v1 features={0};
    long result=__posix_signal_control(&request,&features,sizeof(features));
    value=result==0 && features.size==64 && features.version==1 && (features.capabilities&1) &&
        features.info_signals==(UINT64_C(1)<<SIGUSR1) && features.context_bytes==688 &&
        features.info_bytes==64 && features.max_depth==4?1:-1;
    __atomic_store_n(&public_available,value,__ATOMIC_RELEASE);
    return value>0;
}
static long io_request(const struct srv_signal_io_v1 *wire,void *out,size_t size) {
    return srv_syscall6(SYS_SIGNAL_IO,(long)wire,128,(long)out,(long)size,0,0);
}
int __posix_signal_io_begin(struct posix_signal_io *root,uint64_t kind,uint64_t primary,uint64_t selector,
    uint64_t anchor,uint64_t deadline,const int64_t *fds,size_t count,uint64_t flags,uint64_t mask) {
    if(!root || count>64 || (count && !fds))return SRV_ERR_INVAL;
    root->ticket=0;root->count=0;
    if(__posix_signal_info_depth())return SRV_ERR_NOTSUP;
    if(!__posix_signal_public_available())return 0;
    for(size_t i=0;i<count;i++)root->descriptors[i]=(struct srv_signal_descriptor_v1){fds[i],0};
    struct srv_signal_io_v1 wire={.size=128,.version=1,.operation=SRV_SIGNAL_IO_BEGIN,
        .args={anchor,mask,kind,primary,selector,0},.descriptor_vector=count?(uint64_t)root->descriptors:0,
        .descriptor_count=count,.flags=flags,.deadline=deadline};
    long result=io_request(&wire,count?root->descriptors:NULL,count*16);
    if(result<0)return (int)result;
    if(!result)__posix_signal_runtime_failure();
    root->ticket=(uint64_t)result;root->count=count;return 0;
}
long __posix_signal_io_call(struct posix_signal_io *root,uint64_t number,const uint64_t args[6],
    const struct srv_signal_descriptor_v1 *expected,size_t count) {
    if(!root || !args || count>64 || (count && !expected))return SRV_ERR_INVAL;
    if(__posix_signal_info_depth())return SRV_ERR_NOTSUP;
    if(!root->ticket)return srv_syscall6((long)number,args[0],args[1],args[2],args[3],args[4],args[5]);
    struct srv_signal_io_v1 wire={.size=128,.version=1,.operation=SRV_SIGNAL_IO_CALL,.ticket=root->ticket,
        .raw_syscall=number,.descriptor_vector=count?(uint64_t)expected:0,.descriptor_count=count};
    for(size_t i=0;i<6;i++)wire.args[i]=args[i];
    return io_request(&wire,NULL,0);
}
void __posix_signal_io_end(struct posix_signal_io *root) {
    if(!root || !root->ticket)return;
    struct srv_signal_io_v1 wire={.size=128,.version=1,.operation=SRV_SIGNAL_IO_END,.ticket=root->ticket};
    if(io_request(&wire,NULL,0)<0)__posix_signal_runtime_failure();
    root->ticket=0;
}
long __posix_signal_io_local_begin(struct posix_signal_io *root,uint64_t outcome,int finalize) {
    if(!root)return SRV_ERR_INVAL;
    if(!root->ticket)return 0;
    struct srv_signal_io_v1 wire={.size=128,.version=1,.operation=SRV_SIGNAL_IO_LOCAL_BEGIN,
        .ticket=root->ticket,.args={outcome,0,0,0,0,0},.flags=finalize?SRV_SIGNAL_IO_FINALIZE_NATIVE:0};
    return io_request(&wire,NULL,0);
}
void __posix_signal_io_local_end(struct posix_signal_io *root,uint64_t cookie) {
    if(!root || !root->ticket)return;
    if(!cookie || srv_syscall6(SYS_SIGNAL_IO,SRV_SIGNAL_IO_LOCAL_END,root->ticket,cookie,0,0,1)<0)
        __posix_signal_runtime_failure();
}
int __posix_signal_io_result(struct posix_signal_io *root,struct srv_signal_io_result_v1 *out) {
    if(!root || !out || !root->ticket)return SRV_ERR_INVAL;
    struct srv_signal_io_v1 wire={.size=128,.version=1,.operation=SRV_SIGNAL_IO_RESULT,.ticket=root->ticket};
    return (int)io_request(&wire,out,sizeof(*out));
}
int __posix_signal_io_capture_descriptors(struct posix_signal_io *root,const int64_t *fds,size_t count,uint64_t flags,
    struct srv_signal_descriptor_v1 *out) {
    if(!root || !root->ticket || count>64 || (count && (!fds || !out)))return SRV_ERR_INVAL;
    for(size_t i=0;i<count;i++)out[i]=(struct srv_signal_descriptor_v1){fds[i],0};
    struct srv_signal_io_v1 wire={.size=128,.version=1,.operation=SRV_SIGNAL_IO_CAPTURE_DESCRIPTORS,
        .ticket=root->ticket,.descriptor_vector=count?(uint64_t)out:0,.descriptor_count=count,.flags=flags};
    return (int)io_request(&wire,count?out:NULL,count*16);
}
