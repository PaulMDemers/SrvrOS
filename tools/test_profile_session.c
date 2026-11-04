#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/desk_paths.h>

static const char *id="12345678-1234-4abc-8def-123456789abc";
static void clear(void) {
    const char *names[]={"HOME","SRVROS_DESKTOP_HOME","SRVROS_PROFILE_ID",
                         "XDG_CONFIG_HOME","XDG_DATA_HOME","XDG_CACHE_HOME"};
    for (unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++) unsetenv(names[i]);
}
static int has(struct srv_profile_environment *env,const char *value) {
    for(unsigned i=0;i<env->count;i++) if(!strcmp(env->envp[i],value)) return 1;
    return 0;
}
int main(void) {
    struct srv_profile_session session,copy;
    struct srv_profile_environment env;
    char home[97];
    assert(srv_profile_session_id_valid(id));
    assert(!srv_profile_session_id_valid("12345678-1234-1abc-8def-123456789abc"));
    assert(!srv_profile_session_id_valid("12345678-1234-4abc-7def-123456789abc"));
    assert(!srv_profile_session_id_valid("12345678-1234-4ABC-8def-123456789abc"));
    assert(!srv_profile_session_id_valid("short"));
    memset(home,'a',sizeof(home));home[0]='/';home[95]=0;
    assert(srv_profile_session_init(&session,home,id));
    home[95]='a';home[96]=0;
    assert(!srv_profile_session_init(&session,home,id));
    assert(!srv_profile_session_init(&session,"/a/../b",id));
    assert(srv_profile_session_init(&session,"/fat/profiles/example",id));
    copy=session;memset(&session,0,sizeof(session));
    assert(!strcmp(copy.home,"/fat/profiles/example") && !strcmp(copy.id,id));
    assert(srv_profile_environment_init(&env,&copy,"/fat/bin"));
    assert(env.count==7 && !env.envp[env.count]);
    assert(has(&env,"PATH=/fat/bin") && has(&env,"HOME=/fat/profiles/example"));
    assert(has(&env,"SRVROS_DESKTOP_HOME=/fat/profiles/example"));
    assert(has(&env,"SRVROS_PROFILE_ID=12345678-1234-4abc-8def-123456789abc"));
    assert(has(&env,"XDG_CONFIG_HOME=/fat/profiles/example/.config"));
    assert(has(&env,"XDG_DATA_HOME=/fat/profiles/example/.local/share"));
    assert(has(&env,"XDG_CACHE_HOME=/fat/profiles/example/.cache"));
    clear();assert(srv_profile_session_from_env(&session));assert(!strcmp(session.home,"/fat/home"));
    setenv("SRVROS_DESKTOP_HOME","/work/srvros-work",1);
    assert(srv_profile_session_from_env(&session));assert(!strcmp(session.home,"/work/srvros-work"));
    setenv("SRVROS_DESKTOP_HOME","/fat/profiles/example",1);
    assert(!srv_profile_session_from_env(&session));assert(!strcmp(desk_home(),""));
    setenv("SRVROS_PROFILE_ID",id,1);
    assert(srv_profile_session_from_env(&session));assert(!strcmp(session.home,"/fat/profiles/example"));
    setenv("HOME","/other",1);assert(!srv_profile_session_from_env(&session));
    unsetenv("SRVROS_DESKTOP_HOME");setenv("HOME","/fat/profiles/example",1);
    assert(srv_profile_session_from_env(&session));
    setenv("XDG_CONFIG_HOME","/elsewhere",1);assert(!srv_profile_session_from_env(&session));
    unsetenv("XDG_CONFIG_HOME");setenv("SRVROS_PROFILE_ID","",1);
    assert(!srv_profile_session_from_env(&session));
    clear();puts("profile session: ok");return 0;
}
