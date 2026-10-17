#include <assert.h>
#define main unused_posixutils_main
#include "../userspace/posixutils/posixutils.c"
#undef main
static long result;
long srv_sync(void) { return result; }
int main(void) {
    char *args[]={"sync",NULL};
    result=0;assert(sync_main(1,args)==0);
    result=-1;assert(sync_main(1,args)==1);
    puts("sync-command-host: PASS (flush failure returns nonzero)");
}
