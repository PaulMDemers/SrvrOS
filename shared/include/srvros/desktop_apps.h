#ifndef SRVROS_DESKTOP_APPS_H
#define SRVROS_DESKTOP_APPS_H
#include <srvros/desktop_protocol.h>
/* One launch catalog for the launcher, dock and whitelisted executable path.
 * A zero dock position keeps a tool available through the launcher and Files. */
struct desk_application_info { const char *title,*role,*program;unsigned dock; };
static inline const struct desk_application_info *desk_application_info(unsigned id) {
    static const struct desk_application_info catalog[]={
        {"Files","files","/fat/bin/desktools",1},
        {"Text","text","/fat/bin/desktools",2},
        {"Calculator","calculator","/fat/bin/desktools",3},
        {"System","system","/fat/bin/desktools",4},
        {"Settings","settings","/fat/bin/desktools",5},
        {"Terminal","terminal","/fat/bin/deskterm",6},
        {"Preview","preview","/fat/bin/deskview",0},
        {"Help","help","/fat/bin/deskhelp",0},
        {"Clock","clock","/fat/bin/deskclock",0},
    };
    _Static_assert(sizeof(catalog)/sizeof(*catalog)==DESK_APP_COUNT,"application catalog coverage");
    return id>=1 && id<=DESK_APP_COUNT?&catalog[id-1]:0;
}
static inline unsigned desk_dock_count(void) {
    unsigned count=0;for(unsigned id=1;id<=DESK_APP_COUNT;id++) if(desk_application_info(id)->dock>count) count=desk_application_info(id)->dock;return count;
}
static inline unsigned desk_application_for_path(const char *path) {
    const char *extension=0;
    for(const char *p=path;*p;p++) { if(*p=='/') extension=0;else if(*p=='.') extension=p+1; }
    if(!extension) return DESK_APP_TEXT;
    static const char *images[]={"png","jpg","jpeg","bmp"};
    for(unsigned i=0;i<sizeof(images)/sizeof(*images);i++) {
        const char *a=extension,*b=images[i];
        while(*a && *b) { unsigned c=(unsigned char)*a;if(c>='A' && c<='Z') c+='a'-'A';if(c!=(unsigned char)*b) break;a++;b++; }
        if(!*a && !*b) return DESK_APP_PREVIEW;
    }
    return DESK_APP_TEXT;
}
#endif
