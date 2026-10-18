#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#define _DIRENT_H 1
#include "../userspace/lib/include/dirent.h"
static int allocation_fail,stat_fail,list_fail=-1;
static char paths[220][160];static unsigned path_count;
static void *test_realloc(void *p,size_t n) { if(allocation_fail) return NULL;return realloc(p,n); }
static int test_stat(const char *path,struct stat *st) {
    if(stat_fail) { errno=EACCES;return -1; }
    memset(st,0,sizeof(*st));
    st->st_mode=!strcmp(path,"/") || !strcmp(path,"/fat") || strstr(path,"folder")?S_IFDIR:S_IFREG;return 0;
}
#define realloc test_realloc
#define stat test_stat
#define opendir test_opendir
#define readdir test_readdir
#define readdir64 test_readdir64
#define closedir test_closedir
#define alphasort test_alphasort
#define scandir test_scandir
#define scandir64 test_scandir64
/* The tag must stay struct stat while only calls are redirected. */
#undef stat
#define stat(...) test_stat(__VA_ARGS__)
#include "../userspace/lib/src/posix_dir.c"
#undef realloc
int __posix_make_path(const char *path,char *out,size_t capacity) {
    if(!path || strlen(path)>=capacity) { errno=ENAMETOOLONG;return -1; }
    strcpy(out,path);return 0;
}
long srv_list(uint64_t index,char *path,size_t capacity,uint64_t *size) {
    if(index==(uint64_t)list_fail) return -1;
    if(index>=path_count) return 0;
    assert(strlen(paths[index])<capacity);
    strcpy(path,paths[index]);*size=0;return 1;
}
int main(void) {
    for(unsigned i=0;i<100;i++) snprintf(paths[path_count++],160,"/fat/file%03u",i);
    strcpy(paths[path_count++],"/fat/folder");
    strcpy(paths[path_count++],"/fat/folder/a");strcpy(paths[path_count++],"/fat/folder/b");
    strcpy(paths[path_count++],"/fat/file099");strcpy(paths[path_count++],"/fatigue/outside");
    DIR *d=opendir("/fat");assert(d);unsigned count=0,folders=0;struct dirent *e;
    errno=0;while((e=readdir(d))) { count++;if(!strcmp(e->d_name,"folder")) { folders++;assert(e->d_type==DT_DIR); } }
    assert(!errno && count==101 && folders==1 && d->seen_capacity>=101);assert(!closedir(d));
    d=opendir("/fat");assert(d);allocation_fail=1;errno=0;
    assert(!readdir(d) && errno==ENOMEM && d->index==0);allocation_fail=0;
    assert(readdir(d) && !strcmp(d->entry.d_name,"file000"));closedir(d);
    stat_fail=1;assert(!opendir("/fat") && errno==EACCES);stat_fail=0;
    assert(!opendir("/fat/file000") && errno==ENOTDIR);
    d=opendir("/fat");list_fail=0;errno=0;assert(!readdir(d) && errno==EIO);closedir(d);
    struct dirent **list=(void *)1;assert(scandir("/fat",&list,NULL,NULL)<0 && list==NULL && errno==EIO);list_fail=-1;
    assert(scandir("/fat",&list,NULL,alphasort)==101);for(unsigned i=0;i<101;i++) free(list[i]);free(list);
    path_count=1;strcpy(paths[0],"/fat/");memset(paths[0]+5,'x',100);paths[0][105]=0;
    d=opendir("/fat");errno=0;assert(!readdir(d) && errno==ENAMETOOLONG);closedir(d);
    path_count=0;strcpy(paths[path_count++],"/");strcpy(paths[path_count++],"/fat");strcpy(paths[path_count++],"/fat/folder/file");strcpy(paths[path_count++],"/init");
    d=opendir("/");assert(d);count=0;while((e=readdir(d))) { assert(!strcmp(e->d_name,"fat") || !strcmp(e->d_name,"init"));count++; }assert(count==2);closedir(d);
    puts("posix-dir: PASS >64 unique children, duplicate descendants, directory type, allocation retry, permission/list errors and long names");
}
