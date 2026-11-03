#ifndef SRVROS_PROFILE_SESSION_H
#define SRVROS_PROFILE_SESSION_H

#include <stdlib.h>
#include <string.h>
#include <srvros/profile_paths.h>

#define SRV_PROFILE_SESSION_HOME_MAX 95
#define SRV_PROFILE_SESSION_ID_LEN 36
#define SRV_PROFILE_ENV_CAPACITY 12
#define SRV_PROFILE_LEGACY_RAM_HOME "/fat/home"
#define SRV_PROFILE_LEGACY_WORK_HOME "/work/srvros-work"

struct srv_profile_session {
    char home[SRV_PROFILE_SESSION_HOME_MAX + 1];
    char id[SRV_PROFILE_SESSION_ID_LEN + 1];
};

/* Lowercase canonical UUIDv4, including the RFC 4122 variant. */
static inline int srv_profile_session_id_valid(const char *id) {
    unsigned i;
    if (!id) return 0;
    for (i = 0; i < SRV_PROFILE_SESSION_ID_LEN; ++i) {
        char c = id[i];
        if (!c) return 0;
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return 0;
        } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return !id[i] && id[14] == '4' &&
        (id[19] == '8' || id[19] == '9' || id[19] == 'a' || id[19] == 'b');
}

/* Copy into a self-contained session. Empty ID selects legacy mode. */
static inline int srv_profile_session_init(struct srv_profile_session *out,
                                            const char *home, const char *id) {
    struct srv_profile_session result = {{0}, {0}};
    size_t n;
    if (!out || !srv_profile_home_valid(home)) return 0;
    n = strlen(home);
    if (n > SRV_PROFILE_SESSION_HOME_MAX || (id && *id && !srv_profile_session_id_valid(id))) return 0;
    memcpy(result.home, home, n + 1);
    if (id && *id) memcpy(result.id, id, sizeof(result.id));
    else result.id[0] = 0;
    *out = result;
    return 1;
}

/* Explicit profile variables must agree. HOME alone remains a legacy shell
 * variable; with a profile ID it can recover context in a nested shell. */
static inline int srv_profile_session_from_env(struct srv_profile_session *out) {
    const char *id = getenv("SRVROS_PROFILE_ID");
    const char *desktop = getenv("SRVROS_DESKTOP_HOME");
    const char *home = getenv("HOME");
    struct srv_profile_session session;
    if (id) {
        char derived[160];
        const char *value;
        if (!srv_profile_session_id_valid(id) || (!desktop && !home) ||
            (desktop && home && strcmp(desktop, home)) ||
            !srv_profile_session_init(&session, desktop ? desktop : home, id)) return 0;
        value = getenv("XDG_CONFIG_HOME");
        if (!srv_profile_path(derived, sizeof(derived), session.home, SRV_PROFILE_CONFIG) || (value && strcmp(value, derived))) return 0;
        value = getenv("XDG_DATA_HOME");
        if (!srv_profile_path(derived, sizeof(derived), session.home, SRV_PROFILE_DATA) || (value && strcmp(value, derived))) return 0;
        value = getenv("XDG_CACHE_HOME");
        if (!srv_profile_path(derived, sizeof(derived), session.home, SRV_PROFILE_CACHE) || (value && strcmp(value, derived))) return 0;
    } else {
        if (desktop && strcmp(desktop, SRV_PROFILE_LEGACY_RAM_HOME) && strcmp(desktop, SRV_PROFILE_LEGACY_WORK_HOME)) return 0;
        if (!srv_profile_session_init(&session, desktop && !strcmp(desktop, SRV_PROFILE_LEGACY_WORK_HOME) ?
                                      SRV_PROFILE_LEGACY_WORK_HOME : SRV_PROFILE_LEGACY_RAM_HOME, NULL)) return 0;
    }
    if (!out) return 0;
    *out = session;
    return 1;
}

struct srv_profile_environment {
    char storage[7][160];
    char *envp[SRV_PROFILE_ENV_CAPACITY];
    unsigned count;
};

static inline int srv_profile_environment_add(struct srv_profile_environment *env,
                                               const char *key, const char *value) {
    size_t k = strlen(key), v = strlen(value);
    if (env->count >= 7 || k + 1 + v >= sizeof(env->storage[0])) return 0;
    memcpy(env->storage[env->count], key, k);
    env->storage[env->count][k] = '=';
    memcpy(env->storage[env->count] + k + 1, value, v + 1);
    env->envp[env->count] = env->storage[env->count];
    env->envp[++env->count] = NULL;
    return 1;
}

/* env owns all strings and pointers until exec returns. Append caller-owned
 * extras at envp[count], retaining a trailing NULL and respecting capacity. */
static inline int srv_profile_environment_init(struct srv_profile_environment *env,
                                                const struct srv_profile_session *session,
                                                const char *path) {
    char derived[160];
    struct srv_profile_session checked;
    if (!env || !session || !path || !srv_profile_session_init(&checked, session->home, session->id)) return 0;
    env->count = 0; env->envp[0] = NULL;
    if (!srv_profile_environment_add(env, "PATH", path) ||
        !srv_profile_environment_add(env, "HOME", checked.home) ||
        !srv_profile_environment_add(env, "SRVROS_DESKTOP_HOME", checked.home)) return 0;
    if (checked.id[0] && !srv_profile_environment_add(env, "SRVROS_PROFILE_ID", checked.id)) return 0;
    if (!srv_profile_path(derived, sizeof(derived), checked.home, SRV_PROFILE_CONFIG) || !srv_profile_environment_add(env, "XDG_CONFIG_HOME", derived) ||
        !srv_profile_path(derived, sizeof(derived), checked.home, SRV_PROFILE_DATA) || !srv_profile_environment_add(env, "XDG_DATA_HOME", derived) ||
        !srv_profile_path(derived, sizeof(derived), checked.home, SRV_PROFILE_CACHE) || !srv_profile_environment_add(env, "XDG_CACHE_HOME", derived)) return 0;
    return 1;
}
#endif
