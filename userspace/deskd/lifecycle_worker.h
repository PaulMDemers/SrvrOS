#ifndef DESKD_LIFECYCLE_WORKER_H
#define DESKD_LIFECYCLE_WORKER_H
#include <pthread.h>
/* The UI owns admission and publication. One persistent worker owns blocking
 * syscalls. Release/acquire transfers each record; no scene pointers cross the
 * boundary. A discarded allocation keeps its budget until unmapping completes. */
#define LIFE_MEMORY_SLOTS 256
#define LIFE_LAUNCH_SLOTS 16
#define LIFE_IDLE 0
#define LIFE_QUEUED 1
#define LIFE_DONE 2
#define LIFE_RETIRED 3
struct life_memory {
    unsigned state,discard,width,height;
    uint32_t *pixels;
    uint64_t elapsed;
};
struct life_launch {
    unsigned state;
    int application,child_slot;
    char path[160],home[160];
    long pid;
    uint64_t elapsed;
};
struct lifecycle_worker {
    pthread_t thread;
    unsigned stop;
    struct life_memory memory[LIFE_MEMORY_SLOTS];
    struct life_launch launch[LIFE_LAUNCH_SLOTS];
};
static unsigned life_state(const unsigned *state) { return __atomic_load_n(state,__ATOMIC_ACQUIRE); }
static void life_publish(unsigned *state,unsigned value) { __atomic_store_n(state,value,__ATOMIC_RELEASE); }
static int life_reserved(const struct lifecycle_worker *w,int slot) {
    if(w)for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++)if(life_state(&w->launch[n].state) && w->launch[n].child_slot==slot)return 1;
    return 0;
}
static void life_work(struct lifecycle_worker *w) {
    for(unsigned n=0;n<LIFE_MEMORY_SLOTS;n++) {
        struct life_memory *m=&w->memory[n];
        if(life_state(&m->state)==LIFE_QUEUED) {
            uint64_t begin=(uint64_t)srv_monotonic_us();
            m->pixels=desk_pixels_alloc(m->width,m->height);
            m->elapsed=(uint64_t)srv_monotonic_us()-begin;
            life_publish(&m->state,LIFE_DONE);
        }
        if(life_state(&m->state)==LIFE_DONE && life_state(&m->discard)) {
            desk_pixels_free(m->pixels,m->width,m->height);m->pixels=NULL;
            life_publish(&m->state,LIFE_RETIRED);
        }
    }
    for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++) {
        struct life_launch *j=&w->launch[n];
        if(life_state(&j->state)!=LIFE_QUEUED)continue;
        const struct desk_application_info *info=desk_application_info(j->application);
        char *argv[]={(char *)info->program,(char *)info->role,j->path,NULL};
        char home[192],desktop_home[192];
        snprintf(home,sizeof(home),"HOME=%s",j->home);
        snprintf(desktop_home,sizeof(desktop_home),"SRVROS_DESKTOP_HOME=%s",j->home);
        char *env[]={"PATH=/fat/bin:/:/fat",home,desktop_home,NULL};
        struct srv_exec_request r={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
            .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
        uint64_t begin=(uint64_t)srv_monotonic_us();j->pid=srv_exec(&r);
        j->elapsed=(uint64_t)srv_monotonic_us()-begin;
        life_publish(&j->state,LIFE_DONE);
    }
}
static void *life_thread(void *arg) {
    struct lifecycle_worker *w=arg;
    while(!life_state(&w->stop)) {life_work(w);srv_sleep_ticks(1);}
    /* Shutdown joins before stack/session storage or mappings are released. */
    life_work(w);return NULL;
}
static int life_start(struct lifecycle_worker *w) { return pthread_create(&w->thread,NULL,life_thread,w); }
static int life_stop(struct lifecycle_worker *w) { life_publish(&w->stop,1);return pthread_join(w->thread,NULL); }
#endif
