#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#ifdef SRVROS_PROFILE_HOST_TEST
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef union { max_align_t align; size_t size; } allocation_header;
static size_t allocated,peak;
void *profile_memory_alloc(size_t size){if(size>SIZE_MAX-sizeof(allocation_header))return NULL;allocation_header *p=malloc(sizeof(*p)+size);if(!p)return NULL;p->size=size;allocated+=size;if(allocated>peak)peak=allocated;return p+1;}
void profile_memory_free(void *ptr){if(ptr){allocation_header *p=(allocation_header *)ptr-1;allocated-=p->size;free(p);}}
void profile_memory_report(const char *phase){fprintf(stderr,"profile-memory: owned_peak_bytes=%zu phase=%s\n",peak,phase);}
void profile_memory_init(void){cJSON_Hooks hooks={profile_memory_alloc,profile_memory_free};cJSON_InitHooks(&hooks);}
#else
#include <srvros/sys.h>
void profile_memory_report(const char *phase){const char *trace=getenv("SRVROS_PROFILE_TRACE_MEMORY");if(!trace||strcmp(trace,"1"))return;struct srv_usage usage;if(srv_usage(SRV_USAGE_SELF,&usage)==0)fprintf(stderr,"profile-memory: guest_peak_mapped_bytes=%llu phase=%s\n",(unsigned long long)usage.peak_mapped_bytes,phase);}
void profile_memory_init(void){}
#endif
