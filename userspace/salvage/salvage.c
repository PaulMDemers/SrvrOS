#include <srvros/salvage.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static const char *stage_name(enum srv_salvage_stage stage) {
    switch(stage) {
    case SRV_SALVAGE_SOURCE_POSITION:return "source-position";
    case SRV_SALVAGE_SOURCE_REWIND:return "source-rewind";
    case SRV_SALVAGE_DESTINATION_OPEN:return "destination-open";
    case SRV_SALVAGE_READ:return "read";
    case SRV_SALVAGE_WRITE:return "write";
    case SRV_SALVAGE_FLUSH:return "flush";
    case SRV_SALVAGE_CLOSE:return "close";
    case SRV_SALVAGE_RESTORE:return "source-restore";
    default:return "unknown";
    }
}
int main(int argc,char **argv) {
    unsigned fd=0;
    if(argc!=4 || strcmp(argv[1],"--fd") || !argv[2][0] || !argv[3][0]) goto usage;
    for(const char *p=argv[2];*p;p++) {
        if(*p<'0' || *p>'9' || fd>((unsigned)INT_MAX-(unsigned)(*p-'0'))/10) goto usage;
        fd=fd*10+(unsigned)(*p-'0');
    }
    struct srv_salvage_result result;
    if(srv_salvage_copy((int)fd,argv[3],&result)<0) {
        fprintf(stderr,"salvage: failed stage=%s error=%d close-error=%d restore-error=%d bytes=%llu\n",
            stage_name(result.stage),result.error,result.close_error,result.restore_error,
            (unsigned long long)result.bytes_written);
        if(result.destination_opened) fputs("salvage: destination artifact retained; copy is not acknowledged\n",stderr);
        return 1;
    }
    printf("salvage: copied bytes=%llu\n",(unsigned long long)result.bytes_written);
    return 0;
usage:
    fputs("usage: salvage --fd NUMBER DESTINATION\n",stderr);
    return 2;
}
