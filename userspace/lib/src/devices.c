#include <srvros/sys.h>
long srv_device_list(unsigned kind,uint64_t index,struct srv_device_info *info) {
    return srv_syscall4(SYS_DEVICE_LIST,kind,index,(long)info,sizeof(*info));
}
