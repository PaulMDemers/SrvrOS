#ifndef SRVROS_PROFILE_PREFERENCES_H
#define SRVROS_PROFILE_PREFERENCES_H
#include <srvros/desk_preferences.h>
#include <stdio.h>
#include <string.h>

#define SRV_PROFILE_PREF_MASK (DESK_PREF_TEXT|DESK_PREF_FORMAT|DESK_PREF_DATE|DESK_PREF_WALLPAPER|DESK_PREF_ACCENT|DESK_PREF_GLASS|DESK_PREF_STYLE)
#define SRV_PROFILE_LOCAL_MASK (DESK_PREF_ALL & ~SRV_PROFILE_PREF_MASK)

/* Strict flat JSON object of the six portable integer fields. Missing keys use
 * defaults. Decode is transactional and never modifies the destination on error. */
static inline int srv_profile_preferences_decode(struct desk_preferences *out,const void *bytes,size_t length) {
    const unsigned char *s=bytes;size_t i=0;unsigned seen=0;
    struct desk_preferences v=desk_preferences_default();
    if(!out || !s || length>4096) return -1;
#define PROFILE_WS() do { while(i<length && (s[i]==' ' || s[i]=='\n' || s[i]=='\r' || s[i]=='\t')) i++; } while(0)
    PROFILE_WS();if(i==length || s[i++]!='{') return -1;PROFILE_WS();
    if(i<length && s[i]=='}') i++;
    else for(;;) {
        size_t start;unsigned bit=0,n=0;
        if(i==length || s[i++]!='"') return -1;
        start=i;while(i<length && s[i]>='a' && s[i]<='z') i++;
        /* Keys contain underscores too; no escaping is accepted. */
        while(i<length && (s[i]=='_' || (s[i]>='a' && s[i]<='z'))) i++;
        size_t keylen=i-start;
        if(i==length || s[i++]!='"') return -1;
#define PROFILE_KEY(name,b) if(keylen==sizeof(name)-1 && !memcmp(s+start,name,sizeof(name)-1)) bit=b
        PROFILE_KEY("text_size",1);else PROFILE_KEY("clock_flags",2);else PROFILE_KEY("wallpaper",4);
        else PROFILE_KEY("theme_accent",8);else PROFILE_KEY("glass_level",16);else PROFILE_KEY("theme_style",32);
#undef PROFILE_KEY
        if(!bit || (seen&bit)) return -1;seen|=bit;
        PROFILE_WS();if(i==length || s[i++]!=':') return -1;PROFILE_WS();
        if(i==length || s[i]<'0' || s[i]>'9') return -1;
        if(s[i]=='0' && i+1<length && s[i+1]>='0' && s[i+1]<='9') return -1;
        do { if(n>1000) return -1;n=n*10+(unsigned)(s[i++]-'0'); } while(i<length && s[i]>='0' && s[i]<='9');
        switch(bit) {case 1:v.text_size=n;break;case 2:v.clock_flags=n;break;case 4:v.wallpaper=n;break;
            case 8:v.theme_accent=n;break;case 16:v.glass_level=n;break;case 32:v.theme_style=n;break;}
        PROFILE_WS();if(i==length) return -1;
        if(s[i]=='}') { i++;break; }
        if(s[i++]!=',') return -1;PROFILE_WS();
    }
    PROFILE_WS();if(i!=length || !desk_preferences_valid(&v)) return -1;
    *out=v;return 0;
#undef PROFILE_WS
}
static inline int srv_profile_preferences_encode(const struct desk_preferences *p,char *out,size_t capacity) {
    if(!p || !out || !desk_preferences_valid(p)) return -1;
    int n=snprintf(out,capacity,"{\"clock_flags\":%u,\"glass_level\":%u,\"text_size\":%u,\"theme_accent\":%u,\"theme_style\":%u,\"wallpaper\":%u}\n",
        p->clock_flags,p->glass_level,p->text_size,p->theme_accent,p->theme_style,p->wallpaper);
    return n<0 || (size_t)n>=capacity?-1:n;
}
#endif
