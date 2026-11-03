#ifndef SRVROS_PROFILE_PATHS_H
#define SRVROS_PROFILE_PATHS_H

#include <stddef.h>
#include <srvros/path_limits.h>

/* Path policy only: these routines do not check ownership or access. */
enum srv_profile_path_kind {
    SRV_PROFILE_HOME,
    SRV_PROFILE_CONFIG,
    SRV_PROFILE_DATA,
    SRV_PROFILE_CACHE
};

/* Accept only absolute canonical ASCII paths representable by the guest.
 * A component may be at most SRV_NAME_MAX bytes; the whole path, including
 * its NUL, may be at most SRV_PATH_MAX bytes.  Root is canonical, but other
 * paths may not end in '/'.  No filesystem lookup or normalization occurs. */
static inline int srv_profile_home_valid(const char *home) {
    size_t i, component = 0, start = 1;
    if (!home || home[0] != '/') return 0;
    for (i = 1; i < SRV_PATH_MAX; ++i) {
        unsigned char c = (unsigned char)home[i];
        if (c == '\0' || c == '/') {
            if (!component) return c == '\0' && i == 1;
            if ((component == 1 && home[start] == '.') ||
                (component == 2 && home[start] == '.' && home[start + 1] == '.'))
                return 0;
            if (c == '\0') return 1;
            component = 0;
            start = i + 1;
        } else {
            if (c < 0x20 || c > 0x7e || c == '\\' || ++component > SRV_NAME_MAX)
                return 0;
        }
    }
    return 0;
}

/* Resolve from the explicit installation-local HOME.  On failure return 0 and
 * leave out untouched; on success return 1.  out may overlap home.  A derived
 * path that exceeds the guest path limit is rejected even if out is larger. */
static inline int srv_profile_path(char *out, size_t capacity, const char *home,
                                   enum srv_profile_path_kind kind) {
    char result[SRV_PATH_MAX];
    const char *suffix;
    size_t n = 0, j = 0;
    if (!out || !srv_profile_home_valid(home)) return 0;
    switch (kind) {
    case SRV_PROFILE_HOME: suffix = ""; break;
    case SRV_PROFILE_CONFIG: suffix = "/.config"; break;
    case SRV_PROFILE_DATA: suffix = "/.local/share"; break;
    case SRV_PROFILE_CACHE: suffix = "/.cache"; break;
    default: return 0;
    }
    while (home[n]) { result[n] = home[n]; ++n; }
    /* Root already provides the separator. */
    if (n == 1 && suffix[0] == '/') ++suffix;
    while (suffix[j]) {
        if (n >= SRV_PATH_MAX - 1) return 0;
        result[n++] = suffix[j++];
    }
    result[n] = '\0';
    if (capacity <= n) return 0;
    for (j = 0; j <= n; ++j) out[j] = result[j];
    return 1;
}
#endif
