#define _XOPEN_SOURCE 700
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <srvros/sys.h>

static const char *marker="srvros-profile-v1\n12345678-1234-4234-8234-123456789abc\n";
static const char *prefs="{\"text_size\":20}";
static int missing_directory,bad_mount,missing_marker,missing_prefs,pending_import,pending_error;
static long mock_statfs(const char *path,struct srv_fsinfo *fs) {
    (void)path;strcpy(fs->filesystem,bad_mount?"other":"exfat");strcpy(fs->mountpoint,"/fat");return 0;
}
static int mock_stat(const char *path,struct stat *st) {
    if(strstr(path,".profile-import-pending")) {
        if(pending_import) { memset(st,0,sizeof(*st));return 0; }
        errno=pending_error?EIO:ENOENT;return -1;
    }
    if(missing_directory)return -1;
    memset(st,0,sizeof(*st));st->st_mode=S_IFDIR;return 0;
}
static FILE *mock_fopen(const char *path,const char *mode) {
    const char *data;
    (void)mode;
    if(strstr(path,"profile.id")) { if(missing_marker)return NULL;data=marker; }
    else { if(missing_prefs)return NULL;data=prefs; }
    FILE *f=tmpfile();assert(f);assert(fwrite(data,1,strlen(data),f)==strlen(data));rewind(f);return f;
}
#define srv_statfs mock_statfs
#define stat(path, st) mock_stat(path, st)
#define lstat(path, st) mock_stat(path, st)
#define fopen mock_fopen
#include "../userspace/deskd/profile_startup.h"
#undef fopen
#undef lstat
#undef stat
#undef srv_statfs

int main(void) {
    struct srv_profile_session session;char portable[160];
    const char *home="/fat/home/alice";
    assert(!desktop_profile_load(&session,portable,sizeof(portable),home));
    assert(!strcmp(portable,"/fat/home/alice/.config/srvros/portable.json"));
    assert(desktop_profile_load(&session,portable,sizeof(portable),"/fat/home/../bad")<0);
    assert(desktop_profile_load(&session,portable,sizeof(portable),"/other/home")<0);
    bad_mount=1;assert(desktop_profile_load(&session,portable,sizeof(portable),home)<0);bad_mount=0;
    missing_directory=1;assert(desktop_profile_load(&session,portable,sizeof(portable),home)<0);missing_directory=0;
    pending_import=1;assert(desktop_profile_load(&session,portable,sizeof(portable),home)<0);pending_import=0;
    pending_error=1;assert(desktop_profile_load(&session,portable,sizeof(portable),home)<0);pending_error=0;
    missing_marker=1;assert(desktop_profile_load(&session,portable,sizeof(portable),home)<0);missing_marker=0;
    marker="srvros-profile-v1\ninvalid\n";assert(desktop_profile_load(&session,portable,sizeof(portable),home)<0);
    marker="srvros-profile-v1\n12345678-1234-4234-8234-123456789abc\n";
    missing_prefs=1;assert(desktop_profile_load(&session,portable,sizeof(portable),home)<0);missing_prefs=0;
    prefs="{\"text_size\":true}";assert(desktop_profile_load(&session,portable,sizeof(portable),home)<0);
    prefs="{}";assert(desktop_profile_load(&session,portable,2,home)<0);
    puts("profile startup: PASS");return 0;
}
