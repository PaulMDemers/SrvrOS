#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <srvros/salvage.h>
static int calls,last_fd,fail;
static int mock_copy(int fd,const char *path,struct srv_salvage_result *r) {
    calls++;last_fd=fd;assert(!strcmp(path,"/copy"));memset(r,0,sizeof(*r));
    r->bytes_written=123;
    if(fail) {r->stage=SRV_SALVAGE_WRITE;r->error=EIO;r->destination_opened=1;r->close_error=EINTR;r->restore_error=EIO;return -1;}
    return 0;
}
#define srv_salvage_copy mock_copy
#define main salvage_cli_main
#include "../userspace/salvage/salvage.c"
#undef main
int main(void) {
    char *args[]={"salvage","--fd","3","/copy",NULL};
    assert(salvage_cli_main(4,args)==0 && calls==1 && last_fd==3);
    fail=1;assert(salvage_cli_main(4,args)==1 && calls==2);fail=0;
    const char *invalid[]={"","-1","+3"," 3","3x","2147483648","999999999999999999999999"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {args[2]=(char *)invalid[i];assert(salvage_cli_main(4,args)==2 && calls==2);}
    args[2]="2147483647";assert(salvage_cli_main(4,args)==0 && last_fd==INT_MAX);
    args[2]="0";assert(salvage_cli_main(4,args)==0 && last_fd==0);
    int before=calls;assert(salvage_cli_main(3,args)==2 && calls==before);
    args[1]="--path";assert(salvage_cli_main(4,args)==2 && calls==before);
    args[1]="--fd";args[3]="";assert(salvage_cli_main(4,args)==2 && calls==before);
    puts("salvage-cli: PASS syntax, fd bounds, helper arguments and success/failure statuses");
}
