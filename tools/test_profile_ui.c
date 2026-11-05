/* Exercise the actual Profiles review parser; only its two widget seams are
 * adapted. Rendering and interaction are qualified separately in guest VMs. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define main profiles_app_main
#include "../userspace/apps/deskprofiles/deskprofiles.c"
#undef main
uint64_t desk_tabs_selected(const struct desk_tabs *tabs) { return (uint64_t)tabs->selected; }
int desk_document_set(struct desk_document *doc,const struct desk_document_block *blocks,unsigned count) {
    size_t total=0;if(count>DESK_DOCUMENT_BLOCKS)return -1;
    for(unsigned i=0;i<count;i++){size_t n=strlen(blocks[i].text);if(n>=DESK_DOCUMENT_BLOCK_BYTES)return -1;total+=n;}
    if(total>DESK_DOCUMENT_BYTES)return -1;
    memcpy(doc->blocks,blocks,count*sizeof(*blocks));doc->count=count;return 0;
}
static cJSON *review(unsigned version,unsigned count,unsigned size) {
    cJSON *j=cJSON_CreateObject(),*p=cJSON_CreateObject(),*docs=cJSON_CreateArray();assert(j&&p&&docs);
    cJSON_AddStringToObject(j,"format","srvros-profile-review-v1");cJSON_AddStringToObject(j,"operation","export");
    cJSON_AddStringToObject(j,"sha256","0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    cJSON_AddStringToObject(p,"id","34d7a59e-0237-43de-8c13-dbc3927edb01");cJSON_AddStringToObject(p,"display_name","Portable test");
    cJSON_AddItemToObject(j,"profile",p);cJSON_AddItemToObject(j,"preferences",cJSON_CreateObject());cJSON_AddItemToObject(j,"documents",docs);
    if(version)cJSON_AddNumberToObject(j,"bundle_version",version);
    cJSON_AddNumberToObject(j,"bytes",(double)count*size);
    for(unsigned i=0;i<count;i++) {
        char path[161];memcpy(path,"Documents/",10);memset(path+10,'a',64);path[74]='/';memset(path+75,'b',64);path[139]='/';
        snprintf(path+140,21,"%020u",i);assert(strlen(path)==160);
        cJSON *d=cJSON_CreateObject();cJSON_AddStringToObject(d,"path",path);cJSON_AddNumberToObject(d,"size",size);cJSON_AddItemToArray(docs,d);
    }
    return j;
}
static int parse(cJSON *j) {
    char *text=cJSON_PrintUnformatted(j);assert(text);size_t n=strlen(text);assert(n<sizeof(a.out));
    memset(&a,0,sizeof(a));a.tabs.selected=EXPORT;memcpy(a.out,text,n+1);a.stdout_stream.used=n;cJSON_free(text);return parse_review();
}
int main(void) {
    cJSON *j=review(2,64,1048576);assert(!parse(j));assert(a.report.count==6);assert(strstr(a.report_text[5],"requires"));
    unsigned lines=0;for(unsigned k=2;k<=4;k++)for(const char *p=a.report_text[k];*p;p++)if(*p=='\n')lines++;assert(lines==64);
    cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(j,"bytes"),1);assert(parse(j));cJSON_Delete(j);
    j=review(2,1,16777216);assert(!parse(j));a.stdout_stream.truncated=1;assert(parse_review());cJSON_Delete(j);
    j=review(2,1,16777217);assert(parse(j));cJSON_Delete(j);
    j=review(1,1,1048577);assert(parse(j));cJSON_Delete(j);
    j=review(3,1,0);assert(parse(j));cJSON_Delete(j);
    j=review(0,1,1024);assert(!parse(j));assert(!strstr(a.report_text[5],"requires"));cJSON_Delete(j);
    j=review(2,5,16777216);assert(parse(j));cJSON_Delete(j);
    assert(diagnostic_valid("copy-partial"));assert(!diagnostic_valid("untrusted"));
    puts("Profiles actual-source review tests passed");return 0;
}
