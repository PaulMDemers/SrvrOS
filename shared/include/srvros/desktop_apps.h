#ifndef SRVROS_DESKTOP_APPS_H
#define SRVROS_DESKTOP_APPS_H
#include <srvros/desktop_protocol.h>
/* Built-in application descriptors shared by discovery, launch and document
 * routing. Numeric protocol identities remain stable; keys name applications
 * independently of their display titles. This is not an executable installer.
 * A zero dock position means unpinned; running tools still appear in the dock. */
#define DESK_APPLICATION_DOCUMENT_FALLBACK 1u
struct desk_application_info {
    const char *title,*role,*program;unsigned dock;
    const char *key,*extensions;unsigned documents,services;
};
static inline const struct desk_application_info *desk_application_info(unsigned id) {
    static const struct desk_application_info catalog[]={
        {"Files","files","/fat/bin/desktools",1,"srvros.files","",0,0},
        {"Text","text","/fat/bin/desktools",2,"srvros.text","",DESK_APPLICATION_DOCUMENT_FALLBACK,DESK_SERVICE_OPEN_DOCUMENT},
        {"Calculator","calculator","/fat/bin/desktools",3,"srvros.calculator","",0,0},
        {"System","system","/fat/bin/desktools",4,"srvros.system","",0,0},
        {"Settings","settings","/fat/bin/desktools",5,"srvros.settings","",0,0},
        {"Terminal","terminal","/fat/bin/deskterm",6,"srvros.terminal","",0,0},
        {"Preview","preview","/fat/bin/deskview",0,"srvros.preview","png jpg jpeg bmp",0,DESK_SERVICE_OPEN_DOCUMENT},
        {"Help","help","/fat/bin/deskhelp",0,"srvros.help","",0,0},
        {"Clock","clock","/fat/bin/deskclock",0,"srvros.clock","",0,0},
        {"Sketch","sketch","/fat/bin/desksketch",0,"srvros.sketch","",0,0},
        {"Network Utility","network","/fat/bin/desknet",0,"srvros.network","",0,0},
        {"Find Files","find","/fat/bin/deskfind",0,"srvros.find","",0,0},
        {"Profiles","profiles","/fat/bin/deskprofiles",0,"srvros.profiles","",0,0},
    };
    _Static_assert(sizeof(catalog)/sizeof(*catalog)==DESK_APP_COUNT,"application catalog coverage");
    return id>=1 && id<=DESK_APP_COUNT?&catalog[id-1]:0;
}
static inline unsigned desk_application_by_key(const char *key) {
    if(!key || !*key)return 0;
    for(unsigned id=1;id<=DESK_APP_COUNT;id++) {
        const char *a=key,*b=desk_application_info(id)->key;
        while(*a && *a==*b){a++;b++;}
        if(!*a && !*b)return id;
    }
    return 0;
}
static inline unsigned desk_application_for_role(const char *program,const char *role) {
    if(!program || !role)return 0;
    for(unsigned id=1;id<=DESK_APP_COUNT;id++) {
        const struct desk_application_info *info=desk_application_info(id);
        const char *a=program,*b=info->program;while(*a && *a==*b){a++;b++;}
        if(*a || *b)continue;
        a=role;b=info->role;while(*a && *a==*b){a++;b++;}
        if(!*a && !*b)return id;
    }
    return 0;
}
static inline unsigned desk_dock_count(void) {
    unsigned count=0;for(unsigned id=1;id<=DESK_APP_COUNT;id++) if(desk_application_info(id)->dock>count) count=desk_application_info(id)->dock;return count;
}
/* Extension matches outrank the generic Text fallback. Matching is ASCII case
 * insensitive and examines only the final filename, never directory suffixes.
 * A handler is a candidate; the app still validates actual contents/size. */
static inline unsigned desk_application_document_rank(unsigned id,const char *path) {
    const struct desk_application_info *info=desk_application_info(id);
    if(!info || !path || !*path)return 0;
    const char *extension=0,*leaf=path;
    for(const char *p=path;*p;p++) {
        if(*p=='/') {extension=0;leaf=p+1;}
        else if(*p=='.')extension=p+1;
    }
    if(!*leaf)return 0;
    if(extension && *extension)for(const char *word=info->extensions;*word;) {
        const char *a=extension,*b=word;
        while(*a && *b && *b!=' ') {
            unsigned c=(unsigned char)*a;if(c>='A' && c<='Z')c+='a'-'A';
            if(c!=(unsigned char)*b)break;
            a++;b++;
        }
        if(!*a && (!*b || *b==' '))return 2;
        while(*word && *word!=' ')word++;
        while(*word==' ')word++;
    }
    return info->documents&DESK_APPLICATION_DOCUMENT_FALLBACK?1:0;
}
static inline unsigned desk_application_for_path(const char *path) {
    unsigned best=0,rank=0;
    for(unsigned id=1;id<=DESK_APP_COUNT;id++) {
        unsigned value=desk_application_document_rank(id,path);
        if(value>rank){best=id;rank=value;}
    }
    return best;
}
/* Return total candidates, preferred first, storing at most capacity IDs.
 * NULL/zero capacity queries are supported; no allocation or I/O occurs. */
static inline unsigned desk_application_handlers(const char *path,unsigned *ids,unsigned capacity) {
    unsigned preferred=desk_application_for_path(path),count=0;
    if(!preferred)return 0;
    if(ids && count<capacity)ids[count]=preferred;
    count++;
    for(unsigned id=1;id<=DESK_APP_COUNT;id++)if(id!=preferred && desk_application_document_rank(id,path)) {
        if(ids && count<capacity)ids[count]=id;
        count++;
    }
    return count;
}
static inline int desk_application_services_valid(unsigned id,unsigned services) {
    const struct desk_application_info *info=desk_application_info(id);
    return info && !(services&~info->services);
}
#endif
