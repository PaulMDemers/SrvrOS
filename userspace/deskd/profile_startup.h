#ifndef DESKD_PROFILE_STARTUP_H
#define DESKD_PROFILE_STARTUP_H
#include <srvros/profile_session.h>
#include <srvros/profile_preferences.h>
#include <srvros/sys.h>
#include <sys/stat.h>
#include <stdio.h>

/* Startup only, before graphics/event loops. This validates a provisioned home;
 * it does not authenticate a user or establish a filesystem permission boundary. */
static inline int desktop_profile_read(const char *path,char *data,size_t capacity) {
    FILE *file=fopen(path,"rb");if(!file)return -1;
    size_t n=fread(data,1,capacity,file);int failed=ferror(file);
    if(fclose(file))failed=1;
    return failed || n==capacity?-1:(int)n;
}
static inline int desktop_profile_load(struct srv_profile_session *session,char *portable,size_t capacity,const char *home) {
    char path[160],marker[80],json[513];struct stat st;struct srv_fsinfo fs={0};
    struct srv_profile_session checked;
    if(!srv_profile_session_init(&checked,home,NULL) ||
       (strncmp(home,"/fat/home/",10) && strncmp(home,"/work/srvros-work/",18)))return -1;
    if(srv_statfs(home,&fs) || strcmp(fs.filesystem,"exfat") ||
       (strcmp(fs.mountpoint,"/fat") && strcmp(fs.mountpoint,"/work")))return -1;
    static const char *directories[]={"","/Documents","/.config/srvros","/.local/share","/.cache"};
    for(unsigned i=0;i<sizeof(directories)/sizeof(directories[0]);i++) {
        int n=snprintf(path,sizeof(path),"%s%s",home,directories[i]);
        if(n<0 || (size_t)n>=sizeof(path) || stat(path,&st) || !S_ISDIR(st.st_mode))return -1;
    }
    int n=snprintf(path,sizeof(path),"%s/profile.id",home);
    if(n<0 || (size_t)n>=sizeof(path))return -1;
    n=desktop_profile_read(path,marker,sizeof(marker));
    static const char header[]="srvros-profile-v1\n";
    if(n!=(int)(sizeof(header)-1+SRV_PROFILE_SESSION_ID_LEN+1) ||
       memcmp(marker,header,sizeof(header)-1) || marker[n-1]!='\n')return -1;
    marker[n-1]=0;
    if(!srv_profile_session_init(&checked,home,marker+sizeof(header)-1))return -1;
    n=snprintf(path,sizeof(path),"%s/.config/srvros/portable.json",home);
    if(n<0 || (size_t)n>=sizeof(path) || (size_t)n>=capacity)return -1;
    int length=desktop_profile_read(path,json,sizeof(json));struct desk_preferences prefs;
    if(length<0 || srv_profile_preferences_decode(&prefs,json,(size_t)length))return -1;
    strcpy(portable,path);*session=checked;return 0;
}
#endif
